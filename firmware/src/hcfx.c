/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* The HiChord master effects on the stereo mix, after the master level and before the limiter (fx.c
 * mix_block / perf_master): the FILTER wheel (a low-pass, PF_SVF of perform.c), HI-PASS, the FLANGER
 * (SLOW JET DEEP METAL: a short modulated delay with feedback, the right channel's sweep opposite) and TAPE
 * (LOFI: fewer bits and samples; VINYL: a slow wow and crackle; TAPE: wow and flutter, a softer top).
 * hcfx holds what the live track's settings stand for (hichord.c hc_apply writes it: fx.c is compiled
 * before hichord.c). Everything bypasses bit-exactly when off: the golden renders stay. */
static struct {
    uint8_t filt, hp, flg, tape;         /* the FILTER wheel on, HI-PASS on, HFL_*, HTP_* */
    uint8_t cutoff;                      /* 0..127 -> PF_SVF index 0..63 */
    uint8_t line;                        /* OUT LEVEL LINE: -10 dB */
    uint8_t click;                       /* the metronome (hclooper.c): 1 a beat, 2 the bar's first; taken here */
    uint32_t ck_n, ck_ph, ck_inc;        /* the click: samples left, its phase and step */
    /* runtime */
    int32_t lz[4], hz[4];                /* filter states: L ic1 ic2, R ic1 ic2 */
    int16_t lk[3], hk[3];                /* coefficients */
    uint8_t lc;                          /* the cutoff index the coefficients are for */
    int16_t fl_buf[2][512];              /* the flanger's delay (11.6 ms; Q15 samples) */
    uint32_t fl_w, fl_ph;                /* write index, LFO phase (Q32) */
    int32_t fl_fb[2];                    /* the feedback sample per channel */
    uint32_t tp_ph, tp_ph2;              /* wow and flutter phases */
    int16_t tp_buf[2][256];              /* the wow delay (5.8 ms) */
    uint32_t tp_w;
    int32_t tp_hold[2], tp_lp[2];        /* LOFI's held sample, TAPE's top */
    uint32_t tp_n, tp_rnd;
} hcfx __attribute__((section(".pool"))) = {.cutoff = 127, .tp_rnd = 0x3C6EF35Fu};   /* (the delays: 6 KB, in the pool) */

#define HCFX_FL_LEN 512u
#define HCFX_TP_LEN 256u

static void hcfx_coef(void)
{
    uint32_t i = hcfx.cutoff >> 1;
    if (i > 63u)
        i = 63;
    hcfx.lk[0] = PF_SVF[i][0];
    hcfx.lk[1] = PF_SVF[i][1];
    hcfx.lk[2] = PF_SVF[i][2];
    hcfx.hk[0] = PF_SVF[16][0];                          /* ~150 Hz */
    hcfx.hk[1] = PF_SVF[16][1];
    hcfx.hk[2] = PF_SVF[16][2];
    hcfx.lc = (uint8_t)i;
}

/* one sample of a delay line read at a fractional position (Q16 samples back) */
static inline int32_t hcfx_tap(const int16_t *buf, uint32_t len, uint32_t w, uint32_t back_q16)
{
    uint32_t i = back_q16 >> 16, f = (back_q16 >> 8) & 0xFFu, p0 = (w + len - 1u - i) % len, p1 = (p0 + len - 1u) % len;
    return buf[p0] + (((int32_t)buf[p1] - buf[p0]) * (int32_t)f >> 8);
}
static inline int16_t hcfx_q15(int32_t x) { return (int16_t)(x > 32767 ? 32767 : x < -32767 ? -32767 : x); }

/* a sine of a Q32 phase, Q15 */
static inline int32_t hcfx_sin(uint32_t ph) { return osc_sine(ph); }

static inline void hc_master(int32_t *l, int32_t *r)
{
    if (hcfx.click) {                                    /* the metronome: a 6 ms sine blip, 1.5 kHz on the bar's first beat */
        hcfx.ck_inc = hcfx.click == 2u ? 146087322u : 97391548u;
        hcfx.ck_n = 260;
        hcfx.ck_ph = 0;
        hcfx.click = 0;
    }
    if (hcfx.ck_n) {
        int32_t c = (osc_sine(hcfx.ck_ph) * (int32_t)hcfx.ck_n) / 260 * 5 >> 4;
        hcfx.ck_ph += hcfx.ck_inc;
        hcfx.ck_n--;
        *l += c;
        *r += c;
    }
    if (hcfx.line) {                                     /* OUT LEVEL LINE: -10 dB */
        *l = (*l * 10362) >> 15;
        *r = (*r * 10362) >> 15;
    }
    if (hcfx.filt && hcfx.cutoff < 126u) {               /* the FILTER wheel: a low-pass at the wheel */
        if (hcfx.lc != (hcfx.cutoff >> 1))
            hcfx_coef();
        *l = pf_svf(clamp(*l >> 2, -32767, 32767), &hcfx.lz[0], hcfx.lk, 0) << 2;
        *r = pf_svf(clamp(*r >> 2, -32767, 32767), &hcfx.lz[2], hcfx.lk, 0) << 2;
    } else {
        hcfx.lz[0] = hcfx.lz[1] = hcfx.lz[2] = hcfx.lz[3] = 0;
    }
    if (hcfx.hp) {                                       /* HI-PASS: ~150 Hz */
        if (!hcfx.hk[1])
            hcfx_coef();
        *l = pf_svf(clamp(*l >> 2, -32767, 32767), &hcfx.hz[0], hcfx.hk, 1) << 2;
        *r = pf_svf(clamp(*r >> 2, -32767, 32767), &hcfx.hz[2], hcfx.hk, 1) << 2;
    } else {
        hcfx.hz[0] = hcfx.hz[1] = hcfx.hz[2] = hcfx.hz[3] = 0;
    }
    if (hcfx.flg) {                                      /* FLANGER */
        static const uint32_t RATE[5] = {0, 14600, 48700, 29200, 146000};   /* Q32 phase per sample: 0.15 0.5 0.3 1.5 Hz */
        static const uint16_t DEPTH[5] = {0, 130, 170, 260, 90};   /* samples (3 4 6 2 ms) */
        static const uint16_t BASE[5] = {0, 40, 30, 50, 25};       /* the shortest delay, samples */
        static const uint16_t FB[5] = {0, 13000, 23000, 20000, 28000};   /* Q15 */
        uint32_t m = hcfx.flg % 5u, k;
        int32_t s = hcfx_sin(hcfx.fl_ph), in[2] = {*l, *r}, out[2];
        hcfx.fl_ph += RATE[m];
        for (k = 0; k < 2u; k++) {
            int32_t sw = k ? -s : s;                     /* the right channel sweeps the other way */
            uint32_t back = ((uint32_t)BASE[m] << 16) + (uint32_t)((sw + 32768) * (int32_t)DEPTH[m] >> 1);
            int32_t d = hcfx_tap(hcfx.fl_buf[k], HCFX_FL_LEN, hcfx.fl_w, back);
            int32_t x = in[k] + ((d * FB[m]) >> 15);
            hcfx.fl_buf[k][hcfx.fl_w] = hcfx_q15(x);
            out[k] = in[k] + (d >> 1);
        }
        hcfx.fl_w = (hcfx.fl_w + 1u) % HCFX_FL_LEN;
        *l = out[0];
        *r = out[1];
    }
    if (hcfx.tape) {                                     /* TAPE */
        uint32_t m = hcfx.tape % 4u;
        if (m == 1u) {                                   /* LOFI: 1/4 the samples, 10 bits */
            if (++hcfx.tp_n >= 4u) {
                hcfx.tp_n = 0;
                hcfx.tp_hold[0] = (*l >> 6) << 6;
                hcfx.tp_hold[1] = (*r >> 6) << 6;
            }
            *l = hcfx.tp_hold[0];
            *r = hcfx.tp_hold[1];
        } else {                                         /* VINYL / TAPE: a wow (and a flutter) on a short delay */
            uint32_t k;
            int32_t w = hcfx_sin(hcfx.tp_ph), f = m == 3u ? hcfx_sin(hcfx.tp_ph2) : 0, in[2] = {*l, *r};
            uint32_t back = (64u << 16) + (uint32_t)((w + 32768) * (m == 2u ? 90 : 48) >> 1) +
                            (uint32_t)((f + 32768) * 12 >> 1);
            hcfx.tp_ph += m == 2u ? 29200u : 77900u;      /* 0.3 Hz / 0.8 Hz */
            hcfx.tp_ph2 += 584000u;                      /* 6 Hz */
            for (k = 0; k < 2u; k++) {
                hcfx.tp_buf[k][hcfx.tp_w] = hcfx_q15(in[k]);
                in[k] = hcfx_tap(hcfx.tp_buf[k], HCFX_TP_LEN, hcfx.tp_w + 1u, back);
                if (m == 3u) {                           /* a softer top */
                    hcfx.tp_lp[k] += (in[k] - hcfx.tp_lp[k]) >> 1;
                    in[k] = hcfx.tp_lp[k];
                }
            }
            hcfx.tp_w = (hcfx.tp_w + 1u) % HCFX_TP_LEN;
            if (m == 2u) {                               /* crackle: a sparse click */
                uint32_t rnd = hcfx.tp_rnd;
                rnd ^= rnd << 13; rnd ^= rnd >> 17; rnd ^= rnd << 5;
                hcfx.tp_rnd = rnd;
                if ((rnd & 0x3FFFu) == 0u) {
                    int32_t c = (int32_t)((rnd >> 20) & 0x7FFu) - 1024;
                    in[0] += c;
                    in[1] += c;
                }
            }
            *l = in[0];
            *r = in[1];
        }
    }
}
