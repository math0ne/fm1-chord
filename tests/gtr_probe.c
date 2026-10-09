/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* A lead melody on a sound, through the whole mix, to a WAV: to audition the acoustic guitar against
 * the other plucked sounds.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/gtr_probe tests/gtr_probe.c -lm
 *   build/host/gtr_probe OUT.wav SOUND_INDEX */
#define UI_TEST_NO_MAIN 1
#define HUI_DEFAULT 1
#include "ui_test.c"
static void hframes(uint32_t n) { while (n--) frame(); }
static void run_ms2(uint32_t ms) { uint32_t n = ms * (FS / 1000u) / CTL; while (n--) events_block(CTL); }
static void hui_power_on(void)
{
    ui_power_on();
    hc_init();
    memset(&hui, 0, sizeof hui);
    hui.on = 1;
    hui.force = 1;
    hui.rnd = 0x9E3779B9u;
    settings.palette = 3;
    palette_set(3);
    usb.config = 1;
    song.g[G_BPM] = 120;
    hframes(2);
}

int main(int argc, char **argv)
{
    static const struct { uint32_t at_ms, key; } EV[] = {         /* a little arpeggiated lead: key, 0 = all up */
        {0, 7}, {300, 0}, {320, 11}, {620, 0}, {640, 14}, {940, 0}, {960, 16}, {1260, 0}, {1280, 14}, {1580, 0},
        {1600, 11}, {1900, 0}, {1920, 7}, {2600, 0}, {2650, 9}, {2950, 0}, {2970, 12}, {3270, 0}, {3290, 16}, {4200, 0},
    };
    uint32_t sound = argc > 2 ? (uint32_t)atoi(argv[2]) : 0, frames = 5200u * (FS / 1000u), f, i, ev = 0;
    int32_t peak = 0;
    double sum = 0;
    FILE *w;
    if (argc < 2)
        return 2;
    hui_power_on();
    run_ms2(100);
    if (argc > 3) {                                    /* plain Felucca: engine 9 preset argv[3], notes at 127 */
        hui.on = 0; trk[0].p[P_CHRD] = 0; trk_hc[0] = 0; trk_pair[0] = 0;
        host_preset(&trk[0], argc > 4 ? (uint32_t)atoi(argv[4]) : 9u, (uint32_t)atoi(argv[3]));
        trk[0].p[P_VOICE] = V_POLY;
    } else {
        hc_sound_load(&trk[0], sound);
        hui_mode_set(&trk[0], HP_LEAD);
    }
    run_ms2(100);
    song.master_q12 = 4096;
    w = fopen(argv[1], "wb");
    if (!w)
        return 1;
    wav_hdr(w, frames);
    for (f = 0; f < frames; f += CTL) {
        int32_t o[2 * CTL];
        uint32_t ms = f / (FS / 1000u);
        while (ev < NELEM(EV) && EV[ev].at_ms <= ms) {
            if (argc > 3) {
                static uint32_t held;
                if (held) { input_off(&trk[0], held); held = 0; }
                if (EV[ev].key) { held = 55u + EV[ev].key; input_on(&trk[0], held, 127); }
            } else {
                fm1_in.notes = EV[ev].key ? 1u << EV[ev].key : 0u;
            }
            ev++;
        }
        fm1_ms = ms;
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t l = o[2 * i], a = l < 0 ? -l : l;
            if (a > peak) peak = a;
            sum += (double)l * l;
            wav_put(w, o[2 * i], o[2 * i + 1]);
        }
    }
    fclose(w);
    printf("%s (sound %u, LEAD): rms %.0f peak %d -> %s\n", HC_SOUNDS[sound % HC_NSOUNDS].name, sound, sqrt(sum / frames), peak, argv[1]);
    return 0;
}
