/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 fm1-chord contributors */
/* The chord machine looper, as an event looper: each layer is one of Felucca's parts (its own sound), and holds
 * the notes that were played on it (after the play mode: a strum's roll, an arp's steps) with their time in
 * the loop. The first layer sets the loop's length (free: the second REC press, rounded to a beat; or 1..8
 * bars with BARS); the next layers record from the loop's start for one loop. A layer's states: OFF ->
 * ARMED (BARS can be set) -> REC -> PLAY -> OFF, REC held: cleared. The live instrument is the lowest layer
 * that is not playing yet (the UI moves song.sel there, hui.c); once every layer plays, the last one is
 * both. PLAY pauses / resumes the transport. A metronome clicks on the beats while recording (hcfx.c).
 * Included by chordmachine.c; runs in the audio ISR (hcl_tick from hc_tick). */
#define HCL_LAYERS NTRK
#define HCL_EVENTS 160u                                  /* 80 notes a layer; 2.5 KB in the pool */
enum { HLS_OFF, HLS_ARMED, HLS_REC, HLS_PLAY };
static const char *const HLS_NAME[4] = {"OFF", "ARMED", "REC", "PLAY"};
/* an event in 4 bytes: the time in the loop (24 bits of samples: 6 min), the note, on / off */
typedef uint32_t hcl_ev_t;
#define HCL_AT(e) ((e) & 0xFFFFFFu)
#define HCL_NOTE(e) (((e) >> 24) & 0x7Fu)
#define HCL_ON(e) ((e) >> 31)
#define HCL_EV(at, note, on) (((at) & 0xFFFFFFu) | ((uint32_t)(note) & 0x7Fu) << 24 | (uint32_t)((on) != 0) << 31)
static hcl_ev_t hcl_ev[HCL_LAYERS][HCL_EVENTS] __attribute__((section(".pool")));
static struct {
    uint32_t len;                /* the loop in samples, 0 = none recorded yet */
    uint32_t pos;                /* the transport's place in the loop */
    uint8_t playing;             /* the transport runs */
    uint8_t bars;                /* ARMED: 0 free, 1..8 bars */
    uint8_t sel;                 /* the layer the looper buttons act on */
    uint8_t metro;               /* the metronome: on by default */
    uint8_t advance;             /* a layer just started playing: the live instrument moves on (hui.c) */
    uint8_t any;                 /* any layer plays or records (the transport is meaningful) */
    uint32_t beat_pos;           /* samples into the beat (the metronome) */
    struct {
        uint8_t state;           /* HLS_* */
        uint16_t n;              /* events */
        uint32_t start;          /* REC: hc.clock when it started; free: the length is measured from here */
        uint16_t play_i;         /* PLAY: the next event to replay */
        uint32_t held[4];        /* the notes the replay holds now (a bit per note) */
    } l[HCL_LAYERS];
} hcl = {.metro = 1};

static int hcl_rec_on(uint32_t ti) { return hcl.l[ti % HCL_LAYERS].state == HLS_REC; }

/* a note of part t started (on = 1) or ended: recorded when its layer records (seq.c input_on / input_off) */
static void hcl_note(const track_t *t, uint32_t note, int on)
{
    uint32_t ti = trk_index(t);
    if (ti >= HCL_LAYERS || hcl.l[ti].state != HLS_REC)
        return;
    if (hcl.l[ti].n < HCL_EVENTS)
        hcl_ev[ti][hcl.l[ti].n++] = HCL_EV(hc.clock - hcl.l[ti].start, note, on);
}

static void hcl_layer_silence(uint32_t ti)              /* the notes a layer's replay holds end */
{
    uint32_t i;
    track_t *t = &trk[ti % NTRK];
    for (i = 0; i < 128u; i++)
        if ((hcl.l[ti].held[i >> 5] >> (i & 31u)) & 1u) {
            trk_note_off(t, i);
            midi_out_event(0x08u | (0x80u | trk_midi_ch(ti)) << 8 | i << 16);
        }
    memset(hcl.l[ti].held, 0, sizeof hcl.l[ti].held);
}

static void hcl_clear(uint32_t ti)
{
    hcl_layer_silence(ti);
    hcl.l[ti].state = HLS_OFF;
    hcl.l[ti].n = 0;
    hcl.l[ti].play_i = 0;
}

static uint32_t hcl_count(uint32_t state)
{
    uint32_t i, n = 0;
    for (i = 0; i < HCL_LAYERS; i++)
        n += hcl.l[i].state == state;
    return n;
}

static void hcl_clear_all(void)
{
    uint32_t i;
    for (i = 0; i < HCL_LAYERS; i++)
        hcl_clear(i);
    hcl.len = 0;
    hcl.pos = 0;
    hcl.playing = 0;
    hcl.any = 0;
}

/* the lowest layer not playing: where the live instrument goes (HCL_LAYERS - 1 when all play) */
static uint32_t hcl_live(void)
{
    uint32_t i;
    for (i = 0; i < HCL_LAYERS; i++)
        if (hcl.l[i].state != HLS_PLAY)
            return i;
    return HCL_LAYERS - 1u;
}

/* REC on the selected layer: OFF -> ARMED -> REC -> PLAY -> OFF */
static void hcl_rec_press(void)
{
    uint32_t ti = hcl.sel % HCL_LAYERS;
    switch (hcl.l[ti].state) {
    case HLS_OFF:
        hcl.l[ti].state = HLS_ARMED;
        break;
    case HLS_ARMED:
        if (!hcl.len || !hcl.playing) {                  /* the first layer (or the loop stopped): records now */
            hcl.l[ti].state = HLS_REC;
            hcl.l[ti].n = 0;
            hcl.l[ti].start = hc.clock;
            if (!hcl.len) {
                hcl.pos = 0;
                hcl.beat_pos = 0;
                if (hcl.bars)
                    hcl.len = hcl.bars * 4u * beat_samples();
            }
            hcl.playing = 1;
            hcl.any = 1;
        }                                               /* else: it waits for the loop's start (hcl_tick) */
        break;
    case HLS_REC:
        if (hcl.l[ti].n == 0u && hcl.len && hcl.l[ti].start != hc.clock) {
            /* nothing played: back to OFF */
        }
        if (!hcl.len) {                                  /* the free first layer: its length, rounded to beats */
            uint32_t q = beat_samples(), el = hc.clock - hcl.l[ti].start;
            hcl.len = ((el + q / 2u) / q) * q;
            if (hcl.len < q)
                hcl.len = q;
            hcl.pos = el % hcl.len;
        }
        hcl.l[ti].state = HLS_PLAY;
        hcl.l[ti].play_i = 0;
        hcl.advance = 1;
        break;
    default:                                             /* PLAY -> OFF: silent, kept (REC again: ARMED) */
        hcl_layer_silence(ti);
        hcl.l[ti].state = HLS_OFF;
        hcl.l[ti].n = 0;
        if (!hcl_count(HLS_PLAY) && !hcl_count(HLS_REC)) {
            hcl.len = 0;
            hcl.playing = 0;
            hcl.any = 0;
        }
        break;
    }
}

/* PLAY: the transport pauses / resumes (the notes held end) */
static void hcl_play_press(void)
{
    uint32_t i;
    if (!hcl.len)
        return;
    hcl.playing = (uint8_t)!hcl.playing;
    if (!hcl.playing)
        for (i = 0; i < HCL_LAYERS; i++)
            hcl_layer_silence(i);
}

/* the replay of layer ti over [pos, pos + n) */
static void hcl_replay(uint32_t ti, uint32_t pos, uint32_t n)
{
    track_t *t = &trk[ti % NTRK];
    uint32_t i;
    for (i = hcl.l[ti].play_i; i < hcl.l[ti].n; i++) {
        hcl_ev_t e = hcl_ev[ti][i];
        uint32_t at = HCL_AT(e), note = HCL_NOTE(e);
        if (at >= pos + n)
            break;
        if (at < pos)
            continue;
        if (HCL_ON(e)) {
            trk_note_on(t, note, 100);
            midi_out_event(0x09u | (0x90u | trk_midi_ch(ti)) << 8 | note << 16 | 100u << 24);
            hcl.l[ti].held[note >> 5] |= 1u << (note & 31u);
        } else {
            trk_note_off(t, note);
            midi_out_event(0x08u | (0x80u | trk_midi_ch(ti)) << 8 | note << 16);
            hcl.l[ti].held[note >> 5] &= ~(1u << (note & 31u));
        }
    }
    hcl.l[ti].play_i = (uint16_t)i;
}

/* one block: the transport, the layers' replay, the recordings' ends and starts, the metronome */
static void hcl_tick(uint32_t n)
{
    uint32_t i, q = beat_samples();
    if (!hcl.playing)
        return;
    if (hcl.len) {
        for (i = 0; i < HCL_LAYERS; i++) {
            if (hcl.l[i].state == HLS_PLAY)
                hcl_replay(i, hcl.pos, n);
            else if (hcl.l[i].state == HLS_REC && hcl.len && hc.clock + n - hcl.l[i].start >= hcl.len) {
                hcl.l[i].state = HLS_PLAY;              /* a fixed length ran out */
                hcl.l[i].play_i = 0;
                hcl.advance = 1;
            }
        }
        hcl.pos += n;
        if (hcl.pos >= hcl.len) {                        /* the loop's start: the layers from their first event */
            hcl.pos -= hcl.len;
            for (i = 0; i < HCL_LAYERS; i++) {
                if (hcl.l[i].state == HLS_PLAY) {
                    hcl_layer_silence(i);
                    hcl.l[i].play_i = 0;
                    hcl_replay(i, 0, hcl.pos);
                } else if (hcl.l[i].state == HLS_ARMED) {   /* its turn: one loop from here */
                    hcl.l[i].state = HLS_REC;
                    hcl.l[i].n = 0;
                    hcl.l[i].start = hc.clock + n - hcl.pos;
                }
            }
        }
    }
    hcl.beat_pos += n;                                   /* the metronome on the beats while a layer records */
    if (hcl.beat_pos >= q) {
        hcl.beat_pos -= q;
        if (hcl.metro && hcl_count(HLS_REC))
            hcfx.click = hcl.len && (hcl.pos / q) % 4u == 0u ? 2u : 1u;
    }
}
