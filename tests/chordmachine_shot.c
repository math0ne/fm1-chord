/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* Screens of the chord machine layer as the device draws them (the real UI code on the stub display): HOME with a
 * chord held, the CHORD page. Writes PPMs (tests/ui_render.py style: convert with Pillow).
 *   build/host/chordmachine_shot OUTDIR
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/chordmachine_shot tests/chordmachine_shot.c -lm */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

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
        uint32_t c = host_screen[i], v = ((c & 0xFFu) << 8) | (c >> 8);   /* byte-swapped RGB565 */
        uint8_t rgb[3] = {(uint8_t)(((v >> 11) & 31u) * 255u / 31u), (uint8_t)(((v >> 5) & 63u) * 255u / 63u),
                          (uint8_t)((v & 31u) * 255u / 31u)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("%s\n", path);
}

int main(int argc, char **argv)
{
    uint32_t i;
    if (argc < 2)
        return 2;
    ui_power_on();
    hc_init();
    settings.palette = 1;                                /* a colour palette */
    palette_set(1);
    usb.config = 1;
    for (i = 0; i < ENGINES[0]->npresets; i++)
        if (str_eq(ENGINES[0]->presets[i].name, "STRINGS"))
            host_preset(&trk[0], 0, i);                  /* (ACID, the power-on sound, is LEGATO: the root alone) */
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_CHRD] = CH_HI;
    trk[0].p[P_SCALE] = 1;
    hc.t[0].bass = HB_ROOT;
    for (i = 0; i < 3u; i++)
        frame();
    ppm(argv[1], "home_idle");
    key_down(7);                                         /* C4: C */
    key_down(10);                                        /* D#4: up-right -> C7 */
    for (i = 0; i < 3u; i++)
        frame();
    ppm(argv[1], "home_c7");
    key_up(10);
    key_up(7);
    go_page(GR_CHORD);
    ui.force = 1;
    for (i = 0; i < 3u; i++)
        frame();
    ppm(argv[1], "chord_page");
    return 0;
}
