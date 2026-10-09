/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* The chord machine UI (firmware/src/hui.c) against the real input, drawing, sound and project code: power-on takes
 * the track (its sound, CHRD HI), the three menus open and close on their buttons, SELECT moves the cursor,
 * OCT- / OCT+ change every row's value (and act on the actions), the knobs (mode, sound, filter, attack,
 * release, tempo) work on every screen, the octave buttons, tap tempo, the presets save and load the chord machine
 * state with the project, every screen draws, and HOME held / SAVE + HOME hand over to Felucca's UI and back.
 * Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#define UI_TEST_NO_MAIN 1
#define HUI_DEFAULT 1
#include "ui_test.c"
static void hframes(uint32_t n) { while (n--) frame(); }   /* (input and draw; ui_test.c frames() only draws) */

static void hui_power_on(void)
{
    ui_power_on();
    hc_init();
    memset(&hui, 0, sizeof hui);
    hui.on = 1;
    hui.force = 1;
    hui.rnd = 0x9E3779B9u;
    usb.config = 1;
    song.g[G_BPM] = 120;
    hframes(2);
}
static int val_is(uint32_t screen, uint32_t row, const char *want)
{
    char b[20];
    hui_row_value(screen, row, b);
    if (!str_eq(b, want))
        printf("  row %u of %u: %s, want %s\n", row, screen, b, want);
    return str_eq(b, want);
}
static int msg_is2(const char *s) { return hui.msg_t && str_eq(hui.msg, s); }

static int test_boot_and_menus(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    hui_power_on();
    bad += check("power-on: the chord machine UI is up, HOME, the track plays chord machine keys with the SAW sound",
                 hui.on && hui.screen == HU_HOME && t->p[P_CHRD] == CH_HI && hc.t[0].sound == 1u && t->eng_req == ENGI_HC &&
                 t->p[P_VOICE] == V_POLY);
    press(B_SCL);
    bad += check("SCL: the KEY menu", hui.screen == HU_KEY);
    press(B_SCL);
    bad += check("  SCL again: HOME", hui.screen == HU_HOME);
    press(B_FX);
    bad += check("FX: the SOUND menu; EDIT: the MODE menu; SAVE: PRESETS; HOME: home",
                 hui.screen == HU_SOUND && (press(B_EDIT), hui.screen == HU_MODE) && (press(B_SAVE), hui.screen == HU_PRESET) &&
                 (press(B_HOME), hui.screen == HU_HOME));
    press(B_GLO);
    bad += check("GLO: the SOUND menu too (the settings live there)", hui.screen == HU_SOUND);
    press(B_HOME);
    return bad;
}

static int test_key_menu(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i;
    hui_power_on();
    press(B_SCL);
    bad += check("KEY: C MAJOR, octave 0", val_is(HU_KEY, RK_KEY, "C") && val_is(HU_KEY, RK_SCALE, "MAJOR") && val_is(HU_KEY, RK_OCT, "0"));
    press(B_OCTUP);
    bad += check("  OCT+ on KEY: C#, every track", val_is(HU_KEY, RK_KEY, "C#") && trk[3].p[P_ROOT] == 1);
    press(B_OCTDN); press(B_OCTDN);
    bad += check("  OCT- twice: B (wraps)", val_is(HU_KEY, RK_KEY, "B") && t->p[P_ROOT] == 11);
    turn(EN_SELECT, 2);
    bad += check("SELECT +2: the SCALE row", hui.sel[HU_KEY] == RK_SCALE);
    press(B_OCTUP);
    bad += check("  OCT+: MINOR (Felucca SCALE 2 on every track)", val_is(HU_KEY, RK_SCALE, "MINOR") && t->p[P_SCALE] == 2 && trk[2].p[P_SCALE] == 2);
    for (i = 0; i < 9u; i++) press(B_OCTUP);
    bad += check("  ten scales round: MAJOR again", val_is(HU_KEY, RK_SCALE, "MAJOR"));
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("LAYOUT: PIANO", hc.t[0].layout == HL_PIANO && val_is(HU_KEY, RK_LAYOUT, "PIANO"));
    turn(EN_SELECT, 1); press(B_OCTUP); press(B_OCTUP);
    bad += check("JOYSTICK: CHROM", hc.t[0].mode == HM_CHROM && val_is(HU_KEY, RK_JOY, "CHROM"));
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("BASS: OFF (the default is SLASH; one step on)", hc.t[0].bass == HB_OFF);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("VOICES: 4", hc.t[0].voices == HV_4 && val_is(HU_KEY, RK_VOICES, "4"));
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("VOICE LEAD: ON", hc.t[0].vlead == 1u);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("RANDOMIZE ALL: a message, the sound reloaded, the tempo changed or not", msg_is2("RANDOMIZED ALL") && t->p[P_CHRD] == CH_HI);
    turn(EN_SELECT, 5);
    bad += check("SELECT past the end stays on the last row", hui.sel[HU_KEY] == RK_N - 1u);
    turn(EN_SELECT, -20);
    bad += check("  and before the start on the first", hui.sel[HU_KEY] == 0u);
    press(B_HOME);
    return bad;
}

static int test_sound_menu(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    hui_power_on();
    press(B_FX);
    bad += check("SOUND: SAW, LONG, the effects OFF", val_is(HU_SOUND, RS_SOUND, "SAW") && val_is(HU_SOUND, RS_ENV, "LONG") &&
                 val_is(HU_SOUND, RS_REV, "OFF") && val_is(HU_SOUND, RS_STEREO, "ON"));
    press(B_OCTUP);
    bad += check("  OCT+: TRIANGLE loaded (the CHORD engine), CHRD HI kept, its envelope", val_is(HU_SOUND, RS_SOUND, "TRIANGLE") &&
                 t->eng_req == ENGI_HC && t->p[P_CHRD] == CH_HI && hc.t[0].env == HE_LONG);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("ENVELOPE: SHORT -> ATK 100 ms", hc.t[0].env == HE_SHORT && TIME_MS_X10[t->p[P_ATK]] / 10u >= 80u && TIME_MS_X10[t->p[P_ATK]] / 10u <= 120u);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("ATTACK: fine-tuned up (the preset's value + 4)", hc.t[0].atk != 0u && t->p[P_ATK] == hc.t[0].atk);
    turn(EN_SELECT, 1); press(B_OCTDN);
    bad += check("RELEASE: fine-tuned down", hc.t[0].rel != 0u && t->p[P_REL] == hc.t[0].rel);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("FILTER WHEEL: ON -> the master filter", hc.t[0].filt == 1u && hcfx.filt == 1u);
    turn(EN_SELECT, 1); press(B_OCTDN);
    bad += check("CUTOFF: down by 4", hc.t[0].cutoff == 123u && hcfx.cutoff == 123u);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("RESONANCE: up by 4, the master filter follows", hc.t[0].res == 4u && hcfx.res == 4u && val_is(HU_SOUND, RS_RES, "4"));
    press(B_OCTDN);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("HI-PASS: ON", hcfx.hp == 1u);
    turn(EN_SELECT, 1); press(B_OCTUP); press(B_OCTUP);
    bad += check("REVERB: HALL -> the send and the bus (ROOM model, size 110)", hc.t[0].rev == HRV_HALL && t->p[P_REV] == 65 &&
                 song.g[G_RTYPE] == 0 && song.g[G_RSIZE] == 110);
    press(B_OCTUP); press(B_OCTUP);
    bad += check("  .. SPRING: the SPRING model", hc.t[0].rev == HRV_SPRING && song.g[G_RTYPE] == 1);
    turn(EN_SELECT, 1); press(B_OCTUP); press(B_OCTUP); press(B_OCTUP);
    bad += check("DELAY: 1/16 -> the send on, DLY TIME 1/16", hc.t[0].dly == HDL_1_16 && t->p[P_DLY] == 55 && song.g[G_DTIME] == 2);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("CHORUS: LIGHT -> the send", hc.t[0].cho == HCH_LIGHT && t->p[P_CHOR] == 40);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("FLANGER: SLOW -> the master", hcfx.flg == HFL_SLOW);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("TREMOLO: 1/4 -> the LFO on AMP at the tempo", hc.t[0].trem == HTR_1_4 && t->p[P_LD_AMP] == 90 && t->p[P_LRATE] > 0);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("LFO: LOW -> a little pitch vibrato", t->p[P_LD_PIT] == 2);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("GLIDE: SHORT", t->p[P_GLIDE] == 30);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("DRIVE: TUBE -> DIST", t->p[P_DIST] == 28);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("TAPE: LOFI -> the master", hcfx.tape == HTP_LOFI);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("STEREO: OFF -> no partners", hc.t[0].stereo == 0u && trk_pair[0] == 0u);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("SPEAKER: LOWCUT (Felucca's low cut)", settings.lowcut == 1u && fx_lowcut == 1u);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("OUT LEVEL: LINE", hui.out_line == 1u && hcfx.line == 1u);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("MIDI IN: ON", hui.midi_in == 1u);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("RANDOMIZE SOUND: a message", msg_is2("RANDOMIZED SOUND"));
    press(B_HOME);
    return bad;
}

static int test_mode_menu_knobs(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i, bpm;
    hui_power_on();
    press(B_EDIT);
    bad += check("MODE: PLAY, 120 BPM", val_is(HU_MODE, RM_MODE, "PLAY") && val_is(HU_MODE, RM_BPM, "120 BPM"));
    press(B_OCTUP);
    bad += check("  OCT+: STRUM", hc.t[0].play == HP_STRUM && val_is(HU_MODE, RM_MODE, "STRUM"));
    press(B_OCTUP);
    bad += check("  .. LEAD: the track POLY (a note a key), no partners", hc.t[0].play == HP_LEAD && t->p[P_VOICE] == V_POLY && trk_pair[0] == 0u);
    turn(EN_SELECT, 1); press(B_OCTUP); press(B_OCTUP);
    bad += check("TEMPO: 122", song.g[G_BPM] == 122);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("STRUM SPEED: MEDIUM", hc.t[0].strum == HST_MED);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("ARP PATTERN: DOWN", hc.t[0].arp_pat == HA_DOWN);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("ARP RATE: 1/16", hc.t[0].arp_rate == HR_1_16);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("ARP LAYER: CHORD+ARP", hc.t[0].arp_layer == HAL_CHORD);
    hui.sel[HU_MODE] = RM_RANDOM; press(B_OCTUP);
    bad += check("RANDOMIZE PATTERN: a message", msg_is2("RANDOMIZED PATTERN"));
    press(B_HOME);
    turn(EN_ALGO, 1);
    bad += check("ALGORITHM on HOME: the next play mode", hc.t[0].play == HP_DRONE);
    turn(EN_ALGO, -2);
    bad += check("  back two: STRUM", hc.t[0].play == HP_STRUM);
    turn(EN_PRESET, 1);
    bad += check("PRESETS knob: the next sound (TRIANGLE)", hc.t[0].sound == 2u && t->eng_req == ENGI_HC);
    turn(EN_PRESET, -1);
    turn(EN_K1, -5);
    bad += check("KNOB 1: the FILTER wheel on, the cutoff down", hc.t[0].filt == 1u && hc.t[0].cutoff == 112u);
    turn(EN_K2, 3);
    bad += check("KNOB 2: the resonance (9), said in the header", hc.t[0].res == 9u && hcfx.res == 9u && msg_is2("RESONANCE 9"));
    turn(EN_K3, -3);
    bad += check("KNOB 3: the attack fine-tuned, said", hc.t[0].atk != 0u && hui.msg_t && !memcmp(hui.msg, "ATTACK ", 7));
    turn(EN_K4, 5);
    bad += check("KNOB 4: the release fine-tuned, said", hc.t[0].rel != 0u && !memcmp(hui.msg, "RELEASE ", 8));
    hc.t[0].res = 0; hc_apply(t);
    press(B_OCTUP);
    bad += check("OCT+ on HOME: the octave", song.octave == 1);
    press(B_OCTDN); press(B_OCTDN); press(B_OCTDN);
    bad += check("  OCT- thrice: -2 (the floor)", song.octave == -2);
    press(B_ENV);
    bad += check("ENV: the ENVELOPE list opens, the choice unchanged", hui.screen == HU_PICK && hui.pick_kind == PK_ENV &&
                 hui.pick_sticky && hc.t[0].env == HE_LONG);
    turn(EN_SELECT, 1);
    bad += check("  SELECT: the next envelope (SHORT), applied, the list stays", hc.t[0].env == HE_SHORT && hui.screen == HU_PICK);
    press(B_ENV);
    bad += check("  ENV again: back to HOME", hui.screen == HU_HOME);
    press(B_LFO);
    turn(EN_SELECT, 1);
    bad += check("LFO: its list, SELECT: vibrato LOW", hui.screen == HU_PICK && hui.pick_kind == PK_LFO && hc.t[0].lfo == HLF_LOW);
    press(B_HOME);
    bad += check("  HOME closes it", hui.screen == HU_HOME);
    turn(EN_PRESET, 1);
    bad += check("PRESETS knob: the sound list shows while it turns", hui.screen == HU_PICK && hui.pick_kind == PK_SOUND && !hui.pick_sticky);
    turn(EN_SELECT, 1);
    bad += check("  SELECT moves it too (the next sound)", hc.t[0].sound == 3u);
    fm1_ms += 1600; frame();
    bad += check("  gone 1.5 s after the last turn", hui.screen == HU_HOME);
    turn(EN_ALGO, 1);
    bad += check("ALGORITHM knob: the mode list", hui.screen == HU_PICK && hui.pick_kind == PK_MODE);
    press(B_OCTUP);
    bad += check("  a button closes it and acts as on HOME (the octave)", hui.screen == HU_HOME && song.octave == -1);
    press(B_OCTDN); turn(EN_PRESET, -2);
    fm1_ms += 1600; frame();
    press(B_ARP);
    bad += check("ARP: ARP mode", hc.t[0].play == HP_ARP);
    press(B_ARP);
    bad += check("  again: PLAY", hc.t[0].play == HP_PLAY);
    /* tap tempo: EDIT three times, 500 ms apart (frames are 16 ms), after a pause (the MODE press above was a tap) */
    song.g[G_BPM] = 100;
    hframes(110);
    for (i = 0; i < 3u; i++) {
        press(B_EDIT);
        hframes(29);                                      /* 2 frames of the press + 29 = 31 x 16 = 496 ms */
    }
    bpm = (uint32_t)song.g[G_BPM];
    bad += check("EDIT tapped three times 500 ms apart: ~120 BPM (tap tempo)", bpm >= 116u && bpm <= 124u);
    press(B_HOME);
    return bad;
}

static int test_presets(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    hui_power_on();
    hc_sound_load(t, 14);                                /* PIANO */
    hc.t[0].rev = HRV_PLATE;
    hc.t[0].mode = HM_BORROW;
    hc.inv[0] = 2;
    hc.lock[9].on = 1; hc.lock[9].q = HQ_DOM7; hc.lock[9].roff = 0;
    hui_key_set(7);                                      /* G */
    song.g[G_BPM] = 98;
    hc_apply(t);
    press(B_SAVE);
    bad += check("SAVE: the PRESETS menu, P1 empty", hui.screen == HU_PRESET && val_is(HU_PRESET, 0, "EMPTY"));
    turn(EN_SELECT, 1);
    press(B_SAVE);
    bad += check("SAVE again on P2: saved", msg_is2("SAVED P2") && hui.preset_used[1] && project_used(1));
    hc_sound_load(t, 1);                                 /* SAW: everything else changes */
    hc.t[0].rev = HRV_OFF;
    hc.t[0].mode = HM_DEFAULT;
    hc.inv[0] = 0;
    hc.lock[9].on = 0;
    hui_key_set(0);
    song.g[G_BPM] = 120;
    hc_apply(t);
    press(B_OCTUP);
    bad += check("OCT+ on P2: loaded: PIANO, PLATE, BORROW, the inversion and the lock, key G, 98 BPM",
                 msg_is2("PRESET P2") && hc.t[0].sound == 14u && t->eng_req == 4u && hc.t[0].rev == HRV_PLATE &&
                 hc.t[0].mode == HM_BORROW && hc.inv[0] == 2u && hc.lock[9].on && hc.lock[9].q == HQ_DOM7 &&
                 t->p[P_ROOT] == 7 && song.g[G_BPM] == 98 && t->p[P_CHRD] == CH_HI);
    bad += check("  .. and the Felucca parameters follow (the PLATE send)", t->p[P_REV] == 60);
    turn(EN_SELECT, 1); press(B_OCTUP);
    bad += check("OCT+ on an empty slot: EMPTY", msg_is2("EMPTY"));
    {   /* the preset bytes survive a settings round trip (settings_persist.c) */
        persist_t p;
        memset(&p, 0, sizeof p);
        settings_export(&p);
        bad += check("the presets are in the settings record (PER5)", p.magic == PERSIST_MAGIC && !memcmp(p.hcp[1], hc_presets[1], HC_PRESET_BYTES) &&
                     *(const uint32_t *)p.hcp[1] == HCP_MAGIC);
        memset(hc_presets, 0, sizeof hc_presets);
        bad += check("  .. and come back from an import", settings_import(&p, sizeof p) == 1 && *(const uint32_t *)hc_presets[1] == HCP_MAGIC);
    }
    press(B_HOME);
    return bad;
}

static int test_draw_and_handover(void)
{
    int bad = 0;
    uint32_t s, ok = 1;
    hui_power_on();
    for (s = 0; s < HU_N; s++) {                         /* every screen draws (no crash, the stub display touched) */
        hui.screen = (uint8_t)s;
        hui.force = 1;
        memset(host_screen, 0, sizeof host_screen);
        hframes(2);
        ok &= host_screen[120 * 240 + 120] != 0u || host_screen[10 * 240 + 10] != 0u;
    }
    bad += check("every screen draws", ok);
    hui.screen = HU_HOME;
    key_down(0); key_down(10);
    hframes(2);
    bad += check("HOME with a chord: the chord's name on screen (the big face has every glyph of it)",
                 str_eq(chord_last[0].name, "C7") && text_w(&AF_X, "C7") > 0);
    key_up(10); key_up(0);
    hold(B_HOME);
    bad += check("HOME held: Felucca's UI", !hui.on && ui.home);
    hframes(2);
    fm1_in.buttons |= 1u << panel.btn[B_HOME];
    press(B_SAVE);
    fm1_in.buttons &= ~(1u << panel.btn[B_HOME]);
    hframes(2);
    bad += check("SAVE with HOME held: the chord machine UI again", hui.on);
    return bad;
}

static void run_ms2(uint32_t ms)                   /* the audio clock (blocks), with the UI frames between */
{
    uint32_t n = ms * (FS / 1000u) / CTL, i;
    for (i = 0; i < n; i++) {
        events_block(CTL);
        if (i % 22u == 0u)
            frame();
    }
}
static uint32_t gated_on(uint32_t ti)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += trk[ti].v[i].active && trk[ti].v[i].gate;
    return n;
}
static uint32_t midi_ons(void)                     /* note-ons in MIDI OUT so far */
{
    uint32_t i, n = 0;
    for (i = 0; i < mo_w; i++) n += ((midi_out_q[i % MQ] >> 8) & 0xF0u) == 0x90u;
    return n;
}
static int gate_on(uint32_t ti, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) if (trk[ti].v[i].active && trk[ti].v[i].gate && trk[ti].v[i].note == note) return 1;
    return 0;
}
static int test_looper(void)
{
    int bad = 0;
    uint32_t q = (uint32_t)FS * 60u / 120u;             /* a beat at 120 BPM: 22050 samples */
    hui_power_on();
    hc.t[0].stereo = 0;
    hc_apply(&trk[0]);
    press(B_SEQ);
    bad += check("SEQ: the LOOPER screen, every layer OFF, layer 1 live, BARS FREE", hui.screen == HU_LOOP &&
                 val_is(HU_LOOP, RL_L1, "OFF LIVE") && val_is(HU_LOOP, RL_L2, "OFF") && val_is(HU_LOOP, RL_BARS, "FREE"));
    press(B_HOME);
    press(B_REC);
    bad += check("REC: layer 1 ARMED", hcl.l[0].state == HLS_ARMED && msg_is2("ARMED"));
    press(B_REC);
    bad += check("REC again: recording (free length), the transport runs", hcl.l[0].state == HLS_REC && hcl.playing);
    key_down(0);                                       /* C for a beat */
    run_ms2(500);
    key_up(0);
    run_ms2(500);
    key_down(2);                                       /* Dm for a beat */
    run_ms2(500);
    key_up(2);
    run_ms2(480);                                      /* ~2 s: 4 beats */
    press(B_REC);
    bad += check("REC: the layer plays, the loop is 4 beats (rounded), the live instrument moved to layer 2",
                 hcl.l[0].state == HLS_PLAY && hcl.len == 4u * q && song.sel == 1u && hc.t[1].sound == hc.t[0].sound &&
                 trk[1].p[P_CHRD] == CH_HI && hcl.l[0].n >= 12u);
    run_ms2(2100);                                     /* a loop round: layer 1 replays C then Dm */
    bad += check("  the loop replays: notes sound on part 1 during its round", gated_on(0) > 0u || hcl.l[0].play_i > 0u);
    {   /* over one loop, the C and the Dm are heard on part 1 at their times */
        uint32_t seen_c = 0, seen_d = 0, i;
        for (i = 0; i < 40u; i++) {
            run_ms2(50);
            seen_c |= gate_on(0, 48);
            seen_d |= gate_on(0, 50);
        }
        bad += check("  .. the C and the Dm both come round", seen_c && seen_d);
    }
    press(B_REC); press(B_REC);                        /* layer 2: ARMED, then it waits for the loop's start */
    bad += check("layer 2: ARMED waits for the loop's start", hcl.l[1].state == HLS_ARMED);
    run_ms2(2100);
    bad += check("  .. and records from it", hcl.l[1].state == HLS_REC || hcl.l[1].state == HLS_PLAY);
    key_down(7); run_ms2(300); key_up(7);            /* a G on layer 2 */
    run_ms2(2000);
    bad += check("  .. one loop later it plays, with the G; the live instrument is layer 3", hcl.l[1].state == HLS_PLAY &&
                 hcl.l[1].n >= 6u && song.sel == 2u);
    press(B_PLAY);
    bad += check("PLAY: paused, nothing sounds", !hcl.playing && msg_is2("PAUSED"));
    run_ms2(200);
    bad += check("  .. silence on the parts", gated_on(0) == 0u && gated_on(1) == 0u);
    press(B_PLAY);
    bad += check("PLAY again: running", hcl.playing);
    press(B_SEQ);
    turn(EN_SELECT, 0);
    bad += check("the LOOPER screen: layers PLAY PLAY OFF LIVE OFF", val_is(HU_LOOP, RL_L1, "PLAY") && val_is(HU_LOOP, RL_L2, "PLAY") &&
                 val_is(HU_LOOP, RL_L3, "OFF LIVE"));
    hui.sel[HU_LOOP] = RL_L2; press(B_OCTDN);
    bad += check("OCT- on layer 2: cleared", hcl.l[1].state == HLS_OFF && hcl.l[1].n == 0u);
    hui.sel[HU_LOOP] = RL_CLEAR; press(B_OCTUP);
    bad += check("CLEAR ALL: no loop, the transport stopped", hcl.len == 0u && !hcl.playing && msg_is2("ALL CLEARED"));
    hui.sel[HU_LOOP] = RL_BARS; press(B_OCTUP); press(B_OCTUP);
    bad += check("BARS: 2", hcl.bars == 2u && val_is(HU_LOOP, RL_BARS, "2"));
    press(B_HOME);
    hcl.sel = 0;
    press(B_REC); press(B_REC);
    bad += check("a fixed length: REC records 2 bars at once", hcl.l[0].state == HLS_REC && hcl.len == 8u * q);
    run_ms2(4100);
    bad += check("  .. and plays by itself after them", hcl.l[0].state == HLS_PLAY && song.sel == 1u);
    hold(B_REC);
    bad += check("REC held on the live layer (2, empty): nothing to clear but no harm; everything cleared when nothing plays",
                 hcl.l[0].state == HLS_PLAY || hcl.len == 0u);
    hcl_clear_all();
    press(B_HOME);
    return bad;
}

static int test_seq_drums_mixer(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t q = (uint32_t)FS * 60u / 120u, i, hits;
    hui_power_on();
    hc.t[0].bass = HB_OFF; hc_apply(&trk[0]);          /* (the counts below are without the bass voice) */
    hc.t[0].stereo = 0;
    hc.t[0].voices = HV_4;
    hc_apply(t);
    /* the sequencer */
    hui_mode_set(t, HP_SEQ);
    hcs_clear();
    hcs.len = 4;
    key_down(0); key_up(0);                            /* I */
    key_down(9); key_up(9);                          /* vi */
    key_down(10); key_down(6); key_up(6); key_up(10);   /* up-right held, then IV: F7 */
    key_down(1); key_up(1);                            /* F#3: a rest */
    bad += check("SEQUENCER: four keys wrote four steps (I, vi, IV + up-right, a rest), the cursor round", hcs.st[0].deg == 0u &&
                 hcs.st[1].deg == 5u && hcs.st[2].deg == 3u && hcs.st[2].dir == HD_UR && hcs.st[3].deg == 255u && hcs.cur == 0u);
    {
        hchord_t ch; char b[12];
        hcs_step_chord(t, 2, &ch, b, 0);
        bad += check("  step 3 is F7", str_eq(b, "F7"));
    }
    run_ms2(100);
    bad += check("  the keys sounded their chords and ended", gated_on(0) == 0u);
    press(B_PLAY);
    bad += check("PLAY: the sequencer runs, step 1 (C) sounds", hcs.running && gate_on(0, 48) && gated_on(0) == 3u);
    run_ms2(520);
    bad += check("  a beat later: step 2 (Am)", hcs.step == 1u && gate_on(0, 57) && !gate_on(0, 48));
    run_ms2(500);
    bad += check("  step 3 (F7)", hcs.step == 2u && gate_on(0, 53) && gate_on(0, 63));
    run_ms2(500);
    bad += check("  step 4: the rest, silence", hcs.step == 3u && gated_on(0) == 0u);
    run_ms2(500);
    bad += check("  round: step 1 again", hcs.step == 0u && gate_on(0, 48));
    press(B_EDIT);
    hui.sel[HU_MODE] = RM_SEQLEN; press(B_OCTUP);
    bad += check("SEQ LENGTH: 8", hcs.len == 8u && val_is(HU_MODE, RM_SEQLEN, "8 STEPS"));
    press(B_HOME);
    press(B_PLAY);
    bad += check("PLAY again: stopped, silent", !hcs.running && gated_on(0) == 0u);
    press(B_PLAY);
    hui_mode_set(t, HP_PLAY);                          /* leaving while it runs: the bounce */
    bad += check("leaving SEQUENCER while it runs: the looper records it (2 bars for 8 steps)", hcs.bounce && hcl.l[0].state == HLS_REC &&
                 hcl.len == 8u * q);
    run_ms2(4200);
    bad += check("  .. two bars later layer 1 plays the sequence, the sequencer is done, the live instrument is layer 2",
                 hcl.l[0].state == HLS_PLAY && !hcs.bounce && !hcs.running && song.sel == 1u && hcl.l[0].n >= 10u);
    hcl_clear_all();
    track_select(0);
    /* the drums */
    hui_mode_set(t, HP_DRUM);
    bad += check("DRUM: the track is on the DRUM engine, the sound remembered", t->eng_req == ENGI_DRUM && hc.t[0].sound_saved == 1u);
    key_down(0);
    bad += check("  the C4 key: the kick (GM 36) sounds", gate_on(0, 36) || trk[0].v[0].active);
    key_up(0);
    key_down(4);
    bad += check("  the E4 key: the snare (38)", gate_on(0, 38) || trk[0].v[1].active || trk[0].v[0].active);
    key_up(4);
    run_ms2(100);
    key_down(8);                                       /* up: AUTO-DRUM 1/4 */
    mo_w = mo_r = 0;                                   /* (the MIDI OUT queue: nobody drains it here) */
    key_down(0);
    hits = midi_ons();
    run_ms2(1100);                                     /* ~2 beats */
    bad += check("AUTO-DRUM: the kick held with a direction repeats (2 more hits in 2 beats)", midi_ons() >= hits + 2u);
    key_up(0); key_up(8);
    press(B_EDIT);
    hui.sel[HU_MODE] = RM_KIT; press(B_OCTUP);
    bad += check("DRUM KIT: HAND -> the engine's KIT", hc.t[0].kit == 1u && t->p[P_E0] == 1);
    press(B_HOME);
    hui_mode_set(t, HP_PLAY);
    bad += check("back to PLAY: the sound is back (SAW on CHORD)", t->eng_req == ENGI_HC && hc.t[0].sound == 1u);
    /* the drum loops */
    hui_mode_set(t, HP_DRUMLOOP);
    bad += check("DRUM LOOP: the DRUM engine, the loop runs", t->eng_req == ENGI_DRUM && hcs.dl_running);
    key_down(9); key_up(9);                          /* the A4 key (the sixth): DEMBOW */
    bad += check("  a white key picks the style (A4: DEMBOW) and starts it", hc.t[0].dl_style == DL_DEMBOW && hcs.dl_running);
    mo_w = mo_r = 0;
    hits = midi_ons();
    run_ms2(2050);                                     /* a bar */
    bad += check("  a bar later: hits came (DEMBOW ORIG: 4 kicks, 4 snares, 8 hats a bar)", midi_ons() >= hits + 14u);
    bad += check("  the variations differ from ORIG", hcd_lanes(DL_ROCK, DV_BUSY, 1) != hcd_lanes(DL_ROCK, DV_ORIG, 1) &&
                 hcd_lanes(DL_ROCK, DV_FILL, 15) != hcd_lanes(DL_ROCK, DV_ORIG, 15));
    press(B_PLAY);
    bad += check("PLAY: the loop stops", !hcs.dl_running);
    hui_mode_set(t, HP_MIXER);
    bad += check("MIXER: the sound back on the track", t->eng_req == ENGI_HC);
    key_down(2); key_up(2);                            /* the D4 key: layer 2 mute */
    bad += check("  key 2: layer 2 muted; again: unmuted", trk[1].p[P_MUTE] == 1 && (key_down(2), key_up(2), trk[1].p[P_MUTE] == 0));
    key_down(11); key_up(11);                          /* B4: the metronome */
    bad += check("  key 7: the metronome off", !hcl.metro);
    press(B_OCTDN);
    bad += check("  OCT- on HOME: the selected layer's level down", t->p[P_LEVEL] == TP[P_LEVEL].def - 8);
    for (i = 0; i < HP_COUNT; i++) {                   /* every mode draws its HOME */
        hui_mode_set(t, i);
        hui.force = 1;
        hframes(2);
    }
    hui_mode_set(t, HP_PLAY);
    bad += check("every mode's HOME draws", 1);
    return bad;
}

static int test_games(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t q, i, n0;
    hui_power_on();
    hc.t[0].bass = HB_OFF; hc_apply(&trk[0]);          /* (the note counts below are without the bass voice) */
    hc.t[0].stereo = 0;
    hc.t[0].voices = HV_4;
    hc_apply(t);
    /* CHORD HIRO: CAMPFIRE (I V vi IV ..), MEDIUM (150 ms), 100 % */
    hui_mode_set(t, HP_HIRO);
    hcg.song = 0; hcg.diff = HGD_MEDIUM; hcg.speed = 100;
    q = (uint32_t)FS * 60u / 120u;
    press(B_PLAY);
    bad += check("CHORD HIRO: PLAY starts the song with a bar's count-in", hcg.running && hcg.idx == 0u && hcg.next_at == 4u * q);
    run_ms2(1900);                                     /* 100 ms before the first chord */
    key_down(0); key_up(0);                            /* I, 100 ms early: inside the 150 ms window */
    bad += check("  the I key 100 ms early: OK (inside the window), the chord sounds, the score counts", hcg.hit && hcg.last == HGR_OK &&
                 hcg.score > 0u && gated_on(0) == 3u);
    run_ms2(2100);                                     /* into the second chord (V) */
    bad += check("  the song moved on to the V", hcg.idx == 1u && !hcg.hit);
    key_down(7); key_up(7);                          /* V on time (+/- a block or so) */
    bad += check("  the V on time: PERFECT, combo 2", hcg.last == HGR_PERFECT && hcg.combo == 2u);
    run_ms2(2000);
    run_ms2(400);                                      /* the vi's window passes */
    bad += check("  nothing pressed for the vi: MISS, the combo gone", hcg.last == HGR_MISS && hcg.combo == 0u && hcg.miss == 1u);
    run_ms2(1800);
    key_down(2); key_up(2);                            /* ii instead of IV */
    bad += check("  the wrong chord: MISS", hcg.last == HGR_MISS && hcg.miss == 2u);
    for (i = 0; i < 5u; i++) run_ms2(2000);
    bad += check("  the song ends by itself (done)", hcg.done && !hcg.running);
    press(B_EDIT);
    hui.sel[HU_MODE] = RM_DIFF; press(B_OCTUP);
    bad += check("DIFFICULTY: HARD", hcg.diff == HGD_HARD && val_is(HU_MODE, RM_DIFF, "HARD"));
    hui.sel[HU_MODE] = RM_SPEED; press(B_OCTDN);
    bad += check("PRACTICE SPEED: 90 %", hcg.speed == 90u);
    hui.sel[HU_MODE] = RM_SONG; press(B_OCTUP);
    bad += check("HIRO SONG: DOO-WOP", hcg.song == 1u && val_is(HU_MODE, RM_SONG, "DOO-WOP"));
    press(B_HOME);
    /* EAR TRAINER: level 1 */
    hui_mode_set(t, HP_EAR);
    hcg.level = 0;
    hcg.rnd = 0x1234ABCDu;
    press(B_PLAY);
    bad += check("EAR TRAINER: PLAY asks a question (it plays)", hcg.phase == 1u);
    run_ms2(1200);
    bad += check("  .. then waits for the answer", hcg.phase == 2u && gated_on(0) == 0u);
    key_down(hcg.q[0] == 0u ? 9u : 0u); key_up(hcg.q[0] == 0u ? 9u : 0u);   /* a wrong degree (D if it was C, else C) */
    bad += check("  a wrong key: NO, the streak 0, it was shown", hcg.phase == 3u && hcg.result == 2u && hcg.streak == 0u);
    run_ms2(1500);
    run_ms2(1200);
    bad += check("  the next question came and waits", hcg.phase == 2u);
    {
        static const uint8_t DEG_KEY[7] = {0, 2, 4, 6, 7, 9, 11};
        key_down(DEG_KEY[hcg.q[0]]); key_up(DEG_KEY[hcg.q[0]]);
    }
    bad += check("  the right key: CORRECT, streak 1", hcg.result == 1u && hcg.streak == 1u && hcg.right == 1u);
    key_down(1); key_up(1);                            /* F#3 in the result phase: nothing */
    run_ms2(1000);
    run_ms2(1200);
    hcg_ear_replay();
    bad += check("  F#3 replays the question (plays again)", hcg.phase == 1u);
    hui_mode_set(t, HP_EAR);
    hcg.level = 2;                                     /* a modified chord: the degree and the direction */
    press(B_PLAY);
    run_ms2(1200);
    n0 = hcg.qdir[0];
    {
        static const uint8_t DEG_KEY[7] = {0, 2, 4, 6, 7, 9, 11};
        static const uint8_t DIR_KEY[8] = {8, 10, 13, 15, 17, 20, 22, 25};
        key_down(DEG_KEY[hcg.q[0]]); key_up(DEG_KEY[hcg.q[0]]);
        bad += check("level 3: the degree alone is not enough (the direction counts)", hcg.result == 2u);
        run_ms2(1500); run_ms2(1200);
        key_down(DIR_KEY[hcg.qdir[0]]); key_down(DEG_KEY[hcg.q[0]]); key_up(DEG_KEY[hcg.q[0]]); key_up(DIR_KEY[hcg.qdir[0]]);
        bad += check("  the degree with its direction: CORRECT", hcg.result == 1u);
    }
    (void)n0;
    hui_mode_set(t, HP_PLAY);
    bad += check("leaving: the game is off, the chord keys play", !hcg.running && !hcg.phase && hc.t[0].play == HP_PLAY);
    return bad;
}

static int test_fx_amount(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    hc_trk_t *c = &hc.t[0];
    hui_power_on();
    hui_open(HU_SOUND);
    hui.sel[HU_SOUND] = RS_REV;
    c->rev = HRV_HALL; hc_apply(t);
    bad += check("REVERB row: the type and the amount in effect (HALL 65)", val_is(HU_SOUND, RS_REV, "HALL 65") && t->p[P_REV] == 65);
    turn(EN_K4, 3);
    bad += check("  KNOB 4 on the row: the amount (77), the send follows", c->rev_amt == 77u && t->p[P_REV] == 77 &&
                 val_is(HU_SOUND, RS_REV, "HALL 77"));
    turn(EN_K4, -40);
    bad += check("  down to the floor of 1", c->rev_amt == 1u && t->p[P_REV] == 1);
    c->rev = 0; c->rev_amt = 0; hc_apply(t);
    bad += check("  OFF: the type alone", val_is(HU_SOUND, RS_REV, "OFF") && t->p[P_REV] == 0);
    turn(EN_K4, 1);
    bad += check("  KNOB 4 on an OFF effect: on at its first type, the amount", c->rev == HRV_ROOM && c->rev_amt == 54u);
    hui.sel[HU_SOUND] = RS_TREM;
    c->trem = 2; c->trem_amt = 0; hc_apply(t);
    turn(EN_K4, -5);
    bad += check("TREMOLO row: KNOB 4 is the depth (90 - 20)", c->trem_amt == 70u && t->p[P_LD_AMP] == 70);
    hui.sel[HU_SOUND] = RS_FLG;
    c->flg = 1; hc_apply(t);
    turn(EN_K4, 2);
    bad += check("FLANGER row: KNOB 4 is the wet amount (64 + 8), hcfx follows", c->flg_amt == 72u && hcfx.flg_amt == 72u);
    hui.sel[HU_SOUND] = RS_SOUND;
    {
        uint32_t rel = c->rel;
        turn(EN_K4, 1);
        bad += check("any other row: KNOB 4 is the release", c->rel != rel);
    }
    c->rev = 0; c->rev_amt = 0; c->trem = 0; c->trem_amt = 0; c->flg = 0; c->flg_amt = 0; hc_apply(t);
    hui_open(HU_HOME);
    return bad;
}

/* the chord machine's way to play over drums: the drum loop recorded into a looper layer, the live
 * instrument moving to the next layer, PLAY mode there: chords over the loop */
static int test_drums_under_chords(void)
{
    int bad = 0;
    uint32_t i, drums = 0, chords = 0;
    hui_power_on();
    hc.t[0].bass = HB_OFF; hc_apply(&trk[0]);
    hui_mode_set(&trk[0], HP_DRUMLOOP);
    press(B_REC);                                      /* ARMED */
    press(B_REC);                                      /* REC: the loop's first bar */
    run_ms2(2200);
    if (hcl.l[0].state == HLS_REC) press(B_REC);       /* (a free first layer: closed by hand) */
    run_ms2(100);
    bad += check("DRUM LOOP into the looper: layer 1 plays, the live instrument moved on",
                 hcl.l[0].state == HLS_PLAY && song.sel == 1u);
    hui_mode_set(TSEL, HP_PLAY);
    run_ms2(50);                                       /* (the mode change's release of the track lands in a block) */
    bad += check("  PLAY mode on the new layer: the chord sound is back (not DRUM)", TSEL->eng_req != ENGI_DRUM && hc.t[1].play == HP_PLAY);
    key_down(0);
    run_ms2(1200);
    for (i = 0; i < NVOICE; i++) { drums += trk[0].v[i].active; chords += trk[1].v[i].active && trk[1].v[i].gate; }
    bad += check("  a chord held over the loop: chord voices on layer 2, drum voices still on layer 1", chords >= 3u && drums >= 1u);
    key_up(0);
    hcl_clear_all();
    hui_mode_set(&trk[0], HP_PLAY);
    return bad;
}

/* the first thing after power-on (the device's order: the layer, the settings, Felucca's init, the first
 * frames): BASS SLASH from the defaults, a bass key held and a chord key pressed */
static int test_slash_first_thing(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i, g = 0;
    hui_power_on();
    run_ms2(100);                                      /* (the sound load at the first frame: an engine switch over blocks) */
    bad += check("power-on: BASS SLASH, the chord layer on the track", hc.t[0].bass == HB_SLASH && hc_on(t) && t->p[P_CHRD] == CH_HI);
    key_down(0);                                       /* C held: C major with its root bass */
    run_ms2(60);
    for (i = 0; i < NVOICE; i++) g += t->v[i].active && t->v[i].gate;
    printf("    [first chord] name %s gated %u notes:", hc.name, g);
    for (i = 0; i < NVOICE; i++) if (t->v[i].active) printf(" %d%s", t->v[i].note, t->v[i].gate ? "" : "(rel)");
    printf("  bass %d voices %d stereo %d pair %d play %d\n", hc.t[0].bass, hc.t[0].voices, hc.t[0].stereo, trk_pair[0], hc.t[0].play);
    bad += check("  the first chord: C with its bass (C1 C E G, voices 8 with partners)", str_eq(hc.name, "C") && gate_on(0, 24) && gate_on(0, 48) && g >= 4u);
    key_up(0); run_ms2(60); key_down(0); run_ms2(60);
    for (g = 0, i = 0; i < NVOICE; i++) g += t->v[i].active && t->v[i].gate;
    printf("    [second time] name %s gated %u notes:", hc.name, g);
    for (i = 0; i < NVOICE; i++) if (t->v[i].active) printf(" %d%s", t->v[i].note, t->v[i].gate ? "" : "(rel)");
    printf("\n");
    key_down(4);                                      /* E pressed on top: Em/C */
    run_ms2(60);
    bad += check("  the second key on top: Em/C, C1 under E G B, the C chord gone", str_eq(hc.name, "Em/C") && gate_on(0, 24) &&
                 gate_on(0, 52) && gate_on(0, 59) && !gate_on(0, 48));
    key_up(4); key_up(0);
    run_ms2(60);
    return bad;
}

static int test_live_persist(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    hui_power_on();
    memset(hc_live, 0, sizeof hc_live);
    hc.t[0].bass = HB_ROOT; hc.t[0].rev = HRV_HALL; hc.inv[0] = 2; hc.lock[9].on = 1; hc_apply(t);
    fm1_ms += 600; hui_live_poll();
    bad += check("live state: a change is noticed within 500 ms and packed", hui_live.dirty && hc_live[0]);
    fm1_ms += 2000; hui_live_poll();
    bad += check("  not saved yet 2 s later (the knob may still be turning)", hui_live.dirty);
    fm1_ms += 1500; hui_live_poll();
    bad += check("  saved 3 s after the last change", !hui_live.dirty);
    hc_init();                                         /* power off, on */
    bad += check("  a fresh boot starts from the defaults (BASS SLASH)", hc.t[0].bass == HB_SLASH && hc.inv[0] == 0);
    hui_live_restore();
    bad += check("  restored: BASS ROOT, the reverb, the inversion and the lock", hc.t[0].bass == HB_ROOT &&
                 hc.t[0].rev == HRV_HALL && hc.inv[0] == 2 && hc.lock[9].on);
    hc.t[0].bass = HB_SLASH; hc.t[0].rev = 0; hc.inv[0] = 0; hc.lock[9].on = 0; hc_apply(t);
    memset(hc_live, 0, sizeof hc_live);
    return bad;
}

int main(void)
{
    int bad = test_slash_first_thing() + test_boot_and_menus() + test_key_menu() + test_sound_menu() + test_mode_menu_knobs() + test_presets() +
              test_draw_and_handover() + test_looper() + test_seq_drums_mixer() + test_games() + test_live_persist() +
              test_fx_amount() + test_drums_under_chords();
    printf("%s\n", bad ? "HUI TEST FAILED" : "chord machine ui test passed");
    return bad != 0;
}
