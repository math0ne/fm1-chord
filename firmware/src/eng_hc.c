/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* HICHORD: the HiChord's own voice, as its companion app describes it: every chord slot is one plain
 * oscillator (SINE, TRIANGLE, SAW, a band-limited SQUARE), or a two-operator FM pair (a sine carrier, a
 * sine modulator at a ratio, its depth decaying), or noise; the BASS slot can take another wave (SINE
 * under a SAW chord, TRIANGLE under SINE); the stereo partner (the HiChord's "layer 2", voice.c pair) can
 * take a third (SAW left and SQUARE right: SAW SQUARE; SQUARE subs under SAWs: JUNO POLY; nothing: OCEAN
 * PAD). A TONE low-pass rounds the top. No filter sweep, no drive: the HiChord's sound is the raw wave,
 * the ADSR, the detuned pair and the effects.
 * The bass slot: hichord.c tells the engine which note is the chord's bass (eng_hc_bass[part]). */
enum { HW_SINE, HW_TRI, HW_SAW, HW_SQR, HW_FM, HW_NOISE };
static const char *const N_HC_WAVE[] = {"SINE", "TRI", "SAW", "SQR", "FM", "NOISE"};
static const char *const N_HC_ALT[] = {"SAME", "SINE", "TRI", "SAW", "SQR", "OFF"};
static int16_t eng_hc_bass[NPART] = {-1, -1, -1, -1};   /* the chord's bass note per part, -1 = none */

static void hc_eng_note_on(track_t *t, voice_t *v)
{
    uint32_t ti = (uint32_t)(t - trk);
    v->ph[1] = v->ph[0];
    v->s[0] = 0;                                         /* the tone filter */
    v->s[1] = 32767;                                     /* the FM modulator's envelope, Q15 */
    v->s[7] = eng_hc_bass[ti] >= 0 && v->note == (uint8_t)eng_hc_bass[ti];   /* 1 = the bass slot */
    if (!v->s[2])
        v->s[2] = 0x2545F491 + (int32_t)v->age;          /* noise */
}

/* a plain wave of kind w at phase ph (Q32), step inc */
static inline int32_t hc_wave(uint32_t w, uint32_t ph, uint32_t inc)
{
    switch (w) {
    case HW_TRI: return osc_tri(ph);
    case HW_SAW: return osc_saw(ph, inc);
    case HW_SQR: return osc_pulse(ph, inc, 0x80000000u);
    default: return osc_sine(ph);
    }
}

static void hc_eng_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t w = (uint32_t)p[P_E0], i, ph0 = v->ph[0], ph1 = v->ph[1], inc = m->inc;
    int32_t alt;
    int32_t tone = p[P_E7], tk = tone >= 127 ? 32768 : 400 + tone * tone * 2;   /* the low-pass step, Q15 */
    int32_t fenv = v->s[1], fdec = p[P_E5] ? 32768 - (p[P_E5] * p[P_E5] >> 3) : 32768;   /* per block, Q15 */
    int32_t depth = p[P_E4] * 258, lp = v->s[0], nst = v->s[2];
    uint32_t ratio_inc = (uint32_t)((uint64_t)inc * (uint32_t)(p[P_E3] ? p[P_E3] : 2) / 2u);   /* the modulator: ratio in halves */
    int quiet = 0;
    if (v->s[7]) {                                       /* the bass slot's own wave */
        alt = p[P_E1];
        if (alt)
            w = (uint32_t)alt - 1u;
    } else if (v->pair) {                                /* the partner's (layer 2) */
        alt = p[P_E2];
        if (alt == 5)
            quiet = 1;
        else if (alt)
            w = (uint32_t)alt - 1u;
    }
    if (quiet) {
        v->ph[0] = ph0 + inc * n;
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t s;
        if (w == HW_FM) {                                /* phase modulation: a sine by a sine */
            int32_t md = mulq15(osc_sine(ph1), mulq15(depth, fenv));
            s = osc_sine(ph0 + ((uint32_t)md << 16));
            ph1 += ratio_inc;
        } else if (w == HW_NOISE) {
            s = (int32_t)(noise32(&nst) >> 17) - 16384;
        } else {
            s = hc_wave(w, ph0, inc);
        }
        ph0 += inc;
        if (tk < 32768) {
            lp += ((s - lp) * tk) >> 15;
            s = lp;
        }
        out[i] += voice_amp(s, m, i);
    }
    if (w == HW_FM)
        fenv = mulq15(fenv, fdec);
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[0] = lp;
    v->s[1] = fenv;
    v->s[2] = nst;
}

/* {WAVE, BASS, LAYER2, RATIO (halves), DEPTH, DECAY, -, TONE}; the envelopes are the HiChord's presets
 * (hui.c sets them from the sound's env); the sends as a HiChord with its effects off */
static const preset_t HC_ENG_PRESETS[] = {
    {"SINE", {HW_SINE, 3, 0, 2, 0, 0, 0, 127}, {30, 80, 110, 70}, 0, 0, FX(0, 0, 0, 0), 0},
    {"SAW", {HW_SAW, 2, 0, 2, 0, 0, 0, 110}, {30, 80, 110, 70}, 0, 0, FX(0, 0, 0, 0), 0},
    {"TRIANGLE", {HW_TRI, 2, 0, 2, 0, 0, 0, 127}, {30, 80, 110, 70}, 0, 0, FX(0, 0, 0, 0), 0},
    {"SQUARE", {HW_SQR, 2, 0, 2, 0, 0, 0, 100}, {30, 80, 110, 70}, 0, 0, FX(0, 0, 0, 0), 0},
    {"E.PIANO", {HW_FM, 0, 0, 2, 55, 40, 0, 110}, {2, 90, 60, 60}, 0, 0, FX(0, 0, 0, 0), 0},      /* 1:1, a soft tine */
    {"HX7 PIANO", {HW_FM, 0, 0, 2, 90, 60, 0, 120}, {2, 95, 40, 60}, 0, 0, FX(0, 0, 0, 0), 0},    /* brighter, faster */
    {"FM BELL", {HW_FM, 0, 0, 7, 80, 25, 0, 127}, {2, 100, 30, 80}, 0, 0, FX(0, 0, 0, 0), 0},     /* 3.5:1 */
    {"FM ORGAN", {HW_FM, 0, 0, 4, 30, 0, 0, 120}, {5, 60, 127, 40}, 0, 0, FX(0, 0, 0, 0), 0},     /* 2:1, held */
    {"FM BRASS", {HW_FM, 0, 0, 2, 75, 20, 0, 115}, {40, 80, 100, 50}, 0, 0, FX(0, 0, 0, 0), 0},
    {"SAW SQUARE", {HW_SAW, 2, 5, 2, 0, 0, 0, 110}, {30, 80, 110, 70}, 0, 0, FX(0, 0, 0, 0), 0},   /* SQUARE on the right */
    {"JUNO POLY", {HW_SAW, 0, 5, 2, 0, 0, 0, 100}, {40, 80, 110, 80}, 0, 0, FX(0, 0, 0, 0), 0},   /* square subs under saws */
    {"OCEAN PAD", {HW_SAW, 0, 6, 2, 0, 0, 0, 60}, {90, 80, 110, 100}, 0, 0, FX(0, 0, 0, 0), 0},   /* the left layer alone, dark */
    {"WOBBLE BASS", {HW_SQR, 0, 0, 2, 100, 0, 0, 90}, {5, 60, 120, 40}, 0, 0, FX(0, 0, 0, 0), 0},
    {"PURE SINE", {HW_SINE, 0, 0, 2, 0, 0, 0, 127}, {30, 80, 110, 70}, 0, 0, FX(0, 0, 0, 0), 0},
    {"NOISE", {HW_NOISE, 0, 0, 2, 0, 0, 0, 60}, {30, 80, 110, 70}, 0, 0, FX(0, 0, 0, 0), 0},
};

static const engine_t ENG_HC = {
    .name = "CHORD",
    .page_title = {"VOICE", "FM"},
    .edit = {
        {"WAVE", F_ENUM, 0, 5, 0, N_HC_WAVE, 0},
        {"BASS", F_ENUM, 0, 4, 0, N_HC_ALT, 0},
        {"LAYR2", F_ENUM, 0, 5, 0, N_HC_ALT, 0},
        {"RATIO", F_INT, 1, 24, 2, 0, "/2"},
        {"DEPTH", F_PCT, 0, 127, 0, 0, 0},
        {"DECAY", F_INT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
        {"TONE", F_PCT, 0, 127, 127, 0, 0},
    },
    .presets = HC_ENG_PRESETS,
    .npresets = NELEM(HC_ENG_PRESETS),
    .note_on = hc_eng_note_on,
    .render = hc_eng_render,
    .knob = {P_E0, P_E7, P_ATK, P_REL},
    .keep = 0x01,
};
