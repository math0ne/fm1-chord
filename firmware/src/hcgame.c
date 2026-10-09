/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* The chord machine's games.
 *   CHORD HIRO   ten songs as Nashville charts (a degree, a direction for its seventh, beats). The chart runs
 *                at the tempo (SPEED 50..100 %); the player presses the chord's key within the window of the
 *                DIFFICULTY (EASY +-200 ms, MEDIUM 150, HARD 100, EXPERT 50): PERFECT inside a quarter of it,
 *                GREAT inside half, OK inside it, else MISS; a chord sounds when hit. Score, combo, accuracy.
 *   EAR TRAINER  six levels: 1 a triad (which degree?), 2 a four-chord progression, 3 a chord with a
 *                modification (the degree and the direction), 4 a progression with modifications, 5 and 6
 *                an interval from the tonic (which degree?). F#3 replays the question; the streak counts.
 * Included by chordmachine.c after hcseq.c; runs in the audio ISR (hcg_tick from hc_tick); the UI (hui.c) shows it. */
enum { HGD_EASY, HGD_MEDIUM, HGD_HARD, HGD_EXPERT, HGD_COUNT };
static const char *const HGD_NAME[HGD_COUNT] = {"EASY", "MEDIUM", "HARD", "EXPERT"};
static const uint16_t HGD_WINDOW_MS[HGD_COUNT] = {200, 150, 100, 50};
enum { HGR_NONE, HGR_PERFECT, HGR_GREAT, HGR_OK, HGR_MISS };
static const char *const HGR_NAME[5] = {"", "PERFECT", "GREAT", "OK", "MISS"};
typedef struct { uint8_t deg, dir, beats; } hcg_ev_t;
typedef struct { const char *name; uint8_t n; hcg_ev_t ev[12]; } hcg_song_t;
#define HG_SONGS 10u
static const hcg_song_t HCG_SONG[HG_SONGS] = {
    {"CAMPFIRE", 8, {{0, HD_NONE, 4}, {4, HD_NONE, 4}, {5, HD_NONE, 4}, {3, HD_NONE, 4}, {0, HD_NONE, 4}, {4, HD_NONE, 4}, {5, HD_NONE, 4}, {3, HD_NONE, 4}}},
    {"DOO-WOP", 8, {{0, HD_NONE, 4}, {5, HD_NONE, 4}, {3, HD_NONE, 4}, {4, HD_NONE, 4}, {0, HD_NONE, 4}, {5, HD_NONE, 4}, {3, HD_NONE, 4}, {4, HD_NONE, 4}}},
    {"AXIS", 8, {{5, HD_NONE, 4}, {3, HD_NONE, 4}, {0, HD_NONE, 4}, {4, HD_NONE, 4}, {5, HD_NONE, 4}, {3, HD_NONE, 4}, {0, HD_NONE, 4}, {4, HD_NONE, 4}}},
    {"12-BAR", 12, {{0, HD_UR, 4}, {0, HD_UR, 4}, {0, HD_UR, 4}, {0, HD_UR, 4}, {3, HD_UR, 4}, {3, HD_UR, 4}, {0, HD_UR, 4}, {0, HD_UR, 4},
                    {4, HD_UR, 4}, {3, HD_UR, 4}, {0, HD_UR, 4}, {4, HD_UR, 4}}},
    {"BOSSA", 8, {{1, HD_RIGHT, 4}, {4, HD_UR, 4}, {0, HD_RIGHT, 4}, {5, HD_RIGHT, 4}, {1, HD_RIGHT, 4}, {4, HD_UR, 4}, {0, HD_RIGHT, 4}, {0, HD_RIGHT, 4}}},
    {"CANON", 8, {{0, HD_NONE, 2}, {4, HD_NONE, 2}, {5, HD_NONE, 2}, {2, HD_NONE, 2}, {3, HD_NONE, 2}, {0, HD_NONE, 2}, {3, HD_NONE, 2}, {4, HD_NONE, 2}}},
    {"ANDALUS", 8, {{5, HD_NONE, 4}, {4, HD_NONE, 4}, {3, HD_NONE, 4}, {2, HD_NONE, 4}, {5, HD_NONE, 4}, {4, HD_NONE, 4}, {3, HD_NONE, 4}, {2, HD_NONE, 4}}},
    {"FOLK", 8, {{0, HD_NONE, 4}, {3, HD_NONE, 4}, {0, HD_NONE, 4}, {4, HD_NONE, 4}, {0, HD_NONE, 4}, {3, HD_NONE, 4}, {4, HD_NONE, 4}, {0, HD_NONE, 4}}},
    {"MODERN", 8, {{0, HD_NONE, 4}, {2, HD_NONE, 4}, {5, HD_NONE, 4}, {3, HD_NONE, 4}, {0, HD_NONE, 4}, {2, HD_NONE, 4}, {5, HD_NONE, 4}, {3, HD_NONE, 4}}},
    {"SOLSTICE", 8, {{3, HD_RIGHT, 4}, {4, HD_NONE, 4}, {2, HD_RIGHT, 4}, {5, HD_RIGHT, 4}, {1, HD_RIGHT, 4}, {4, HD_UR, 4}, {0, HD_RIGHT, 4}, {0, HD_RIGHT, 4}}},
};
#define HGE_LEVELS 6u
static const char *const HGE_LEVEL_NAME[HGE_LEVELS] = {"A CHORD", "A PROGRESSION", "A MODIFIED CHORD", "A MODIFIED PROGRESSION",
                                                       "AN INTERVAL", "A WIDE INTERVAL"};

static struct {
    /* CHORD HIRO */
    uint8_t song, diff, speed;   /* the song, HGD_*, SPEED 50..100 */
    uint8_t running, done;
    uint8_t idx;                 /* the chord expected */
    uint32_t pos;                /* samples into the song */
    uint32_t next_at;            /* when the chord idx is due (samples into the song) */
    uint8_t hit;                 /* the chord idx was hit (or missed) already */
    uint8_t last;                /* HGR_*: the last rating (the display) */
    uint16_t score, combo, best, perfect, great, ok, miss;
    uint8_t sn, snote[HS_N];     /* the notes sounding */
    uint32_t soff;               /* when they end */
    /* EAR TRAINER */
    uint8_t level;               /* 0..5 */
    uint8_t q[4], qdir[4], qn;   /* the question: degrees (or the interval's degree), directions, their number */
    uint8_t ans;                 /* answers given so far */
    uint8_t phase;               /* 0 idle, 1 playing the question, 2 waiting, 3 showing the result */
    uint8_t play_i;              /* the chord of the question sounding */
    uint32_t phase_t;            /* samples left in the phase step */
    uint8_t result;              /* 1 right, 2 wrong (the display) */
    uint8_t wrong_was;           /* the degree it was */
    uint16_t streak, right, total;
    uint32_t rnd;
} hcg = {.speed = 100, .diff = HGD_MEDIUM, .rnd = 0x1234ABCDu};

static uint32_t hcg_rand(uint32_t n)
{
    uint32_t r = hcg.rnd;
    r ^= r << 13; r ^= r >> 17; r ^= r << 5;
    hcg.rnd = r;
    return n ? (r >> 7) % n : 0u;
}

static void hcg_notes_off(track_t *t)
{
    uint32_t i;
    for (i = 0; i < hcg.sn; i++)
        hc_note_off(t, hcg.snote[i]);
    hcg.sn = 0;
}

/* the chord of degree d with direction dir sounds on t for ms milliseconds */
static void hcg_chord(track_t *t, uint32_t d, uint32_t dir, uint32_t ms, char *name)
{
    const hc_trk_t *c = hc_of(t);
    uint32_t mask = hc_mask(t), q, i;
    int32_t r, roff, root, bass;
    hchord_t ch;
    hcg_notes_off(t);
    q = hs_degree_chord(mask, d % 7u, &r);
    root = HC_BASE + t->p[P_ROOT] + r + 12 * song.octave;
    q = hq_modify(c->mode, dir, q, &roff);
    root += roff;
    bass = c->bass == HB_OFF ? -1 : root - 24;
    while (bass >= 0 && bass < 24)
        bass += 12;
    hs_voice(&ch, root, q, bass, c->voices, 0);
    if (name)
        hs_name(name, (uint32_t)t->p[P_ROOT], root, q, bass);
    for (i = 0; i < ch.n && i < HS_N; i++) {
        hc_note_on(t, ch.note[i]);
        hcg.snote[hcg.sn++] = ch.note[i];
    }
    hcg.soff = hc.clock + ms * (FS / 1000u);
}

/* a single note of degree d (the EAR TRAINER's intervals) */
static void hcg_note(track_t *t, uint32_t d, uint32_t oct, uint32_t ms)
{
    int32_t r;
    hcg_notes_off(t);
    hs_degree_chord(hc_mask(t), d, &r);
    hcg.snote[0] = (uint8_t)clamp(HC_BASE + t->p[P_ROOT] + r + 12 * (int32_t)oct + 12 * song.octave, 0, 127);
    hcg.sn = 1;
    hc_note_on(t, hcg.snote[0]);
    hcg.soff = hc.clock + ms * (FS / 1000u);
}

/* ------------------------------------------------------- CHORD HIRO --- */
static uint32_t hcg_beat(void) { return beat_samples() * 100u / (hcg.speed ? hcg.speed : 100u); }

static void hcg_hiro_start(void)
{
    hcg.running = 1;
    hcg.done = 0;
    hcg.idx = 0;
    hcg.pos = 0;
    hcg.next_at = hcg_beat() * 4u;                      /* a bar's count-in */
    hcg.hit = 0;
    hcg.last = HGR_NONE;
    hcg.score = hcg.combo = hcg.best = hcg.perfect = hcg.great = hcg.ok = hcg.miss = 0;
}
static void hcg_hiro_stop(track_t *t)
{
    hcg.running = 0;
    hcg_notes_off(t);
}
static void hcg_rate(uint32_t r)
{
    static const uint8_t PTS[5] = {0, 100, 70, 40, 0};
    hcg.last = (uint8_t)r;
    if (r == HGR_MISS) {
        hcg.miss++;
        hcg.combo = 0;
    } else {
        hcg.combo++;
        if (hcg.combo > hcg.best)
            hcg.best = hcg.combo;
        hcg.score = (uint16_t)(hcg.score + PTS[r] + (hcg.combo > 4u ? 20u : 0u));
        if (r == HGR_PERFECT) hcg.perfect++; else if (r == HGR_GREAT) hcg.great++; else hcg.ok++;
    }
}
/* the chord idx is over: the next one's time */
static void hcg_hiro_advance(void)
{
    const hcg_song_t *s = &HCG_SONG[hcg.song % HG_SONGS];
    hcg.next_at += hcg_beat() * s->ev[hcg.idx].beats;
    hcg.idx++;
    hcg.hit = 0;
    if (hcg.idx >= s->n) {
        hcg.running = 0;
        hcg.done = 1;
    }
}

/* a chord key in CHORD HIRO: the degree against the chord due (within the window) */
static void hcg_hiro_key(track_t *t, uint32_t k)
{
    const hcg_song_t *s = &HCG_SONG[hcg.song % HG_SONGS];
    int32_t oct, d, w, dlt;
    uint32_t r;
    char name[12];
    if (!hcg.running || hcg.hit)
        return;
    d = (int32_t)(hc_degree_of_key(t, k, &oct) % 7u);
    w = (int32_t)(HGD_WINDOW_MS[hcg.diff % HGD_COUNT] * (FS / 1000u));
    dlt = (int32_t)hcg.pos - (int32_t)hcg.next_at;
    if (dlt < -w)
        return;                                          /* too early for this one: ignored */
    if (d != s->ev[hcg.idx].deg) {
        hcg_rate(HGR_MISS);                              /* the wrong chord: a miss, the chord stands */
        hcg.hit = 1;
        return;
    }
    dlt = dlt < 0 ? -dlt : dlt;
    r = dlt <= w / 4 ? HGR_PERFECT : dlt <= w / 2 ? HGR_GREAT : HGR_OK;
    hcg_rate(r);
    hcg.hit = 1;
    hcg_chord(t, s->ev[hcg.idx].deg, s->ev[hcg.idx].dir, hcg_beat() * s->ev[hcg.idx].beats * 1000u / FS, name);
}

/* ------------------------------------------------------ EAR TRAINER --- */
static void hcg_ear_question(void)
{
    uint32_t i, n = hcg.level == 1u || hcg.level == 3u ? 4u : 1u;
    static const uint8_t DIRS[4] = {HD_UP, HD_UR, HD_RIGHT, HD_DOWN};
    hcg.qn = (uint8_t)n;
    for (i = 0; i < n; i++) {
        hcg.q[i] = (uint8_t)(hcg.level >= 4u ? 1u + hcg_rand(6) : hcg_rand(7));
        hcg.qdir[i] = (uint8_t)(hcg.level == 2u || hcg.level == 3u ? DIRS[hcg_rand(4)] : HD_NONE);
    }
    hcg.ans = 0;
    hcg.result = 0;
    hcg.phase = 1;
    hcg.play_i = 0;
    hcg.phase_t = 0;
}
static void hcg_ear_start(void)
{
    hcg.streak = hcg.right = hcg.total = 0;
    hcg_ear_question();
}
static void hcg_ear_replay(void)
{
    if (hcg.phase == 2u) {
        hcg.phase = 1;
        hcg.play_i = 0;
        hcg.phase_t = 0;
    }
}
/* a chord key in the EAR TRAINER: the answer */
static void hcg_ear_key(track_t *t, uint32_t k)
{
    int32_t oct;
    uint32_t d = hc_degree_of_key(t, k, &oct) % 7u, want = hcg.q[hcg.ans], ok = d == want;
    char name[12];
    if (hcg.phase != 2u)
        return;
    if (hcg.level == 2u || hcg.level == 3u)
        ok = ok && hc.dir == hcg.qdir[hcg.ans];
    if (hcg.level >= 4u)
        hcg_note(t, d, hcg.level == 5u ? 1u : 0u, 400);
    else
        hcg_chord(t, d, hc.dir, 400, name);
    hcg.total++;
    if (ok) {
        hcg.right++;
        hcg.ans++;
        if (hcg.ans >= hcg.qn) {
            hcg.streak++;
            hcg.result = 1;
            hcg.phase = 3;
            hcg.phase_t = (FS / 1000u) * 900u;
        }
    } else {
        hcg.streak = 0;
        hcg.result = 2;
        hcg.wrong_was = (uint8_t)want;
        hcg.phase = 3;
        hcg.phase_t = (FS / 1000u) * 1400u;
    }
}

/* one block: the song's clock and misses; the question's playback and the result's pause */
static void hcg_tick(uint32_t n)
{
    track_t *t = TSEL;
    const hc_trk_t *c = hc_of(t);
    if (hcg.sn && (int32_t)(hc.clock - hcg.soff) >= 0)
        hcg_notes_off(t);
    if (c->play == HP_HIRO && hcg.running) {
        int32_t w = (int32_t)(HGD_WINDOW_MS[hcg.diff % HGD_COUNT] * (FS / 1000u));
        hcg.pos += n;
        if (!hcg.hit && (int32_t)hcg.pos > (int32_t)hcg.next_at + w) {   /* the window passed: a miss */
            hcg_rate(HGR_MISS);
            hcg.hit = 1;
        }
        if (hcg.hit && hcg.pos >= hcg.next_at + hcg_beat() * HCG_SONG[hcg.song % HG_SONGS].ev[hcg.idx].beats - w)
            hcg_hiro_advance();
    }
    if (c->play == HP_EAR && hcg.phase) {
        if (hcg.phase == 1u) {                           /* the question: each chord 700 ms, a 150 ms gap */
            if (hcg.phase_t <= n) {
                if (hcg.play_i < hcg.qn) {
                    char name[12];
                    if (hcg.level >= 4u) {
                        if (hcg.play_i == 0u)
                            hcg_note(t, 0, 0, 500);      /* the tonic first */
                        else
                            hcg_note(t, hcg.q[0], hcg.level == 5u ? 1u : 0u, 500);
                        hcg.phase_t = (FS / 1000u) * 650u;
                        if (hcg.play_i == 0u)
                            hcg.qn = 2;                  /* (two notes to play; one answer) */
                    } else {
                        hcg_chord(t, hcg.q[hcg.play_i], hcg.qdir[hcg.play_i], 700, name);
                        hcg.phase_t = (FS / 1000u) * 850u;
                    }
                    hcg.play_i++;
                } else {
                    hcg.phase = 2;
                    if (hcg.level >= 4u)
                        hcg.qn = 1;
                }
            } else {
                hcg.phase_t -= n;
            }
        } else if (hcg.phase == 3u) {
            if (hcg.phase_t <= n)
                hcg_ear_question();
            else
                hcg.phase_t -= n;
        }
    }
}

static void hcg_mode_left(track_t *t)
{
    hcg_hiro_stop(t);
    hcg.phase = 0;
}
