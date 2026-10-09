/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* DRUM LOOP on the host: two bars rendered through the whole mix to a WAV, the level per eighth and the
 * voices started per hit, to hear / see what the loop does.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/drum_probe tests/drum_probe.c -lm
 *   build/host/drum_probe [OUT.wav] */
#define UI_TEST_NO_MAIN 1
#define HUI_DEFAULT 1
#include "ui_test.c"
static void hframes(uint32_t n) { while (n--) frame(); }
static void hui_power_on(void)                           /* as tests/hui_shot.c */
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
    FILE *w = argc > 1 ? fopen(argv[1], "wb") : 0;
    track_t *t = &trk[0];
    uint32_t f, i, seg = 0, nblk = 0, busy_max = 0;
    const uint32_t frames = 4000u * (FS / 1000u);
    double sum = 0;
    int32_t peak = 0;
    hui_power_on();
    song.master_q12 = 4096;
    hui_mode_set(t, HP_DRUMLOOP);
    if (argc > 2 && argv[2][0] == 'c') t->p[P_CHRD] = 0;             /* variant: no chord layer on the hits */
    if (argc > 2 && argv[2][0] == 's') { hc.t[0].stereo = 0; hc_apply(t); }   /* variant: no stereo partners */
    printf("engine %s preset %d P_CHRD %d P_VOICE %d dl_style %d running %d\n", ENGINES[eng_idx(t->eng_req)]->name,
           t->preset, t->p[P_CHRD], t->p[P_VOICE], hc.t[0].dl_style, hcs.dl_running);
    printf("ATK %d DEC %d SUS %d REL %d DIST %d LEVEL %d GLIDE %d E0..7 %d %d %d %d %d %d %d %d LD_AMP %d\n",
           t->p[P_ATK], t->p[P_DEC], t->p[P_SUS], t->p[P_REL], t->p[P_DIST], t->p[P_LEVEL], t->p[P_GLIDE],
           t->p[P_E0], t->p[P_E0 + 1], t->p[P_E0 + 2], t->p[P_E0 + 3], t->p[P_E0 + 4], t->p[P_E0 + 5], t->p[P_E0 + 6], t->p[P_E0 + 7], t->p[P_LD_AMP]);
    if (argc > 2 && argv[2][0] == 'e') { t->p[P_ATK] = 0; t->p[P_DEC] = 64; t->p[P_SUS] = 127; t->p[P_REL] = 64; }
    if (argc > 2 && argv[2][0] == 'p') {                            /* variant: Felucca plain, a kick every 500 ms */
        hcd_loop_stop(); hui.on = 0; t->p[P_CHRD] = 0; hc_play_set(t, HP_PLAY); set_engine_of(t, ENGI_DRUM);
    }
    if (w) wav_hdr(w, frames);
    for (f = 0; f < frames; f += CTL) {
        int32_t o[2 * CTL];
        fm1_ms = f / (FS / 1000u);
        if (argc > 2 && argv[2][0] == 'p' && f % (FS / 2u) == 0u) { input_on(t, 36, 100); input_off(t, 36); }
        mix_block(o, CTL);
        {
            static uint32_t shown, want;
            int32_t pk = 0;
            for (i = 0; i < CTL; i++) { int32_t a = o[2 * i] < 0 ? -o[2 * i] : o[2 * i]; if (a > pk) pk = a; }
            if ((pk > 3000 && shown < 3u) || want) { want = want ? 0u : 1u; if (!want) shown++; printf("  HIT? peak %d at", pk); goto dump; }
        }
        if (f == 0u || f == CTL) {
        dump:
            uint32_t k;
            printf("  after block %u:", f / CTL);
            for (k = 0; k < NVOICE; k++) if (t->v[k].active || t->v[k].note) printf(" v%u[note %d act %d gate %d s0 %d env %d]", k, t->v[k].note, t->v[k].active, t->v[k].gate, (int)t->v[k].s[0], (int)(t->v[k].env >> 20));
            for (k = 0; k < DV_NLANE; k++) if (drum_kit[0][k].owner || drum_kit[0][k].v.live) printf(" lane%u[owner %u live %d]", k, drum_kit[0][k].owner, (int)drum_kit[0][k].v.live);
            printf("\n");
        }
        if (f == CTL * 2u || f == CTL * 40u) {
            const drum_lane_t *K = drum_kit[0];
            printf("  block %u: engine %d eng_req %d xf %d |", f / CTL, t->engine, t->eng_req, t->xf);
            printf(" kick lane owner %u live %d trig %d; voice0 active %d gate %d note %d stage %d\n",
                   K[0].owner, (int)K[0].v.live, (int)K[0].v.trig, t->v[0].active, t->v[0].gate, t->v[0].note, t->v[0].stage);
        }
        if (busy_now() > busy_max) busy_max = busy_now();
        for (i = 0; i < CTL; i++) {
            int32_t l = o[2 * i], a = l < 0 ? -l : l;
            if (a > peak) peak = a;
            sum += (double)l * l;
            if (w) wav_put(w, o[2 * i], o[2 * i + 1]);
        }
        nblk++;
        if ((f + CTL) % (FS / 4u) < CTL) {               /* every 250 ms */
            printf("%4u ms  rms %6.0f  peak %6d  voices busy %u\n", (f + CTL) / (FS / 1000u),
                   sqrt(sum / (nblk * CTL)), peak, busy_max);
            sum = 0; peak = 0; nblk = 0; busy_max = 0; seg++;
        }
    }
    if (w) fclose(w);
    return 0;
}
