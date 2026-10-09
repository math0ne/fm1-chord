/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* The FILTER wheel and HI-PASS on a held chord: per segment the RMS, the high-frequency RMS (first
 * difference) and the largest sample-to-sample jump, to see whether the filter adds noise or clicks.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/hcfx_probe tests/hcfx_probe.c -lm
 *   build/host/hcfx_probe [OUT.wav] [MASTER_Q12] */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define K_C4 7u

static void segment(const char *name, uint32_t ms0, uint32_t ms1, FILE *w, int sweep, int hp, int cut)
{
    uint32_t f0 = ms0 * (FS / 1000u), f1 = ms1 * (FS / 1000u), f, i;
    double sum = 0, hsum = 0;
    int32_t prev = 0, jump = 0, n = 0, peak = 0;
    for (f = f0; f < f1; f += CTL) {
        int32_t o[2 * CTL];
        uint32_t ms = f / (FS / 1000u);
        if (sweep) {                                     /* 127 -> 16 -> 127 over the segment */
            uint32_t t = (f - f0) * 2u / (f1 - f0 ? f1 - f0 : 1u), pos = (f - f0) * 222u / (f1 - f0 ? f1 - f0 : 1u);
            int32_t c = t == 0 ? 127 - (int32_t)pos : 16 + ((int32_t)pos - 111);
            hc.t[0].cutoff = (uint8_t)clamp(c, 16, 127);
            hc.t[0].filt = 1;
            hc_apply(&trk[0]);
        }
        fm1_ms = ms;
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t l = o[2 * i], d = l - prev, a = l < 0 ? -l : l;
            if (d < 0) d = -d;
            if (d > jump && f > f0 + CTL) jump = d;
            if (a > peak) peak = a;
            sum += (double)l * l;
            hsum += (double)(l - prev) * (l - prev);
            prev = l;
            n++;
            if (w) wav_put(w, o[2 * i], o[2 * i + 1]);
        }
    }
    printf("%-26s cut %3d hp %d  rms %7.0f  hf-rms %7.0f  peak %6d  max jump %6d\n", name, cut, hp,
           sqrt(sum / n), sqrt(hsum / n), peak, jump);
}

int main(int argc, char **argv)
{
    FILE *w = argc > 1 ? fopen(argv[1], "wb") : 0;
    ui_power_on();
    hc_init();
    usb.config = 1;
    host_preset(&trk[0], ENGI_HC, 1);                               /* SAW */
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_SCALE] = 1;
    trk[0].p[P_ROOT] = 0;
    trk[0].p[P_CHRD] = CH_HI;
    hc.t[0].voices = HV_8;
    if (argc > 3) hc.t[0].bass = HB_OFF;                            /* a third argument: no bass voice */
    song.master_q12 = argc > 2 ? (int32_t)atoi(argv[2]) : 4096;       /* MASTER: 4096 full, 256 quiet */
    if (w) wav_hdr(w, 6000u * (FS / 1000u));
    fm1_in.notes = 1u << K_C4;                                       /* C major held throughout */
    hc.t[0].filt = 0; hc.t[0].cutoff = 127; hc_apply(&trk[0]);
    segment("settle", 0, 500, w, 0, 0, 127);
    segment("bypass", 500, 1500, w, 0, 0, 127);
    hc.t[0].filt = 1; hc.t[0].cutoff = 64; hc_apply(&trk[0]);
    segment("filter fixed 64", 1500, 2500, w, 0, 0, 64);
    hc.t[0].cutoff = 20; hc_apply(&trk[0]);
    segment("filter fixed 20", 2500, 3500, w, 0, 0, 20);
    hc.t[0].cutoff = 40; hc.t[0].res = 0; hc_apply(&trk[0]);
    segment("filter 40, res 0", 3500, 4000, w, 0, 0, 40);
    hc.t[0].res = 110; hc_apply(&trk[0]);
    segment("filter 40, res 110", 4000, 4500, w, 0, 0, 40);
    hc.t[0].res = 0; hc.t[0].cutoff = 127; hc_apply(&trk[0]);
    hc.t[0].filt = 0; hc.t[0].cutoff = 127; hc.t[0].hp = 1; hc_apply(&trk[0]);
    segment("hi-pass", 4500, 5500, w, 0, 1, 127);
    hc.t[0].hp = 0; hc_apply(&trk[0]);
    segment("bypass again", 5500, 6000, w, 0, 0, 127);
    if (w) fclose(w);
    return 0;
}
