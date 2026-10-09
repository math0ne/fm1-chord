/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* The HiChord key layer (CHRD HI, chord.c): the white keys are scale degrees of the track's ROOT / SCALE
 * (DEGREE layout: C4 is the tonic, F3..B3 the degrees below it an octave down, C5..G5 an octave up;
 * PIANO layout: the key's own letter is the root, snapped onto the scale), the black keys the eight
 * modifier directions and three gestures:
 *   C#4 up, D#4 up-right, F#4 right, G#4 down-right, A#4 down, C#5 down-left, D#5 left, F#5 up-left
 *   F#3 INVERT (with chord keys held: their inversion cycles), G#3 LOCK (with a chord key and a
 *   direction held: the key keeps that chord; again: unlocked), A#3 HOLD (latch: a chord key let go keeps
 *   sounding until HOLD is tapped again).
 * A direction is momentary: pressed before or after the chord key, it reshapes every chord key held
 * (the notes that stay keep sounding, the others are exchanged) and lets go with it. BASS SLASH: the
 * first chord key held is the bass of the chords the next keys play (the screen: "Em/C").
 * Both layouts sound an octave below the printed key (HC_BASE: the C4 key's tonic is C3), as the HiChord
 * does, so a chord with its bass two octaves down stays inside the keyboard's range.
 *
 * Every HiChord setting of a track lives in hc.t[] (hc_trk_t) and is projected onto the Felucca parameters
 * it stands for by hc_apply (the envelope presets, glide, vibrato, drive, the sends and the bus settings,
 * the voice mode). The play modes (PLAY STRUM LEAD DRONE ARP REPEAT) sit between the keys and key_on /
 * key_off and run on the block clock (hc_tick, from events_block).
 * Runs where chord.c runs: the audio ISR. The UI (hui.c) writes hc.t[] and calls hc_apply. */
#include "harmony.c"

enum { HL_DEGREE, HL_PIANO };                            /* LAYOUT */
enum { HB_OFF, HB_ROOT, HB_SLASH };                      /* BASS (HiChord CC 45) */
enum { HP_PLAY, HP_STRUM, HP_LEAD, HP_DRONE, HP_ARP, HP_REPEAT, HP_SEQ, HP_DRUM, HP_DRUMLOOP, HP_HIRO, HP_EAR, HP_MIXER,
       HP_COUNT };                                     /* the modes, in the HiChord's order */
enum { HST_SLOW, HST_MED, HST_FAST };                    /* STRUM speed */
enum { HA_UP, HA_DOWN, HA_UPDN, HA_DNUP, HA_RND, HA_PICK, HA_COUNT };   /* ARP pattern */
enum { HR_1_1, HR_1_2, HR_1_4, HR_1_8, HR_1_16, HR_1_16T, HR_1_32, HR_SW8, HR_SW16, HR_COUNT };   /* ARP / REPEAT rate */
enum { HAL_ARP, HAL_CHORD, HAL_RHYTHM };                 /* ARP layering: ARP ONLY, CHORD+ARP, RHYTHM+ARP */
enum { HE_LONG, HE_SHORT, HE_SWELL, HE_PLUCK, HE_TOUCH, HE_SUSTAIN, HE_KEYS, HE_COUNT };   /* ADSR presets */
enum { HRV_OFF, HRV_ROOM, HRV_HALL, HRV_PLATE, HRV_SPRING, HRV_AMBIENT, HRV_COUNT };
enum { HDL_OFF, HDL_1_4, HDL_1_8, HDL_1_16, HDL_1_16T, HDL_1_32, HDL_COUNT };
enum { HCH_OFF, HCH_LIGHT, HCH_WARM, HCH_WIDE, HCH_LUSH, HCH_COUNT };
enum { HFL_OFF, HFL_SLOW, HFL_JET, HFL_DEEP, HFL_METAL, HFL_COUNT };
enum { HTR_OFF, HTR_1_4, HTR_1_8, HTR_1_16, HTR_1_16T, HTR_1_32, HTR_COUNT };
enum { HLF_OFF, HLF_LOW, HLF_MED, HLF_HIGH, HLF_COUNT };
enum { HGL_OFF, HGL_SHORT, HGL_MEDIUM, HGL_LONG, HGL_COUNT };
enum { HDR_OFF, HDR_TUBE, HDR_DRIVE, HDR_DIST, HDR_FUZZ, HDR_COUNT };
enum { HTP_OFF, HTP_LOFI, HTP_VINYL, HTP_TAPE, HTP_COUNT };
static const char *const HL_NAME[2] = {"DEGREE", "PIANO"};
static const char *const HB_NAME[3] = {"OFF", "ROOT", "SLASH"};
static const char *const HP_NAME[HP_COUNT] = {"PLAY", "STRUM", "LEAD", "DRONE", "ARP", "REPEAT", "SEQUENCER", "DRUM",
                                              "DRUM LOOP", "CHORD HIRO", "EAR TRAINER", "MIXER"};
static const char *const HST_NAME[3] = {"SLOW", "MEDIUM", "FAST"};
static const char *const HA_NAME[HA_COUNT] = {"UP", "DOWN", "UP/DOWN", "DOWN/UP", "RANDOM", "FINGERPICK"};
static const char *const HR_NAME[HR_COUNT] = {"1/1", "1/2", "1/4", "1/8", "1/16", "1/16T", "1/32", "SWING 8", "SWING 16"};
static const char *const HAL_NAME[3] = {"ARP ONLY", "CHORD+ARP", "RHYTHM+ARP"};
static const char *const HE_NAME[HE_COUNT] = {"LONG", "SHORT", "SWELL", "PLUCK", "TOUCH", "SUSTAIN", "KEYS"};
static const char *const HRV_NAME[HRV_COUNT] = {"OFF", "ROOM", "HALL", "PLATE", "SPRING", "AMBIENT"};
static const char *const HDL_NAME[HDL_COUNT] = {"OFF", "1/4", "1/8", "1/16", "1/16T", "1/32"};
static const char *const HCH_NAME[HCH_COUNT] = {"OFF", "LIGHT", "WARM", "WIDE", "LUSH"};
static const char *const HFL_NAME[HFL_COUNT] = {"OFF", "SLOW", "JET", "DEEP", "METAL"};
static const char *const HTR_NAME[HTR_COUNT] = {"OFF", "1/4", "1/8", "1/16", "1/16T", "1/32"};
static const char *const HLF_NAME[HLF_COUNT] = {"OFF", "LOW", "MED", "HIGH"};
static const char *const HGL_NAME[HGL_COUNT] = {"OFF", "SHORT", "MEDIUM", "LONG"};
static const char *const HDR_NAME[HDR_COUNT] = {"OFF", "TUBE", "DRIVE", "DIST", "FUZZ"};
static const char *const HTP_NAME[HTP_COUNT] = {"OFF", "LOFI", "VINYL", "TAPE"};
/* the ADSR presets: attack, decay, release in ms, sustain in percent (the Rev 3.0 manual) */
static const uint16_t HE_ADSR[HE_COUNT][4] = {
    {800, 1000, 70, 2000}, {100, 100, 100, 1000}, {800, 300, 80, 2000}, {5, 80, 0, 180},
    {50, 260, 66, 450}, {200, 300, 85, 3000}, {5, 1200, 55, 700},
};
#define HC_BASE 48                                       /* the tonic of octave 0: C3 (HC_BASE + ROOT) */
#define HC_NOKEY 255u
#define HC_NBLACK 11u
/* black key place (F#3 = 0 .. F#5 = 10) -> its function: HD_* a direction, HG_* a gesture */
enum { HG_INVERT = 8, HG_LOCK, HG_HOLD };
static const uint8_t HC_BLACK_FN[HC_NBLACK] = {HG_INVERT, HG_LOCK, HG_HOLD, HD_UP, HD_UR, HD_RIGHT, HD_DR, HD_DOWN,
                                               HD_DL, HD_LEFT, HD_UL};

typedef struct {                 /* a track's HiChord settings (a preset stores them) */
    uint8_t sound;               /* the sound (hui.c HC_SOUNDS) */
    uint8_t play;                /* HP_* */
    uint8_t strum;               /* HST_* */
    uint8_t arp_pat, arp_rate, arp_layer;   /* HA_*, HR_*, HAL_* */
    uint8_t env;                 /* HE_* */
    uint8_t rev, dly, cho, flg, trem, lfo, glide, drive, tape;   /* the effects */
    uint8_t stereo, filt, hp;    /* STEREO, the filter wheel, HI-PASS: on / off */
    uint8_t cutoff;              /* the filter wheel: 0..127 */
    uint8_t atk, rel;            /* ATTACK / RELEASE fine-tuned (Felucca values), 0 = the envelope preset's */
    uint8_t mode;                /* HM_*: the modifier table (JOYSTICK) */
    uint8_t bass;                /* HB_* */
    uint8_t voices;              /* HV_* */
    uint8_t vlead;               /* VOICE LEADING */
    uint8_t layout;              /* HL_* */
    uint8_t kit;                 /* DRUM / DRUM LOOP: Felucca's DRUM KIT (STD HAND CYM H+CYM) */
    uint8_t dl_style, dl_var;    /* DRUM LOOP: the style and its variation */
    uint8_t sound_saved;         /* the sound before DRUM mode took the track (hui.c) */
    uint8_t rev_amt, dly_amt, cho_amt, flg_amt, trem_amt;   /* KNOB 4 on the effect's row (hui.c): the amount,
                                                          * 1..127; 0 = the type's own (the HiChord's app CCs) */
} hc_trk_t;

typedef struct {                 /* an arp pattern: two notes a step, slot roles 0..4 (ROOT 3RD 5TH 7TH 9TH/11TH) */
    int8_t note[8][2];           /* -1 = rest */
    int8_t oct[8][2];            /* octaves */
    uint8_t len;                 /* 2..8 */
} hc_arp_t;

static struct {
    hc_trk_t t[NTRK];
    uint8_t hold;                /* HOLD latched */
    uint8_t dir;                 /* the direction held (the last pressed), HD_NONE */
    uint8_t dstack[8], ndir;     /* directions held, in press order */
    uint8_t inv[27];             /* per key: inversion 0..2 */
    struct { uint8_t on, q; int8_t roff; } lock[27];   /* per key: a locked chord */
    uint8_t order[27], nheld;    /* chord keys held (sounding), in press order: the first is SLASH's bass */
    uint32_t latched;            /* HOLD: keys let go that still sound, a bit per key */
    uint32_t black;              /* black keys down that are the layer's, a bit per key */
    uint8_t dirty;               /* the held chords must be revoiced (a direction, LOCK, INVERT changed) */
    uint8_t release;             /* HOLD went off: the latched keys not held end (hc_block) */
    struct { uint8_t n; uint8_t note[HS_N]; } last[NTRK];   /* the upper notes last played per track (VOICE LEADING) */
    char name[12];               /* the name of the chord last built (the display) */
    uint8_t cur_key;             /* the key chord_make builds for (keyboard_block), HC_NOKEY: a MIDI / step note */
    uint8_t cur_dir, cur_q;      /* what the last chord_make applied: the direction, the quality (the display) */
    int32_t cur_root;
    hchord_t cur;                /* the chord last built (its slots: STRUM order, the ARP's roles) */
    /* the play modes */
    uint32_t clock;              /* samples (hc_tick) */
    struct { uint8_t n, i; uint8_t note[HS_N]; uint32_t due; } strum[27];   /* a key's notes still to roll */
    struct {
        uint32_t pos, len;       /* samples into the step, the step's length */
        uint8_t step;            /* ARP: the pattern step; REPEAT: the gate phase (bit 0) */
        uint8_t gated;           /* REPEAT / RHYTHM: the chord is gated off now */
        uint8_t an, anote[2];    /* the arp notes sounding */
        uint32_t aoff;           /* when they end (clock) */
        hchord_t chord;          /* the chord the arp plays (the last key pressed) */
        uint8_t chord_on;        /* a chord key is held (or latched) */
        uint32_t rnd;
    } run[NTRK];
    uint8_t arp_user_on;         /* FINGERPICK: the user's pattern (hc.arp_user) instead of the built-in one */
    hc_arp_t arp_user;
} hc;

/* the Felucca value of a time in ms (the nearest value of the table) */
static uint32_t hc_time_v(uint32_t ms)
{
    uint32_t v;
    for (v = 0; v < 127u && TIME_MS_X10[v] < ms * 10u; v++)
        ;
    if (v && TIME_MS_X10[v] - ms * 10u > ms * 10u - TIME_MS_X10[v - 1u])
        v--;
    return v;
}

static void hc_trk_defaults(hc_trk_t *c)
{
    memset(c, 0, sizeof *c);
    c->sound = 1;                                        /* SAW */
    c->arp_rate = HR_1_8;
    c->env = HE_LONG;
    c->stereo = 1;
    c->cutoff = 127;
    c->voices = HV_8;
    c->bass = HB_SLASH;                                  /* (the HiChord: OFF; the owner's choice: slash chords from the start) */
}

static void hcs_key_write(const track_t *t, uint32_t k);              /* hcseq.c */
static void hcs_rest_write(void);
static void hcs_stop(track_t *t);
static void hcd_key(track_t *t, uint32_t k, int down);
static void hcd_loop_start(void);
static void hcd_loop_stop(void);
static void hcs_tick(uint32_t n);
static void hcm_key(uint32_t k);                                       /* hcseq.c: MIXER keys */
static void hcs_mode_left(track_t *t);                                 /* hcseq.c: leaving a mode */
static void hcg_hiro_key(track_t *t, uint32_t k);                      /* hcgame.c */
static void hcg_ear_key(track_t *t, uint32_t k);
static void hcg_ear_replay(void);
static void hcg_tick(uint32_t n);
static void hcg_mode_left(track_t *t);

static void hc_init(void)
{
    uint32_t i;
    memset(&hc, 0, sizeof hc);
    hc.dir = HD_NONE;
    hc.cur_dir = HD_NONE;
    hc.cur_key = HC_NOKEY;
    for (i = 0; i < NTRK; i++) {
        hc_trk_defaults(&hc.t[i]);
        hc.run[i].rnd = 0x2545F491u + i;
    }
}

/* 1 = track t plays HiChord keys (a kit too: DRUM / DRUM LOOP / MIXER take the keys as well) */
static int hc_on(const track_t *t) { return t->p[P_CHRD] == CH_HI; }
static hc_trk_t *hc_of(const track_t *t) { return &hc.t[trk_index(t) % NTRK]; }

/* ----------------------------------------------------- hc_apply --- */
/* the Felucca parameters a track's HiChord settings stand for. The sound itself (the engine and its preset) is
 * the UI's (hui.c hc_sound_load, which calls this after). The master filter, STEREO partners, the flanger, the
 * tremolo's own LFO and TAPE are fx.c / voice.c extensions that read hc.t[] directly. */
static void hc_apply(track_t *t)
{
    const hc_trk_t *c = hc_of(t);
    static const uint8_t GLIDE_V[HGL_COUNT] = {0, 30, 55, 85};
    static const uint8_t DRIVE_V[HDR_COUNT] = {0, 28, 56, 90, 127};
    static const uint8_t CHO_SEND[HCH_COUNT] = {0, 40, 60, 85, 110}, CHO_RATE[HCH_COUNT] = {40, 40, 30, 50, 60},
                         CHO_DEPTH[HCH_COUNT] = {60, 30, 60, 90, 127};
    static const uint8_t REV_SEND[HRV_COUNT] = {0, 50, 65, 60, 60, 90}, REV_TYPE[HRV_COUNT] = {0, 0, 0, 0, 1, 0},
                         REV_SIZE[HRV_COUNT] = {90, 60, 110, 90, 80, 127}, REV_DAMP[HRV_COUNT] = {60, 70, 40, 20, 50, 60};
    static const uint8_t DLY_DIV[HDL_COUNT] = {1, 0, 1, 2, 5, 3};   /* N_DIV: 1/4 1/8 1/16 1/32 8T 16T .. */
    uint32_t i;
    /* the envelope */
    t->p[P_ATK] = (int16_t)hc_time_v(HE_ADSR[c->env % HE_COUNT][0]);
    t->p[P_DEC] = (int16_t)hc_time_v(HE_ADSR[c->env % HE_COUNT][1]);
    t->p[P_SUS] = (int16_t)(HE_ADSR[c->env % HE_COUNT][2] * 127u / 100u);
    t->p[P_REL] = (int16_t)hc_time_v(HE_ADSR[c->env % HE_COUNT][3]);
    if (c->atk)                                          /* fine-tuned (the wheel with GRAY / YELLOW held) */
        t->p[P_ATK] = c->atk;
    if (c->rel)
        t->p[P_REL] = c->rel;
    if (c->play == HP_REPEAT && t->p[P_REL] > (int16_t)hc_time_v(200))   /* REPEAT caps the release at 200 ms */
        t->p[P_REL] = (int16_t)hc_time_v(200);
    /* glide, vibrato, tremolo (one LFO: the tremolo's tempo rate when it is on, else ~5.5 Hz) */
    t->p[P_GLIDE] = GLIDE_V[c->glide % HGL_COUNT];
    t->p[P_GLMODE] = 0;
    t->p[P_LWAVE] = 0;
    t->p[P_LFADE] = 0;
    t->p[P_LD_PIT] = (int16_t)(c->lfo == HLF_OFF ? 0 : c->lfo == HLF_LOW ? 2 : c->lfo == HLF_MED ? 4 : 7);
    t->p[P_LD_AMP] = (int16_t)(c->trem ? (c->trem_amt ? c->trem_amt : 90) : 0);
    if (c->trem) {
        static const uint8_t TR_DEN[HTR_COUNT] = {1, 1, 2, 4, 6, 8};
        uint32_t hz100 = (uint32_t)song.g[G_BPM] * 100u / 60u * TR_DEN[c->trem % HTR_COUNT], v;
        for (v = 0; v < 127u && LFO_HZ_X100[v] < hz100; v++)
            ;
        t->p[P_LRATE] = (int16_t)v;
    } else {
        t->p[P_LRATE] = 82;                              /* ~5.5 Hz */
    }
    /* drive, the sends, the buses */
    t->p[P_DIST] = DRIVE_V[c->drive % HDR_COUNT];
    t->p[P_CHOR] = (int16_t)(c->cho ? (c->cho_amt ? c->cho_amt : CHO_SEND[c->cho % HCH_COUNT]) : 0);
    t->p[P_DLY] = (int16_t)(c->dly ? (c->dly_amt ? c->dly_amt : 55) : 0);
    t->p[P_REV] = (int16_t)(c->rev ? (c->rev_amt ? c->rev_amt : REV_SEND[c->rev % HRV_COUNT]) : 0);
    if (c->cho) {
        song.g[G_CRATE] = CHO_RATE[c->cho % HCH_COUNT];
        song.g[G_CDEPTH] = CHO_DEPTH[c->cho % HCH_COUNT];
    }
    if (c->rev) {
        song.g[G_RTYPE] = REV_TYPE[c->rev % HRV_COUNT];
        song.g[G_RSIZE] = REV_SIZE[c->rev % HRV_COUNT];
        song.g[G_RDAMP] = REV_DAMP[c->rev % HRV_COUNT];
    }
    if (c->dly) {
        song.g[G_DTIME] = DLY_DIV[c->dly % HDL_COUNT];
        song.g[G_DFDBK] = 55;
    }
    /* the voice mode: LEAD is one voice with glide between held keys; the rest POLY. Felucca's own arp off */
    if (ENGINES[eng_idx(t->eng_req)] == &ENG_DRUM)      /* a drum mode (hui.c switched the engine): the kit */
        t->p[P_E0] = (int16_t)(c->kit % 4u);
    t->p[P_VOICE] = (int16_t)(c->play == HP_LEAD ? V_LEGATO : V_POLY);
    t->p[P_AMODE] = 0;
    t->p[P_AHOLD] = 0;
    t->p[P_CHRD] = CH_HI;
    for (i = 0; i < 4u; i++)                             /* the matrix: nothing (the sound is the preset's) */
        t->p[P_M1SRC + 3u * i] = 0;
    /* STEREO partners (voice.c, fx.c): not in LEAD (one centred voice, as the HiChord's 3.0 LEAD) */
    trk_pair[trk_index(t)] = (uint8_t)(c->stereo && c->play != HP_LEAD);
    if (t == TSEL) {                                     /* the master effects follow the live track (hcfx.c) */
        hcfx.filt = c->filt;
        hcfx.hp = c->hp;
        hcfx.flg = c->flg;
        hcfx.flg_amt = c->flg_amt ? c->flg_amt : 64;
        hcfx.tape = c->tape;
        hcfx.cutoff = c->cutoff;
    }
}

/* the scale the degrees walk: the track's, CHR as major */
static uint32_t hc_mask(const track_t *t)
{
    uint32_t m = scale_mask(t);
    return m == 0xFFFu ? SCALE_MASK[1] : m;
}

/* white key k -> its degree (0 = the tonic) and *oct its octave from the key's place on the keyboard */
static uint32_t hc_degree_of_key(const track_t *t, uint32_t k, int32_t *oct)
{
    uint32_t p = key_place(k);                           /* white 0..15 from F3 */
    *oct = (int32_t)((p + 3u) / 7u) - 1;
    if (hc_of(t)->layout == HL_PIANO) {                  /* the key's letter: its degree in the scale */
        static const uint8_t WPC[7] = {5, 7, 9, 11, 0, 2, 4};   /* F G A B C D E */
        return hs_degree_of(hc_mask(t), (uint32_t)t->p[P_ROOT], WPC[p % 7u]);
    }
    return (p + 3u) % 7u;
}

/* the root note of key k's chord as it is now (kb_note: step entry, LEDs; chord_make builds the rest) */
static uint32_t hc_root_note(const track_t *t, uint32_t k)
{
    int32_t oct, r;
    uint32_t d = hc_degree_of_key(t, k, &oct);
    hs_degree_chord(hc_mask(t), d, &r);
    r += t->p[P_ROOT];
    if (hc_of(t)->layout == HL_PIANO)                    /* the key's own pitch class, in the key's octave */
        r %= 12;
    return (uint32_t)clamp(HC_BASE + r + 12 * (oct + song.octave) + t->p[P_TRANS], 0, 127);
}

/* the bass of a chord on root for key k (HC_NOKEY: a MIDI note): -1 none, else its note */
static int32_t hc_bass_of(const track_t *t, uint32_t k, int32_t root)
{
    int32_t b;
    const hc_trk_t *c = hc_of(t);
    if (c->bass == HB_OFF)
        return -1;
    b = root - 24;
    if (c->bass == HB_SLASH && k != HC_NOKEY && hc.nheld && hc.order[0] != k)
        b = (int32_t)kb_note[hc.order[0]] - 24;          /* the first key held: its root (hc_slash_take) */
    while (b < 24)
        b += 12;
    return b;
}

/* the chord of track t on root note (the degree it falls on; the key hc.cur_key's inversion and lock) ->
 * out (ascending, at most CHORD_MAX), *rp its root, *maskp its tones above the root; the number of notes */
static uint32_t hc_make(const track_t *t, uint32_t root, uint8_t *out, int32_t *rp, uint16_t *maskp)
{
    const hc_trk_t *c = hc_of(t);
    uint32_t k = hc.cur_key, mask = hc_mask(t), tonic = (uint32_t)t->p[P_ROOT], d, q, i, n, inv, ti = trk_index(t);
    int32_t r, roff = 0, oct, bass;
    if (k < 27u) {
        d = hc_degree_of_key(t, k, &oct);
        q = hs_degree_chord(mask, d, &r);
        r = (int32_t)root;                               /* (hc_root_note: the same degree, placed) */
    } else {                                             /* a MIDI note or a step: the degree it falls on */
        r = scale_snap(t, (int32_t)root);
        d = hs_degree_of(mask, tonic, (uint32_t)(r + 1200) % 12u);
        q = hs_degree_chord(mask, d, &oct);              /* (oct: the root offset, unused) */
    }
    if (k < 27u && hc.lock[k].on) {                      /* LOCK: the key's chord */
        q = hc.lock[k].q;
        r += hc.lock[k].roff;
    }
    q = hq_modify(c->mode, hc.dir, q, &roff);
    r += roff;
    bass = hc_bass_of(t, k, r);
    inv = k < 27u ? hc.inv[k] : 0u;
    if (c->vlead && hc.last[ti].n) {
        int32_t o;
        inv = hs_lead(r, q, bass, c->voices, hc.last[ti].note, hc.last[ti].n, &o);
        r += o;
        if (bass >= 0)
            bass += o;
    }
    hs_voice(&hc.cur, r, q, bass, c->voices, inv);
    eng_hc_bass[ti % NPART] = (int16_t)bass;            /* the HICHORD engine: the bass slot's own wave */
    {                                                    /* the name: an inversion with no bass voice is named over
                                                          * its lowest note (C/E, C/G), as written; a bass voice is
                                                          * the lowest and names itself (ROOT: none, SLASH: /bass) */
        int32_t nb = bass;
        if (bass < 0 && inv && hc.cur.n)
            nb = hc.cur.note[0];
        hs_name(hc.name, tonic, r, q, nb);
    }
    hc.cur_dir = hc.dir;
    hc.cur_q = (uint8_t)q;
    hc.cur_root = r;
    hc.last[ti].n = 0;
    for (i = 0; i < hc.cur.n && hc.last[ti].n < HS_N; i++)   /* the upper structure, for the next chord's leading */
        if ((int32_t)hc.cur.note[i] != bass)
            hc.last[ti].note[hc.last[ti].n++] = hc.cur.note[i];
    *rp = clamp(r, 0, 127);
    for (i = 0, *maskp = 1; i < HQ_NIV; i++)
        if (HQ_IV[q][i] >= 0)
            *maskp |= (uint16_t)(1u << ((uint32_t)HQ_IV[q][i] % 12u));
    n = hc.cur.n < CHORD_MAX ? hc.cur.n : CHORD_MAX;
    for (i = 0; i < n; i++)
        out[i] = hc.cur.note[i];
    return n;
}

/* -------------------------------------------------- the key layer --- */
static uint32_t chord_build(track_t *t, uint32_t root, uint8_t *out);   /* chord.c */
static void input_on(track_t *t, uint32_t note, uint32_t vel);     /* seq.c */
static void input_off(track_t *t, uint32_t note);
static void key_on(uint32_t k, track_t *t);
static void key_off(uint32_t k, track_t *t);
static int midi_local_held(const track_t *t, uint32_t note);         /* midi_control.c */
static void midi_out_event(uint32_t pkt);                            /* usb.c */

static void hc_note_on(track_t *t, uint32_t note)
{
    input_on(t, note, 100);
    midi_out_event(0x09u | (0x90u | trk_midi_ch(trk_index(t))) << 8 | note << 16 | 100u << 24);
}
static void hc_note_off(track_t *t, uint32_t note)
{
    input_off(t, note);
    midi_out_event(0x08u | (0x80u | trk_midi_ch(trk_index(t))) << 8 | note << 16);
}

static void hc_order_add(uint32_t k)
{
    uint32_t i;
    for (i = 0; i < hc.nheld; i++)
        if (hc.order[i] == k)
            return;
    if (hc.nheld < 27u)
        hc.order[hc.nheld++] = (uint8_t)k;
}

static void hc_order_remove(uint32_t k)
{
    uint32_t i, j = 0;
    for (i = 0; i < hc.nheld; i++)
        if (hc.order[i] != k)
            hc.order[j++] = hc.order[i];
    hc.nheld = (uint8_t)j;
}

/* the chord key k plays now (its chord as kb_chord holds it: the notes that sound, or roll, or the arp's) */
static void hc_chord_of_key(track_t *t, uint32_t k, uint8_t *nn, uint32_t *np)
{
    uint8_t was = hc.cur_key;
    hc.cur_key = (uint8_t)k;
    *np = chord_build(t, kb_note[k], nn);
    hc.cur_key = was;
}

/* the arp plays the chord of key k (the last pressed) */
static void hc_arp_take(track_t *t, uint32_t k)
{
    uint32_t ti = trk_index(t), n;
    uint8_t nn[CHORD_MAX];
    hc_chord_of_key(t, k, nn, &n);
    hc.run[ti].chord = hc.cur;
    hc.run[ti].chord_on = 1;
}

/* a chord key held: the notes it should play now against the ones it plays (kb_chord): the ones that go
 * end, the new ones start (MIDI OUT too); the ones that stay keep sounding. ARP ONLY: the arp's chord only */
static void hc_revoice(uint32_t k)
{
    track_t *t = &trk[kb_trk[k] % NTRK];
    const hc_trk_t *c = hc_of(t);
    uint8_t nn[CHORD_MAX], old[CHORD_MAX];
    uint32_t n, on = kb_chn[k], i, j;
    if (c->play == HP_ARP && hc.run[trk_index(t)].chord_on && hc.nheld && hc.order[hc.nheld - 1u] == k)
        hc_arp_take(t, k);
    if (c->play == HP_ARP && c->arp_layer == HAL_ARP)
        return;                                          /* nothing of the key sounds itself */
    for (i = 0; i < on; i++)
        old[i] = kb_chord[k][i];
    kb_chn[k] = 0;                                       /* (as key_off: this key holds nothing while the others are
                                                          * asked, midi_local_held) */
    hc_chord_of_key(t, k, nn, &n);
    if (hc.strum[k].n) {                                 /* a roll under way: the rest of it is the new chord's */
        hc.strum[k].n = 0;
    }
    for (i = 0; i < on; i++) {                           /* gone: end them */
        for (j = 0; j < n && nn[j] != old[i]; j++)
            ;
        if (j < n || midi_local_held(t, old[i]))
            continue;
        hc_note_off(t, old[i]);
    }
    for (i = 0; i < n; i++) {                            /* new: start them */
        for (j = 0; j < on && old[j] != nn[i]; j++)
            ;
        if (j < on || midi_local_held(t, nn[i]))
            continue;
        hc_note_on(t, nn[i]);
    }
    for (i = 0; i < n; i++)
        kb_chord[k][i] = nn[i];
    kb_chn[k] = (uint8_t)n;
}

static void hc_dir_set(uint32_t d, int down)
{
    uint32_t i, j = 0;
    for (i = 0; i < hc.ndir; i++)                        /* out of the stack (again: to its top) */
        if (hc.dstack[i] != d)
            hc.dstack[j++] = hc.dstack[i];
    hc.ndir = (uint8_t)j;
    if (down && hc.ndir < 8u)
        hc.dstack[hc.ndir++] = (uint8_t)d;
    hc.dir = hc.ndir ? hc.dstack[hc.ndir - 1u] : HD_NONE;
    hc.dirty = 1;
}

/* black key k of the layer went down (1) or up (0) */
static void hc_black(uint32_t k, int down)
{
    uint32_t fn = HC_BLACK_FN[key_place(k) % HC_NBLACK], i;
    if (down)
        hc.black |= 1u << k;
    else
        hc.black &= ~(1u << k);
    if (fn < 8u) {
        hc_dir_set(fn, down);
        return;
    }
    if (!down)
        return;
    if (fn == HG_INVERT && hc_of(TSEL)->play == HP_SEQ) {   /* SEQUENCER: a rest at the cursor */
        hcs_rest_write();
        return;
    }
    if (fn == HG_INVERT && hc_of(TSEL)->play == HP_EAR) {   /* EAR TRAINER: the question again */
        hcg_ear_replay();
        return;
    }
    switch (fn) {
    case HG_INVERT:                                      /* the keys held: the next inversion */
        for (i = 0; i < hc.nheld; i++)
            hc.inv[hc.order[i]] = (uint8_t)((hc.inv[hc.order[i]] + 1u) % 3u);
        hc.dirty = 1;
        break;
    case HG_LOCK:                                        /* the keys held: lock the direction's chord, or unlock */
        for (i = 0; i < hc.nheld; i++) {
            uint32_t key = hc.order[i];
            if (hc.lock[key].on) {
                hc.lock[key].on = 0;
            } else if (hc.dir != HD_NONE) {
                const track_t *t = &trk[kb_trk[key] % NTRK];
                int32_t oct, r, roff;
                uint32_t q = hs_degree_chord(hc_mask(t), hc_degree_of_key(t, key, &oct), &r);
                hc.lock[key].q = (uint8_t)hq_modify(hc_of(t)->mode, hc.dir, q, &roff);
                hc.lock[key].roff = (int8_t)roff;
                hc.lock[key].on = 1;
            }
        }
        hc.dirty = 1;
        break;
    default:                                             /* HOLD: on; off ends the keys no longer held */
        hc.hold = (uint8_t)!hc.hold;
        if (!hc.hold)
            hc.release = 1;
        break;
    }
}

/* ------------------------------------------------- the play modes --- */
/* the length of an ARP / REPEAT step at rate r, in samples; swing: the odd steps longer (2:1 at most) */
static uint32_t hc_rate_samples(uint32_t r, uint32_t step)
{
    uint32_t q = beat_samples();
    switch (r) {
    case HR_1_1: return q * 4u;
    case HR_1_2: return q * 2u;
    case HR_1_4: return q;
    case HR_1_8: return q / 2u;
    case HR_1_16: return q / 4u;
    case HR_1_16T: return q / 6u;
    case HR_1_32: return q / 8u;
    case HR_SW8: return step & 1u ? q / 3u : q * 2u / 3u;
    default: return step & 1u ? q / 6u : q / 3u;         /* SWING 16 */
    }
}

/* the note of slot role s (0 ROOT 1 3RD 2 5TH 3 7TH 4 9TH/11TH) of chord c an octave o up, -1 = none: a role
 * the chord has not falls back to the nearest one below it (assumption) */
static int32_t hc_role_note(const hchord_t *c, int32_t s, int32_t o)
{
    static const uint8_t SLOT[5] = {HS_ROOT, HS_THIRD, HS_FIFTH, HS_EXT1, HS_EXT2};
    int32_t n;
    if (s < 0)
        return -1;
    for (; s >= 0; s--)
        if (c->slot[SLOT[s > 4 ? 4 : s]] >= 0)
            break;
    if (s < 0)
        return -1;
    n = c->slot[SLOT[s]] + 12 * o;
    return n < 0 || n > 127 ? -1 : n;
}

/* the built-in patterns (two notes a step: the second -1): UP DOWN UP/DOWN DOWN/UP RANDOM FINGERPICK */
static void hc_arp_pattern(const hc_trk_t *c, hc_arp_t *p)
{
    static const int8_t UP[4] = {0, 1, 2, 3}, DOWN[4] = {3, 2, 1, 0}, UPDN[6] = {0, 1, 2, 3, 2, 1},
                        DNUP[6] = {3, 2, 1, 0, 1, 2}, PICK[8] = {0, 2, 1, 2, 3, 2, 1, 2};
    const int8_t *s = UP;
    uint32_t i, n = 4;
    if (c->arp_pat == HA_PICK && hc.arp_user_on) {
        *p = hc.arp_user;
        return;
    }
    switch (c->arp_pat) {
    case HA_DOWN: s = DOWN; break;
    case HA_UPDN: s = UPDN; n = 6; break;
    case HA_DNUP: s = DNUP; n = 6; break;
    case HA_PICK: s = PICK; n = 8; break;
    default: break;                                      /* UP, RANDOM (random picks at play time) */
    }
    memset(p, 0, sizeof *p);
    p->len = (uint8_t)n;
    for (i = 0; i < n; i++) {
        p->note[i][0] = s[i];
        p->note[i][1] = -1;
    }
}

static void hc_arp_notes_off(track_t *t)                /* (a note a chord key holds stays: the key ends it) */
{
    uint32_t ti = trk_index(t), i;
    for (i = 0; i < hc.run[ti].an; i++)
        if (!midi_local_held(t, hc.run[ti].anote[i]))
            hc_note_off(t, hc.run[ti].anote[i]);
    hc.run[ti].an = 0;
}

/* the chord keys of track t gated off (REPEAT, RHYTHM+ARP) or on again */
static void hc_gate(track_t *t, int on)
{
    uint32_t k, i;
    for (k = 0; k < 27u; k++)
        if (!key_black(k) && kb_chn[k] && &trk[kb_trk[k] % NTRK] == t)
            for (i = 0; i < kb_chn[k]; i++) {
                if (on)
                    hc_note_on(t, kb_chord[k][i]);
                else
                    hc_note_off(t, kb_chord[k][i]);
            }
}

/* a chord key went down (key k, track t): the play mode decides what sounds now */
/* BASS SLASH (the HiChord: "hold one Chord Button for the bass, press another for the chord. Screen shows
 * Em/C"; its diagram of Am/C is C A C E: the bass, the chord on top). The key held first gives only its
 * bass note: its own chord, and any earlier chord key's, stop when chord key k comes down (k is in
 * hc.order already; the bass key stays there, held). In the modes where a key's chord sounds by itself. */
static void hc_slash_take(track_t *t, uint32_t k)
{
    uint32_t i;
    if (hc_of(t)->bass != HB_SLASH || hc.nheld < 2u || hc.order[0] == k)
        return;
    for (i = 0; i < hc.nheld; i++) {
        uint32_t j = hc.order[i];
        if (j != k && kb_chn[j] && &trk[kb_trk[j] % NTRK] == t)
            key_off(j, t);
    }
}

static void hc_key_on(uint32_t k, track_t *t)
{
    const hc_trk_t *c = hc_of(t);
    uint32_t ti = trk_index(t), i, n;
    uint8_t nn[CHORD_MAX];
    hc_order_add(k);
    if ((hc.latched >> k) & 1u) {                        /* HOLD: it sounds already */
        hc.latched &= ~(1u << k);
        return;
    }
    hc.cur_key = (uint8_t)k;                             /* (key_on -> chord_build -> hc_make: this key's) */
    switch (c->play) {
    case HP_SEQ:                                         /* the step at the cursor, and the chord sounds */
        hcs_key_write(t, k);
        key_on(k, t);
        break;
    case HP_DRUM:
        hcd_key(t, k, 1);
        break;
    case HP_DRUMLOOP:                                    /* the style, started */
        hc_of(t)->dl_style = (uint8_t)((key_place(k) + 3u) % 7u);
        hcd_loop_start();
        break;
    case HP_MIXER:
        hcm_key(k);
        break;
    case HP_HIRO:
        hcg_hiro_key(t, k);
        break;
    case HP_EAR:
        hcg_ear_key(t, k);
        break;
    case HP_DRONE:                                       /* the chord before it ends (its release: the crossfade) */
        for (i = 0; i < 27u; i++)
            if (i != k && kb_chn[i] && &trk[kb_trk[i] % NTRK] == t) {
                key_off(i, t);
                hc.latched &= ~(1u << i);
                hc_order_remove(i);
            }
        key_on(k, t);
        break;
    case HP_STRUM: {                                     /* the first note now, the rest in slot order, spaced */
        static const uint16_t GAP_MS[3] = {200, 80, 40};
        uint32_t j;
        hc_slash_take(t, k);
        hc_chord_of_key(t, k, nn, &n);
        kb_chn[k] = 0;
        hc.strum[k].n = 0;
        for (i = 0, j = 0; i < HS_N; i++) {              /* slot order: ROOT THIRD FIFTH BASS EXT1 EXT2 */
            int32_t x = hc.cur.slot[i];
            uint32_t m;
            if (x < 0 || x > 127)
                continue;
            for (m = 0; m < j && hc.strum[k].note[m] != (uint8_t)x; m++)
                ;
            if (m < j)
                continue;
            hc.strum[k].note[j++] = (uint8_t)x;
        }
        hc.strum[k].n = (uint8_t)j;
        hc.strum[k].i = 1;
        hc.strum[k].due = hc.clock + GAP_MS[c->strum % 3u] * (uint32_t)(FS / 1000u);
        if (!midi_local_held(t, hc.strum[k].note[0]))   /* (asked before this key holds it) */
            hc_note_on(t, hc.strum[k].note[0]);
        kb_chord[k][0] = hc.strum[k].note[0];
        kb_chn[k] = 1;
        last_note = kb_note[k];
        break;
    }
    case HP_ARP:
        if (c->arp_layer == HAL_ARP) {                   /* the arp alone: nothing of the key sounds (kb_chn 0: it
                                                          * holds no note; hc.order remembers it is down) */
            kb_chn[k] = 0;
            last_note = kb_note[k];
        } else {
            key_on(k, t);
        }
        if (!hc.run[ti].chord_on) {                      /* the first key: the arp starts on it, from its first step */
            hc.run[ti].pos = hc.run[ti].len = 0;
            hc.run[ti].step = 0;
            hc.run[ti].gated = 0;
        }
        hc_arp_take(t, k);
        break;
    case HP_REPEAT:
        if (!hc.run[ti].chord_on) {
            hc.run[ti].pos = hc.run[ti].len = 0;
            hc.run[ti].step = 0;
            hc.run[ti].gated = 0;
        }
        hc.run[ti].chord_on = 1;
        key_on(k, t);
        if (hc.run[ti].gated)                            /* in the gap: silent until the next pulse */
            for (i = 0; i < kb_chn[k]; i++)
                hc_note_off(t, kb_chord[k][i]);
        break;
    default:
        hc_slash_take(t, k);
        key_on(k, t);
        break;
    }
    hc.cur_key = HC_NOKEY;
}

/* 1 = track t has a chord key down or latched (hc.order: the keys down, whether or not they hold notes) */
static int hc_any_held(const track_t *t)
{
    uint32_t i, k;
    for (i = 0; i < hc.nheld; i++)
        if (&trk[kb_trk[hc.order[i]] % NTRK] == t)
            return 1;
    for (k = 0; k < 27u; k++)
        if (((hc.latched >> k) & 1u) && &trk[kb_trk[k] % NTRK] == t)
            return 1;
    return 0;
}

/* a chord key went up */
static void hc_key_off(uint32_t k, track_t *t)
{
    const hc_trk_t *c = hc_of(t);
    uint32_t ti = trk_index(t);
    if (c->play == HP_DRONE)
        return;                                          /* it rings until the next chord or another mode */
    if (c->play == HP_DRUM) {
        hcd_key(t, k, 0);
        hc_order_remove(k);
        return;
    }
    if (c->play == HP_DRUMLOOP || c->play == HP_MIXER || c->play == HP_HIRO || c->play == HP_EAR) {
        hc_order_remove(k);
        return;
    }
    if (hc.hold && (kb_chn[k] || (c->play == HP_ARP && c->arp_layer == HAL_ARP))) {
        hc.latched |= 1u << k;
        hc_order_remove(k);
        return;
    }
    hc_order_remove(k);
    if (hc.strum[k].n) {                                 /* the roll stops: only the notes that started end */
        hc.strum[k].n = 0;
    }
    if (c->play == HP_ARP && c->arp_layer == HAL_ARP)
        kb_chn[k] = 0;                                   /* (nothing of it sounded) */
    else if (c->play == HP_REPEAT && hc.run[ti].gated)
        kb_chn[k] = 0;                                   /* (gated off already) */
    else
        key_off(k, t);
    if (c->bass == HB_SLASH && hc.nheld && c->play != HP_ARP && c->play != HP_REPEAT &&
        &trk[kb_trk[hc.order[0]] % NTRK] == t) {
        uint32_t b = hc.order[0];
        if (!kb_chn[b]) {                                /* the chord key let go, the bass key still held: its own
                                                          * chord again (it is a chord button) */
            hc.cur_key = (uint8_t)b;
            key_on(b, t);
            hc.cur_key = HC_NOKEY;
        } else {
            hc.dirty = 1;                                /* the bass key let go: the chord revoices over its root */
        }
    }
    if ((c->play == HP_ARP || c->play == HP_REPEAT) && !hc_any_held(t)) {
        hc.run[ti].chord_on = 0;
        hc_arp_notes_off(t);
        if (c->play == HP_ARP && c->arp_layer == HAL_RHYTHM)
            hc.run[ti].gated = 0;
    } else if (c->play == HP_ARP && hc.nheld) {
        hc_arp_take(t, hc.order[hc.nheld - 1u]);         /* the arp follows the key held last */
    }
}

/* the play mode of track t changed (the UI): what sounds ends; the settings are projected */
static void hc_play_set(track_t *t, uint32_t play)
{
    uint32_t ti = trk_index(t), k;
    hc_arp_notes_off(t);
    for (k = 0; k < 27u; k++)
        if (!key_black(k) && &trk[kb_trk[k] % NTRK] == t && (kb_chn[k] || ((hc.latched >> k) & 1u))) {
            if (!hc.run[ti].gated && kb_chn[k])
                key_off(k, t);
            kb_chn[k] = 0;
            hc.strum[k].n = 0;
            hc.latched &= ~(1u << k);
        }
    for (k = hc.nheld; k > 0; k--)                       /* the keys down stay down, they just sound no more */
        if (&trk[kb_trk[hc.order[k - 1u]] % NTRK] == t)
            hc_order_remove(hc.order[k - 1u]);
    hc.run[ti].chord_on = 0;
    hc.run[ti].gated = 0;
    hc.run[ti].step = 0;
    hc.run[ti].pos = hc.run[ti].len = 0;
    hcs_mode_left(t);
    hcg_mode_left(t);
    hc_of(t)->play = (uint8_t)(play % HP_COUNT);
    hc_apply(t);
}

#include "hclooper.c"                                   /* the looper (hcl_tick below) */

/* one block of n samples: the rolls, the ARP steps, the REPEAT gate, the looper */
static void hc_tick(uint32_t n)
{
    uint32_t k, ti;
    hc.clock += n;
    hcl_tick(n);
    hcs_tick(n);
    hcg_tick(n);
    for (k = 0; k < 27u; k++) {                          /* STRUM: the next note of a roll */
        track_t *t;
        if (!hc.strum[k].n || hc.strum[k].i >= hc.strum[k].n || (int32_t)(hc.clock - hc.strum[k].due) < 0)
            continue;
        t = &trk[kb_trk[k] % NTRK];
        {
            uint32_t x = hc.strum[k].note[hc.strum[k].i++];
            if (!midi_local_held(t, x))                  /* (asked before this key holds it) */
                hc_note_on(t, x);
            kb_chord[k][kb_chn[k]++] = (uint8_t)x;
            hc.strum[k].due += (uint32_t)(hc_of(t)->strum == HST_SLOW ? 200 : hc_of(t)->strum == HST_MED ? 80 : 40) * (FS / 1000u);
            if (hc.strum[k].i >= hc.strum[k].n)
                hc.strum[k].n = 0;
        }
    }
    for (ti = 0; ti < NTRK; ti++) {
        track_t *t = &trk[ti];
        const hc_trk_t *c = &hc.t[ti];
        uint32_t rate;
        if (!hc_on(t) || !hc.run[ti].chord_on)
            continue;
        if (c->play != HP_ARP && c->play != HP_REPEAT)
            continue;
        rate = c->play == HP_ARP ? c->arp_rate : c->arp_rate;
        if (hc.run[ti].an && (int32_t)(hc.clock - hc.run[ti].aoff) >= 0)
            hc_arp_notes_off(t);
        if (c->play == HP_REPEAT || c->arp_layer == HAL_RHYTHM) {   /* the gap: the chord off at half the step */
            if (!hc.run[ti].gated && hc.run[ti].len && hc.run[ti].pos + n >= hc.run[ti].len / 2u) {
                hc.run[ti].gated = 1;
                hc_gate(t, 0);
            }
        }
        hc.run[ti].pos += n;
        if (hc.run[ti].pos < hc.run[ti].len)
            continue;
        {
            uint32_t fresh = hc.run[ti].len == 0u;       /* the first step after the press: the chord sounds already */
            hc.run[ti].pos = fresh ? 0u : hc.run[ti].pos - hc.run[ti].len;
            hc.run[ti].len = hc_rate_samples(rate, hc.run[ti].step);
            if ((c->play == HP_REPEAT || c->arp_layer == HAL_RHYTHM) && !fresh) {   /* the pulse: the chord on again */
                if (hc.run[ti].gated)
                    hc_gate(t, 1);
                else if (c->play == HP_REPEAT)
                    hc_gate(t, 0), hc_gate(t, 1);        /* (retrigger) */
                hc.run[ti].gated = 0;
            }
        }
        if (c->play == HP_ARP) {                         /* the step's note(s) */
            hc_arp_t p;
            uint32_t s, j, m = 0;
            hc_arp_pattern(c, &p);
            hc_arp_notes_off(t);
            s = hc.run[ti].step % (p.len ? p.len : 1u);
            for (j = 0; j < 2u; j++) {
                int32_t role = p.note[s][j], note;
                if (c->arp_pat == HA_RND && j == 0) {    /* RANDOM: any role the chord has */
                    uint32_t r = hc.run[ti].rnd;
                    r ^= r << 13; r ^= r >> 17; r ^= r << 5;
                    hc.run[ti].rnd = r;
                    role = (int32_t)(r % 5u);
                }
                note = hc_role_note(&hc.run[ti].chord, role, p.oct[s][j]);
                if (note < 0)
                    continue;
                hc_note_on(t, (uint32_t)note);
                hc.run[ti].anote[m++] = (uint8_t)note;
            }
            hc.run[ti].an = (uint8_t)m;
            hc.run[ti].aoff = hc.clock + hc.run[ti].len / 2u;   /* the gate: half the step (the release does the rest) */
        }
        hc.run[ti].step++;
    }
}

/* after the block's key edges: HOLD gone off, and the held chords revoiced after a change */
static void hc_block(void)
{
    uint32_t k;
    if (hc.release) {
        hc.release = 0;
        for (k = 0; k < 27u; k++)
            if ((hc.latched >> k) & 1u) {
                hc.latched &= ~(1u << k);
                hc_order_remove(k);
                key_off(k, &trk[kb_trk[k] % NTRK]);
            }
        for (k = 0; k < NTRK; k++)
            if ((hc.t[k].play == HP_ARP || hc.t[k].play == HP_REPEAT) && !hc_any_held(&trk[k])) {
                hc.run[k].chord_on = 0;
                hc_arp_notes_off(&trk[k]);
            }
    }
    if (!hc.dirty)
        return;
    hc.dirty = 0;
    for (k = 0; k < 27u; k++)
        if (!key_black(k) && kb_chn[k] && hc_on(&trk[kb_trk[k] % NTRK]))
            hc_revoice(k);
    for (k = 0; k < NTRK; k++)                           /* ARP ONLY: the arp's chord follows the key held last */
        if (hc.t[k].play == HP_ARP && hc.t[k].arp_layer == HAL_ARP && hc.run[k].chord_on && hc.nheld &&
            &trk[kb_trk[hc.order[hc.nheld - 1u]] % NTRK] == &trk[k])
            hc_arp_take(&trk[k], hc.order[hc.nheld - 1u]);
}

#include "hcseq.c"                                      /* SEQUENCER, DRUM, DRUM LOOP */
#include "hcgame.c"                                     /* CHORD HIRO, EAR TRAINER */
