/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* The HiChord user interface on the FM-1's 240 x 240 screen: HOME (the chord played, big; the key, the mode,
 * the tempo; the keys as a keyboard; the sound and its effects) and three menus as the HiChord has them,
 * colour-coded: KEY (SCL, grey), SOUND (FX, yellow: the sounds, the effects, the settings), MODE (EDIT, red:
 * the play modes and the tempo), and PRESETS (SAVE: P1..P4). Every option of the HiChord's menus and no
 * other. Felucca's own UI stays underneath (HOME held: the advanced synth; SAVE + HOME: back).
 *   SELECT    the cursor of a menu; on HOME the key
 *   OCT- / +  the value of the row (a menu); the octave (HOME); both: RANDOMIZE (ALL on KEY, the sound on
 *             SOUND, the pattern on MODE)
 *   ALGORITHM the play mode, PRESETS the sound, KNOB 1 the FILTER wheel, KNOB 2 RESONANCE, KNOB 3 ATTACK, KNOB 4 RELEASE,
 *             the value shown in the header as it turns (KNOB 4 on a SOUND effect row: the amount): on every screen
 *   ENV / LFO the envelope preset / the vibrato; ARP: ARP mode and back; EDIT tapped three times: tap tempo
 * Included after Felucca's ui*.c (it uses their drawing, sounds and projects); ui_input / ui_draw / ui_leds
 * hand over to hui_* while hui.on. */

/* ------------------------------------------------------------ colours --- */
#define HC_GREY RGB(150, 152, 160)
#define HC_YELLOW RGB(245, 196, 0)
#define HC_RED RGB(226, 62, 62)
#define HC_GREEN RGB(70, 200, 120)
#define HC_INK RGB(16, 16, 20)
static const uint16_t HC_DEG_COL[7] = {RGB(255, 92, 92), RGB(255, 160, 48), RGB(250, 220, 70), RGB(96, 214, 120),
                                       RGB(72, 200, 236), RGB(120, 128, 255), RGB(206, 112, 240)};   /* I .. vii */

/* ------------------------------------------------------------ sounds --- */
typedef struct { const char *name; uint8_t engine, preset, env; } hc_sound_t;
#define HCE ((uint8_t)ENGI_HC)
static const hc_sound_t HC_SOUNDS[] = {
    {"SINE", HCE, 0, HE_LONG},      {"SAW", HCE, 1, HE_LONG},        {"TRIANGLE", HCE, 2, HE_LONG},
    {"SQUARE", HCE, 3, HE_LONG},    {"E.PIANO", HCE, 4, HE_KEYS},    {"HX7 PIANO", HCE, 5, HE_KEYS},
    {"FM BELL", HCE, 6, HE_KEYS},   {"FM ORGAN", HCE, 7, HE_SUSTAIN}, {"FM BRASS", HCE, 8, HE_SUSTAIN},
    {"STRINGS", 0, 11, HE_LONG},    {"CLARINET", 0, 10, HE_SUSTAIN}, {"CELLOS", 2, 2, HE_LONG},
    {"ACOUSTIC", 9, 2, HE_KEYS},    {"BRASS", 0, 9, HE_SUSTAIN},     {"PIANO", 4, 0, HE_KEYS},
    {"VIBES", 9, 1, HE_KEYS},       {"VIOLINS", 0, 3, HE_LONG},      {"VOX AHH", 5, 0, HE_LONG},
    {"SAX", 4, 3, HE_SUSTAIN},      {"HARP", 9, 8, HE_KEYS},         {"HUMMING", 5, 1, HE_LONG},
    {"SYNTH BASS", 6, 0, HE_SHORT}, {"ARCADE", 11, 2, HE_SHORT},     {"FLUTE", 4, 2, HE_SUSTAIN},
    {"SAW SQUARE", HCE, 9, HE_LONG}, {"JUNO POLY", HCE, 10, HE_LONG}, {"OCEAN PAD", HCE, 11, HE_SWELL},
    {"WOBBLE BASS", HCE, 12, HE_SHORT}, {"BUZZ ORGAN", 7, 4, HE_SUSTAIN}, {"ORGAN", 7, 0, HE_SUSTAIN},
    {"HORNS", 2, 0, HE_SUSTAIN},    {"E GUITAR", 6, 2, HE_KEYS},     {"KALIMBA", 9, 4, HE_KEYS},
    {"SAW BASS", 0, 7, HE_SHORT},   {"SHIMMER", 8, 3, HE_SWELL},     {"GOSPEL ORGAN", 7, 2, HE_SUSTAIN},
    {"PURE SINE", HCE, 13, HE_LONG},
    {"ACOUSTIC GTR", 9, 9, HE_KEYS},                    /* fm1-chord: a plucky guitar for LEAD (PHYS) */
};
#define HC_NSOUNDS (sizeof HC_SOUNDS / sizeof HC_SOUNDS[0])
/* the HiChord's ten scales (CC 103 order) -> Felucca's SCALE values */
static const uint8_t HC_SCALE_V[10] = {1, 2, 7, 11, 5, 6, 12, 3, 4, 9};
static const char *const HC_SCALE_NAME[10] = {"MAJOR", "MINOR", "HARM MIN", "MELOD MIN", "MAJ PEN", "MIN PEN", "BLUES",
                                              "DORIAN", "MIXOLYD", "LYDIAN"};
static const char *const HC_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const char *const HC_DEG_NAME[7] = {"I", "ii", "iii", "IV", "V", "vi", "vii"};
static const char *const HC_DIR_NAME[8] = {"UP", "UP-RIGHT", "RIGHT", "DOWN-RIGHT", "DOWN", "DOWN-LEFT", "LEFT", "UP-LEFT"};
static const char *const HP_SHORT[HP_COUNT] = {"PLAY", "STRUM", "LEAD", "DRONE", "ARP", "REPEAT", "SEQ", "DRUM", "LOOPS", "HIRO",
                                               "EAR", "MIXER"};   /* (the header band) */

/* ------------------------------------------------------------- state --- */
enum { HU_HOME, HU_KEY, HU_SOUND, HU_MODE, HU_PRESET, HU_LOOP, HU_PICK, HU_N };   /* HU_PICK: a list (hui_pick_*) */
enum { RL_L1, RL_L2, RL_L3, RL_L4, RL_BARS, RL_METRO, RL_CLEAR, RL_N };   /* (RL_L1 + HCL_LAYERS - 1 = RL_L4) */
enum { RK_KEY, RK_OCT, RK_SCALE, RK_LAYOUT, RK_JOY, RK_BASS, RK_VOICES, RK_VLEAD, RK_RANDOM, RK_N };
enum { RS_SOUND, RS_ENV, RS_ATK, RS_REL, RS_FILT, RS_CUT, RS_RES, RS_HP, RS_REV, RS_DLY, RS_CHO, RS_FLG, RS_TREM, RS_LFO,
       RS_GLIDE, RS_DRIVE, RS_TAPE, RS_STEREO, RS_SPK, RS_OUT, RS_MIDIIN, RS_RANDOM, RS_N };
enum { RM_MODE, RM_BPM, RM_STRUM, RM_APAT, RM_ARATE, RM_ALAYER, RM_SEQLEN, RM_KIT, RM_DLSTYLE, RM_DLVAR, RM_SONG, RM_DIFF,
       RM_SPEED, RM_LEVEL, RM_RANDOM, RM_N };
#define HC_BLUE RGB(80, 150, 255)
static const char *const HU_TITLE[HU_N] = {"", "KEY", "SOUND", "MODE", "PRESETS", "LOOPER", ""};
static const uint16_t HU_COL[HU_N] = {0, HC_GREY, HC_YELLOW, HC_RED, HC_GREEN, HC_BLUE, 0};
static const uint8_t HU_ROWS[HU_N] = {0, RK_N, RS_N, RM_N, 4, RL_N, 0};
static const char *const RL_LABEL[RL_N] = {"LAYER 1", "LAYER 2", "LAYER 3", "LAYER 4", "BARS", "METRONOME", "CLEAR ALL"};
static const char *const RK_LABEL[RK_N] = {"KEY", "OCTAVE", "SCALE", "LAYOUT", "JOYSTICK", "BASS", "VOICES", "VOICE LEAD",
                                           "RANDOMIZE ALL"};
static const char *const RS_LABEL[RS_N] = {"SOUND", "ENVELOPE", "ATTACK", "RELEASE", "FILTER WHEEL", "CUTOFF", "RESONANCE", "HI-PASS",
                                           "REVERB", "DELAY", "CHORUS", "FLANGER", "TREMOLO", "LFO", "GLIDE", "DRIVE", "TAPE",
                                           "STEREO", "SPEAKER", "OUT LEVEL", "MIDI IN", "RANDOMIZE SOUND"};
static const char *const RM_LABEL[RM_N] = {"MODE", "TEMPO", "STRUM SPEED", "ARP PATTERN", "ARP RATE", "ARP LAYER",
                                           "SEQ LENGTH", "DRUM KIT", "LOOP STYLE", "LOOP VARIATION", "HIRO SONG", "DIFFICULTY",
                                           "PRACTICE SPEED", "EAR LEVEL", "RANDOMIZE PATTERN"};
static const char *const HC_OUT_NAME[2] = {"HOT", "LINE"};
static const char *const HC_SPK_NAME[3] = {"FLAT", "LOWCUT", "BASS+"};

#ifndef HUI_DEFAULT
#define HUI_DEFAULT 1                                   /* the HiChord UI at power-on (the host tests: 0) */
#endif
static struct {
    uint8_t on;                  /* the HiChord UI is up (0: Felucca's) */
    uint8_t screen;              /* HU_* */
    uint8_t sel[HU_N];           /* the cursor of each menu */
    uint8_t top[HU_N];           /* the first row shown */
    uint32_t sig[4];             /* drawn-state caches: header, body, strip, footer */
    uint8_t force;
    char msg[20];
    uint8_t msg_t;
    uint32_t home_t0;            /* HOME held (btn_hold) */
    uint32_t rec_t0;             /* REC held: clear the layer */
    uint32_t tap_ms[4];          /* tap tempo: the EDIT taps */
    uint8_t ntap;
    uint8_t midi_in;             /* the MIDI IN setting (hichord.c reads it) */
    uint8_t out_line;            /* OUT LEVEL: LINE (-10 dB) */
    uint32_t rnd;
    uint8_t preset_used[4];      /* P1..P4 hold something */
    uint8_t pick_kind;           /* HU_PICK: PK_* */
    uint8_t pick_sticky;         /* opened by ENV / LFO (stays), else by a knob (goes 1.5 s after the last turn) */
    uint32_t pick_ms;            /* the last turn */
} hui = {.on = HUI_DEFAULT, .rnd = 0x9E3779B9u};
static int hui_active(void) { return hui.on; }
static void hui_mode_set(track_t *t, uint32_t play);   /* below: the mode, with the drum engine swap */
static void hui_resume(void)
{
    hui.on = 1;
    hui.force = 1;
    lcd_fill(0, 0, 240, 240, T_BG);
}

static void hui_say(const char *s)
{
    str_cpy(hui.msg, s, sizeof hui.msg);
    hui.msg_t = 70;
}
static uint32_t hui_rand(uint32_t n)
{
    uint32_t r = hui.rnd;
    r ^= r << 13; r ^= r >> 17; r ^= r << 5;
    hui.rnd = r;
    return n ? (r >> 8) % n : 0u;
}

/* the live track's HiChord settings */
static hc_trk_t *hui_c(void) { return &hc.t[song.sel]; }
static void hui_apply(void) { hc_apply(TSEL); }
static int hui_row_value(uint32_t sc, uint32_t row, char *b);   /* (below) */
/* a knob turned: its name and value in the header bar for a moment ("ATTACK 790ms") */
static void hui_say_value(const char *label, uint32_t row)
{
    char v[20];
    hui_row_value(HU_SOUND, row, v);
    str_cpy(hui.msg, label, sizeof hui.msg);
    str_cpy(hui.msg + str_len(hui.msg), " ", sizeof hui.msg - str_len(hui.msg));
    str_cpy(hui.msg + str_len(hui.msg), v, sizeof hui.msg - str_len(hui.msg));
    hui.msg_t = 70;
}

/* the effect rows with an amount (KNOB 4): REVERB, DELAY, CHORUS (their sends), FLANGER (its wet),
 * TREMOLO (its depth). The amount in effect: the one set, else the type's own */
static int hui_fx_row(uint32_t row) { return row == RS_REV || row == RS_DLY || row == RS_CHO || row == RS_FLG || row == RS_TREM; }
static uint32_t hui_fx_amount(const hc_trk_t *c, uint32_t row)
{
    static const uint8_t REV_DEF[HRV_COUNT] = {0, 50, 65, 60, 60, 90}, CHO_DEF[HCH_COUNT] = {0, 40, 60, 85, 110};
    switch (row) {
    case RS_REV: return c->rev_amt ? c->rev_amt : REV_DEF[c->rev % HRV_COUNT];
    case RS_DLY: return c->dly_amt ? c->dly_amt : 55u;
    case RS_CHO: return c->cho_amt ? c->cho_amt : CHO_DEF[c->cho % HCH_COUNT];
    case RS_FLG: return c->flg_amt ? c->flg_amt : 64u;
    default: return c->trem_amt ? c->trem_amt : 90u;
    }
}
static uint8_t *hui_fx_amount_of(hc_trk_t *c, uint32_t row)
{
    return row == RS_REV ? &c->rev_amt : row == RS_DLY ? &c->dly_amt : row == RS_CHO ? &c->cho_amt :
           row == RS_FLG ? &c->flg_amt : &c->trem_amt;
}
/* the row's value: the type, and the amount when the effect is on ("HALL 65") */
static void hui_fx_value(char *b, const char *type, uint32_t on, uint32_t amount)
{
    str_cpy(b, type, 16);
    if (on) {
        uint32_t len = str_len(b);
        b[len] = ' ';
        fmt_int(b + len + 1u, (int32_t)amount);
    }
}
/* KNOB 4 on an effect row: the amount (an effect that is OFF comes on at its first type) */
static void hui_fx_turn(hc_trk_t *c, uint32_t row, int32_t s)
{
    uint8_t *amt = hui_fx_amount_of(c, row);
    uint8_t *on = row == RS_REV ? &c->rev : row == RS_DLY ? &c->dly : row == RS_CHO ? &c->cho : row == RS_FLG ? &c->flg : &c->trem;
    if (!*on)
        *on = 1;
    *amt = (uint8_t)clamp((int32_t)hui_fx_amount(c, row) + s * 4, 1, 127);
}

/* the key (ROOT) and the scale are the HiChord's: global, every track's */
static void hui_key_set(int32_t root)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        trk[i].p[P_ROOT] = (int16_t)((root % 12 + 12) % 12);
}
static uint32_t hui_scale_get(void)                     /* the HiChord scale index of the tracks' SCALE */
{
    uint32_t i;
    for (i = 0; i < 10u; i++)
        if (HC_SCALE_V[i] == TSEL->p[P_SCALE])
            return i;
    return 0;
}
static void hui_scale_set(uint32_t s)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        trk[i].p[P_SCALE] = HC_SCALE_V[s % 10u];
}

/* a sound of the list onto track t: its engine, its preset (ui.c), its envelope; the HiChord settings on top */
static void hc_sound_load(track_t *t, uint32_t idx)
{
    const hc_sound_t *s = &HC_SOUNDS[idx % HC_NSOUNDS];
    hc_trk_t *c = hc_of(t);
    c->sound = (uint8_t)(idx % HC_NSOUNDS);
    c->env = s->env;
    c->atk = c->rel = 0;
    set_engine_of(t, s->engine);
    if (s->preset)
        apply_preset_to(t, s->preset);
    t->p[P_LEVEL] = (int16_t)(s->engine == ENGI_PHYS ? 127 : TP[P_LEVEL].def);   /* the physical models sit
                                                          * ~12 dB under the rest (3 voices): full level, no trim (fx.c) */
    hc_apply(t);
}

/* the HiChord UI takes a track: its chord keys on, its sound loaded */
static void hui_track_init(track_t *t)
{
    hc_trk_t *c = hc_of(t);
    if (t->p[P_CHRD] != CH_HI || !hc_on(t)) {
        hc_sound_load(t, c->sound);
        hc_apply(t);
    }
}

/* ----------------------------------------------------- presets P1..P4 --- */
/* a preset's own state packed into HC_PRESET_BYTES (settings_persist.c hc_presets): magic, the tracks' settings,
 * the key inversions and locks. Saving: the project slot (ui.c project_save: sounds, key, tempo, the Felucca
 * parameters) and this; loading: both, then hc_apply */
#define HCP_MAGIC 0x48435033u                           /* "HCP3": hc_trk_t grew res */
static void hc_preset_pack(uint8_t *b)
{
    uint32_t i, k = 4;
    memset(b, 0, HC_PRESET_BYTES);
    memcpy(b, &(uint32_t){HCP_MAGIC}, 4);
    for (i = 0; i < NTRK; i++, k += sizeof(hc_trk_t))
        memcpy(b + k, &hc.t[i], sizeof(hc_trk_t));
    for (i = 0; i < 27u; i++)
        b[k++] = hc.inv[i];
    for (i = 0; i < 27u; i++) {
        b[k++] = hc.lock[i].on;
        b[k++] = hc.lock[i].q;
        b[k++] = (uint8_t)hc.lock[i].roff;
    }
    b[k++] = hui.midi_in;
    b[k++] = hui.out_line;
}
static int hc_preset_unpack(const uint8_t *b)
{
    uint32_t m, i, k = 4;
    memcpy(&m, b, 4);
    if (m != HCP_MAGIC)
        return 0;
    for (i = 0; i < NTRK; i++, k += sizeof(hc_trk_t))
        memcpy(&hc.t[i], b + k, sizeof(hc_trk_t));
    for (i = 0; i < 27u; i++)
        hc.inv[i] = (uint8_t)(b[k++] % 3u);
    for (i = 0; i < 27u; i++) {
        hc.lock[i].on = (uint8_t)(b[k++] & 1u);
        hc.lock[i].q = (uint8_t)(b[k++] % HQ_COUNT);
        hc.lock[i].roff = (int8_t)b[k++];
    }
    hui.midi_in = b[k++] & 1u;
    hui.out_line = b[k++] & 1u;
    return 1;
}
/* The live state across power-off, as the HiChord keeps its sound, effects, mode, inversions and locks
 * (key, scale, octave and tempo are Felucca's song and reset, as on the HiChord): packed like a preset
 * into hc_live (settings_persist.c, PER6), saved with the settings 3 s after the last change (one flash
 * erase per edit session, not per knob click), restored at boot (main.c) */
static struct { uint32_t at_ms, poll_ms; uint8_t dirty; } hui_live;
static void hui_live_restore(void)
{
    uint32_t i;
    if (!hc_preset_unpack(hc_live))
        return;
    for (i = 0; i < NTRK; i++)
        hc_play_set(&trk[i], hc.t[i].play);
}
static void hui_live_poll(void)
{
    uint8_t b[HC_PRESET_BYTES];
    if ((uint32_t)(fm1_ms - hui_live.poll_ms) < 500u)
        return;
    hui_live.poll_ms = fm1_ms;
    hc_preset_pack(b);
    if (memcmp(b, hc_live, sizeof b)) {
        memcpy(hc_live, b, sizeof b);
        hui_live.at_ms = fm1_ms;
        hui_live.dirty = 1;
    } else if (hui_live.dirty && (uint32_t)(fm1_ms - hui_live.at_ms) >= 3000u) {
        hui_live.dirty = 0;
        settings_save();
    }
}
static void hui_preset_scan(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        hui.preset_used[i] = (uint8_t)project_used(i);
}
static void hui_preset_save(uint32_t slot)
{
    char b[16] = "SAVED P";
    if (chain_busy() || song.playing) {
        hui_say("STOP TO SAVE");
        return;
    }
    hc_preset_pack(hc_presets[slot & 3u]);
    if (project_save(slot & 3u) != 0) {
        hui_say("SAVE FAILED");
        return;
    }
    settings_save();
    hui_preset_scan();
    b[7] = (char)('1' + (slot & 3u));
    b[8] = 0;
    hui_say(b);
}
static void hui_preset_load(uint32_t slot)
{
    char b[16] = "PRESET P";
    uint32_t i;
    if (!project_used(slot & 3u)) {
        hui_say("EMPTY");
        return;
    }
    project_load(slot & 3u);
    if (!hc_preset_unpack(hc_presets[slot & 3u]))
        for (i = 0; i < NTRK; i++)
            hc_trk_defaults(&hc.t[i]);
    for (i = 0; i < NTRK; i++)
        hc_play_set(&trk[i], hc.t[i].play);             /* (hc_apply on every track; a drum mode's engine is the project's) */
    b[8] = (char)('1' + (slot & 3u));
    b[9] = 0;
    hui_say(b);
}

/* --------------------------------------------------------- randomize --- */
static void hui_random_sound(void)
{
    hc_trk_t *c = hui_c();
    hc_sound_load(TSEL, hui_rand(HC_NSOUNDS));
    c->env = (uint8_t)hui_rand(HE_COUNT);
    c->rev = (uint8_t)(hui_rand(3) ? hui_rand(HRV_COUNT) : 0);
    c->dly = (uint8_t)(hui_rand(3) == 0 ? hui_rand(HDL_COUNT) : 0);
    c->cho = (uint8_t)(hui_rand(2) ? hui_rand(HCH_COUNT) : 0);
    c->flg = (uint8_t)(hui_rand(4) == 0 ? hui_rand(HFL_COUNT) : 0);
    c->trem = (uint8_t)(hui_rand(4) == 0 ? hui_rand(HTR_COUNT) : 0);
    c->lfo = (uint8_t)(hui_rand(3) == 0 ? hui_rand(HLF_COUNT) : 0);
    c->drive = (uint8_t)(hui_rand(4) == 0 ? hui_rand(HDR_COUNT) : 0);
    c->tape = (uint8_t)(hui_rand(5) == 0 ? hui_rand(HTP_COUNT) : 0);
    c->glide = 0;
    hui_apply();
    hui_say("RANDOMIZED SOUND");
}
static void hui_random_pattern(void)
{
    hc_trk_t *c = hui_c();
    c->arp_pat = (uint8_t)hui_rand(HA_COUNT);
    c->arp_rate = (uint8_t)(HR_1_4 + hui_rand(5));
    c->arp_layer = (uint8_t)hui_rand(3);
    c->strum = (uint8_t)hui_rand(3);
    hui_apply();
    hui_say("RANDOMIZED PATTERN");
}
static void hui_random_all(void)
{
    hc_trk_t *c = hui_c();
    hui_key_set((int32_t)hui_rand(12));
    hui_scale_set(hui_rand(10));
    song.g[G_BPM] = (int16_t)(70 + hui_rand(110));
    c->mode = (uint8_t)hui_rand(HM_COUNT);
    c->bass = (uint8_t)hui_rand(3);
    c->voices = (uint8_t)hui_rand(2);
    hui_random_sound();
    hui_random_pattern();
    hui_mode_set(TSEL, hui_rand(HP_REPEAT + 1u));
    hui_say("RANDOMIZED ALL");
}

/* ------------------------------------------------------- the values --- */
/* the value of a row as text; 1 = the row is an action */
static int hui_row_value(uint32_t screen, uint32_t row, char *b)
{
    const hc_trk_t *c = hui_c();
    b[0] = 0;
    switch (screen) {
    case HU_KEY:
        switch (row) {
        case RK_KEY: str_cpy(b, HC_NOTE_NAME[(uint32_t)TSEL->p[P_ROOT] % 12u], 16); break;
        case RK_OCT: str_cpy(b, song.octave > 0 ? "+" : "", 16); fmt_int(b + str_len(b), song.octave); break;
        case RK_SCALE: str_cpy(b, HC_SCALE_NAME[hui_scale_get()], 16); break;
        case RK_LAYOUT: str_cpy(b, HL_NAME[c->layout % 2u], 16); break;
        case RK_JOY: str_cpy(b, HM_NAME[c->mode % HM_COUNT], 16); break;
        case RK_BASS: str_cpy(b, HB_NAME[c->bass % 3u], 16); break;
        case RK_VOICES: str_cpy(b, HV_NAME[c->voices % 4u], 16); break;
        case RK_VLEAD: str_cpy(b, c->vlead ? "ON" : "OFF", 16); break;
        default: return 1;
        }
        return 0;
    case HU_SOUND:
        switch (row) {
        case RS_SOUND: str_cpy(b, HC_SOUNDS[c->sound % HC_NSOUNDS].name, 16); break;
        case RS_ENV: str_cpy(b, HE_NAME[c->env % HE_COUNT], 16); break;
        case RS_ATK: case RS_REL: {
            const char *u;
            param_format(&TP[row == RS_ATK ? P_ATK : P_REL], TSEL->p[row == RS_ATK ? P_ATK : P_REL], b, &u);
            str_cpy(b + str_len(b), u, 16 - str_len(b));
            break;
        }
        case RS_FILT: str_cpy(b, c->filt ? "ON" : "OFF", 16); break;
        case RS_CUT: {
            const char *u;
            param_format(&(param_desc_t){"CUT", F_CUTOFF, 0, 127, 127, 0, 0}, c->cutoff, b, &u);
            str_cpy(b + str_len(b), u, 16 - str_len(b));
            break;
        }
        case RS_RES: fmt_int(b, c->res); break;
        case RS_HP: str_cpy(b, c->hp ? "ON" : "OFF", 16); break;
        case RS_REV: hui_fx_value(b, HRV_NAME[c->rev % HRV_COUNT], c->rev, hui_fx_amount(c, RS_REV)); break;
        case RS_DLY: hui_fx_value(b, HDL_NAME[c->dly % HDL_COUNT], c->dly, hui_fx_amount(c, RS_DLY)); break;
        case RS_CHO: hui_fx_value(b, HCH_NAME[c->cho % HCH_COUNT], c->cho, hui_fx_amount(c, RS_CHO)); break;
        case RS_FLG: hui_fx_value(b, HFL_NAME[c->flg % HFL_COUNT], c->flg, hui_fx_amount(c, RS_FLG)); break;
        case RS_TREM: hui_fx_value(b, HTR_NAME[c->trem % HTR_COUNT], c->trem, hui_fx_amount(c, RS_TREM)); break;
        case RS_LFO: str_cpy(b, HLF_NAME[c->lfo % HLF_COUNT], 16); break;
        case RS_GLIDE: str_cpy(b, HGL_NAME[c->glide % HGL_COUNT], 16); break;
        case RS_DRIVE: str_cpy(b, HDR_NAME[c->drive % HDR_COUNT], 16); break;
        case RS_TAPE: str_cpy(b, HTP_NAME[c->tape % HTP_COUNT], 16); break;
        case RS_STEREO: str_cpy(b, c->stereo ? "ON" : "OFF", 16); break;
        case RS_SPK: str_cpy(b, HC_SPK_NAME[settings.lowcut % 3u], 16); break;
        case RS_OUT: str_cpy(b, HC_OUT_NAME[hui.out_line & 1u], 16); break;
        case RS_MIDIIN: str_cpy(b, hui.midi_in ? "ON" : "OFF", 16); break;
        default: return 1;
        }
        return 0;
    case HU_MODE:
        switch (row) {
        case RM_MODE: str_cpy(b, HP_NAME[c->play % HP_COUNT], 16); break;
        case RM_BPM: fmt_int(b, song.g[G_BPM]); str_cpy(b + str_len(b), " BPM", 16 - str_len(b)); break;
        case RM_STRUM: str_cpy(b, HST_NAME[c->strum % 3u], 16); break;
        case RM_APAT: str_cpy(b, HA_NAME[c->arp_pat % HA_COUNT], 16); break;
        case RM_ARATE: str_cpy(b, HR_NAME[c->arp_rate % HR_COUNT], 16); break;
        case RM_ALAYER: str_cpy(b, HAL_NAME[c->arp_layer % 3u], 16); break;
        case RM_SEQLEN: fmt_int(b, hcs.len); str_cpy(b + str_len(b), " STEPS", 16 - str_len(b)); break;
        case RM_KIT: str_cpy(b, N_DRUM_KIT[c->kit % 4u], 16); break;
        case RM_DLSTYLE: str_cpy(b, DL_NAME[c->dl_style % DL_STYLES], 16); break;
        case RM_DLVAR: str_cpy(b, DV_NAME[c->dl_var % DV_VARS], 16); break;
        case RM_SONG: str_cpy(b, HCG_SONG[hcg.song % HG_SONGS].name, 16); break;
        case RM_DIFF: str_cpy(b, HGD_NAME[hcg.diff % HGD_COUNT], 16); break;
        case RM_SPEED: fmt_int(b, hcg.speed); str_cpy(b + str_len(b), "%", 16 - str_len(b)); break;
        case RM_LEVEL: fmt_int(b, hcg.level + 1); break;
        default: return 1;
        }
        return 0;
    case HU_LOOP:
        if (row < HCL_LAYERS) {
            str_cpy(b, HLS_NAME[hcl.l[row].state & 3u], 16);
            if (row == hcl_live() && hcl.l[row].state != HLS_PLAY)
                str_cpy(b + str_len(b), " LIVE", 16 - str_len(b));
        } else if (row == RL_BARS) {
            if (hcl.bars) fmt_int(b, hcl.bars); else str_cpy(b, "FREE", 16);
        } else if (row == RL_METRO) {
            str_cpy(b, hcl.metro ? "ON" : "OFF", 16);
        } else {
            return 1;
        }
        return 0;
    default:
        if (hui.preset_used[row & 3u]) {
            char nm[16];
            if (project_name(row & 3u, nm) && nm[0])
                str_cpy(b, nm, 16);
            else
                str_cpy(b, "SAVED", 16);
        } else {
            str_cpy(b, "EMPTY", 16);
        }
        return 0;
    }
}

static uint32_t hui_cycle(uint32_t v, int32_t d, uint32_t n) { return (uint32_t)(((int32_t)v + d) % (int32_t)n + (int32_t)n) % n; }

/* OCT- / OCT+ on a row: its value, or its action */
static void hui_row_change(uint32_t screen, uint32_t row, int32_t d)
{
    hc_trk_t *c = hui_c();
    switch (screen) {
    case HU_KEY:
        switch (row) {
        case RK_KEY: hui_key_set(TSEL->p[P_ROOT] + d); break;
        case RK_OCT: song.octave = (int8_t)clamp(song.octave + d, -2, 2); break;
        case RK_SCALE: hui_scale_set(hui_cycle(hui_scale_get(), d, 10)); break;
        case RK_LAYOUT: c->layout = (uint8_t)hui_cycle(c->layout, d, 2); break;
        case RK_JOY: c->mode = (uint8_t)hui_cycle(c->mode, d, HM_COUNT); break;
        case RK_BASS: c->bass = (uint8_t)hui_cycle(c->bass, d, 3); break;
        case RK_VOICES: c->voices = (uint8_t)hui_cycle(c->voices, d, 4); break;
        case RK_VLEAD: c->vlead = (uint8_t)!c->vlead; break;
        default: hui_random_all(); break;
        }
        hc.dirty = 1;
        break;
    case HU_SOUND:
        switch (row) {
        case RS_SOUND: hc_sound_load(TSEL, hui_cycle(c->sound, d, HC_NSOUNDS)); return;
        case RS_ENV: c->env = (uint8_t)hui_cycle(c->env, d, HE_COUNT); c->atk = c->rel = 0; break;
        case RS_ATK: c->atk = (uint8_t)clamp((c->atk ? c->atk : TSEL->p[P_ATK]) + d * 4, 1, 127); break;
        case RS_REL: c->rel = (uint8_t)clamp((c->rel ? c->rel : TSEL->p[P_REL]) + d * 4, 1, 127); break;
        case RS_FILT: c->filt = (uint8_t)!c->filt; break;
        case RS_CUT: c->cutoff = (uint8_t)clamp(c->cutoff + d * 4, 0, 127); c->filt = 1; break;
        case RS_RES: c->res = (uint8_t)clamp(c->res + d * 4, 0, 127); c->filt = 1; break;
        case RS_HP: c->hp = (uint8_t)!c->hp; break;
        case RS_REV: c->rev = (uint8_t)hui_cycle(c->rev, d, HRV_COUNT); break;
        case RS_DLY: c->dly = (uint8_t)hui_cycle(c->dly, d, HDL_COUNT); break;
        case RS_CHO: c->cho = (uint8_t)hui_cycle(c->cho, d, HCH_COUNT); break;
        case RS_FLG: c->flg = (uint8_t)hui_cycle(c->flg, d, HFL_COUNT); break;
        case RS_TREM: c->trem = (uint8_t)hui_cycle(c->trem, d, HTR_COUNT); break;
        case RS_LFO: c->lfo = (uint8_t)hui_cycle(c->lfo, d, HLF_COUNT); break;
        case RS_GLIDE: c->glide = (uint8_t)hui_cycle(c->glide, d, HGL_COUNT); break;
        case RS_DRIVE: c->drive = (uint8_t)hui_cycle(c->drive, d, HDR_COUNT); break;
        case RS_TAPE: c->tape = (uint8_t)hui_cycle(c->tape, d, HTP_COUNT); break;
        case RS_STEREO: c->stereo = (uint8_t)!c->stereo; break;
        case RS_SPK: settings.lowcut = hui_cycle(settings.lowcut, d, 3); fx_lowcut = (uint8_t)settings.lowcut; break;
        case RS_OUT: hui.out_line = (uint8_t)!hui.out_line; hcfx.line = hui.out_line; break;
        case RS_MIDIIN: hui.midi_in = (uint8_t)!hui.midi_in; break;
        default: hui_random_sound(); return;
        }
        break;
    case HU_MODE:
        switch (row) {
        case RM_MODE: hui_mode_set(TSEL, hui_cycle(c->play, d, HP_COUNT)); return;
        case RM_BPM: song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + d, GP[G_BPM].min, GP[G_BPM].max); break;
        case RM_STRUM: c->strum = (uint8_t)hui_cycle(c->strum, d, 3); break;
        case RM_APAT: c->arp_pat = (uint8_t)hui_cycle(c->arp_pat, d, HA_COUNT); break;
        case RM_ARATE: c->arp_rate = (uint8_t)hui_cycle(c->arp_rate, d, HR_COUNT); break;
        case RM_ALAYER: c->arp_layer = (uint8_t)hui_cycle(c->arp_layer, d, 3); break;
        case RM_SEQLEN: hcs.len = (uint8_t)clamp((int32_t)hcs.len + d * 4, 4, 16); if (hcs.cur >= hcs.len) hcs.cur = 0; return;
        case RM_KIT: c->kit = (uint8_t)hui_cycle(c->kit, d, 4); break;
        case RM_DLSTYLE: c->dl_style = (uint8_t)hui_cycle(c->dl_style, d, DL_STYLES); return;
        case RM_DLVAR: c->dl_var = (uint8_t)hui_cycle(c->dl_var, d, DV_VARS); return;
        case RM_SONG: hcg.song = (uint8_t)hui_cycle(hcg.song, d, HG_SONGS); return;
        case RM_DIFF: hcg.diff = (uint8_t)hui_cycle(hcg.diff, d, HGD_COUNT); return;
        case RM_SPEED: hcg.speed = (uint8_t)clamp(hcg.speed + d * 10, 50, 100); return;
        case RM_LEVEL: hcg.level = (uint8_t)hui_cycle(hcg.level, d, HGE_LEVELS); return;
        default: hui_random_pattern(); return;
        }
        break;
    case HU_LOOP:
        if (row < HCL_LAYERS) {
            hcl.sel = (uint8_t)row;
            if (d > 0)
                hcl_rec_press();                         /* OFF -> ARMED -> REC -> PLAY -> OFF */
            else
                hcl_clear(row);
        } else if (row == RL_BARS) {
            hcl.bars = (uint8_t)clamp(hcl.bars + d, 0, 8);
        } else if (row == RL_METRO) {
            hcl.metro = (uint8_t)!hcl.metro;
        } else {
            hcl_clear_all();
            hui_say("ALL CLEARED");
        }
        return;
    default:                                             /* PRESETS: OCT+ loads */
        if (d > 0)
            hui_preset_load(row);
        return;
    }
    hui_apply();
}

/* the mode of the live track: a drum mode takes the DRUM engine (the sound kept for the way back); the chord
 * modes get the sound back. hc_play_set does the rest (what ran stops; a running sequencer bounces) */
static int hui_drum_mode(uint32_t play) { return play == HP_DRUM || play == HP_DRUMLOOP; }
static void hui_mode_set(track_t *t, uint32_t play)
{
    hc_trk_t *c = hc_of(t);
    uint32_t was = c->play;
    play %= HP_COUNT;
    hc_play_set(t, play);
    if (hui_drum_mode(play) && !hui_drum_mode(was)) {
        c->sound_saved = c->sound;
        set_engine_of(t, ENGI_DRUM);
        hc_apply(t);
    } else if (!hui_drum_mode(play) && hui_drum_mode(was)) {
        if (t->p[P_LEVEL] <= 96)                         /* the drum modes cap the level (hc_apply): back to full */
            t->p[P_LEVEL] = TP[P_LEVEL].def;
        hc_sound_load(t, c->sound_saved);
    }
    if (play == HP_DRUMLOOP)
        hcd_loop_start();
}

/* a layer began to play: the live instrument moves to the lowest layer not playing, with the same sound
 * and settings (the HiChord: live playing follows the layer being built) */
static void hui_looper_advance(void)
{
    uint32_t live = hcl_live(), cur = song.sel;
    hcl.advance = 0;
    if (live == cur)
        return;
    hc.t[live] = hc.t[cur];
    track_select(live);
    hc_sound_load(&trk[live], hc.t[live].sound);
    hcl.sel = (uint8_t)live;
    hui_say("LAYER");
    hui.msg[5] = (char)('1' + live);
    hui.msg[6] = 0;
}

/* --------------------------------------------------------------- input --- */
/* The lists (HU_PICK): ENVELOPE and LFO as pages (the ENV / LFO buttons), the SOUND and MODE lists shown
 * while the PRESETS / ALGORITHM knob turns. SELECT moves the choice, applied as it moves */
enum { PK_ENV, PK_LFO, PK_SOUND, PK_MODE };
static uint32_t hui_pick_count(uint32_t k)
{
    return k == PK_ENV ? HE_COUNT : k == PK_LFO ? HLF_COUNT : k == PK_SOUND ? (uint32_t)HC_NSOUNDS : HP_COUNT;
}
static const char *hui_pick_name(uint32_t k, uint32_t i)
{
    return k == PK_ENV ? HE_NAME[i % HE_COUNT] : k == PK_LFO ? HLF_NAME[i % HLF_COUNT] :
           k == PK_SOUND ? HC_SOUNDS[i % HC_NSOUNDS].name : HP_NAME[i % HP_COUNT];
}
static uint32_t hui_pick_cur(uint32_t k)
{
    const hc_trk_t *c = hui_c();
    return k == PK_ENV ? c->env : k == PK_LFO ? c->lfo : k == PK_SOUND ? c->sound : c->play;
}
static void hui_pick_apply(uint32_t k, uint32_t i)
{
    hc_trk_t *c = hui_c();
    switch (k) {
    case PK_ENV: c->env = (uint8_t)(i % HE_COUNT); c->atk = c->rel = 0; hui_apply(); break;
    case PK_LFO: c->lfo = (uint8_t)(i % HLF_COUNT); hui_apply(); break;
    case PK_SOUND: hc_sound_load(TSEL, i % HC_NSOUNDS); break;
    default: hui_mode_set(TSEL, i % HP_COUNT); break;
    }
}
static void hui_pick_open(uint32_t k, int sticky)
{
    hui.pick_kind = (uint8_t)k;
    hui.pick_sticky = (uint8_t)sticky;
    hui.pick_ms = fm1_ms;
    if (hui.screen != HU_PICK) {
        hui.screen = HU_PICK;
        hui.force = 1;
    }
}
static void hui_pick_move(int32_t s)
{
    uint32_t k = hui.pick_kind, n = hui_pick_count(k);
    hui_pick_apply(k, (uint32_t)clamp((int32_t)hui_pick_cur(k) + s, 0, (int32_t)n - 1));
    hui.pick_ms = fm1_ms;
}

static void hui_open(uint32_t screen)
{
    if (hui.screen == screen)
        screen = HU_HOME;
    hui.screen = (uint8_t)screen;
    hui.force = 1;
    if (screen == HU_PRESET)
        hui_preset_scan();
}

static void hui_tap_tempo(void)
{
    uint32_t now = fm1_ms, i;
    if (hui.ntap && now - hui.tap_ms[hui.ntap - 1u] > 1500u)   /* a pause: a new series (40 BPM: 1.5 s) */
        hui.ntap = 0;
    if (hui.ntap < 4u)
        hui.tap_ms[hui.ntap++] = now;
    else {
        for (i = 1; i < 4u; i++)
            hui.tap_ms[i - 1u] = hui.tap_ms[i];
        hui.tap_ms[3] = now;
    }
    if (hui.ntap >= 3u) {
        uint32_t span = hui.tap_ms[hui.ntap - 1u] - hui.tap_ms[0], bpm = 60000u * (hui.ntap - 1u) / (span ? span : 1u);
        song.g[G_BPM] = (int16_t)clamp((int32_t)bpm, GP[G_BPM].min, GP[G_BPM].max);
        hui_say("TAP TEMPO");
        hui_apply();
    }
}

static void hui_input(void)
{
    uint32_t pressed = fm1_input_edges(0), now = fm1_ticks(), i;
    uint32_t home = btn_hold(&hui.home_t0, B_HOME, now, 1);
    hc_trk_t *c = hui_c();
    int32_t s;
    uint32_t sc = hui.screen, nrows = HU_ROWS[sc];
    fm1_input_note_edges();                              /* (the keys are the ISR's: nothing of the UI's) */
    fm6_poll();
    hui_live_poll();
    if (sc == HU_PICK && !hui.pick_sticky && (pressed || (uint32_t)(fm1_ms - hui.pick_ms) > 1500u)) {
        hui.screen = HU_HOME;                            /* a knob's list: over (a button acts as on HOME) */
        hui.force = 1;
        sc = HU_HOME;
        nrows = HU_ROWS[sc];
    }
    kb_mask = perf_mask = 0;                             /* no layer takes the keys */
    song.grid = 0;
    song.seq_mode = 0;
    hui_track_init(TSEL);
#define DOWN(b) ((pressed >> panel.btn[b]) & 1u)
    if (home == BT_HOLD) {                               /* HOME held: Felucca's own UI */
        hui.on = 0;
        ui.home = 1;
        ui.force = 1;
        lcd_fill(0, 0, 240, 240, T_BG);
        return;
    }
    if (home == BT_TAP) {
        hui.screen = HU_HOME;
        hui.force = 1;
    }
    if (DOWN(B_SCL)) hui_open(HU_KEY);
    if (DOWN(B_FX) || DOWN(B_GLO)) hui_open(HU_SOUND);
    if (DOWN(B_EDIT)) {
        hui_open(HU_MODE);
        hui_tap_tempo();
    }
    if (DOWN(B_SAVE)) {
        if (sc == HU_PRESET)
            hui_preset_save(hui.sel[HU_PRESET]);
        else
            hui_open(HU_PRESET);
    }
    if (DOWN(B_ENV)) {                                   /* the ENVELOPE page (again: back) */
        if (hui.screen == HU_PICK && hui.pick_sticky && hui.pick_kind == PK_ENV) {
            hui.screen = HU_HOME;
            hui.force = 1;
        } else {
            hui_pick_open(PK_ENV, 1);
        }
    }
    if (DOWN(B_LFO)) {                                   /* the LFO page */
        if (hui.screen == HU_PICK && hui.pick_sticky && hui.pick_kind == PK_LFO) {
            hui.screen = HU_HOME;
            hui.force = 1;
        } else {
            hui_pick_open(PK_LFO, 1);
        }
    }
    if (DOWN(B_ARP)) {
        hui_mode_set(TSEL, c->play == HP_ARP ? HP_PLAY : HP_ARP);
        hui_say(HP_NAME[c->play]);
    }
    {   /* OCT- / OCT+: a menu's value, HOME's octave; both together: RANDOMIZE */
        uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
        static uint8_t chord_seen;
        if ((fm1_in.buttons & both) == both) {
            if (!chord_seen) {
                chord_seen = 1;
                if (sc == HU_SOUND) hui_random_sound();
                else if (sc == HU_MODE) hui_random_pattern();
                else hui_random_all();
            }
        } else if (!(fm1_in.buttons & both)) {
            chord_seen = 0;
        } else if (!chord_seen && (DOWN(B_OCTDN) || DOWN(B_OCTUP))) {
            int32_t d = DOWN(B_OCTUP) ? 1 : -1;
            if (sc == HU_HOME && c->play == HP_MIXER) {
                TSEL->p[P_LEVEL] = (int16_t)clamp(TSEL->p[P_LEVEL] + d * 8, 0, 127);
            } else if (sc == HU_HOME) {
                song.octave = (int8_t)clamp(song.octave + d, -2, 2);
            } else {
                hui_row_change(sc, hui.sel[sc], d);
            }
        }
    }
    if ((s = panel_enc(EN_SELECT)) != 0) {               /* the cursor, or the key; MIXER: the layer; SEQUENCER: the step */
        if (sc == HU_HOME && c->play == HP_MIXER) {
            track_select((uint32_t)clamp((int32_t)song.sel + s, 0, NTRK - 1));
        } else if (sc == HU_HOME && c->play == HP_SEQ) {
            hcs.cur = (uint8_t)((hcs.cur + (uint32_t)(s % (int32_t)hcs.len + (int32_t)hcs.len)) % hcs.len);
        } else if (sc == HU_HOME) {
            hui_key_set(TSEL->p[P_ROOT] + s);
            hc.dirty = 1;
        } else if (sc == HU_PICK) {
            hui_pick_move(s);
        } else if (nrows) {
            hui.sel[sc] = (uint8_t)clamp((int32_t)hui.sel[sc] + s, 0, (int32_t)nrows - 1);
        }
    }
    if ((s = panel_enc(EN_ALGO)) != 0) {                 /* the mode, its list shown while the knob turns */
        hui_mode_set(TSEL, hui_cycle(c->play, s, HP_COUNT));
        hui_pick_open(PK_MODE, 0);
    }
    if ((s = panel_enc(EN_PRESET)) != 0) {               /* the sound, its list shown while the knob turns */
        hc_sound_load(TSEL, hui_cycle(c->sound, s, HC_NSOUNDS));
        hui_pick_open(PK_SOUND, 0);
    }
    if ((s = panel_enc(EN_K1)) != 0) {                   /* KNOB 1: the FILTER wheel */
        c->cutoff = (uint8_t)clamp(c->cutoff + s * 3, 0, 127);
        c->filt = 1;
        hui_apply();
        hui_say_value("FILTER", RS_CUT);
    }
    if ((s = panel_enc(EN_K2)) != 0) {                   /* KNOB 2: RESONANCE */
        c->res = (uint8_t)clamp(c->res + s * 3, 0, 127);
        c->filt = 1;
        hui_apply();
        hui_say_value("RESONANCE", RS_RES);
    }
    if ((s = panel_enc(EN_K3)) != 0) {                   /* KNOB 3: ATTACK */
        c->atk = (uint8_t)clamp((c->atk ? c->atk : TSEL->p[P_ATK]) + s * 2, 1, 127);
        hui_apply();
        hui_say_value("ATTACK", RS_ATK);
    }
    if ((s = panel_enc(EN_K4)) != 0) {                   /* KNOB 4: RELEASE (an effect row of the SOUND menu: its amount) */
        if (sc == HU_SOUND && hui_fx_row(hui.sel[sc])) {
            hui_fx_turn(c, hui.sel[sc], s);
            hui_apply();
        } else {
            c->rel = (uint8_t)clamp((c->rel ? c->rel : TSEL->p[P_REL]) + s * 2, 1, 127);
            hui_apply();
            hui_say_value("RELEASE", RS_REL);
        }
    }
    {   /* the looper: REC cycles the layer (held: clears it), PLAY pauses / resumes, SEQ shows the LOOPER screen */
        uint32_t rec = btn_hold(&hui.rec_t0, B_REC, now, 1);
        if (sc != HU_LOOP)
            hcl.sel = (uint8_t)hcl_live();
        else
            hcl.sel = (uint8_t)(hui.sel[HU_LOOP] < HCL_LAYERS ? hui.sel[HU_LOOP] : hcl_live());
        if (rec == BT_TAP) {
            hcl_rec_press();
            hui_say(HLS_NAME[hcl.l[hcl.sel].state & 3u]);
        } else if (rec == BT_HOLD) {
            hcl_clear(hcl.sel);
            if (!hcl_count(HLS_PLAY) && !hcl_count(HLS_REC))
                hcl_clear_all();
            hui_say("CLEARED");
        }
        if (DOWN(B_PLAY)) {
            if (c->play == HP_HIRO) {                    /* the song starts or stops */
                if (hcg.running) hcg_hiro_stop(TSEL); else hcg_hiro_start();
                hui_say(hcg.running ? "GO!" : "STOPPED");
            } else if (c->play == HP_EAR) {              /* a new round */
                hcg_ear_start();
                hui_say("LISTEN");
            } else if (c->play == HP_SEQ) {                     /* the sequencer runs or stops */
                if (hcs.running) hcs_stop(TSEL); else hcs_start(TSEL);
                hui_say(hcs.running ? "SEQ RUNNING" : "SEQ STOPPED");
            } else if (c->play == HP_DRUMLOOP) {
                if (hcs.dl_running) hcd_loop_stop(); else hcd_loop_start();
                hui_say(hcs.dl_running ? "LOOP RUNNING" : "LOOP STOPPED");
            } else {
                hcl_play_press();
                hui_say(hcl.playing ? "PLAYING" : hcl.len ? "PAUSED" : "NO LOOP");
            }
        }
        if (DOWN(B_SEQ))
            hui_open(HU_LOOP);
        if (hcl.advance)
            hui_looper_advance();
    }
    for (i = 0; i < NE; i++)                             /* (nothing else takes a turn) */
        panel_enc(i);
#undef DOWN
}

/* ---------------------------------------------------------------- LEDs --- */
static void hui_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0};
    uint32_t k, c, held = fm1_in.notes | hc.latched;
    static const uint8_t MENU_BTN[HU_N] = {B_HOME, B_SCL, B_FX, B_EDIT, B_SAVE, B_HOME, B_HOME};
    static uint8_t ready;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    led_put(nl, panel.btn[MENU_BTN[hui.screen]], 1);
    if (hui.screen == HU_PICK && hui.pick_sticky)
        led_put(nl, panel.btn[hui.pick_kind == PK_ENV ? B_ENV : B_LFO], 1);
    if (hui_c()->play == HP_ARP)
        led_put(nl, panel.btn[B_ARP], 1);
    led_put(nl, panel.btn[B_REC], hcl_count(HLS_REC) != 0u || (hcl_count(HLS_ARMED) != 0u && ((fm1_ms / 250u) & 1u) == 0u));
    led_put(nl, panel.btn[B_PLAY], hcl.playing);
    led_put(nl, panel.btn[B_OCTDN], song.octave < 0);
    led_put(nl, panel.btn[B_OCTUP], song.octave > 0);
    for (k = 0; k < 27u; k++) {
        led_put(nl, 14u + k, (int)((held >> k) & 1u) || (key_black(k) && key_place(k) == 2u && hc.hold));
        led_put(nd, 14u + k, 1);
    }
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = nd[c];
        fm1_led[c] = nl[c];
    }
}

/* ------------------------------------------------------------- drawing --- */
#define HU_HEAD 24
#define HU_ROW 23
#define HU_ROWS_Y 28
#define HU_NROWS_SHOWN 8
#define HU_FOOT_Y 214

static uint32_t hui_fx_mask(const hc_trk_t *c)
{
    return (c->rev ? 1u : 0u) | (c->dly ? 2u : 0u) | (c->cho ? 4u : 0u) | (c->flg ? 8u : 0u) | (c->trem ? 16u : 0u) |
           (c->lfo ? 32u : 0u) | (c->glide ? 64u : 0u) | (c->drive ? 128u : 0u) | (c->tape ? 256u : 0u) |
           (c->filt ? 512u : 0u) | (c->hp ? 1024u : 0u) | (c->stereo ? 2048u : 0u);
}

/* the header: the key and scale (grey), the mode (red), the tempo, USB, the battery */
static void hui_draw_head(void)
{
    char b[24];
    const hc_trk_t *c = hui_c();
    uint32_t sig = (uint32_t)TSEL->p[P_ROOT] * 3u + hui_scale_get() * 37u + c->play * 101u + (uint32_t)song.g[G_BPM] * 1009u +
                   (uint32_t)batt_shown() * 7777u + (usb.config && !usb.suspended) * 99991u + (hui.msg_t ? str_hash(5u, hui.msg) : 0u) +
                   (uint32_t)(song.octave + 3) * 50021u;
    if (!hui.force && sig == hui.sig[0])
        return;
    hui.sig[0] = sig;
    cv_begin(240, HU_HEAD, T_BG);
    if (hui.msg_t) {
        cv_rrect(0, 0, 240, HU_HEAD, 0, HC_YELLOW, T_BG);
        cv_text_c(120, 4, &AF_M, hui.msg, HC_INK, HC_YELLOW);
        cv_blit(0, 0);
        return;
    }
    cv_rrect(0, 0, 94, HU_HEAD, 0, HC_GREY, T_BG);      /* the key */
    str_cpy(b, HC_NOTE_NAME[(uint32_t)TSEL->p[P_ROOT] % 12u], sizeof b);
    str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
    str_cpy(b + str_len(b), HC_SCALE_NAME[hui_scale_get()], sizeof b - str_len(b));
    cv_text_on(6, 4, &AF_M, b, HC_INK, HC_GREY);
    if (song.octave) {
        str_cpy(b, song.octave > 0 ? "+" : "", sizeof b);
        fmt_int(b + str_len(b), song.octave);
        cv_text_r(91, 6, &AF_S, b, HC_INK, HC_GREY);
    }
    cv_rrect(96, 0, 52, HU_HEAD, 0, HC_RED, T_BG);      /* the mode */
    cv_text_c(122, 4, &AF_M, HP_SHORT[c->play % HP_COUNT], RGB(255, 240, 240), HC_RED);
    fmt_int(b, song.g[G_BPM]);
    cv_text_on(152, 4, &AF_M, b, T_TEXT, T_BG);          /* the tempo, the USB icon and the battery 8 px apart */
    if (usb.config && !usb.suspended)
        cv_icon_mid(198, HU_HEAD / 2, 16, ICON_X_USB, T_MID, T_BG);
    draw_battery(214);
    cv_blit(0, 0);
}

/* the footer: the four menu buttons, coloured (HOME); a menu's hints */
static void hui_draw_foot(void)
{
    uint32_t sig = hui.screen * 7u + 1u + (hui.screen == HU_SOUND && hui_fx_row(hui.sel[HU_SOUND]) ? 3u : 0u) +
                   (hui.screen == HU_PICK ? hui.pick_kind * 11u + hui.pick_sticky * 13u : 0u);
    if (!hui.force && sig == hui.sig[3])
        return;
    hui.sig[3] = sig;
    cv_begin(240, 240 - HU_FOOT_Y, T_BG);
    {   /* keycaps and their actions, spread over the width; shorter words when the long ones do not fit */
        typedef struct { uint32_t kc; const char *act, *alt; uint16_t fill, ink; } hint_t;
        hint_t h[5];
        uint32_t n = 0, i, pass;
        if (hui.screen == HU_HOME) {
            h[n++] = (hint_t){KC_SCL, "KEY", "KEY", HC_GREY, HC_INK};
            h[n++] = (hint_t){KC_FX, "SOUND", "SND", HC_YELLOW, HC_INK};
            h[n++] = (hint_t){KC_EDIT, "MODE", "MODE", HC_RED, HC_INK};         /* (dark on red: the panel LCD washes white out) */
            h[n++] = (hint_t){KC_SAVE, "PRESET", "PRE", HC_GREEN, HC_INK};
        } else if (hui.screen == HU_PRESET) {
            h[n++] = (hint_t){KC_SELECT, "SLOT", "SLOT", T_KEY, T_INK};
            h[n++] = (hint_t){KC_OCTUP, "LOAD", "LOAD", T_KEY, T_INK};
            h[n++] = (hint_t){KC_SAVE, "SAVE", "SAVE", HC_GREEN, HC_INK};
            h[n++] = (hint_t){KC_HOME, "BACK", "BACK", T_KEY, T_INK};
        } else if (hui.screen == HU_LOOP) {
            h[n++] = (hint_t){KC_SELECT, "LAYER", "LAYR", T_KEY, T_INK};
            h[n++] = (hint_t){KC_REC, "REC", "REC", HC_RED, HC_INK};
            h[n++] = (hint_t){KC_PLAY, "PLAY", "PLAY", HC_GREEN, HC_INK};
            h[n++] = (hint_t){KC_HOME, "BACK", "BACK", T_KEY, T_INK};
        } else if (hui.screen == HU_PICK) {
            h[n++] = (hint_t){KC_SELECT, "CHOOSE", "PICK", T_KEY, T_INK};
            h[n++] = (hint_t){KC_HOME, "BACK", "BACK", T_KEY, T_INK};
        } else if (hui.screen == HU_SOUND && hui_fx_row(hui.sel[HU_SOUND])) {   /* an effect row: KNOB 4 = its amount */
            h[n++] = (hint_t){KC_SELECT, "ROW", "ROW", T_KEY, T_INK};
            h[n++] = (hint_t){KC_OCTUP, "TYPE", "TYPE", T_KEY, T_INK};
            h[n++] = (hint_t){KC_K4, "AMOUNT", "AMT", HC_YELLOW, HC_INK};
            h[n++] = (hint_t){KC_HOME, "BACK", "BACK", T_KEY, T_INK};
        } else {
            h[n++] = (hint_t){KC_SELECT, "ROW", "ROW", T_KEY, T_INK};
            h[n++] = (hint_t){KC_OCTDN, "", "", T_KEY, T_INK};
            h[n++] = (hint_t){KC_OCTUP, "VALUE", "SET", T_KEY, T_INK};
            h[n++] = (hint_t){KC_HOME, "BACK", "BACK", T_KEY, T_INK};
        }
        for (pass = 0; pass < 2u; pass++) {
            int32_t w = 0, x = 4, gap;
            for (i = 0; i < n; i++) {
                const char *a = pass ? h[i].alt : h[i].act;
                w += kc_w(h[i].kc) + (a[0] ? 2 + text_w(&AF_S, a) : 0);
            }
            gap = (232 - w) / (int32_t)(n - 1u);
            if (gap < 2 && !pass)
                continue;                                /* too wide: the short words */
            if (gap > 12)
                gap = 12;
            if (gap < 1)
                gap = 1;
            for (i = 0; i < n; i++) {
                const char *a = pass ? h[i].alt : h[i].act;
                x = cv_keycap(x, 5, h[i].kc, h[i].fill, h[i].ink, T_BG);
                if (a[0])
                    x = cv_text_on(x + 2, 4, &AF_S, a, T_MID, T_BG);   /* (on the keycap centre line, as cv_key_hint) */
                x += gap;
            }
            break;
        }
    }
    cv_blit(0, HU_FOOT_Y);
}

/* HOME in SEQUENCER, DRUM, DRUM LOOP and MIXER mode: the steps, the pads, the loop, the layers (the chord area
 * and the keyboard strip: 190 rows from HU_HEAD) */
static void hui_loop_strip(int32_t y);                 /* (below) */
static void hui_draw_mode_body(void)
{
    const hc_trk_t *c = hui_c();
    uint32_t i, j, held = fm1_in.notes;
    char b[20];
    cv_begin(240, 124, T_BG);
    if (c->play == HP_SEQ) {                             /* 16 cells, 4 x 4: the step's chord, the cursor, the step playing */
        for (i = 0; i < 16u; i++) {
            int32_t x = 4 + (int32_t)(i % 4u) * 59, y = 4 + (int32_t)(i / 4u) * 30;
            int in = i < hcs.len, cur = i == hcs.cur, now = hcs.running && i == hcs.step;
            hchord_t ch;
            uint16_t bg = !in ? T_BG : now ? HC_DEG_COL[hcs.st[i].deg % 7u] : T_SURF;
            uint16_t fg = !in ? T_DIM : now ? HC_INK : T_TEXT;
            cv_rrect(x, y, 56, 27, 5, bg, T_BG);
            if (cur)
                cv_rrect(x, y, 56, 27, 5, bg, T_BG), cv_frame(x, y, 56, 27, HC_RED);
            if (in && hcs_step_chord(TSEL, i, &ch, b, 0))
                cv_text_c(x + 28, y + 5, &AF_M, b, hcs.st[i].deg < 7u && !now ? HC_DEG_COL[hcs.st[i].deg] : fg, bg);
            else if (in)
                cv_text_c(x + 28, y + 7, &AF_S, "-", T_DIM, bg);
        }
        cv_blit(0, HU_HEAD);
        cv_begin(240, 66, T_BG);
        cv_text_on(4, 2, &AF_S, "KEYS WRITE   F#3 REST   PLAY RUNS", T_MID, T_BG);
        fmt_int(b, hcs.len);
        str_cpy(b + str_len(b), " STEPS", sizeof b - str_len(b));
        cv_text_on(4, 24, &AF_M, b, HC_RED, T_BG);
        cv_text_on(4, 47, &AF_M, HC_SOUNDS[c->sound % HC_NSOUNDS].name, HC_YELLOW, T_BG);
        cv_blit(0, HU_HEAD + 124);
        return;
    }
    if (c->play == HP_DRUM) {                            /* the seven pads, lit while held; the direction = AUTO-DRUM */
        for (i = 0; i < 7u; i++) {
            int32_t x = 4 + (int32_t)(i % 4u) * 59, y = 10 + (int32_t)(i / 4u) * 52;
            uint32_t on = 0, k;
            for (k = 0; k < 27u; k++)
                if (!key_black(k) && key_place(k) % 7u == i && ((held >> k) & 1u))
                    on = 1;
            cv_rrect(x, y, 56, 46, 6, on ? HC_DEG_COL[i] : T_SURF, T_BG);
            cv_text_c(x + 28, y + 15, &AF_S, HC_PAD_NAME[i], on ? HC_INK : T_TEXT, on ? HC_DEG_COL[i] : T_SURF);
        }
        cv_rrect(181, 62, 56, 46, 6, hc.dir != HD_NONE ? HC_YELLOW : T_SURF, T_BG);
        cv_text_c(209, 68, &AF_S, "AUTO", hc.dir != HD_NONE ? HC_INK : T_MID, hc.dir != HD_NONE ? HC_YELLOW : T_SURF);
        cv_text_c(209, 84, &AF_S, hc.dir != HD_NONE ? HR_NAME[hcd_rate_of_dir(hc.dir)] : "BLACK KEY", hc.dir != HD_NONE ? HC_INK : T_DIM,
                  hc.dir != HD_NONE ? HC_YELLOW : T_SURF);
        cv_blit(0, HU_HEAD);
        cv_begin(240, 66, T_BG);
        cv_text_on(4, 2, &AF_S, "WHITE KEYS: PADS   BLACK KEY: REPEAT", T_MID, T_BG);
        cv_text_on(4, 22, &AF_M, "KIT", T_MID, T_BG);
        cv_text_on(40, 22, &AF_M, N_DRUM_KIT[c->kit % 4u], HC_RED, T_BG);
        hui_loop_strip(44);
        cv_blit(0, HU_HEAD + 124);
        return;
    }
    if (c->play == HP_DRUMLOOP) {                        /* the styles, the variation, the 16 steps of 6 lanes */
        for (i = 0; i < DL_STYLES; i++) {                /* the styles: four, then three */
            int32_t x = 4 + (int32_t)(i % 4u) * 59, y = 2 + (int32_t)(i / 4u) * 20;
            int on = i == c->dl_style;
            cv_rrect(x, y, 56, 18, 5, on ? HC_RED : T_SURF, T_BG);
            cv_text_c(x + 28, y + 3, &AF_S, DL_NAME[i], on ? RGB(255, 240, 240) : T_MID, on ? HC_RED : T_SURF);
        }
        cv_rrect(181, 22, 56, 18, 5, hcs.dl_running ? HC_GREEN : T_SURF, T_BG);
        cv_text_c(209, 25, &AF_S, hcs.dl_running ? "RUNNING" : "STOPPED", hcs.dl_running ? HC_INK : T_DIM, hcs.dl_running ? HC_GREEN : T_SURF);
        cv_text_on(4, 44, &AF_S, DV_NAME[c->dl_var % DV_VARS], HC_YELLOW, T_BG);
        for (j = 0; j < 6u; j++)
            for (i = 0; i < 16u; i++) {
                uint32_t on = (hcd_lanes(c->dl_style, c->dl_var, i) >> j) & 1u, now = hcs.dl_running && i == hcs.dl_step;
                int32_t x = 4 + (int32_t)i * 14, y = 60 + (int32_t)j * 10;
                cv_rrect(x, y, 12, 8, 2, on ? (now ? T_TEXT : HC_DEG_COL[j]) : now ? T_RAISE : T_SURF, T_BG);
            }
        cv_blit(0, HU_HEAD);
        cv_begin(240, 66, T_BG);
        cv_text_on(4, 2, &AF_S, "WHITE KEY: STYLE   MODE MENU: VARIATION", T_MID, T_BG);
        cv_text_on(4, 22, &AF_M, "KIT", T_MID, T_BG);
        cv_text_on(40, 22, &AF_M, N_DRUM_KIT[c->kit % 4u], HC_RED, T_BG);
        hui_loop_strip(44);
        cv_blit(0, HU_HEAD + 124);
        return;
    }
    if (c->play == HP_HIRO) {                           /* the song's chords coming, the one due big, the score */
        const hcg_song_t *sg = &HCG_SONG[hcg.song % HG_SONGS];
        cv_text_on(4, 2, &AF_S, sg->name, HC_RED, T_BG);
        cv_text_r(236, 2, &AF_S, HGD_NAME[hcg.diff % HGD_COUNT], T_MID, T_BG);
        if (!hcg.running && !hcg.done) {
            cv_text_c(120, 44, &AF_M, "PLAY STARTS THE SONG", T_DIM, T_BG);
            cv_text_c(120, 66, &AF_S, "PRESS EACH CHORD ON ITS BEAT", T_DIM, T_BG);
        } else {
            for (i = 0; i < 4u && hcg.idx + i < sg->n; i++) {   /* the chord due and the three after it */
                uint32_t d = sg->ev[hcg.idx + i].deg, dir = sg->ev[hcg.idx + i].dir;
                int32_t x = 4 + (int32_t)i * 59;
                uint16_t bg = i ? T_SURF : (hcg.hit ? (hcg.last == HGR_MISS ? HC_RED : HC_GREEN) : HC_DEG_COL[d % 7u]);
                str_cpy(b, HC_DEG_NAME[d % 7u], sizeof b);
                if (dir == HD_UR) str_cpy(b + str_len(b), "7", sizeof b - str_len(b));
                if (dir == HD_RIGHT) str_cpy(b + str_len(b), "M7", sizeof b - str_len(b));
                cv_rrect(x, 20, 56, 40, 6, bg, T_BG);
                cv_text_c(x + 28, 28, i ? &AF_M : &AF_L, b, i ? T_TEXT : HC_INK, bg);
            }
            if (hcg.running) {                           /* the beat coming: a bar fills up to the chord's time */
                int32_t left = (int32_t)hcg.next_at - (int32_t)hcg.pos, full = (int32_t)(hcg_beat() * 4u);
                int32_t w = left <= 0 ? 232 : left >= full ? 0 : 232 - left * 232 / full;
                cv_rrect(4, 66, 232, 6, 2, T_LINE, T_BG);
                cv_rrect(4, 66, w, 6, 2, HC_YELLOW, T_LINE);
            }
            cv_text_on(4, 80, &AF_M, HGR_NAME[hcg.last % 5u], hcg.last == HGR_MISS ? HC_RED : HC_GREEN, T_BG);
            fmt_int(b, hcg.score);
            cv_text_r(236, 80, &AF_M, b, T_TEXT, T_BG);
            str_cpy(b, "COMBO ", sizeof b);
            fmt_int(b + str_len(b), hcg.combo);
            cv_text_on(4, 102, &AF_S, b, T_MID, T_BG);
            if (hcg.done) {
                str_cpy(b, "DONE  ", sizeof b);
                fmt_int(b + str_len(b), hcg.perfect + hcg.great + hcg.ok);
                str_cpy(b + str_len(b), " HIT  ", sizeof b - str_len(b));
                fmt_int(b + str_len(b), hcg.miss);
                str_cpy(b + str_len(b), " MISSED", sizeof b - str_len(b));
                cv_text_r(236, 102, &AF_S, b, HC_YELLOW, T_BG);
            }
        }
        cv_blit(0, HU_HEAD);
        cv_begin(240, 66, T_BG);
        cv_text_on(4, 2, &AF_S, "MODE MENU: SONG, DIFFICULTY, SPEED", T_MID, T_BG);
        cv_text_on(4, 47, &AF_M, HC_SOUNDS[c->sound % HC_NSOUNDS].name, HC_YELLOW, T_BG);
        cv_blit(0, HU_HEAD + 124);
        return;
    }
    if (c->play == HP_EAR) {                             /* the level, the phase, the streak */
        str_cpy(b, "LEVEL ", sizeof b);
        fmt_int(b + str_len(b), hcg.level + 1);
        cv_text_on(4, 2, &AF_S, b, HC_RED, T_BG);
        cv_text_r(236, 2, &AF_S, HGE_LEVEL_NAME[hcg.level % HGE_LEVELS], T_MID, T_BG);
        if (!hcg.phase)
            cv_text_c(120, 44, &AF_M, "PLAY STARTS A ROUND", T_DIM, T_BG);
        else if (hcg.phase == 1u)
            cv_text_c(120, 40, &AF_L, "LISTEN", HC_YELLOW, T_BG);
        else if (hcg.phase == 2u) {
            cv_text_c(120, 40, &AF_L, "YOUR TURN", HC_GREEN, T_BG);
            if (hcg.qn > 1u) {
                str_cpy(b, "CHORD ", sizeof b);
                fmt_int(b + str_len(b), hcg.ans + 1);
                str_cpy(b + str_len(b), " OF ", sizeof b - str_len(b));
                fmt_int(b + str_len(b), hcg.qn);
                cv_text_c(120, 80, &AF_S, b, T_MID, T_BG);
            }
        } else if (hcg.result == 1u)
            cv_text_c(120, 40, &AF_L, "CORRECT", HC_GREEN, T_BG);
        else {
            cv_text_c(120, 40, &AF_L, "NO", HC_RED, T_BG);
            str_cpy(b, "IT WAS ", sizeof b);
            str_cpy(b + str_len(b), HC_DEG_NAME[hcg.wrong_was % 7u], sizeof b - str_len(b));
            cv_text_c(120, 80, &AF_M, b, T_MID, T_BG);
        }
        str_cpy(b, "STREAK ", sizeof b);
        fmt_int(b + str_len(b), hcg.streak);
        cv_text_on(4, 102, &AF_S, b, T_MID, T_BG);
        fmt_int(b, hcg.right);
        str_cpy(b + str_len(b), " / ", sizeof b - str_len(b));
        fmt_int(b + str_len(b), hcg.total);
        cv_text_r(236, 102, &AF_S, b, T_MID, T_BG);
        cv_blit(0, HU_HEAD);
        cv_begin(240, 66, T_BG);
        cv_text_on(4, 2, &AF_S, "F#3 REPLAYS   MODE MENU: EAR LEVEL", T_MID, T_BG);
        cv_text_on(4, 47, &AF_M, HC_SOUNDS[c->sound % HC_NSOUNDS].name, HC_YELLOW, T_BG);
        cv_blit(0, HU_HEAD + 124);
        return;
    }
    /* MIXER: the four layers, their level, mute, state; the metronome */
    for (i = 0; i < HCL_LAYERS; i++) {
        int32_t x = 4 + (int32_t)i * 59, lvl = trk[i].p[P_LEVEL];
        uint32_t st = hcl.l[i].state;
        int mute = trk[i].p[P_MUTE] != 0;
        uint16_t col = st == HLS_PLAY ? HC_GREEN : st == HLS_REC ? HC_RED : T_MID;
        cv_rrect(x, 4, 56, 116, 6, i == song.sel ? T_RAISE : T_SURF, T_BG);
        b[0] = 'L'; b[1] = (char)('1' + i); b[2] = 0;
        cv_text_c(x + 28, 8, &AF_M, b, col, i == song.sel ? T_RAISE : T_SURF);
        cv_rrect(x + 22, 30, 12, 60, 3, T_LINE, i == song.sel ? T_RAISE : T_SURF);
        cv_rrect(x + 22, 30 + 60 - lvl * 60 / 127, 12, lvl * 60 / 127, 3, mute ? T_DIM : HC_BLUE, T_LINE);
        cv_rrect(x + 6, 96, 44, 18, 4, mute ? HC_RED : T_LINE, i == song.sel ? T_RAISE : T_SURF);
        cv_text_c(x + 28, 98, &AF_S, mute ? "MUTE" : HLS_NAME[st & 3u], mute ? RGB(255, 240, 240) : T_MID, mute ? HC_RED : T_LINE);
    }
    cv_blit(0, HU_HEAD);
    cv_begin(240, 66, T_BG);
    cv_text_on(4, 2, &AF_S, "KEYS 1-4 MUTE   KEY 7 METRO   OCT LEVEL", T_MID, T_BG);
    cv_rrect(4, 24, 90, 18, 4, hcl.metro ? HC_GREEN : T_SURF, T_BG);
    cv_text_c(49, 26, &AF_S, hcl.metro ? "METRONOME ON" : "METRONOME OFF", hcl.metro ? HC_INK : T_MID, hcl.metro ? HC_GREEN : T_SURF);
    cv_blit(0, HU_HEAD + 124);
}

/* the looper on the drum screens, to time REC (the bounce): the layers' dots, the live layer's state
 * (recording: the bar it is in), a bar of the loop with its bars ticked (a free first recording fills
 * against BARS, or the bar it is in) */
static void hui_loop_strip(int32_t y)
{
    uint32_t i, bar = beat_samples() * 4u, sel = hcl.sel % HCL_LAYERS, st = hcl.l[sel].state, total = hcl.len, pos = hcl.pos;
    char b[24];
    if (!bar)
        bar = 1;
    for (i = 0; i < HCL_LAYERS; i++) {
        uint32_t s = hcl.l[i].state;
        uint16_t col = s == HLS_PLAY ? HC_GREEN : s == HLS_REC ? HC_RED : s == HLS_ARMED ? HC_YELLOW : T_RAISE;
        cv_rrect(4 + (int32_t)i * 12, y + 2, 9, 9, 4, col, T_BG);
        if (i == sel)
            cv_rrect(6 + (int32_t)i * 12, y + 4, 5, 5, 2, s == HLS_OFF || s == HLS_ARMED ? T_TEXT : HC_INK, col);
    }
    if (st == HLS_REC) {
        uint32_t el = hc.clock - hcl.l[sel].start;
        if (!total) {                                    /* the first layer, free: against BARS, else the bar it is in */
            total = hcl.bars ? hcl.bars * bar : bar;
            pos = el % total;
        }
        str_cpy(b, "REC  BAR ", sizeof b);
        fmt_int(b + str_len(b), (int32_t)(el / bar) + 1);
        cv_text_on(56, y, &AF_S, b, HC_RED, T_BG);
    } else {
        str_cpy(b, "LAYER ", sizeof b);
        fmt_int(b + str_len(b), (int32_t)sel + 1);
        str_cpy(b + str_len(b), st == HLS_PLAY ? "  PLAY" : st == HLS_ARMED ? "  ARMED" : "  OFF", sizeof b - str_len(b));
        cv_text_on(56, y, &AF_S, b, st == HLS_PLAY ? HC_GREEN : st == HLS_ARMED ? HC_YELLOW : T_MID, T_BG);
    }
    cv_text_r(236, y, &AF_S, st == HLS_REC ? "REC: STOP" : "REC: BOUNCE", T_DIM, T_BG);
    cv_rrect(4, y + 15, 232, 4, 1, T_LINE, T_BG);
    if (total) {
        uint32_t bars = (total + bar / 2u) / bar;
        for (i = 1; i < bars && i < 32u; i++)
            cv_rrect(4 + (int32_t)(i * 232u / bars), y + 14, 1, 6, 0, T_MID, T_BG);
        cv_rrect(4, y + 15, 2 + (int32_t)((uint64_t)pos * 230u / total), 4, 1,
                 st == HLS_REC ? HC_RED : hcl.playing ? HC_GREEN : T_MID, T_LINE);
    }
}

/* the chord that sounds, on a piano of four octaves: its notes in the degree's colour, the root marked
 * with a dot, a note beyond the window as a dot at that edge. The window starts at the C at or below the
 * lowest note, moved up when the top note would not fit */
static void hui_draw_piano(uint32_t deg)
{
    static const uint8_t WHITE_OF[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};   /* the white key at or below */
    static const uint8_t IS_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    uint16_t col = deg < 7u ? HC_DEG_COL[deg] : HC_YELLOW, ccol;
    uint32_t i, lo = 127, hi = 0, start, root = (uint32_t)(hc.cur.root + 1200) % 12u, held = fm1_in.notes | hc.latched;
    uint8_t on[128];                                     /* 1 a note of the chord, 2 a strumplate note */
    memset(on, 0, sizeof on);
    if (hui_c()->play == HP_LEAD) {                      /* LEAD: the root alone sounds */
        on[clamp(hc.cur_root, 0, 127)] = 1;
    } else {
        for (i = 0; i < hc.cur.n; i++)
            if (hc.cur.note[i] <= 127u)
                on[hc.cur.note[i]] = 1;
    }
    for (i = 0; i < 27u; i++)
        if (((held >> i) & 1u) && hc_plate[i])
            on[hc_plate[i]] = 2;
    for (i = 0; i < 128u; i++)
        if (on[i]) {
            if (i < lo) lo = i;
            if (i > hi) hi = i;
        }
    if (lo > hi)
        return;
    ccol = hc.nheld || hc.latched ? col : T_MID;         /* the chord in colour while a chord key sounds, else dim:
                                                          * the plate strums the chord last played */
    start = lo - lo % 12u;
    if (hi >= start + 48u) {
        start = hi - 47u;
        start += (12u - start % 12u) % 12u;
    }
    for (i = 0; i < 28u; i++)
        cv_rrect(8 + (int32_t)i * 8, 2, 7, 66, 1, T_SURF, T_BG);
    for (i = start; i < start + 48u && i < 128u; i++)
        if (on[i] && !IS_BLACK[i % 12u]) {
            int32_t x = 8 + (int32_t)((i - start) / 12u * 7u + WHITE_OF[i % 12u]) * 8;
            uint16_t c = on[i] == 2u ? HC_INK : ccol;
            cv_rrect(x, 2, 7, 66, 1, c, T_BG);
            if (on[i] == 1u && i % 12u == root)
                cv_rrect(x + 2, 60, 3, 3, 1, HC_INK, c);
        }
    for (i = start; i < start + 48u && i < 128u; i++)
        if (IS_BLACK[i % 12u]) {
            int32_t x = 8 + (int32_t)((i - start) / 12u * 7u + WHITE_OF[i % 12u]) * 8 + 5;
            uint16_t c = on[i] == 2u ? HC_INK : on[i] ? ccol : T_RAISE;
            cv_rrect(x, 2, 5, 40, 1, c, T_BG);
            if (on[i] == 1u && i % 12u == root)
                cv_rrect(x + 1, 32, 3, 3, 1, HC_INK, c);
        }
    for (i = 0; i < 128u; i++)                           /* a note beyond the window: a dot at that edge */
        if (on[i] && (i < start || i >= start + 48u))
            cv_rrect(i < start ? 2 : 234, 32, 4, 4, 2, on[i] == 2u ? HC_INK : ccol, T_BG);
}

/* HOME's middle: the chord big, its degree and modifier; the keyboard; the sound and the effects */
static void hui_draw_home(void)
{
    const hc_trk_t *c = hui_c();
    uint32_t k = song.sel, held = fm1_in.notes | hc.latched, i, fxm = hui_fx_mask(c);
    const char *name = chord_last[k].n && chord_last[k].name[0] ? chord_last[k].name : hc.name[0] ? hc.name : "";
    uint32_t deg = 7, lock = 0, invk = 0, sig;
    char lead[8];
    if (c->play == HP_LEAD) {                            /* LEAD: the one note that sounds (the chord's root), named */
        if (hc.cur.n) {
            uint32_t r = (uint32_t)clamp(hc.cur_root, 0, 127);
            str_cpy(lead, HC_NOTE_NAME[r % 12u], sizeof lead);
            fmt_int(lead + str_len(lead), (int32_t)(r / 12u) - 1);
            name = lead;
        } else {
            name = "";
        }
    }
    sig = str_hash(11u, name) + held * 31u + hc.dir * 7u + hc.cur_dir * 13u + fxm * 1013u + c->sound * 4099u +
          hc.hold * 65537u + chord_last[k].gen * 3u;
    for (i = hc.nheld; i-- > 0;)                         /* the degree and lock of the chord key: pressed last
                                                          * (BASS SLASH: the key held first is only the bass) */
        if (!key_black(hc.order[i])) {
            int32_t o;
            deg = hc_degree_of_key(TSEL, hc.order[i], &o) % 7u;
            lock |= hc.lock[hc.order[i]].on;
            invk = hc.inv[hc.order[i]];
            break;
        }
    if (deg == 7u)
        for (i = 0; i < 27u; i++)                        /* (latched keys: HOLD) */
            if (((held >> i) & 1u) && !key_black(i)) {
                int32_t o;
                deg = hc_degree_of_key(TSEL, i, &o) % 7u;
                lock |= hc.lock[i].on;
            }
    sig += deg * 101u + lock * 7u + invk * 131u + hc.cur.n * 131u + (uint32_t)(hc.cur.root + 1200) * 3u;
    for (i = 0; i < hc.cur.n && i < HS_N; i++)           /* the piano: the chord's notes */
        sig += (uint32_t)hc.cur.note[i] * (7u + i * 3u);
    for (i = 0; i < HCL_LAYERS; i++)
        sig += (hcl.l[i].state + 1u) * (3001u << i);
    sig += hcl.playing * 7u + (hcl.len ? hcl.pos * 24u / hcl.len : 0u) * 51u + hcl.sel * 5u;
    if (hcl.l[hcl.sel % HCL_LAYERS].state == HLS_REC)   /* a recording: its eighths (the drum screens' strip) */
        sig += ((hc.clock - hcl.l[hcl.sel % HCL_LAYERS].start) / (beat_samples() / 2u + 1u)) * 29u;
    sig += hcg.running * 53u + hcg.done * 59u + hcg.idx * 61u + hcg.last * 67u + hcg.score * 71u + hcg.combo * 73u +
           hcg.phase * 79u + hcg.ans * 83u + hcg.result * 89u + hcg.streak * 97u + hcg.total * 101u + hcg.level * 103u +
           hcg.song * 107u + (hcg.running ? (hcg.pos / 2048u) * 109u : 0u);
    sig += c->play * 7919u + hcs.cur * 13u + hcs.step * 17u + hcs.running * 3u + hcs.dl_running * 5u + hcs.dl_step * 19u +
           hcs.len * 23u + c->dl_style * 29u + c->dl_var * 31u + hcs.pads * 37u + hcl.metro * 41u;
    for (i = 0; i < NTRK; i++)
        sig += (uint32_t)(trk[i].p[P_LEVEL] + 1) * (43u << i) + (uint32_t)trk[i].p[P_MUTE] * (47u << i);
    if (c->play >= HP_SEQ) {                             /* SEQUENCER, DRUM, DRUM LOOP, MIXER: their own body */
        if (!hui.force && sig == hui.sig[1])
            return;
        hui.sig[1] = sig;
        hui_draw_mode_body();
        return;
    }
    if (!hui.force && sig == hui.sig[1])
        return;
    hui.sig[1] = sig;
    /* the chord */
    cv_begin(240, 98, T_BG);
    for (i = 0; i < HCL_LAYERS; i++) {                   /* the looper's layers: OFF dim, ARMED yellow, REC red, PLAY green */
        uint32_t st = hcl.l[i].state;
        uint16_t c = st == HLS_PLAY ? HC_GREEN : st == HLS_REC ? HC_RED : st == HLS_ARMED ? HC_YELLOW : T_RAISE;
        cv_rrect(8 + (int32_t)i * 14, 6, 10, 10, 5, c, T_BG);
        if (i == song.sel)
            cv_rrect(10 + (int32_t)i * 14, 8, 6, 6, 3, st == HLS_OFF || st == HLS_ARMED ? T_TEXT : HC_INK, c);
    }
    if (hcl.len) {
        cv_rrect(8, 19, 52, 3, 1, T_LINE, T_BG);
        cv_rrect(8, 19, 2 + (int32_t)(hcl.pos * 50u / hcl.len), 3, 1, hcl.playing ? HC_GREEN : T_MID, T_LINE);
    }
    {
        uint16_t col = deg < 7u ? HC_DEG_COL[deg] : T_THEME;
        int32_t w = text_w(&AF_X, name[0] ? name : "-");
        if (w > 232) {
            cv_text_c(120, 30, &AF_L, name, col, T_BG);
        } else {
            cv_text_c(120, 18, &AF_X, name[0] ? name : "-", col, T_BG);
        }
        if (deg < 7u) {
            char b[32];
            str_cpy(b, HC_DEG_NAME[deg], sizeof b);
            if (hc.dir != HD_NONE) {
                str_cpy(b + str_len(b), "  ", sizeof b - str_len(b));
                str_cpy(b + str_len(b), HC_DIR_NAME[hc.dir], sizeof b - str_len(b));
            }
            if (invk) {                                  /* the key's inversion (INVERT): 1ST / 2ND */
                str_cpy(b + str_len(b), "  ", sizeof b - str_len(b));
                str_cpy(b + str_len(b), invk == 1u ? "1ST INV" : "2ND INV", sizeof b - str_len(b));
            }
            cv_text_c(120, 76, &AF_M, b, T_MID, T_BG);
        } else if (!name[0]) {
            cv_text_c(120, 76, &AF_M, "PLAY A WHITE KEY", T_DIM, T_BG);
        }
        if (lock)
            cv_rrect(170, 79, 56, 18, 5, HC_RED, T_BG), cv_text_c(198, 81, &AF_S, "LOCKED", RGB(255, 240, 240), HC_RED);
        if (hc.hold)
            cv_rrect(14, 79, 46, 18, 5, HC_GREEN, T_BG), cv_text_c(37, 81, &AF_S, "HOLD", HC_INK, HC_GREEN);
    }
    cv_blit(0, HU_HEAD);
    /* the keyboard: 16 white keys, 11 black, the held ones in their degree's colour, the directions dim */
    cv_begin(240, 92, T_BG);
    if (held && hc.cur.n) {                              /* a chord sounds: its notes on a piano */
        hui_draw_piano(deg);
    } else {
        int32_t ww = 14, x0 = 4;
        for (i = 0; i < 27u; i++) {
            uint32_t p = key_place(i), on = (held >> i) & 1u;
            if (!key_black(i)) {
                int32_t o, j = hc_plate_of_key(TSEL, i), x = x0 + (int32_t)p * (ww + 1);
                uint32_t d = hc_degree_of_key(TSEL, i, &o) % 7u;
                char dn[2] = {(char)('1' + d), 0};
                if (j >= 0) {                            /* a strumplate key: a bar as high as its note */
                    int32_t h = 4 + j * 2;
                    cv_rrect(x, 2, ww, 66, 2, on ? HC_INK : T_SURF, T_BG);
                    cv_rrect(x + 4, 65 - h, ww - 8, h, 1, on ? T_BG : T_MID, on ? HC_INK : T_SURF);
                    continue;
                }
                cv_rrect(x, 2, ww, 66, 2, on ? HC_DEG_COL[d] : T_SURF, T_BG);
                cv_rrect(x + 3, 62, ww - 6, 3, 1, HC_DEG_COL[d], on ? HC_DEG_COL[d] : T_SURF);   /* the degree's colour */
                cv_text_c(x + ww / 2, 44, &AF_S, dn, on ? HC_INK : T_MID, on ? HC_DEG_COL[d] : T_SURF);   /* its number */
            }
        }
        for (i = 0; i < 27u; i++) {
            uint32_t p = key_place(i), on = (held >> i) & 1u;
            if (key_black(i)) {
                static const uint8_t AFTER[HC_NBLACK] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};   /* the white key it follows */
                int32_t x = x0 + (int32_t)AFTER[p] * (ww + 1) + ww - 4;
                uint16_t col = on ? (p < 3u ? HC_GREEN : HC_YELLOW) : T_RAISE;
                cv_rrect(x, 2, 9, 40, 2, col, T_BG);
                if (p < 3u) {                            /* INVERT LOCK HOLD */
                    char fl[2] = {"ILH"[p], 0};
                    cv_text_c(x + 4, 14, &AF_S, fl, on ? HC_INK : T_MID, col);
                } else {                                 /* a joystick direction: the dot where it points */
                    static const int8_t DX[8] = {0, 1, 1, 1, 0, -1, -1, -1}, DY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
                    uint32_t dir = HC_BLACK_FN[p] - HD_UP;
                    cv_rrect(x + 3 + DX[dir % 8u] * 2, 20 + DY[dir % 8u] * 8, 3, 3, 1, on ? HC_INK : T_MID, col);
                }
            }
        }
    }
    {
        /* the sound and its effects */
        cv_text_on(4, 73, &AF_M, HC_SOUNDS[c->sound % HC_NSOUNDS].name, HC_YELLOW, T_BG);
        {
            static const char *const CHIP[12] = {"REV", "DLY", "CHO", "FLG", "TRM", "LFO", "GLD", "DRV", "TAPE", "FLT", "HP", "ST"};
            int32_t x = 236;
            for (i = 12; i > 0; i--)
                if ((fxm >> (i - 1u)) & 1u) {
                    int32_t w = text_w(&AF_S, CHIP[i - 1u]) + 6;
                    x -= w + 3;
                    cv_rrect(x, 74, w, 15, 3, HC_YELLOW, T_BG);
                    cv_text_c(x + w / 2, 75, &AF_S, CHIP[i - 1u], HC_INK, HC_YELLOW);
                }
        }
    }
    cv_blit(0, HU_HEAD + 98);
}

/* a menu: its rows, the selected one marked in the menu's colour */
static void hui_draw_menu(void)
{
    uint32_t sc = hui.screen, nrows = HU_ROWS[sc], i, sig = sc * 3u + hui.sel[sc] * 17u, pass;
    char val[20];
    const char *const *labels = sc == HU_KEY ? RK_LABEL : sc == HU_SOUND ? RS_LABEL : sc == HU_MODE ? RM_LABEL :
                                sc == HU_LOOP ? RL_LABEL : 0;
    uint16_t col = HU_COL[sc];
    if (hui.sel[sc] < hui.top[sc])
        hui.top[sc] = hui.sel[sc];
    if (hui.sel[sc] >= hui.top[sc] + HU_NROWS_SHOWN)
        hui.top[sc] = (uint8_t)(hui.sel[sc] - HU_NROWS_SHOWN + 1u);
    for (i = 0; i < nrows; i++) {
        hui_row_value(sc, i, val);
        sig = str_hash(sig + i * 7u, val);
    }
    sig += hui.top[sc] * 131u + (sc == HU_LOOP ? hcl.playing * 7u + (hcl.len ? hcl.pos * 16u / hcl.len : 0u) * 977u : 0u);
    if (!hui.force && sig == hui.sig[1])
        return;
    hui.sig[1] = sig;
    for (pass = 0; pass < 2u; pass++) {
        int32_t top = pass ? HU_ROWS_Y + 124 : HU_ROWS_Y;
        uint32_t h = pass ? (uint32_t)(HU_FOOT_Y - HU_ROWS_Y - 124) : 124u;
        cv_begin(240, h, T_BG);
        cv_oy = -top;
        for (i = hui.top[sc]; i < nrows && i < hui.top[sc] + HU_NROWS_SHOWN; i++) {
            int32_t y = HU_ROWS_Y + (int32_t)(i - hui.top[sc]) * HU_ROW;
            int sel = i == hui.sel[sc], act;
            uint16_t bg = sel ? T_SURF : T_BG;
            char lb[20];
            if (y + HU_ROW <= top || y >= top + (int32_t)h)
                continue;
            act = hui_row_value(sc, i, val);
            if (sc == HU_PRESET) {
                str_cpy(lb, "P1", sizeof lb);
                lb[1] = (char)('1' + i);
            } else {
                str_cpy(lb, labels[i], sizeof lb);
            }
            cv_rrect(4, y, 232, HU_ROW - 2, 5, bg, T_BG);
            if (sel)
                cv_rrect(4, y, 5, HU_ROW - 2, 2, col, bg);
            cv_text_on(16, y + 4, &AF_S, lb, sel ? T_TEXT : T_MID, bg);
            if (act)
                cv_text_r(228, y + 3, &AF_M, "GO", sel ? col : T_DIM, bg);
            else
                cv_text_r(228, y + 3, &AF_M, val, sel ? col : T_THEME, bg);
        }
        if (nrows > HU_NROWS_SHOWN) {                    /* a scroll bar */
            int32_t bar = (HU_FOOT_Y - HU_ROWS_Y - 6), th = bar * HU_NROWS_SHOWN / (int32_t)nrows;
            cv_rrect(237, HU_ROWS_Y + 2, 2, bar, 1, T_LINE, T_BG);
            cv_rrect(237, HU_ROWS_Y + 2 + (bar - th) * (int32_t)hui.top[sc] / (int32_t)(nrows - HU_NROWS_SHOWN), 2, th, 1, col, T_LINE);
        }
        cv_oy = 0;
        cv_blit(0, (uint32_t)top);
    }
}

static void hui_draw_menu_head(void)
{
    uint32_t sc = hui.screen, sig = sc * 5u + 2u + (hui.msg_t ? str_hash(5u, hui.msg) : 0u);
    uint16_t col = HU_COL[sc];
    if (!hui.force && sig == hui.sig[0])
        return;
    hui.sig[0] = sig;
    cv_begin(240, HU_ROWS_Y, T_BG);
    cv_rrect(0, 0, 240, HU_HEAD, 0, hui.msg_t ? HC_YELLOW : col, T_BG);
    cv_text_on(8, 4, &AF_M, hui.msg_t ? hui.msg : HU_TITLE[sc], sc == HU_MODE && !hui.msg_t ? RGB(255, 240, 240) : HC_INK,
               hui.msg_t ? HC_YELLOW : col);
    if (!hui.msg_t)
        cv_text_r(232, 6, &AF_S, sc == HU_KEY ? "KEY & CHORDS" : sc == HU_SOUND ? "SOUNDS, EFFECTS, SETTINGS" :
                  sc == HU_MODE ? "MODES & TEMPO" : sc == HU_LOOP ? "4 LAYERS" : "P1..P4",
                  sc == HU_MODE ? RGB(255, 220, 220) : HC_INK, col);
    cv_blit(0, 0);
}

/* a list: its title, the choices with the current one marked (scrolled to it), a scroll bar */
static void hui_draw_pick(void)
{
    static const char *const TITLE[4] = {"ENVELOPE", "LFO", "SOUND", "MODE"};
    static const char *const SUB[4] = {"ENV: BACK", "LFO: BACK", "PRESETS KNOB", "ALGORITHM KNOB"};
    uint32_t k = hui.pick_kind % 4u, n = hui_pick_count(k), cur = hui_pick_cur(k), i, top, pass;
    uint16_t col = k == PK_MODE ? HC_RED : HC_YELLOW, ink = k == PK_MODE ? RGB(255, 240, 240) : HC_INK;
    uint32_t sig = 0x50494Bu + k * 7u + cur * 13u + n * 3u;
    if (cur >= hui.top[HU_PICK] + HU_NROWS_SHOWN)
        hui.top[HU_PICK] = (uint8_t)(cur - HU_NROWS_SHOWN + 1u);
    if (cur < hui.top[HU_PICK])
        hui.top[HU_PICK] = (uint8_t)cur;
    top = hui.top[HU_PICK];
    sig += top * 131u;
    if (!hui.force && sig == hui.sig[1])
        return;
    hui.sig[1] = sig;
    hui.sig[0] = 0;
    cv_begin(240, HU_ROWS_Y, T_BG);
    cv_rrect(0, 0, 240, HU_HEAD, 0, col, T_BG);
    cv_text_on(8, 4, &AF_M, TITLE[k], ink, col);
    cv_text_r(232, 6, &AF_S, SUB[k], ink, col);
    cv_blit(0, 0);
    for (pass = 0; pass < 2u; pass++) {
        int32_t y0 = pass ? HU_ROWS_Y + 124 : HU_ROWS_Y;
        uint32_t h = pass ? (uint32_t)(HU_FOOT_Y - HU_ROWS_Y - 124) : 124u;
        cv_begin(240, h, T_BG);
        cv_oy = -y0;
        for (i = top; i < n && i < top + HU_NROWS_SHOWN; i++) {
            int32_t y = HU_ROWS_Y + (int32_t)(i - top) * HU_ROW;
            int sel = i == cur;
            uint16_t bg = sel ? T_SURF : T_BG;
            if (y + HU_ROW <= y0 || y >= y0 + (int32_t)h)
                continue;
            cv_rrect(4, y, 232, HU_ROW - 2, 5, bg, T_BG);
            if (sel)
                cv_rrect(4, y, 5, HU_ROW - 2, 2, col, bg);
            cv_text_on(16, y + 3, &AF_M, hui_pick_name(k, i), sel ? col : T_MID, bg);
        }
        if (n > HU_NROWS_SHOWN) {
            int32_t bar = (HU_FOOT_Y - HU_ROWS_Y - 6), th = bar * HU_NROWS_SHOWN / (int32_t)n;
            cv_rrect(237, HU_ROWS_Y + 2, 2, bar, 1, T_LINE, T_BG);
            cv_rrect(237, HU_ROWS_Y + 2 + (bar - th) * (int32_t)top / (int32_t)(n - HU_NROWS_SHOWN), 2, th, 1, col, T_LINE);
        }
        cv_oy = 0;
        cv_blit(0, y0);
    }
}

static void hui_draw(void)
{
    if (hui.force)
        lcd_fill(0, 0, 240, 240, T_BG);
    if (hui.msg_t)
        hui.msg_t--;
    if (hui.screen == HU_HOME) {
        hui_draw_head();
        hui_draw_home();
    } else if (hui.screen == HU_PICK) {
        hui_draw_pick();
    } else {
        hui_draw_menu_head();
        hui_draw_menu();
    }
    hui_draw_foot();
    hui.force = 0;
    ui.force = 0;
}
