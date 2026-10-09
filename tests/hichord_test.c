/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* The HiChord key layer (firmware/src/hichord.c, harmony.c; CHRD HI) against the real keyboard, voice and MIDI
 * code: the degrees of the DEGREE layout, the HiChord manual's key-of-C examples for every direction of the
 * DEFAULT, EXTEND, CHROM and BORROW tables, a direction pressed before or after the chord key and let go
 * (revoicing: the notes that stay keep sounding), INVERT, LOCK, HOLD, BASS ROOT / SLASH, VOICES, VOICE
 * LEADING, the PIANO layout, every scale, MONO plays the root, releases end exactly what a key started,
 * MIDI OUT balanced, and the name shown.
 * Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int gate_note(const track_t *t, uint32_t note)
{
    for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].gate && t->v[i].note == note) return 1;
    return 0;
}
static uint32_t ngated(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate;
    return n;
}
static void reset(void)
{
    ui_power_on();
    memset(kb_chn, 0, sizeof kb_chn); memset(mchord, 0, sizeof mchord); memset(chord_last, 0, sizeof chord_last);
    fm1_in.notes = kb_prev = 0; mo_w = mo_r = 0; usb.config = 1;
    hc_init();
    trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_SCALE] = 1; trk[0].p[P_ROOT] = 0;   /* C major, POLY */
    trk[0].p[P_CHRD] = CH_HI;
    hc.t[0].bass = HB_OFF;
    hc.t[0].voices = HV_4;
    hc.t[0].stereo = 0;                                /* (the counts below are of single voices; stereo() tests the pairs) */
    trk_pair[0] = 0;
    song.g[G_BPM] = 120;
    events_block(CTL);
}
/* blocks rendered through the whole mix: the RMS of L and R (Q15 units) and their difference */
static void render_rms(uint32_t blocks, uint32_t *rl, uint32_t *rr, uint32_t *diff)
{
    int32_t o[2 * CTL];
    uint64_t sl = 0, sr = 0, sd = 0;
    uint32_t b, i;
    for (b = 0; b < blocks; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            int64_t l = o[2 * i], r = o[2 * i + 1];
            sl += (uint64_t)(l * l); sr += (uint64_t)(r * r); sd += (uint64_t)((l - r) * (l - r));
        }
    }
    *rl = (uint32_t)sqrt((double)sl / (blocks * CTL));
    *rr = (uint32_t)sqrt((double)sr / (blocks * CTL));
    *diff = (uint32_t)sqrt((double)sd / (blocks * CTL));
}
/* the clock runs ms milliseconds (blocks of CTL samples through events_block) */
static void run_ms(uint32_t ms)
{
    uint32_t n = ms * (FS / 1000u) / CTL, i;
    for (i = 0; i < n; i++)
        events_block(CTL);
}
static uint32_t midi_ons(void)                     /* note-ons in MIDI OUT so far */
{
    uint32_t i, n = 0;
    for (i = 0; i < mo_w; i++) n += ((midi_out_q[i % MQ] >> 8) & 0xF0u) == 0x90u;
    return n;
}
/* the chord key k plays now: its notes == want (0-terminated); prints both on a mismatch */
static int key_is(uint32_t k, const uint8_t *want)
{
    uint8_t out[CHORD_MAX];
    uint32_t n, i, ok = 1;
    hc.cur_key = (uint8_t)k;
    n = chord_build(&trk[0], hc_root_note(&trk[0], k), out);
    hc.cur_key = HC_NOKEY;
    for (i = 0; i < n; i++) ok &= want[i] == out[i];
    ok &= i == CHORD_MAX || !want[i];
    if (!ok) {
        printf("  key %u:", k);
        for (i = 0; i < n; i++) printf(" %u", out[i]);
        printf("  want:");
        for (i = 0; want[i] && i < CHORD_MAX; i++) printf(" %u", want[i]);
        printf("\n");
    }
    return (int)ok;
}
static int name_is(uint32_t k, const char *want)
{
    uint8_t out[CHORD_MAX];
    hc.cur_key = (uint8_t)k;
    chord_build(&trk[0], hc_root_note(&trk[0], k), out);
    hc.cur_key = HC_NOKEY;
    if (!str_eq(hc.name, want)) printf("  name of key %u: %s, want %s\n", k, hc.name, want);
    return str_eq(hc.name, want);
}
/* keys from F3 (0): the seven white keys of the degrees at the left (F3..E4), the strumplate's nine
 * (F4..G5), the black keys of the directions / gestures */
#define K_I 0u
#define K_II 2u
#define K_III 4u
#define K_IV 6u
#define K_V 7u
#define K_VI 9u
#define K_VII 11u
#define K_I_UP 12u                                 /* F4: I an octave up (chord_build; the plate in the chord modes) */
#define K_P0 12u                                   /* the plate: F4 G4 B4 C5 G5 */
#define K_P1 14u
#define K_P3 18u
#define K_P4 19u
#define K_P8 26u
#define K_INVERT 1u                                /* F#3 */
#define K_LOCK 3u                                  /* G#3 */
#define K_HOLD 5u                                  /* A#3 */
#define K_UP 8u                                    /* C#4 */
#define K_UR 10u                                   /* D#4 */
#define K_RIGHT 13u                                /* F#4 */
#define K_DR 15u                                   /* G#4 */
#define K_DOWN 17u                                 /* A#4 */
#define K_DL 20u                                   /* C#5 */
#define K_LEFT 22u                                 /* D#5 */
#define K_UL 25u                                   /* F#5 */
#define N(...) ((const uint8_t[]){__VA_ARGS__, 0})

static int degrees(void)
{
    int bad = 0, ok = 1;
    static const uint8_t DEG[7] = {K_I, K_II, K_III, K_IV, K_V, K_VI, K_VII};
    static const char *const MAJ[7] = {"C", "Dm", "Em", "F", "G", "Am", "Bdim"};
    static const char *const MIN[7] = {"Am", "Bdim", "C", "Dm", "Em", "F", "G"};
    static const char *const DOR[7] = {"Dm", "Em", "F", "G", "Am", "Bdim", "C"};
    static const char *const MIX[7] = {"G", "Am", "Bdim", "C", "Dm", "Em", "F"};
    static const char *const LYD[7] = {"F", "G", "Am", "Bdim", "C", "Dm", "Em"};
    static const char *const HARM[7] = {"Am", "Bdim", "Caug", "Dm", "E", "F", "G#dim"};
    static const char *const MEL[7] = {"Am", "Bm", "Caug", "D", "E", "F#dim", "G#dim"};
    uint32_t i;
    reset();
    bad += check("DEGREE layout, C major: the first key (F3) is I at C3 (C3 E3 G3)", key_is(K_I, N(48, 52, 55)));
    bad += check("  the second ii: Dm; the seventh (E4) vii: Bdim; the eighth (F4): I an octave up", key_is(K_II, N(50, 53, 57)) &&
                 key_is(K_VII, N(59, 62, 65)) && key_is(K_I_UP, N(60, 64, 67)));
    bad += check("  B4, C5: IV V an octave above (F4 A4 C5)", key_is(18u, N(65, 69, 72)) && key_is(19u, N(67, 71, 74)));
    for (i = 0; i < 7u; i++) ok &= name_is(DEG[i], MAJ[i]);
    bad += check("  the seven names: C Dm Em F G Am Bdim", ok);
    trk[0].p[P_ROOT] = 7;
    bad += check("key of G: I is G (G3 B3 D4), vii is F#dim", key_is(K_I, N(55, 59, 62)) && name_is(K_VII, "F#dim"));
    trk[0].p[P_ROOT] = 10;
    bad += check("key of Bb: flat spellings (Bb, Eb, Adim)", name_is(K_I, "Bb") && name_is(K_IV, "Eb") && name_is(K_VII, "Adim"));
    trk[0].p[P_ROOT] = 9; trk[0].p[P_SCALE] = 2;
    for (i = 0, ok = 1; i < 7u; i++) ok &= name_is(DEG[i], MIN[i]);
    bad += check("A natural minor: Am Bdim C Dm Em F G", ok);
    trk[0].p[P_ROOT] = 2; trk[0].p[P_SCALE] = 3;
    for (i = 0, ok = 1; i < 7u; i++) ok &= name_is(DEG[i], DOR[i]);
    bad += check("D dorian: Dm Em F G Am Bdim C", ok);
    trk[0].p[P_ROOT] = 7; trk[0].p[P_SCALE] = 4;
    for (i = 0, ok = 1; i < 7u; i++) ok &= name_is(DEG[i], MIX[i]);
    bad += check("G mixolydian: G Am Bdim C Dm Em F", ok);
    trk[0].p[P_ROOT] = 5; trk[0].p[P_SCALE] = 9;
    for (i = 0, ok = 1; i < 7u; i++) ok &= name_is(DEG[i], LYD[i]);
    bad += check("F lydian: F G Am Bdim C Dm Em", ok);
    trk[0].p[P_ROOT] = 9; trk[0].p[P_SCALE] = 7;
    for (i = 0, ok = 1; i < 7u; i++) ok &= name_is(DEG[i], HARM[i]);
    bad += check("A harmonic minor: Am Bdim Caug Dm E F G#dim", ok);
    trk[0].p[P_ROOT] = 9; trk[0].p[P_SCALE] = 11;
    for (i = 0, ok = 1; i < 7u; i++) ok &= name_is(DEG[i], MEL[i]);
    bad += check("A melodic minor: Am Bm Caug D E F#dim G#dim", ok);
    trk[0].p[P_ROOT] = 0; trk[0].p[P_SCALE] = 0;
    bad += check("CHR: the major scale of ROOT (C: Dm on ii)", name_is(K_II, "Dm"));
    trk[0].p[P_SCALE] = 5;
    bad += check("C major pentatonic: five roots (C D E G A), the parent scale's triads (Dm, G, Am), then the octave (key 6: C)",
                 name_is(K_I, "C") && name_is(K_II, "Dm") && key_is(K_IV, N(55, 59, 62)) && name_is(K_V, "Am") &&
                 key_is(K_VI, N(60, 64, 67)));
    trk[0].p[P_SCALE] = 12; trk[0].p[P_ROOT] = 9;
    bad += check("A blues: the roots A C D Eb E G, minor-parent triads (Am, C, Dm)", name_is(K_I, "Am") && name_is(K_II, "C") &&
                 name_is(K_III, "Dm"));
    {   /* every scale, root and key: 1..CHORD_MAX notes, ascending, unique, in range; a name that fits */
        uint32_t s, r, k, n, j;
        uint8_t out[CHORD_MAX];
        ok = 1;
        for (s = 0; s <= (uint32_t)TP[P_SCALE].max; s++)
            for (r = 0; r < 12u; r++)
                for (k = 0; k < 27u; k++) {
                    if (key_black(k)) continue;
                    trk[0].p[P_SCALE] = (int16_t)s; trk[0].p[P_ROOT] = (int16_t)r;
                    hc.cur_key = (uint8_t)k;
                    n = chord_build(&trk[0], hc_root_note(&trk[0], k), out);
                    hc.cur_key = HC_NOKEY;
                    ok &= n >= 1u && n <= CHORD_MAX;
                    for (j = 1; j < n; j++) ok &= out[j] > out[j - 1u];
                    ok &= str_len(hc.name) >= 1u && str_len(hc.name) <= 7u;
                }
        bad += check("every scale x root x key: 1..6 notes, ascending, unique, a name of 1..7 chars", ok);
    }
    return bad;
}

/* the manual's key-of-C table for every direction (hc.dir set directly: the tables themselves) */
static int tables(void)
{
    int bad = 0;
    reset();
    hc.t[0].mode = HM_DEFAULT;
    hc.dir = HD_UP;    bad += check("DEFAULT up: C -> Cm, Dm -> D", name_is(K_I, "Cm") && name_is(K_II, "D") && key_is(K_I, N(48, 51, 55)));
    hc.dir = HD_UR;    bad += check("  up-right: C7 (C E G Bb)", name_is(K_I, "C7") && key_is(K_I, N(48, 52, 55, 58)));
    hc.dir = HD_RIGHT; bad += check("  right: Cmaj7, Am7, Bm7b5", name_is(K_I, "Cmaj7") && name_is(K_VI, "Am7") && name_is(K_VII, "Bm7b5"));
    hc.dir = HD_DR;    bad += check("  down-right: Cmaj9 (C E G B D), Dm9, Bm7b5", name_is(K_I, "Cmaj9") && name_is(K_II, "Dm9") &&
                                    name_is(K_VII, "Bm7b5"));
    hc.t[0].voices = HV_8;  bad += check("  .. VOICES 8: the 9th sounds (C E G B D)", key_is(K_I, N(48, 52, 55, 59, 62)));
    hc.t[0].voices = HV_4;
    hc.dir = HD_DOWN;  bad += check("  down: Csus4 (C F G)", name_is(K_I, "Csus4") && key_is(K_I, N(48, 53, 55)));
    hc.dir = HD_DL;    bad += check("  down-left: C6 on major, Dsus2 on minor", name_is(K_I, "C6") && name_is(K_II, "Dsus2") &&
                                    key_is(K_II, N(50, 52, 57)));
    hc.dir = HD_LEFT;  bad += check("  left: Cdim, Ddim, the dim chord -> Bm", name_is(K_I, "Cdim") && name_is(K_II, "Ddim") && name_is(K_VII, "Bm"));
    hc.dir = HD_UL;    bad += check("  up-left: Caug (C E G#)", name_is(K_I, "Caug") && key_is(K_I, N(48, 52, 56)));
    hc.t[0].mode = HM_EXTEND;
    hc.dir = HD_UP;    bad += check("EXTEND up: flip (Cm)", name_is(K_I, "Cm"));
    hc.dir = HD_UR;    bad += check("  up-right: C9", name_is(K_I, "C9"));
    hc.dir = HD_RIGHT; bad += check("  right: Cadd11", name_is(K_I, "Cadd11"));
    hc.dir = HD_DR;    bad += check("  down-right: Cm11", name_is(K_I, "Cm11"));
    hc.dir = HD_DOWN;  bad += check("  down: C7#9", name_is(K_I, "C7#9"));
    hc.dir = HD_DL;    bad += check("  down-left: Cadd9", name_is(K_I, "Cadd9"));
    hc.dir = HD_LEFT;  bad += check("  left: C7sus4", name_is(K_I, "C7sus4"));
    hc.dir = HD_UL;    bad += check("  up-left: Cm7b5", name_is(K_I, "Cm7b5"));
    hc.t[0].mode = HM_CHROM;
    hc.dir = HD_UP;    bad += check("CHROM up: CmM7", name_is(K_I, "CmM7"));
    hc.dir = HD_UR;    bad += check("  up-right: C7alt", name_is(K_I, "C7alt"));
    hc.dir = HD_RIGHT; bad += check("  right: the key a semitone up, the quality kept (C -> C#, Dm -> D#m)", name_is(K_I, "C#") && name_is(K_II, "D#m"));
    hc.dir = HD_LEFT;  bad += check("  left: a semitone down (C -> B)", name_is(K_I, "B") && key_is(K_I, N(47, 51, 54)));
    hc.dir = HD_DR;    bad += check("  down-right: Cm7b5", name_is(K_I, "Cm7b5"));
    hc.dir = HD_DOWN;  bad += check("  down: Cmaj13", name_is(K_I, "Cmaj13"));
    hc.dir = HD_DL;    bad += check("  down-left: C6/9", name_is(K_I, "C6/9"));
    hc.dir = HD_UL;    bad += check("  up-left: C7b9", name_is(K_I, "C7b9"));
    hc.t[0].mode = HM_BORROW;
    hc.dir = HD_UP;    bad += check("BORROW up: secondary dominant (button 2: D7)", name_is(K_II, "D7") && key_is(K_II, N(50, 54, 57, 60)));
    hc.dir = HD_DOWN;  bad += check("  down: parallel (button 4: Fm)", name_is(K_IV, "Fm"));
    hc.dir = HD_LEFT;  bad += check("  left: flat-side major (button 7: A#, button 2: C#)", name_is(K_VII, "A#") && name_is(K_II, "C#"));
    hc.dir = HD_RIGHT; bad += check("  right: passing dim7 (button 4: F#dim7)", name_is(K_IV, "F#dim7") && key_is(K_IV, N(54, 57, 60, 63)));
    hc.dir = HD_UL;    bad += check("  up-left: tritone sub (button 5: C#7)", name_is(K_V, "C#7"));
    hc.dir = HD_UR;    bad += check("  up-right: backdoor dom7 (button 1: A#7)", name_is(K_I, "A#7"));
    hc.dir = HD_DL;    bad += check("  down-left: borrowed 7ths (C -> Cm7, Dm -> Dmaj7)", name_is(K_I, "Cm7") && name_is(K_II, "Dmaj7"));
    hc.dir = HD_DR;    bad += check("  down-right: half-dim7 (Cm7b5)", name_is(K_I, "Cm7b5"));
    hc.t[0].mode = HM_OG;
    hc.dir = HD_DR;    bad += check("OG: as DEFAULT (Cmaj9)", name_is(K_I, "Cmaj9"));
    hc.dir = HD_NONE; hc.t[0].mode = HM_DEFAULT;
    return bad;
}

/* the strumplate: the nine white keys right of the chord keys play the chord last built, one note each, rising
 * from its root at C4 */
static int plate(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    reset();
    key_down(K_P0);
    bad += check("strumplate before any chord: the tonic's (F4 key: C4), no chord key held", gate_note(t, 60) && ngated(t) == 1u && hc.nheld == 0u);
    key_up(K_P0);
    bad += check("  let go: it ends", ngated(t) == 0u);
    hc.hold = 1;
    key_down(K_P1); key_up(K_P1);
    bad += check("  HOLD does not latch a plate note", ngated(t) == 0u);
    hc.hold = 0;
    key_down(K_VI); key_up(K_VI);                      /* Am */
    key_down(K_P0); key_down(K_P1); key_down(K_P3); key_down(K_P4); key_down(K_P8);
    bad += check("after Am: F4 A4, G4 C5, B4 A5, C5 C6, G5 E7 (A C E from the root, octave after octave)",
                 gate_note(t, 69) && gate_note(t, 72) && gate_note(t, 81) && gate_note(t, 84) && gate_note(t, 100) && ngated(t) == 5u);
    key_up(K_P0); key_up(K_P1); key_up(K_P3); key_up(K_P4); key_up(K_P8);
    key_down(K_I);                                     /* C held: the plate follows it */
    key_down(K_P4);
    bad += check("C held, the C5 key: E5 (its fifth plate note); the chord keeps sounding",
                 gate_note(t, 76) && gate_note(t, 48) && gate_note(t, 52) && gate_note(t, 55) && ngated(t) == 4u);
    key_up(K_P4);
    bad += check("  the plate key up: its note ends, the chord stays", !gate_note(t, 76) && ngated(t) == 3u);
    key_up(K_I);
    song.octave = 1;
    key_down(K_P0);
    bad += check("OCT+: the plate an octave up (F4 key: C5)", gate_note(t, 72) && ngated(t) == 1u);
    key_up(K_P0);
    song.octave = 0;
    hc_play_set(t, HP_LEAD);                           /* LEAD: the root alone, the chord still built for the plate */
    key_down(K_VI);
    bad += check("LEAD: the vi key plays A3 alone, the chord (Am) built for the display and the plate",
                 gate_note(t, 57) && ngated(t) == 1u && str_eq(hc.name, "Am") && hc.cur.n == 3u);
    key_down(K_P1);
    bad += check("  a plate key: C5 (Am's third, an octave up) takes the one voice", gate_note(t, 72) && ngated(t) == 1u);
    key_up(K_P1); key_up(K_VI);
    hc_play_set(t, HP_SEQ);                            /* not a chord mode: the key is a degree an octave up */
    key_down(K_P0);
    bad += check("SEQ mode: F4 is the I chord an octave up again (C4 E4 G4)", gate_note(t, 60) && gate_note(t, 64) && gate_note(t, 67));
    key_up(K_P0);
    hc_play_set(t, HP_PLAY);
    return bad;
}

static int keys(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t mo;
    reset();
    key_down(K_I);
    bad += check("a chord key plays its chord (C3 E3 G3), MIDI OUT the three, the name kept for the display",
                 gate_note(t, 48) && gate_note(t, 52) && gate_note(t, 55) && ngated(t) == 3u && mo_w == 3u &&
                 str_eq(chord_last[0].name, "C") && chord_last[0].n == 3u);
    key_down(K_UR);                                    /* the direction after the key: revoiced */
    bad += check("a direction pressed while held: C7 (the Bb added, C E G kept sounding), one note-on sent",
                 gate_note(t, 58) && gate_note(t, 48) && ngated(t) == 4u && mo_w == 4u && str_eq(hc.name, "C7"));
    key_down(K_UP);
    bad += check("a second direction: the last pressed wins (Cm: E -> Eb, the Bb gone)", gate_note(t, 51) && !gate_note(t, 52) &&
                 !gate_note(t, 58) && ngated(t) == 3u && str_eq(hc.name, "Cm"));
    key_up(K_UP);
    bad += check("  let go: back to the one still held (C7)", gate_note(t, 52) && gate_note(t, 58) && ngated(t) == 4u);
    key_up(K_UR);
    bad += check("  both let go: the plain chord (C E G)", ngated(t) == 3u && !gate_note(t, 58) && str_eq(hc.name, "C"));
    key_up(K_I);
    bad += check("the key let go: nothing left, MIDI OUT balanced (as many offs as ons)", !ngated(t) && mo_w % 2u == 0u);
    mo = mo_w;
    key_down(K_DOWN); key_down(K_II);                  /* the direction before the key */
    bad += check("a direction held before the key: Dsus4 (D G A)", gate_note(t, 50) && gate_note(t, 55) && gate_note(t, 57) &&
                 ngated(t) == 3u && mo_w == mo + 3u);
    key_up(K_II); key_up(K_DOWN);
    bad += check("  released", !ngated(t));
    key_down(K_UP);
    bad += check("a black key alone: silent, no MIDI", !ngated(t) && mo_w == mo + 6u);
    key_up(K_UP);
    key_down(K_I); key_down(K_III);                    /* C E G and E G B: E and G shared */
    bad += check("two chords: the shared notes sound once (C E G B)", ngated(t) == 4u && gate_note(t, 59));
    key_down(K_UR);
    bad += check("  a direction reshapes both (C7 and E7: C E G Bb, E G# B D)", gate_note(t, 58) && gate_note(t, 56) && gate_note(t, 62) &&
                 gate_note(t, 48) && gate_note(t, 52) && gate_note(t, 59));
    key_up(K_UR); key_up(K_I);
    bad += check("  C up: C ends, E G stay for the E key", !gate_note(t, 48) && gate_note(t, 52) && gate_note(t, 55) && gate_note(t, 59));
    key_up(K_III);
    bad += check("  E up: nothing left", !ngated(t));
    t->p[P_VOICE] = V_MONO;
    key_down(K_II);
    bad += check("MONO (LEAD): the root alone (D3)", ngated(t) == 1u && gate_note(t, 50));
    key_up(K_II);
    t->p[P_VOICE] = V_POLY;
    song.octave = 1;
    bad += check("OCT+: the chord an octave up", key_is(K_I, N(60, 64, 67)));
    song.octave = 0;
    t->p[P_TRANS] = 2;
    bad += check("TRN: transposed (D)", name_is(K_I, "D") && key_is(K_I, N(50, 54, 57)));
    t->p[P_TRANS] = 0;
    return bad;
}

static int gestures(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    reset();
    key_down(K_I); key_down(K_INVERT);
    bad += check("INVERT with the key held: 1st inversion (E G C)", key_is(K_I, N(52, 55, 60)) && gate_note(t, 60) && !gate_note(t, 48) &&
                 ngated(t) == 3u);
    key_up(K_INVERT); key_down(K_INVERT);
    bad += check("  again: 2nd inversion (G C E)", key_is(K_I, N(55, 60, 64)) && gate_note(t, 64));
    key_up(K_INVERT); key_down(K_INVERT); key_up(K_INVERT);
    bad += check("  again: root position; the inversion is the key's own (D4 still root position)",
                 key_is(K_I, N(48, 52, 55)) && key_is(K_II, N(50, 53, 57)));
    key_up(K_I);
    key_down(K_I); key_down(K_INVERT); key_up(K_INVERT);
    bad += check("INVERT: the 1st inversion while the key is held", key_is(K_I, N(52, 55, 60)));
    key_up(K_I);
    bad += check("  let go: the inversion is forgotten (the key is not locked): root position next", hc.inv[K_I] == 0 &&
                 key_is(K_I, N(48, 52, 55)));
    key_up(K_I);
    key_down(K_I); key_down(K_INVERT); key_up(K_INVERT); key_down(K_LOCK); key_up(K_LOCK);
    bad += check("  LOCK without a direction on an inverted key: locked, the inversion sticks", hc.lock[K_I].on && hc.inv[K_I] == 1u);
    key_up(K_I);
    bad += check("  next press: still the 1st inversion", hc.inv[K_I] == 1u && key_is(K_I, N(52, 55, 60)));
    key_up(K_I);
    key_down(K_I); key_down(K_LOCK); key_up(K_LOCK); key_up(K_I);
    bad += check("  LOCK again then let go: unlocked, root position next", !hc.lock[K_I].on && hc.inv[K_I] == 0 &&
                 key_is(K_I, N(48, 52, 55)));
    key_up(K_I);
    key_down(K_I); key_down(K_UR); key_down(K_LOCK); key_up(K_LOCK); key_up(K_UR);
    bad += check("LOCK with a direction held: the key keeps C7 after the direction is let go", key_is(K_I, N(48, 52, 55, 58)) &&
                 gate_note(t, 58) && hc.lock[K_I].on);
    key_up(K_I);
    bad += check("  and plays C7 next time; D4 unlocked (Dm)", key_is(K_I, N(48, 52, 55, 58)) && name_is(K_I, "C7") && name_is(K_II, "Dm"));
    key_down(K_I); key_down(K_UP);
    bad += check("  a direction on a locked chord modifies it (C7 -> Cm)", name_is(K_I, "Cm"));
    key_up(K_UP); key_down(K_LOCK); key_up(K_LOCK);
    bad += check("  LOCK again without a direction: unlocked (C)", !hc.lock[K_I].on && name_is(K_I, "C") && !gate_note(t, 58));
    key_up(K_I);
    key_down(K_HOLD); key_up(K_HOLD);
    key_down(K_I); key_up(K_I);
    bad += check("HOLD: the key let go keeps sounding", hc.hold && ngated(t) == 3u && gate_note(t, 48));
    key_down(K_II); key_up(K_II);
    bad += check("  a second chord latches too (C and Dm: C D E F G A)", ngated(t) == 6u);
    key_down(K_I);
    bad += check("  the latched key pressed again: still one set of notes (no restart)", ngated(t) == 6u);
    key_up(K_I);
    key_down(K_HOLD); key_up(K_HOLD);
    bad += check("  HOLD off: everything let go ends", !hc.hold && !ngated(t) && mo_w % 2u == 0u);
    return bad;
}

static int bass_voices_leading(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    reset();
    hc.t[0].bass = HB_ROOT; hc.t[0].voices = HV_8;
    bad += check("BASS ROOT, VOICES 8: the bass two octaves down, the triad doubled (C1 C3 E3 G3 C4 G4)",
                 key_is(K_I, N(24, 48, 52, 55, 60, 67)));
    hc.t[0].voices = HV_4;
    bad += check("  VOICES 4: root third fifth and the bass", key_is(K_I, N(24, 48, 52, 55)));
    hc.t[0].voices = HV_2;
    bad += check("  VOICES 2: root and third", key_is(K_I, N(48, 52)));
    hc.t[0].voices = HV_1;
    bad += check("  VOICES 1: the root", key_is(K_I, N(48)));
    hc.t[0].voices = HV_4;
    hc.dir = HD_UR;
    bad += check("  a seventh with VOICES 4 and the bass: C1 C E G (the bass wins the fourth slot)", key_is(K_I, N(24, 48, 52, 55)));
    hc.dir = HD_NONE;
    hc.t[0].bass = HB_SLASH;
    key_down(K_I);                                    /* the bass key first */
    key_down(K_III);
    bad += check("BASS SLASH: the first key held is the bass of the next (Em/C: C1 under E G B)", str_eq(hc.name, "Em/C") &&
                 gate_note(t, 24) && gate_note(t, 52) && gate_note(t, 55) && gate_note(t, 59));
    bad += check("  the bass key gives its bass note only: its own chord stopped (no C3), 4 notes",
                 !gate_note(t, 48) && ngated(t) == 4u);
    key_up(K_III);
    bad += check("  the chord key let go, the bass key held: its own chord returns (C1 C E G)", str_eq(hc.name, "C") &&
                 gate_note(t, 24) && gate_note(t, 48) && gate_note(t, 52) && gate_note(t, 55) && !gate_note(t, 59));
    key_down(K_III);
    bad += check("  pressed again: Em/C again", str_eq(hc.name, "Em/C") && gate_note(t, 24) && !gate_note(t, 48));
    key_up(K_I);
    bad += check("  the bass key let go first: Em over its own root (E1 E G B)", str_eq(hc.name, "Em") &&
                 gate_note(t, 28) && !gate_note(t, 24) && gate_note(t, 52) && gate_note(t, 59));
    key_down(K_I);
    bad += check("  a key after the chord key is the chord, the held one the bass: C/E", str_eq(hc.name, "C/E") &&
                 gate_note(t, 28) && gate_note(t, 48) && !gate_note(t, 59));
    key_up(K_III); key_up(K_I);
    bad += check("  released", !ngated(t));
    hc.t[0].bass = HB_OFF;
    key_down(K_I);
    key_down(K_INVERT); key_up(K_INVERT);
    bad += check("INVERT on the display: the 1st inversion is named over its lowest note (C/E: E G C)",
                 str_eq(hc.name, "C/E") && gate_note(t, 52) && gate_note(t, 55) && gate_note(t, 60) && !gate_note(t, 48));
    key_down(K_INVERT); key_up(K_INVERT);
    bad += check("  the 2nd: C/G", str_eq(hc.name, "C/G") && gate_note(t, 55));
    key_down(K_INVERT); key_up(K_INVERT);
    bad += check("  root position again: C", str_eq(hc.name, "C") && gate_note(t, 48));
    key_up(K_I);
    hc.inv[K_I] = 0;
    hc.t[0].vlead = 1;
    key_down(K_I); key_up(K_I);                      /* C E G, then G: root position would jump up */
    key_down(K_V);
    bad += check("VOICE LEADING: after C (C3 E3 G3), G is voiced near it (B2 D3 G3 or D3 G3 B3), not G3 B3 D4",
                 gate_note(t, 55) && ((gate_note(t, 47) && gate_note(t, 50)) || (gate_note(t, 50) && gate_note(t, 59))) &&
                 !gate_note(t, 62));
    key_up(K_V);
    hc.t[0].vlead = 0;
    hc.t[0].layout = HL_PIANO;
    bad += check("PIANO layout: the key's own letter is the root (E4 key: Em, F3 key: F an octave down)", name_is(11u, "Em") &&
                 key_is(11u, N(52, 55, 59)) && key_is(0u, N(41, 45, 48)));
    t->p[P_ROOT] = 7;
    bad += check("  in G: the F key snaps onto the scale (E: Em), the C key plays C", name_is(12u, "Em") && name_is(7u, "C"));
    hc.t[0].layout = HL_DEGREE;
    t->p[P_ROOT] = 0;
    return bad;
}

static int midi_in_steps(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    reset();
    midi_enqueue(0x9u | 0x90u << 8 | 62u << 16 | 100u << 24, 1);
    events_block(CTL);
    bad += check("MIDI IN: a note plays the chord of its degree (D: Dm, D F A)", gate_note(t, 62) && gate_note(t, 65) && gate_note(t, 69) &&
                 ngated(t) == 3u);
    midi_enqueue(0x8u | 0x80u << 8 | 62u << 16, 1);
    events_block(CTL);
    bad += check("  its note-off ends the chord", !ngated(t));
    midi_enqueue(0x9u | 0x90u << 8 | 61u << 16 | 100u << 24, 1);
    events_block(CTL);
    bad += check("  a note outside the scale: the degree below (C#: C)", gate_note(t, 60) && gate_note(t, 64) && ngated(t) == 3u);
    midi_enqueue(0x8u | 0x80u << 8 | 61u << 16, 1);
    events_block(CTL);
    t->p[P_CHRD] = CH_DIA3;
    key_down(7u);
    bad += check("CHRD DIA3 still plays as before (the C4 key: C4 E4 G4)", gate_note(t, 60) && gate_note(t, 64) && gate_note(t, 67));
    key_up(7u);
    t->p[P_CHRD] = CH_HI;
    key_down(K_UP);
    t->p[P_CHRD] = CH_OFF;                             /* CHRD switched while a black key is down */
    key_up(K_UP);
    bad += check("a modifier let go after CHRD changed: forgotten, nothing sounds", !hc.black && !ngated(t));
    return bad;
}

static int play_modes(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t mo;
    reset();
    hc_play_set(t, HP_STRUM);
    hc.t[0].strum = HST_MED;                           /* 80 ms between the notes */
    key_down(K_I);
    bad += check("STRUM: the root alone at the press", ngated(t) == 1u && gate_note(t, 48) && midi_ons() == 1u);
    run_ms(90);
    bad += check("  80 ms on: the third", ngated(t) == 2u && gate_note(t, 52));
    run_ms(90);
    bad += check("  160 ms on: the fifth; the roll is over", ngated(t) == 3u && gate_note(t, 55) && !hc.strum[K_I].n);
    key_up(K_I);
    bad += check("  released: nothing left, MIDI balanced", !ngated(t) && mo_w == 6u);
    key_down(K_II);
    run_ms(90);
    key_up(K_II);
    bad += check("a release during the roll: only the notes that started end (MIDI balanced)", !ngated(t) && mo_w == 10u);
    hc.t[0].strum = HST_SLOW;
    key_down(K_III);
    run_ms(150);
    bad += check("SLOW: 200 ms apart (one note after 150 ms)", ngated(t) == 1u);
    run_ms(60);
    bad += check("  two after 210 ms", ngated(t) == 2u);
    key_up(K_III); run_ms(300);
    hc_play_set(t, HP_LEAD);
    bad += check("LEAD: the track is LEGATO (one voice)", t->p[P_VOICE] == V_LEGATO);
    key_down(K_I);
    bad += check("  a key: the root alone (C3)", ngated(t) == 1u && gate_note(t, 48));
    key_down(K_III);
    bad += check("  a second key: the new root, still one voice (E3)", ngated(t) == 1u && gate_note(t, 52));
    key_up(K_III); key_up(K_I);
    bad += check("  released", !ngated(t));
    hc_play_set(t, HP_DRONE);
    key_down(K_I); key_up(K_I);
    bad += check("DRONE: the chord rings after the key is let go", ngated(t) == 3u && gate_note(t, 48));
    key_down(K_II);
    bad += check("  the next key: the chord before ends, Dm sounds", !gate_note(t, 48) && gate_note(t, 50) && gate_note(t, 53) &&
                 ngated(t) == 3u);
    key_up(K_II);
    hc_play_set(t, HP_PLAY);
    bad += check("  leaving DRONE: silence", !ngated(t));
    hc_play_set(t, HP_REPEAT);
    hc.t[0].arp_rate = HR_1_8;                          /* 250 ms at 120 BPM */
    bad += check("REPEAT: the release capped at 200 ms", TIME_MS_X10[t->p[P_REL]] <= 2300u);
    key_down(K_I);
    mo = midi_ons();
    bad += check("  the chord on at the press", ngated(t) == 3u);
    run_ms(140);
    bad += check("  gated off at half the step (125 ms)", !ngated(t));
    run_ms(130);
    bad += check("  on again at the step (250 ms), MIDI note-ons sent again", ngated(t) == 3u && midi_ons() == mo + 3u);
    run_ms(300);
    key_up(K_I);
    run_ms(300);
    bad += check("  released: silent, MIDI balanced", !ngated(t) && mo_w % 2u == 0u);
    hc_play_set(t, HP_ARP);
    hc.t[0].arp_pat = HA_UP; hc.t[0].arp_rate = HR_1_8; hc.t[0].arp_layer = HAL_ARP;
    key_down(K_I);
    run_ms(1);
    bad += check("ARP ONLY, UP: the chord itself is silent; the first step plays the root", ngated(t) == 1u && gate_note(t, 48));
    run_ms(260);
    bad += check("  the second step: the third (the root ended at half the step)", ngated(t) == 1u && gate_note(t, 52));
    run_ms(260);
    bad += check("  the third: the fifth", gate_note(t, 55));
    run_ms(260);
    bad += check("  the fourth (a 7th the triad has not): the fifth again", gate_note(t, 55) && ngated(t) == 1u);
    run_ms(260);
    bad += check("  round again: the root", gate_note(t, 48));
    key_down(K_UR);                                    /* C7 while arping: the 7th appears */
    run_ms(260 * 3);
    bad += check("  a direction while the arp runs: the 7th step plays Bb", gate_note(t, 58));
    key_up(K_UR);
    key_up(K_I);
    run_ms(10);
    bad += check("  the key let go: the arp stops, nothing sounds, MIDI balanced", !ngated(t) && mo_w % 2u == 0u);
    hc.t[0].arp_layer = HAL_CHORD;
    key_down(K_I);
    run_ms(1);
    bad += check("CHORD+ARP: the chord sustains under the arp (C E G + the arp's C)", ngated(t) >= 3u && gate_note(t, 48) &&
                 gate_note(t, 52) && gate_note(t, 55));
    run_ms(260);
    bad += check("  .. the arp's third doubles the chord's", ngated(t) == 3u || ngated(t) == 4u);
    key_up(K_I);
    run_ms(10);
    bad += check("  released", !ngated(t));
    hc.t[0].arp_layer = HAL_RHYTHM;
    key_down(K_I);
    run_ms(1);
    bad += check("RHYTHM+ARP: the chord pulses (on at the step)", ngated(t) >= 3u);
    run_ms(140);
    bad += check("  .. off at half the step, the arp note gone too", !ngated(t));
    key_up(K_I);
    run_ms(300);
    hc_play_set(t, HP_PLAY);
    bad += check("back to PLAY: POLY, release restored (LONG: 2 s)", t->p[P_VOICE] == V_POLY && TIME_MS_X10[t->p[P_REL]] >= 20000u);
    return bad;
}

static int stereo_master(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i, pairs = 0, sideb = 0, rl, rr, diff, rl2, rr2, diff2, rl3, rr3, diff3;
    reset();
    host_preset(t, 0, 0);                              /* ANALOG SAW LEAD: a bright source */
    t->p[P_CHRD] = CH_HI;
    song.master_q12 = 4096;                            /* (the master knob: ui_power_on leaves it at 0) */
    hc.t[0].stereo = 1;
    hc.t[0].env = HE_SHORT;
    hc_apply(t);
    bad += check("STEREO: the part is paired", trk_pair[0] == 1u && t->p[P_VOICE] == V_POLY);
    key_down(K_I);
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate) {
            pairs += t->v[i].pair;
            sideb += t->v[i].side;
        }
    bad += check("  a triad: six voices, three partners, three on side B", ngated(t) == 6u && pairs == 3u && sideb == 3u);
    bad += check("  the partners are detuned the other way from their mains", t->v[1].fine != 0 || t->v[3].fine != 0 || t->v[5].fine != 0);
    render_rms(40, &rl, &rr, &diff);
    printf("  stereo: L %u R %u |L-R| %u\n", rl, rr, diff);
    bad += check("  the mix is stereo (L and R differ) and both sides play", rl > 500u && rr > 500u && diff > rl / 8u);
    key_up(K_I);
    run_ms(400);
    hc.t[0].stereo = 0;
    hc_apply(t);
    key_down(K_I);
    bad += check("STEREO off: three voices, none paired", ngated(t) == 3u);
    render_rms(40, &rl2, &rr2, &diff2);
    printf("  mono: L %u R %u |L-R| %u\n", rl2, rr2, diff2);
    bad += check("  the mix is mono (L == R, but for the DC blockers' history)", diff2 * 64u < rl2 && rl2 > 500u);
    key_up(K_I);
    run_ms(400);
    hc.t[0].filt = 1; hc.t[0].cutoff = 10;             /* the FILTER wheel nearly closed */
    hc_apply(t);
    bad += check("FILTER wheel: the master follows the live track", hcfx.filt == 1u && hcfx.cutoff == 10u);
    key_down(K_I);
    render_rms(40, &rl3, &rr3, &diff3);
    key_up(K_I);
    run_ms(400);
    bad += check("  a closed low-pass takes most of a saw chord away", rl3 < rl2 / 2u);
    hc.t[0].filt = 0; hc.t[0].flg = HFL_JET;
    hc_apply(t);
    key_down(K_I);
    render_rms(40, &rl3, &rr3, &diff3);
    key_up(K_I);
    run_ms(400);
    bad += check("FLANGER: sound, and the two channels sweep apart", rl3 > 500u && diff3 > 0u);
    hc.t[0].flg = HFL_OFF; hc.t[0].tape = HTP_LOFI;
    hc_apply(t);
    key_down(K_I);
    render_rms(40, &rl3, &rr3, &diff3);
    key_up(K_I);
    run_ms(400);
    printf("  lofi: L %u R %u |L-R| %u\n", rl3, rr3, diff3);
    bad += check("TAPE LOFI: sound, still mono", rl3 > 400u && diff3 * 64u < rl3);
    hc.t[0].tape = HTP_VINYL;
    hc_apply(t);
    key_down(K_I);
    render_rms(40, &rl3, &rr3, &diff3);
    key_up(K_I);
    run_ms(400);
    bad += check("TAPE VINYL: sound", rl3 > 400u);
    hc.t[0].tape = HTP_OFF; hc.t[0].hp = 0;
    hc_apply(t);
    key_down(K_I);
    run_ms(1200);                                      /* sustained: the reference, then the same chord hi-passed */
    render_rms(40, &rl2, &rr2, &diff2);
    hc.t[0].hp = 1;
    hc_apply(t);
    render_rms(40, &rl3, &rr3, &diff3);
    key_up(K_I);
    run_ms(400);
    bad += check("HI-PASS: sound (less bass)", rl3 > 150u && rl3 < rl2);
    hc.t[0].hp = 0;
    hc_apply(t);
    bad += check("everything off: the master stage bypassed", !hcfx.filt && !hcfx.hp && !hcfx.flg && !hcfx.tape);
    return bad;
}

int main(void)
{
    int bad = degrees() + tables() + keys() + plate() + gestures() + bass_voices_leading() + midi_in_steps() + play_modes() + stereo_master();
    printf("%s\n", bad ? "HICHORD TEST FAILED" : "hichord keys test passed");
    return bad != 0;
}
