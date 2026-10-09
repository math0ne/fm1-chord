/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* The HiChord UI's screens as the device draws them (the real UI code on the stub display, a colour palette):
 * HOME idle, HOME with a chord and a modifier held, the KEY, SOUND (top and scrolled), MODE and PRESETS menus,
 * a message. PPMs into OUTDIR (Pillow converts them: tests/hui_shot.py or by hand).
 *   build/host/hui_shot OUTDIR
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/hui_shot tests/hui_shot.c -lm */
#define UI_TEST_NO_MAIN 1
#define HUI_DEFAULT 1
#include "ui_test.c"
static void hframes(uint32_t n) { while (n--) frame(); }   /* (input and draw; ui_test.c frames() only draws) */

static void ppm(const char *dir, const char *name)
{
    char path[512];
    uint32_t i;
    FILE *f;
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    f = fopen(path, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint32_t c = host_screen[i], v = ((c & 0xFFu) << 8) | (c >> 8);
        uint8_t rgb[3] = {(uint8_t)(((v >> 11) & 31u) * 255u / 31u), (uint8_t)(((v >> 5) & 63u) * 255u / 63u),
                          (uint8_t)((v & 31u) * 255u / 31u)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("%s\n", path);
}
static void hui_power_on(void)
{
    ui_power_on();
    hc_init();
    memset(&hui, 0, sizeof hui);
    hui.on = 1;
    hui.force = 1;
    hui.rnd = 0x9E3779B9u;
    settings.palette = 3;                                /* ICE */
    palette_set(3);
    usb.config = 1;
    song.g[G_BPM] = 120;
    hframes(2);                                           /* (hui_track_init: the sound, CHRD HI) */
}

int main(int argc, char **argv)
{
    uint32_t i;
    if (argc < 2)
        return 2;
    hui_power_on();
    hframes(3);
    ppm(argv[1], "home_idle");
    key_down(7);                                         /* C4: C */
    key_down(10);                                        /* D#4: up-right -> C7 */
    hframes(3);
    ppm(argv[1], "home_c7");
    key_up(10);
    key_up(7);
    key_down(16); key_down(3);                           /* A4 (Am) with LOCK's key: LOCK needs a direction; HOLD on A#3 */
    key_up(3);
    key_down(5); key_up(5);                              /* HOLD */
    hframes(3);
    ppm(argv[1], "home_am_hold");
    key_up(16);
    key_down(5); key_up(5);                              /* HOLD off */
    hframes(3);
    hc.t[0].bass = HB_SLASH; hc_apply(&trk[0]);          /* BASS SLASH: C4 held (the bass), E4 pressed: Em/C */
    key_down(7); hframes(2);
    key_down(11); hframes(3);
    ppm(argv[1], "home_slash");
    key_up(11); key_up(7); hframes(2);
    hc.t[0].bass = HB_OFF; hc_apply(&trk[0]);
    key_down(7); hframes(2);
    key_down(1); key_up(1); hframes(3);                  /* INVERT tapped while C is held: C/E, 1ST INV */
    ppm(argv[1], "home_inverted");
    key_up(7); hc.inv[7] = 0; hframes(2);
    hc.t[0].rev = HRV_HALL; hc.t[0].dly = HDL_1_8; hc.t[0].cho = HCH_WARM; hc.t[0].glide = HGL_SHORT;
    hc_apply(&trk[0]);
    hui.force = 1;
    hframes(3);
    ppm(argv[1], "home_fx");
    press(B_SCL);
    hframes(3);
    ppm(argv[1], "menu_key");
    press(B_FX);
    hframes(3);
    ppm(argv[1], "menu_sound");
    press(B_HOME); press(B_ENV); hframes(3);             /* the ENVELOPE page */
    ppm(argv[1], "pick_env");
    press(B_ENV); turn(EN_PRESET, 1); hframes(2);         /* the sound list while the PRESETS knob turns */
    ppm(argv[1], "pick_sound");
    fm1_ms += 1600; hframes(2); turn(EN_PRESET, -1); fm1_ms += 1600; hframes(2);
    hui_open(HU_SOUND); hframes(2);
    hui.sel[HU_SOUND] = RS_REV; hframes(3);              /* an effect row: the amount, KNOB 4's hint */
    ppm(argv[1], "menu_sound_amount");
    hui.sel[HU_SOUND] = RS_SOUND; hframes(2);
    turn(EN_SELECT, 9);
    hframes(3);
    ppm(argv[1], "menu_sound_fx");
    turn(EN_SELECT, 9);
    hframes(3);
    ppm(argv[1], "menu_sound_settings");
    press(B_EDIT);
    hframes(3);
    ppm(argv[1], "menu_mode");
    press(B_SAVE);
    hframes(3);
    ppm(argv[1], "menu_presets");
    press(B_SAVE);                                       /* SAVE again: saved P1 */
    hframes(3);
    ppm(argv[1], "menu_presets_saved");
    press(B_HOME);
    hframes(3);
    hui_say("RANDOMIZED ALL");
    hframes(2);
    ppm(argv[1], "home_message");
    hframes(80);
    hui_mode_set(&trk[0], HP_SEQ);                       /* the sequencer: four steps */
    hcs_clear(); hcs.len = 8;
    key_down(7); key_up(7); key_down(16); key_up(16); key_down(12); key_down(10); key_up(12); key_up(10); key_down(14); key_up(14);
    press(B_PLAY);
    hframes(3);
    ppm(argv[1], "mode_sequencer");
    press(B_PLAY);
    hui_mode_set(&trk[0], HP_DRUM);
    key_down(7);
    hframes(3);
    ppm(argv[1], "mode_drum");
    key_up(7);
    hui_mode_set(&trk[0], HP_DRUMLOOP);
    hc.t[0].dl_style = DL_FUNK; hc.t[0].dl_var = DV_BUSY;
    hframes(3);
    ppm(argv[1], "mode_drumloop");
    hcl.bars = 2;                                        /* recording the loop into layer 1: the strip, bar 2 of 2 */
    press(B_REC); press(B_REC);
    for (i = 0; i < 60u; i++) { events_block(CTL); events_block(CTL); events_block(CTL); events_block(CTL); }
    hframes(3);
    ppm(argv[1], "mode_drumloop_rec");
    hcl_clear_all(); hcl.bars = 0; hframes(2);
    hui_mode_set(&trk[0], HP_MIXER);
    trk[1].p[P_MUTE] = 1; trk[2].p[P_LEVEL] = 70;
    hframes(3);
    ppm(argv[1], "mode_mixer");
    hui_mode_set(&trk[0], HP_PLAY);
    press(B_SEQ);
    hcl.l[0].state = HLS_PLAY; hcl.l[1].state = HLS_REC; hcl.len = 88200; hcl.pos = 30000; hcl.playing = 1;
    hui.force = 1;
    hframes(3);
    ppm(argv[1], "menu_looper");
    hcl_clear_all();
    press(B_HOME);
    hui_mode_set(&trk[0], HP_HIRO);
    hcg.song = 3; hcg.diff = HGD_HARD;
    press(B_PLAY);
    for (i = 0; i < 2900; i++) { events_block(CTL); if (i % 22u == 0u) frame(); }
    key_down(7); key_up(7);
    hframes(3);
    ppm(argv[1], "mode_hiro");
    hui_mode_set(&trk[0], HP_EAR);
    hcg.level = 1;
    press(B_PLAY);
    for (i = 0; i < 4200; i++) { events_block(CTL); if (i % 22u == 0u) frame(); }
    hframes(3);
    ppm(argv[1], "mode_ear");
    return 0;
}
