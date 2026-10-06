/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* A HiChord progression rendered through the real keys, voices and FX to a WAV, to hear the layer without
 * hardware: C major on ANALOG STRINGS with BASS ROOT and VOICES 8; chord keys and modifier keys pressed on
 * the simulated keyboard, as a player would (a direction held before or after the chord key, INVERT, HOLD).
 *   build/host/hichord_demo OUT.wav
 * Built by hand (tests/run_tests.sh does not need it):
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/hichord_demo tests/hichord_demo.c -lm */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define K_C4 7u
#define K_D4 9u
#define K_E4 11u
#define K_F4 12u
#define K_G4 14u
#define K_A4 16u
#define K_INVERT 1u
#define K_UR 10u
#define K_DR 15u
#define K_DOWN 17u
#define K_UL 25u

typedef struct { uint32_t at_ms; uint32_t keys; } ev_t;   /* at at_ms the keys held become `keys` (a bit per key) */

int main(int argc, char **argv)
{
    static const ev_t EV[] = {
        {0, 1u << K_C4},                                            /* C */
        {700, 0},
        {750, 1u << K_A4},                                          /* Am */
        {1450, 0},
        {1500, 1u << K_F4},                                         /* F */
        {2200, 0},
        {2250, 1u << K_G4},                                         /* G .. */
        {2500, (1u << K_G4) | (1u << K_UR)},                        /* .. G7: the direction pressed while held */
        {2950, 0},
        {3000, (1u << K_DR) | (1u << K_C4)},                        /* Cmaj9: the direction first */
        {3700, 0},
        {3750, 1u << K_D4},                                         /* Dm */
        {3900, (1u << K_D4) | (1u << K_INVERT)},                    /* INVERT tapped: Dm/F */
        {3950, 1u << K_D4},
        {4450, 0},
        {4500, (1u << K_G4) | (1u << K_DOWN)},                      /* Gsus4 .. */
        {4850, 1u << K_G4},                                         /* .. resolving to G: the direction let go */
        {5200, 0},
        {5250, (1u << K_C4) | (1u << K_UL)},                        /* Caug (a tension) .. */
        {5500, 1u << K_C4},                                         /* .. to C */
        {6900, 0},
    };
    const uint32_t ms_total = 8000, frames = ms_total * (FS / 1000u);
    uint32_t f, ev = 0, i, busy_max = 0;
    int32_t o[2 * CTL], peak = 0;
    FILE *w;
    if (argc < 2) {
        fprintf(stderr, "usage: hichord_demo OUT.wav\n");
        return 2;
    }
    ui_power_on();
    hc_init();
    usb.config = 1;
    for (i = 0; i < ENGINES[0]->npresets; i++)
        if (str_eq(ENGINES[0]->presets[i].name, "STRINGS"))
            host_preset(&trk[0], 0, i);
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_SCALE] = 1;                                          /* C major */
    trk[0].p[P_ROOT] = 0;
    trk[0].p[P_CHRD] = CH_HI;
    trk[0].p[P_REL] = 70;
    hc.t[0].bass = HB_ROOT;
    hc.t[0].voices = HV_8;
    song.master_q12 = 4096;
    w = fopen(argv[1], "wb");
    if (!w)
        return 1;
    wav_hdr(w, frames);
    for (f = 0; f < frames; f += CTL) {
        uint32_t ms = f / (FS / 1000u);
        while (ev < NELEM(EV) && EV[ev].at_ms <= ms) {
            fm1_in.notes = EV[ev].keys;
            ev++;
        }
        fm1_ms = ms;
        mix_block(o, CTL);
        if (busy_now() > busy_max)
            busy_max = busy_now();
        for (i = 0; i < CTL; i++) {
            int32_t l = o[2 * i] < 0 ? -o[2 * i] : o[2 * i];
            if (l > peak)
                peak = l;
            wav_put(w, o[2 * i], o[2 * i + 1]);
        }
        if (ev && EV[ev - 1u].keys && ms == EV[ev - 1u].at_ms)
            printf("%5u ms  %s\n", ms, hc.name);
    }
    fclose(w);
    printf("hichord_demo: %u ms, peak %d (%.1f dBFS), %u voices at most -> %s\n", ms_total, peak,
           20.0 * log10((double)(peak ? peak : 1) / 32767.0), busy_max, argv[1]);
    return 0;
}
