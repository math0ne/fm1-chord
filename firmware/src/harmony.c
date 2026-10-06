/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* HiChord-style harmony: a scale degree of the track's ROOT / SCALE becomes a chord (its default
 * quality: the triad stacked on that degree), a modifier (one of eight directions in one of five
 * tables) reshapes it, and the result is voiced into six slots: ROOT, THIRD, FIFTH, BASS, EXT1, EXT2.
 * Pure functions over small tables: no state, no hardware. Included by hichord.c. Tables and the
 * choices marked (assumption) are in spec/docs/04-chord-engine-tables.md. */

/* chord qualities: the persisted index (locks, presets): append, never reorder */
enum { HQ_MAJ, HQ_MIN, HQ_DIM, HQ_B5, HQ_AUG, HQ_SUS4, HQ_SUS2, HQ_MAJ7, HQ_MIN7, HQ_DOM7, HQ_MAJ6, HQ_MIN6,
       HQ_MAJ9, HQ_MIN9, HQ_DOM7S9, HQ_HDIM7, HQ_DOM9, HQ_ADD9, HQ_ADD11, HQ_MIN11, HQ_SUS47, HQ_MINMAJ7,
       HQ_MAJ13, HQ_69, HQ_DOM7B9, HQ_DOM7ALT, HQ_MAJ7S11, HQ_DOM13, HQ_DIM7, HQ_COUNT };

/* the intervals of a quality in slot order: ROOT, THIRD, FIFTH, EXT1, EXT2 (-1 = the slot is empty) */
#define HQ_NIV 5
static const int8_t HQ_IV[HQ_COUNT][HQ_NIV] = {
    {0, 4, 7, -1, -1},   /* MAJ */
    {0, 3, 7, -1, -1},   /* MIN */
    {0, 3, 6, -1, -1},   /* DIM */
    {0, 4, 6, -1, -1},   /* B5 */
    {0, 4, 8, -1, -1},   /* AUG */
    {0, 5, 7, -1, -1},   /* SUS4 */
    {0, 2, 7, -1, -1},   /* SUS2 */
    {0, 4, 7, 11, -1},   /* MAJ7 */
    {0, 3, 7, 10, -1},   /* MIN7 */
    {0, 4, 7, 10, -1},   /* DOM7 */
    {0, 4, 7, 9, -1},    /* MAJ6 */
    {0, 3, 7, 9, -1},    /* MIN6 */
    {0, 4, 7, 11, 14},   /* MAJ9 */
    {0, 3, 7, 10, 14},   /* MIN9 */
    {0, 4, 7, 10, 15},   /* DOM7#9 */
    {0, 3, 6, 10, -1},   /* HDIM7 (m7b5) */
    {0, 4, 7, 10, 14},   /* DOM9 */
    {0, 4, 7, 14, -1},   /* ADD9 */
    {0, 4, 7, 17, -1},   /* ADD11 */
    {0, 3, 7, 10, 17},   /* MIN11 (the 9th dropped: two extension slots) (assumption) */
    {0, 5, 7, 10, -1},   /* 7SUS4 */
    {0, 3, 7, 11, -1},   /* MIN(MAJ7) */
    {0, 4, 7, 11, 21},   /* MAJ13 */
    {0, 4, 7, 9, 14},    /* 6/9 */
    {0, 4, 7, 10, 13},   /* DOM7b9 */
    {0, 4, 6, 10, 13},   /* DOM7alt: b5 and b9 (assumption) */
    {0, 4, 7, 11, 18},   /* MAJ7#11 */
    {0, 4, 7, 10, 21},   /* DOM13 */
    {0, 3, 6, 9, -1},    /* DIM7 */
};
/* the symbol after the root name (<= 5 chars: a name fits 7 with "A#") */
static const char *const HQ_SYM[HQ_COUNT] = {
    "", "m", "dim", "b5", "aug", "sus4", "sus2", "maj7", "m7", "7", "6", "m6",
    "maj9", "m9", "7#9", "m7b5", "9", "add9", "add11", "m11", "7sus4", "mM7",
    "maj13", "6/9", "7b9", "7alt", "M7#11", "13", "dim7",
};
/* the quality's family by its third: 4 = major, 3 = minor, 2 / 5 = suspended (treated as major) */
static int hq_minor(uint32_t q) { return HQ_IV[q % HQ_COUNT][1] == 3; }
static int hq_dimlike(uint32_t q) { return HQ_IV[q % HQ_COUNT][1] == 3 && HQ_IV[q % HQ_COUNT][2] == 6; }
static uint32_t hq_flip(uint32_t q) { return hq_minor(q) ? HQ_MAJ : HQ_MIN; }

/* ------------------------------------------------------- modifiers --- */
enum { HM_DEFAULT, HM_EXTEND, HM_CHROM, HM_BORROW, HM_OG, HM_COUNT };   /* the joystick mode (HiChord CC 23) */
enum { HD_UP, HD_UR, HD_RIGHT, HD_DR, HD_DOWN, HD_DL, HD_LEFT, HD_UL, HD_NONE };   /* clockwise from up */
static const char *const HM_NAME[HM_COUNT] = {"DEFAULT", "EXTEND", "CHROM", "BORROW", "OG"};
static const char *const HD_ARROW[8] = {"\x18", "\x18\x1A", "\x1A", "\x19\x1A", "\x19", "\x19\x1B", "\x1B", "\x18\x1B"};

/* direction d in mode m applied to a chord of quality q -> its new quality; *roff its root offset in
 * semitones (BORROW and CHROM's key shifts move the root) */
static uint32_t hq_modify(uint32_t m, uint32_t d, uint32_t q, int32_t *roff)
{
    int mi = hq_minor(q), dm = hq_dimlike(q);
    *roff = 0;
    if (d >= HD_NONE)
        return q;
    switch (m) {
    case HM_EXTEND:
        switch (d) {
        case HD_UP: return hq_flip(q);
        case HD_UR: return HQ_DOM9;
        case HD_RIGHT: return mi ? HQ_MIN11 : HQ_ADD11;   /* (minor: assumption) */
        case HD_DR: return HQ_MIN11;
        case HD_DOWN: return HQ_DOM7S9;
        case HD_DL: return HQ_ADD9;
        case HD_LEFT: return HQ_SUS47;
        default: return HQ_HDIM7;
        }
    case HM_CHROM:
        switch (d) {
        case HD_UP: return HQ_MINMAJ7;
        case HD_UR: return HQ_DOM7ALT;
        case HD_RIGHT: *roff = 1; return q;              /* the key a semitone up, the quality kept */
        case HD_DR: return HQ_HDIM7;
        case HD_DOWN: return HQ_MAJ13;
        case HD_DL: return HQ_69;
        case HD_LEFT: *roff = -1; return q;
        default: return HQ_DOM7B9;
        }
    case HM_BORROW:
        switch (d) {
        case HD_UP: return HQ_DOM7;                      /* secondary dominant */
        case HD_UR: *roff = -2; return HQ_DOM7;          /* backdoor dominant */
        case HD_RIGHT: *roff = 1; return HQ_DIM7;        /* passing dim7 */
        case HD_DR: return HQ_HDIM7;
        case HD_DOWN: return hq_flip(q);                 /* parallel major / minor */
        case HD_DL: return mi ? HQ_MAJ7 : HQ_MIN7;       /* borrowed sevenths */
        case HD_LEFT: *roff = -1; return HQ_MAJ;         /* flat side, Neapolitan */
        default: *roff = 6; return HQ_DOM7;              /* tritone substitute */
        }
    default:                                             /* DEFAULT and OG (OG: the same chords) */
        switch (d) {
        case HD_UP: return hq_flip(q);
        case HD_UR: return HQ_DOM7;
        case HD_RIGHT: return dm ? HQ_HDIM7 : mi ? HQ_MIN7 : HQ_MAJ7;
        case HD_DR: return dm ? HQ_HDIM7 : mi ? HQ_MIN9 : HQ_MAJ9;
        case HD_DOWN: return HQ_SUS4;
        case HD_DL: return mi ? HQ_SUS2 : HQ_MAJ6;
        case HD_LEFT: return dm ? HQ_MIN : HQ_DIM;
        default: return HQ_AUG;
        }
    }
}

/* ---------------------------------------------------- scale degrees --- */
/* the pitch classes of scale mask (12 bits) in order -> deg[], their number */
static uint32_t hs_degrees(uint32_t mask, uint8_t *deg)
{
    uint32_t i, c = 0;
    for (i = 0; i < 12u; i++)
        if ((mask >> i) & 1u)
            deg[c++] = (uint8_t)i;
    return c;
}

/* degree d (0 = the tonic; beyond the scale's length: the next octave) of a scale -> its root above the
 * tonic in semitones (*root) and the default quality: the triad stacked on it in scale steps. Scales of
 * fewer than seven notes that have a fifth (the pentatonics, blues): the roots walk the scale, the triads
 * come from its parent seven-note scale, major or natural minor by the scale's third (assumption: the
 * HiChord's own tables for these are not published) */
static uint32_t hs_degree_chord(uint32_t mask, uint32_t d, int32_t *root)
{
    uint8_t deg[12], pdeg[12];
    uint32_t c = hs_degrees(mask, deg), pc, third, fifth, pd;
    int32_t r, t3, t5;
    if (c < 2u) {                                        /* (no scale: major on the tonic) */
        *root = (int32_t)d * 12;
        return HQ_MAJ;
    }
    r = (int32_t)deg[d % c] + 12 * (int32_t)(d / c);
    *root = r;
    if (c < 7u && ((mask >> 7) & 1u)) {                  /* the parent scale's triad on that root (whole tone and the
                                                          * diminished scales, without a fifth or with 8 notes: stacked) */
        uint32_t parent = (mask >> 4) & 1u ? 0xAB5u : 0x5ADu;   /* major 0 2 4 5 7 9 11, minor 0 2 3 5 7 8 10 */
        pc = hs_degrees(parent, pdeg);
        for (pd = 0; pd < pc && pdeg[pd] != deg[d % c]; pd++)
            ;
        if (pd == pc)                                    /* (a root outside the parent: the one below) */
            for (pd = 0; pd + 1u < pc && pdeg[pd + 1u] <= deg[d % c]; pd++)
                ;
        t3 = (int32_t)pdeg[(pd + 2u) % pc] + 12 * (int32_t)((pd + 2u) / pc) - (int32_t)pdeg[pd];
        t5 = (int32_t)pdeg[(pd + 4u) % pc] + 12 * (int32_t)((pd + 4u) / pc) - (int32_t)pdeg[pd];
    } else {
        t3 = (int32_t)deg[(d + 2u) % c] + 12 * (int32_t)((d + 2u) / c) - r;
        t5 = (int32_t)deg[(d + 4u) % c] + 12 * (int32_t)((d + 4u) / c) - r;
    }
    third = (uint32_t)t3;
    fifth = (uint32_t)t5;
    if (third == 4u && fifth == 7u) return HQ_MAJ;
    if (third == 3u && fifth == 7u) return HQ_MIN;
    if (third == 3u && fifth == 6u) return HQ_DIM;
    if (third == 4u && fifth == 8u) return HQ_AUG;
    if (third == 4u && fifth == 6u) return HQ_B5;
    if (third == 2u && fifth == 7u) return HQ_SUS2;
    if (third == 5u && fifth == 7u) return HQ_SUS4;
    if (third == 3u) return HQ_MIN;                      /* pentatonic / blues: by the third alone */
    if (third == 4u) return HQ_MAJ;
    return fifth == 6u ? HQ_DIM : HQ_MAJ;
}

/* the degree (0..c-1) of pitch class pc in the scale (mask, tonic), snapping an outside note down onto
 * the scale note below it (as DIA does) */
static uint32_t hs_degree_of(uint32_t mask, uint32_t tonic, uint32_t pc)
{
    uint8_t deg[12];
    uint32_t c = hs_degrees(mask, deg), rel = (pc + 12u - tonic % 12u) % 12u, i, best = 0;
    for (i = 0; i < c; i++)
        if (deg[i] <= rel)
            best = i;
    return best;
}

/* ----------------------------------------------------------- voicing --- */
enum { HS_ROOT, HS_THIRD, HS_FIFTH, HS_BASS, HS_EXT1, HS_EXT2, HS_N };   /* the six voice slots */
enum { HV_8, HV_4, HV_2, HV_1 };                         /* VOICES (HiChord CC 49): 8 4 2 1 */
static const char *const HV_NAME[4] = {"8", "4", "2", "1"};

typedef struct {
    int32_t root;                /* MIDI note of the root (the slot pitches before the octave clamp) */
    uint8_t q;                   /* HQ_* */
    int16_t slot[HS_N];          /* the slot's note, -1 = silent */
    uint8_t n;                   /* notes after sorting / deduplication */
    uint8_t note[HS_N];          /* ascending, unique, 0..127 */
    int32_t bass;                /* the BASS slot's note, -1 = none */
} hchord_t;

/* the slots of quality q on root (MIDI note): bass (-1 = none, else its note), voices HV_*, inversion 0..2
 * of the upper structure -> c (sorted, unique, clamped). doubling: VOICES 8 fills empty extension slots
 * with the root and the fifth an octave up (assumption: "doubled notes") */
static void hs_voice(hchord_t *c, int32_t root, uint32_t q, int32_t bass, uint32_t voices, uint32_t inv)
{
    const int8_t *iv = HQ_IV[q % HQ_COUNT];
    int32_t s[HS_N];
    uint32_t i, j, n = 0;
    c->root = root;
    c->q = (uint8_t)(q % HQ_COUNT);
    c->bass = bass;
    s[HS_ROOT] = root;
    s[HS_THIRD] = root + iv[1];
    s[HS_FIFTH] = root + iv[2];
    s[HS_BASS] = bass;
    s[HS_EXT1] = iv[3] >= 0 ? root + iv[3] : -1;
    s[HS_EXT2] = iv[4] >= 0 ? root + iv[4] : -1;
    switch (voices) {
    case HV_1:
        s[HS_THIRD] = s[HS_FIFTH] = s[HS_BASS] = s[HS_EXT1] = s[HS_EXT2] = -1;
        break;
    case HV_2:
        s[HS_FIFTH] = s[HS_BASS] = s[HS_EXT1] = s[HS_EXT2] = -1;
        break;
    case HV_4:                                           /* root, third, fifth and the bass or the first extension */
        if (s[HS_BASS] >= 0)
            s[HS_EXT1] = -1;
        s[HS_EXT2] = -1;
        break;
    default:                                             /* 8: every slot; triads doubled */
        if (s[HS_EXT1] < 0)
            s[HS_EXT1] = root + 12;
        if (s[HS_EXT2] < 0)
            s[HS_EXT2] = (iv[3] < 0 ? s[HS_FIFTH] : root) + 12;
        break;
    }
    for (j = 0; j < (inv > 2u ? 2u : inv); j++) {        /* the lowest upper note an octave up, twice for INV2 */
        int32_t low = 0x7FFF, li = -1;
        for (i = 0; i < HS_N; i++)
            if (i != HS_BASS && s[i] >= 0 && s[i] < low) {
                low = s[i];
                li = (int32_t)i;
            }
        if (li >= 0)
            s[li] += 12;
    }
    for (i = 0; i < HS_N; i++)
        c->slot[i] = (int16_t)s[i];
    for (i = 0; i < HS_N; i++) {                         /* ascending, unique, inside 0..127 */
        int32_t x = s[i];
        uint32_t k;
        if (x < 0 || x > 127)
            continue;
        for (k = 0; k < n && c->note[k] != (uint8_t)x; k++)
            ;
        if (k < n)
            continue;
        for (k = n; k > 0 && c->note[k - 1u] > (uint8_t)x; k--)
            c->note[k] = c->note[k - 1u];
        c->note[k] = (uint8_t)x;
        n++;
    }
    if (!n) {
        c->note[0] = (uint8_t)(root < 0 ? 0 : root > 127 ? 127 : root);
        n = 1;
    }
    c->n = (uint8_t)n;
}

/* VOICE LEADING: of the inversions 0..2 an octave down, as is, or up, the upper structure closest to the
 * previous chord's (prev[0..np), ascending): the sum of the distances of the sorted notes, the shorter list
 * padded with its top note; +6 when the lowest note moves more than a fifth. Returns the best inversion and
 * *oct its octave shift (-12, 0, 12); without a previous chord inversion 0 as it is */
static uint32_t hs_lead(int32_t root, uint32_t q, int32_t bass, uint32_t voices, const uint8_t *prev, uint32_t np,
                        int32_t *oct)
{
    uint32_t inv, best_inv = 0, i, j;
    int32_t o, best_cost = 0x7FFFFFFF, best_oct = 0;
    *oct = 0;
    if (!np)
        return 0;
    for (o = -12; o <= 12; o += 12)
        for (inv = 0; inv < 3u; inv++) {
            hchord_t c;
            uint8_t up[HS_N];
            uint32_t nu = 0;
            int32_t cost = 0;
            hs_voice(&c, root + o, q, -1, voices, inv);
            for (i = 0; i < c.n; i++)
                up[nu++] = c.note[i];
            if (!nu)
                continue;
            for (i = 0, j = 0; i < (nu > np ? nu : np); i++) {
                int32_t a = up[i < nu ? i : nu - 1u], b = prev[i < np ? i : np - 1u], d = a - b;
                cost += d < 0 ? -d : d;
            }
            {
                int32_t d = (int32_t)up[0] - (int32_t)prev[0];
                if (d > 7 || d < -7)
                    cost += 6;
            }
            cost = cost * 4 + (int32_t)inv + (o ? 2 : 0);   /* ties: root position, no octave shift */
            if (cost < best_cost) {
                best_cost = cost;
                best_inv = inv;
                best_oct = o;
            }
        }
    (void)bass;
    *oct = best_oct;
    return best_inv;
}

/* "C", "Dm7", "F#m7b5", "Em/C" -> b (12 bytes). Flat spellings for the flat keys (tonic F Bb Eb Ab Db Gb) */
static const char *const HN_SHARP[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const HN_FLAT[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
static int hn_flat_key(uint32_t tonic) { return (0x56Au >> (tonic % 12u)) & 1u; }   /* bits 1 3 5 6 8 10 */
static void hs_name(char *b, uint32_t tonic, int32_t root, uint32_t q, int32_t bass)
{
    const char *const *nn = hn_flat_key(tonic) ? HN_FLAT : HN_SHARP;
    uint32_t len;
    str_cpy(b, nn[(uint32_t)(root + 1200) % 12u], 12);
    len = str_len(b);
    str_cpy(b + len, HQ_SYM[q % HQ_COUNT], 12 - len);
    if (bass >= 0 && (uint32_t)(bass + 1200) % 12u != (uint32_t)(root + 1200) % 12u) {
        len = str_len(b);
        if (len < 9u) {
            b[len] = '/';
            str_cpy(b + len + 1u, nn[(uint32_t)(bass + 1200) % 12u], 12 - len - 1u);
        }
    }
}
