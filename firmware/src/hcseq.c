/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* The HiChord's SEQUENCER, DRUM and DRUM LOOP modes.
 *   SEQUENCER  up to 16 chord steps of a beat each on the live track: a white key writes the step at the
 *              cursor (its degree, its octave, the direction held with it) and sounds it; F#3 writes a rest;
 *              the cursor moves on. PLAY runs it round (hui.c); the steps' chords are built as the keys'
 *              are, so the key, the scale, BASS, VOICES and the joystick table apply as they are now.
 *   DRUM       the white keys are seven pads (KICK, KICK 2, SNARE, HAT CL, TOM, BELL, HAT OP: the HiChord's
 *              order, General MIDI notes on Felucca's DRUM engine); a direction held with a pad repeats it
 *              at that rate (AUTO-DRUM). The engine switch is the UI's (hui.c hui_mode_set).
 *   DRUM LOOP  7 styles x 8 variations of 16 sixteenths over 6 lanes; a white key picks the style and starts
 *              it, PLAY stops and starts it. The notes go through hc_note_on: the looper records them.
 * Included by hichord.c after the play modes; runs in the audio ISR (hcs_tick from hc_tick). */
static const uint8_t HC_PAD_NOTE[7] = {36, 35, 38, 42, 45, 56, 46};
static const char *const HC_PAD_NAME[7] = {"KICK", "KICK 2", "SNARE", "HAT CL", "TOM", "BELL", "HAT OP"};
static const uint8_t HC_LANE_NOTE[6] = {36, 38, 42, 45, 46, 49};   /* the loops' lanes: kick snare hat tom open cymbal */
enum { DL_ROCK, DL_BREAK, DL_DUB, DL_FUNK, DL_HOUSE, DL_DEMBOW, DL_SWING, DL_STYLES };
enum { DV_ORIG, DV_GHOST, DV_BUSY, DV_SYNC, DV_FILL, DV_HALF, DV_DOUBLE, DV_PERC, DV_VARS };
static const char *const DL_NAME[DL_STYLES] = {"ROCK", "BREAK", "DUB", "FUNK", "HOUSE", "DEMBOW", "SWING"};
static const char *const DV_NAME[DV_VARS] = {"ORIG", "GHOST", "BUSY", "SYNCOPATED", "FILL", "HALF-TIME", "DOUBLE-TIME", "PERC"};
#define HL_LK 1u
#define HL_LS 2u
#define HL_LH 4u
#define HL_LT 8u
#define HL_LO 16u
#define HL_LC 32u
/* the ORIG of each style: 16 sixteenths, a bit per lane */
static const uint8_t DL_ORIG[DL_STYLES][16] = {
    {HL_LK | HL_LH, 0, HL_LH, 0, HL_LS | HL_LH, 0, HL_LH, 0, HL_LK | HL_LH, 0, HL_LK | HL_LH, 0, HL_LS | HL_LH, 0, HL_LH, 0},            /* ROCK */
    {HL_LK | HL_LH, 0, HL_LH, 0, HL_LS | HL_LH, 0, HL_LK | HL_LH, 0, HL_LH, 0, HL_LK | HL_LH, 0, HL_LS | HL_LH, 0, HL_LH, HL_LS},            /* BREAK */
    {HL_LH, 0, HL_LH, 0, HL_LH, 0, HL_LH, 0, HL_LK | HL_LS | HL_LH, 0, HL_LH, 0, HL_LH, 0, HL_LO, 0},                            /* DUB */
    {HL_LK | HL_LH, 0, HL_LH, HL_LK, HL_LS | HL_LH, 0, HL_LH, 0, HL_LK | HL_LH, 0, HL_LH, HL_LK, HL_LS | HL_LH, HL_LO, HL_LH, 0},               /* FUNK */
    {HL_LK, HL_LH, HL_LO, HL_LH, HL_LK | HL_LS, HL_LH, HL_LO, HL_LH, HL_LK, HL_LH, HL_LO, HL_LH, HL_LK | HL_LS, HL_LH, HL_LO, HL_LH},                     /* HOUSE */
    {HL_LK | HL_LH, 0, HL_LH, HL_LS, HL_LK | HL_LH, 0, HL_LS | HL_LH, 0, HL_LK | HL_LH, 0, HL_LH, HL_LS, HL_LK | HL_LH, 0, HL_LS | HL_LH, 0},      /* DEMBOW */
    {HL_LK | HL_LC, 0, 0, HL_LC, HL_LC | HL_LH, HL_LS, 0, HL_LC, HL_LK | HL_LC, 0, 0, HL_LC, HL_LC | HL_LH, HL_LS, 0, HL_LC},                /* SWING */
};
#undef HL_LK
#undef HL_LS
#undef HL_LH
#undef HL_LT
#undef HL_LO
#undef HL_LC

/* the lanes of step s (0..15) of style / variation: the ORIG shaped by the variation */
static uint32_t hcd_lanes(uint32_t style, uint32_t var, uint32_t s)
{
    uint32_t m = DL_ORIG[style % DL_STYLES][s & 15u];
    switch (var % DV_VARS) {
    case DV_GHOST: return m | ((s == 7u || s == 15u) ? 2u : 0u);                      /* snares */
    case DV_BUSY: return m | 4u;                                                        /* hats on every sixteenth */
    case DV_SYNC: return (m & ~(s == 8u ? 1u : 0u)) | ((s == 7u || s == 10u) ? 1u : 0u);   /* the kicks pushed */
    case DV_FILL: return s < 12u ? m : (uint32_t)(s == 12u || s == 13u ? 8u : s == 14u ? 2u | 8u : 2u | 32u);   /* toms, a crash */
    case DV_HALF: return (m & ~3u) | (s == 0u ? 1u : s == 8u ? 2u : 0u);
    case DV_DOUBLE: return (m & ~3u) | ((s & 3u) == 0u ? 1u : (s & 3u) == 2u ? 2u : 0u);
    case DV_PERC: return (m & ~3u) | ((s & 3u) == 2u ? 32u : 0u) | ((s == 5u || s == 13u) ? 8u : 0u);
    default: return m;
    }
}
/* GHOST's extra snares and SWING's pedal hats play soft */
static uint32_t hcd_vel(uint32_t style, uint32_t var, uint32_t s, uint32_t lane)
{
    if (var % DV_VARS == DV_GHOST && lane == 1u && (s == 7u || s == 15u))
        return 55u;
    if (style % DL_STYLES == DL_SWING && (lane == 2u || (lane == 1u && (s == 5u || s == 13u))))
        return 50u;
    return 100u;
}

static struct {
    /* the sequencer */
    uint8_t len;                 /* 4..16 steps */
    uint8_t cur;                 /* the step the next key writes */
    uint8_t running;
    uint8_t step;                /* the step playing */
    uint32_t pos;                /* samples into it */
    struct { uint8_t deg; int8_t oct; uint8_t dir; } st[16];   /* deg 255 = a rest */
    uint8_t sn, snote[HS_N];     /* the step's notes sounding */
    char name[12];               /* the step's chord (the display) */
    /* the drum loop */
    uint8_t dl_running;
    uint8_t dl_step;
    uint8_t dl_off;              /* lanes hit in the last step: their note-offs go out a block later */
    uint32_t dl_pos;
    uint8_t bounce;              /* leaving SEQUENCER while it ran: it plays on into the looper's first layer */
    /* AUTO-DRUM: the pads held, the rate of the direction held */
    uint32_t pads;               /* a bit per key */
    uint32_t ad_pos, ad_len;
    uint8_t ad_step;
} hcs = {.len = 4};

static void hcs_notes_off(track_t *t)
{
    uint32_t i;
    for (i = 0; i < hcs.sn; i++)
        hc_note_off(t, hcs.snote[i]);
    hcs.sn = 0;
}

/* the chord of step s on track t (the step's degree, octave and direction, the track's settings; VOICE LEADING
 * against the chord before when lead = 1) -> ch and its name; 0 = a rest */
static int hcs_step_chord(const track_t *t, uint32_t s, hchord_t *ch, char *name, int lead)
{
    const hc_trk_t *c = hc_of(t);
    uint32_t mask = hc_mask(t), q;
    int32_t r, roff, root, bass, o = 0;
    if (hcs.st[s].deg == 255u) {
        name[0] = 0;
        return 0;
    }
    q = hs_degree_chord(mask, hcs.st[s].deg, &r);
    root = HC_BASE + t->p[P_ROOT] + r + 12 * (hcs.st[s].oct + song.octave) + t->p[P_TRANS];
    q = hq_modify(c->mode, hcs.st[s].dir, q, &roff);
    root += roff;
    bass = c->bass == HB_OFF ? -1 : root - 24;
    while (bass >= 0 && bass < 24)
        bass += 12;
    if (lead && c->vlead && hc.last[trk_index(t)].n) {
        uint32_t inv = hs_lead(root, q, bass, c->voices, hc.last[trk_index(t)].note, hc.last[trk_index(t)].n, &o);
        hs_voice(ch, root + o, q, bass < 0 ? -1 : bass + o, c->voices, inv);
    } else {
        hs_voice(ch, root, q, bass, c->voices, 0);
    }
    hs_name(name, (uint32_t)t->p[P_ROOT], root + o, q, bass);
    return 1;
}

/* the chord of step s sounds */
static void hcs_step_play(track_t *t, uint32_t s)
{
    uint32_t i;
    hchord_t ch;
    hcs_notes_off(t);
    if (!hcs_step_chord(t, s, &ch, hcs.name, 1))
        return;
    hc.last[trk_index(t)].n = 0;
    for (i = 0; i < ch.n && hc.last[trk_index(t)].n < HS_N; i++)
        if ((int32_t)ch.note[i] != ch.bass)
            hc.last[trk_index(t)].note[hc.last[trk_index(t)].n++] = ch.note[i];
    for (i = 0; i < ch.n && i < HS_N; i++) {
        hc_note_on(t, ch.note[i]);
        hcs.snote[hcs.sn++] = ch.note[i];
    }
}

static void hcs_start(track_t *t)
{
    hcs.running = 1;
    hcs.step = 0;
    hcs.pos = 0;
    hcs_step_play(t, 0);
}
static void hcs_stop(track_t *t)
{
    hcs.running = 0;
    hcs_notes_off(t);
    hcs.name[0] = 0;
}
static void hcs_clear(void)
{
    uint32_t i;
    for (i = 0; i < 16u; i++)
        hcs.st[i].deg = 255;
    hcs.cur = 0;
}

/* SEQUENCER: key k writes the step at the cursor (the direction held with it), the cursor moves on */
static void hcs_key_write(const track_t *t, uint32_t k)
{
    int32_t oct;
    uint32_t d = hc_degree_of_key(t, k, &oct);
    hcs.st[hcs.cur].deg = (uint8_t)(d % 7u);
    hcs.st[hcs.cur].oct = (int8_t)oct;
    hcs.st[hcs.cur].dir = hc.dir;
    hcs.cur = (uint8_t)((hcs.cur + 1u) % (hcs.len ? hcs.len : 1u));
}
static void hcs_rest_write(void)
{
    hcs.st[hcs.cur].deg = 255;
    hcs.cur = (uint8_t)((hcs.cur + 1u) % (hcs.len ? hcs.len : 1u));
}

/* DRUM: pad key k down / up; AUTO-DRUM when a direction is held */
static uint32_t hcd_pad_note(uint32_t k) { return HC_PAD_NOTE[key_place(k) % 7u]; }
static void hcd_key(track_t *t, uint32_t k, int down)
{
    if (down) {
        hcs.pads |= 1u << k;
        hc_note_on(t, hcd_pad_note(k));
        if (hc.dir != HD_NONE && !(hcs.pads & ~(1u << k)))
            hcs.ad_pos = hcs.ad_len = 0;                 /* the first pad with a direction: the repeat starts now */
    } else {
        hcs.pads &= ~(1u << k);
        hc_note_off(t, hcd_pad_note(k));
    }
}
static uint32_t hcd_rate_of_dir(uint32_t d)
{
    static const uint8_t R[8] = {HR_1_4, HR_SW8, HR_1_8, HR_SW16, HR_1_16, HR_1_16T, HR_1_32, HR_SW8};
    return d < 8u ? R[d] : HR_1_8;
}

/* DRUM LOOP: the style's loop starts (a white key); PLAY stops and starts */
static void hcd_loop_start(void)
{
    hcs.dl_running = 1;
    hcs.dl_step = 0;
    hcs.dl_pos = 0;
}
static void hcd_loop_stop(void) { hcs.dl_running = 0; }
static void hcd_loop_step(track_t *t, uint32_t s)
{
    const hc_trk_t *c = hc_of(t);
    uint32_t m = hcd_lanes(c->dl_style, c->dl_var, s), l;
    for (l = 0; l < 6u; l++)
        if ((m >> l) & 1u) {
            uint32_t v = hcd_vel(c->dl_style, c->dl_var, s, l);
            input_on(t, HC_LANE_NOTE[l], v);
            midi_out_event(0x09u | (0x90u | trk_midi_ch(trk_index(t))) << 8 | (uint32_t)HC_LANE_NOTE[l] << 16 | v << 24);
            hcs.dl_off |= (uint8_t)(1u << l);            /* the note-off next block (hcd_loop_release): a voice
                                                          * starts with its ADSR at 0, and a release before its
                                                          * first block ends it at once, a click; one block in, the
                                                          * drum's own hit holds it (drum_amp) */
        }
}
/* the loop's hits of the last step: their note-offs, a block after the note-ons */
static void hcd_loop_release(track_t *t)
{
    uint32_t l;
    for (l = 0; l < 6u && hcs.dl_off; l++)
        if ((hcs.dl_off >> l) & 1u) {
            hcs.dl_off &= (uint8_t)~(1u << l);
            input_off(t, HC_LANE_NOTE[l]);
            midi_out_event(0x08u | (0x80u | trk_midi_ch(trk_index(t))) << 8 | (uint32_t)HC_LANE_NOTE[l] << 16);
        }
}

static void hcs_mode_left(track_t *t)                  /* a mode change: what ran stops (a bounce plays on) */
{
    if (hcs.running && hc_of(t)->play == HP_SEQ && !hcl.len && hcl.l[trk_index(t)].state == HLS_OFF && trk_index(t) == hcl_live()) {
        hcl.bars = (uint8_t)((hcs.len + 3u) / 4u);     /* the HiChord: leaving the sequencer bounces it to the looper */
        hcl.sel = (uint8_t)trk_index(t);
        hcl_rec_press();
        hcl_rec_press();
        hcs.bounce = 1;
        hcs.step = 0;
        hcs.pos = 0;
        hcs_step_play(t, 0);
    } else {
        hcs_stop(t);
    }
    hcd_loop_stop();
    hcs.pads = 0;
}

/* MIXER: the white keys 1..4 mute / unmute the looper's layers, key 7 the metronome */
static void hcm_key(uint32_t k)
{
    uint32_t p = key_place(k) % 7u;
    if (p < HCL_LAYERS)
        trk[p].p[P_MUTE] = (int16_t)!trk[p].p[P_MUTE];
    else if (p == 6u)
        hcl.metro = (uint8_t)!hcl.metro;
}

/* one block: the sequencer's steps, the drum loop's sixteenths, AUTO-DRUM */
static void hcs_tick(uint32_t n)
{
    track_t *t = TSEL;
    const hc_trk_t *c = hc_of(t);
    if (hcs.dl_off)
        hcd_loop_release(t);
    if (hcs.bounce && hcl.l[trk_index(t)].state != HLS_REC) {   /* the bounce recorded: the sequencer is done */
        hcs.bounce = 0;
        hcs_stop(t);
    }
    if (hcs.running && (c->play == HP_SEQ || hcs.bounce)) {
        uint32_t q = beat_samples();
        hcs.pos += n;
        if (hcs.pos >= q) {
            hcs.pos -= q;
            hcs.step = (uint8_t)((hcs.step + 1u) % (hcs.len ? hcs.len : 1u));
            hcs_step_play(t, hcs.step);
        }
    }
    if (hcs.dl_running && c->play == HP_DRUMLOOP) {
        uint32_t q = beat_samples() / 4u;
        hcs.dl_pos += n;
        if (hcs.dl_pos >= q) {
            hcs.dl_pos -= q;
            hcs.dl_step = (uint8_t)((hcs.dl_step + 1u) & 15u);
            hcd_loop_step(t, hcs.dl_step);
        }
    }
    if (c->play == HP_DRUM && hcs.pads && hc.dir != HD_NONE) {   /* AUTO-DRUM: the pads held repeat at the direction's rate */
        hcs.ad_pos += n;
        if (hcs.ad_pos >= hcs.ad_len) {
            uint32_t k;
            hcs.ad_pos = hcs.ad_len ? hcs.ad_pos - hcs.ad_len : 0u;
            hcs.ad_len = hc_rate_samples(hcd_rate_of_dir(hc.dir), hcs.ad_step++);
            if (hcs.ad_len)
                for (k = 0; k < 27u; k++)
                    if ((hcs.pads >> k) & 1u) {
                        hc_note_off(t, hcd_pad_note(k));
                        hc_note_on(t, hcd_pad_note(k));
                    }
        }
    } else {
        hcs.ad_len = 0;
    }
}
