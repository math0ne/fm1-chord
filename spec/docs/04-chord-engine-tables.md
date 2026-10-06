# 04 — Chord engine tables (implementation data)

Data tables for `harmony.c` / `chordkb.c`. Semitones are relative to the chord root unless noted.
Where the HiChord's exact behaviour is unpublished, the choice is marked **(assumption)** so it can be
revisited against a real unit.

## 1. Scales

Index order matches HiChord CC 103. Pitch classes relative to the key tonic.

| # | Name | Degrees (semitones) | Notes |
|---|---|---|---|
| 0 | MAJOR | 0 2 4 5 7 9 11 | |
| 1 | MINOR | 0 2 3 5 7 8 10 | natural minor |
| 2 | HARM MIN | 0 2 3 5 7 8 11 | |
| 3 | MELOD MIN | 0 2 3 5 7 9 11 | ascending |
| 4 | MAJ PEN | 0 2 4 7 9 | 5 notes |
| 5 | MIN PEN | 0 3 5 7 10 | 5 notes |
| 6 | BLUES | 0 3 5 6 7 10 | 6 notes |
| 7 | DORIAN | 0 2 3 5 7 9 10 | |
| 8 | MIXOLYD | 0 2 4 5 7 9 10 | major with ♭7 |
| 9 | LYDIAN | 0 2 4 6 7 9 11 | |

## 2. Default chord per button (degree → root + quality)

For the seven-note scales, button *n* is the triad stacked in thirds on degree *n* (standard diatonic
harmonisation). Qualities derived; only MAJOR is confirmed by the manual.

| Scale | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| MAJOR | Maj | min | min | Maj | Maj | min | dim |
| MINOR | min | dim | Maj | min | min | Maj | Maj |
| HARM MIN | min | dim | Aug | min | Maj | Maj | dim |
| MELOD MIN | min | min | Aug | Maj | Maj | dim | dim |
| DORIAN | min | min | Maj | Maj | min | dim | Maj |
| MIXOLYD | Maj | min | dim | Maj | min | min | Maj |
| LYDIAN | Maj | Maj | min | dim | Maj | min | min |

**Pentatonic and blues (assumption)**: the manual says "Picking a scale relights the 7 buttons with a
different set of notes" and the chord table hints that MIN PEN button 6 plays Min6. Policy: buttons map
to the scale's notes in order, wrapping into the next octave for buttons beyond the scale length
(MAJ PEN: 1 2 3 5 6 1' 2'; MIN PEN: 1 ♭3 4 5 ♭7 1' ♭3'; BLUES: 1 ♭3 4 ♭5 5 ♭7 1'). Each root takes the
triad built from scale tones nearest a third and a fifth above it, falling back to Maj/min by the
third that is present, and to a power chord if neither third is in the scale (BLUES ♭5 root). Store
the result as an explicit 7-entry table per scale after listening tests on a real HiChord.

## 3. Chord types (intervals)

Index is the persisted `chord_type_t`. Append only.

| # | Name | Symbol | Intervals | 3rd | Ext slots (ext1, ext2) |
|---|---|---|---|---|---|
| 0 | Major | "" | 0 4 7 | 4 | — |
| 1 | Minor | m | 0 3 7 | 3 | — |
| 2 | Dim | dim | 0 3 6 | 3 | — |
| 3 | Flat5 | ♭5 | 0 4 6 | 4 | — |
| 4 | Aug | aug | 0 4 8 | 4 | — |
| 5 | Sus4 | sus4 | 0 5 7 | 5 | — |
| 6 | Sus2 | sus2 | 0 2 7 | 2 | — |
| 7 | Maj7 | maj7 | 0 4 7 11 | 4 | 11 |
| 8 | Min7 | m7 | 0 3 7 10 | 3 | 10 |
| 9 | Dom7 | 7 | 0 4 7 10 | 4 | 10 |
| 10 | Maj6 | 6 | 0 4 7 9 | 4 | 9 |
| 11 | Min6 | m6 | 0 3 7 9 | 3 | 9 |
| 12 | Maj9 | maj9 | 0 4 7 11 14 | 4 | 11, 14 |
| 13 | Min9 | m9 | 0 3 7 10 14 | 3 | 10, 14 |
| 14 | Dom7#9 | 7#9 | 0 4 7 10 15 | 4 | 10, 15 |
| 15 | HalfDim7 | m7♭5 | 0 3 6 10 | 3 | 10 |
| 16 | Dom9 | 9 | 0 4 7 10 14 | 4 | 10, 14 |
| 17 | Add9 | add9 | 0 4 7 14 | 4 | 14 |
| 18 | Add11 | add11 | 0 4 7 17 | 4 | 17 |
| 19 | Min11 | m11 | 0 3 7 10 14 17 | 3 | 10, 17 (9th dropped when only 2 ext slots) |
| 20 | Sus4_7 | 7sus4 | 0 5 7 10 | 5 | 10 |
| 21 | MinMaj7 | m(maj7) | 0 3 7 11 | 3 | 11 |
| 22 | Maj13 | maj13 | 0 4 7 11 14 21 | 4 | 11, 21 |
| 23 | Six9 | 6/9 | 0 4 7 9 14 | 4 | 9, 14 |
| 24 | Dom7b9 | 7♭9 | 0 4 7 10 13 | 4 | 10, 13 |
| 25 | Dom7alt | 7alt | 0 4 10 13 18 | 4 | 10, 13 (♭5 replaces 5th) **(assumption: ♭9 + ♭5)** |
| 26 | Maj7s11 | maj7#11 | 0 4 7 11 18 | 4 | 11, 18 |
| 27 | Dom13 | 13 | 0 4 7 10 14 21 | 4 | 10, 21 |
| 28 | Dim7 | dim7 | 0 3 6 9 | 3 | 9 (BORROW passing dim7) |

Families used by the modifier tables: **major family** = {Major, Maj7, Maj6, Maj9, Add9, Add11, Six9,
Maj13, Maj7s11, Aug, Flat5, Sus*}; **minor family** = {Minor, Min7, Min6, Min9, Min11, MinMaj7, Dim,
HalfDim7, Dim7}; **dominant** = {Dom7, Dom9, Dom7#9, Dom7b9, Dom7alt, Dom13, Sus4_7}.

## 4. Modifier tables (joystick directions → black keys)

Direction indices: 0 ↑, 1 ↗, 2 →, 3 ↘, 4 ↓, 5 ↙, 6 ←, 7 ↖. Input: the button's default (root, quality).
Output: (root offset, quality). `~` means "depends on the input family".

### DEFAULT (mode 0)

| Dir | Major family | Minor family | Dim (button 7) |
|---|---|---|---|
| ↑ | Minor | Major | Minor |
| ↗ | Dom7 | Dom7 | Dom7 |
| → | Maj7 | Min7 | HalfDim7 |
| ↘ | Maj9 | Min9 | HalfDim7 |
| ↓ | Sus4 | Sus4 | Sus4 |
| ↙ | Maj6 | Sus2 | Sus2 |
| ← | Dim | Dim | Minor |
| ↖ | Aug | Aug | Aug |

### EXTENDED (mode 1)

| Dir | Result |
|---|---|
| ↑ | Maj ↔ Min flip |
| ↗ | Dom9 |
| → | Add11 (on minor: Min11 **assumption**) |
| ↘ | Min11 |
| ↓ | Dom7#9 |
| ↙ | Add9 |
| ← | Sus4_7 |
| ↖ | HalfDim7 |

### CHROMATIC (mode 2)

| Dir | Result |
|---|---|
| ↑ | MinMaj7 |
| ↗ | Dom7alt |
| → | **key +1 semitone** (global, latches while held? No: HiChord modulates live; implement as a key change) |
| ↘ | HalfDim7 |
| ↓ | Maj13 |
| ↙ | Six9 |
| ← | **key −1 semitone** |
| ↖ | Dom7b9 |

### BORROW (mode 3): root offset + quality

| Dir | Root offset | Quality | Theory |
|---|---|---|---|
| ↑ | 0 | Dom7 | secondary dominant |
| ↗ | −2 | Dom7 | backdoor dominant |
| → | +1 | Dim7 | passing dim7 |
| ↘ | 0 | HalfDim7 | |
| ↓ | 0 | Maj ↔ Min flip | parallel |
| ↙ | 0 | Min7 if major family, Maj7 if minor family | borrowed 7ths |
| ← | −1 | Major | flat-side / Neapolitan |
| ↖ | +6 | Dom7 | tritone sub |

### OG (mode 4)

DEFAULT table, but Maj9/Min9 voice as "doubled 3rd" (Rev 2.8 add9 style: ext2 = 3rd + 12 instead of the 9th… **assumption**, exact OG voicing unverified).

## 5. Voicing: chord → 6 voice slots

```
slot 0 ROOT   = root
slot 1 THIRD  = root + intervals[1]          (3rd, or 2/4 for sus)
slot 2 FIFTH  = root + intervals[2]          (5, ♭5, #5)
slot 3 BASS   = bass mode OFF: silent
                ROOT: root − 24
                SLASH: held-bass-button root − 24 (or −12 if that would be < 24)
slot 4 EXT1   = root + ext1  (silent for triads unless doubling)
slot 5 EXT2   = root + ext2  (silent unless the chord has two extensions)
```

Root octave: `BASE_ROOT = 48` (C3) **(config constant; the companion app's example uses 36)**. Global octave −2…+2 and per-button octave shift add ±12 each. Clamp slots to 0…127.

**Voice Count** (CC 49 semantics): 8 = all six slots, triads double ROOT+12 into EXT1 and FIFTH+12 into
EXT2 **(assumption: "doubled notes")**, and every slot gets a detuned stereo partner; 4 = ROOT, THIRD,
FIFTH, BASS/EXT1, partners on; 2 = ROOT + THIRD, no partners; 1 = ROOT only, no partner.

**Stereo partner**: every sounding slot spawns a second voice at `+d` cents (d random in 5…15, sign
alternating per slot) panned opposite the main voice. Stereo OFF: no partner, centre pan.

## 6. Inversions

Applied to the upper structure (slots 0, 1, 2 and the extensions), never to BASS.

- Root: as built.
- 1st: lowest upper note + 12.
- 2nd: the two lowest upper notes + 12.

Per-button inversion is stored in the preset; CC 56 overrides all.

## 7. Voice leading (V.LEAD ON)

When a new chord is triggered and the previous chord is still known:

1. Build candidates: inversion ∈ {0, 1, 2} × octave shift ∈ {−12, 0, +12} of the upper structure.
2. Sort the previous upper notes and each candidate's upper notes ascending; pad the shorter list by repeating its top note.
3. Cost = Σ |candidate[i] − previous[i]|, +6 penalty if the candidate's lowest note is more than 7 semitones from the previous lowest.
4. Pick the lowest-cost candidate; ties prefer the one closest to the root position. BASS slot is never moved.

## 8. Chord naming

`root_name + quality_symbol [+ "/" + bass_name when SLASH and bass ≠ root]`. Root spelling: use flats
for keys F, B♭, E♭, A♭, D♭, G♭ and sharps otherwise. Display examples: "C", "Dm", "G7", "Em/C",
"F#m7b5", "B♭maj9".

## 9. Arp patterns

```c
typedef struct {
    int8_t  note[8][2];   /* slot role 0..5 (ROOT,3,5,7,9,11), -1 rest; two notes per step */
    uint8_t oct[8][2];    /* 0 = -1 oct, 1 = 0, 2 = +1, 3 = +2 */
    uint8_t amp[8][2];    /* Q8: 0..384 (0.0..1.5) */
    uint8_t len;          /* 2..8 */
    uint8_t chord_mode;   /* 0 ARP_ONLY, 1 CHORD_AND_ARP, 2 RHYTHM_AND_ARP */
} arp_pattern_t;
```

Built-ins: UP {R,3,5,7}, DOWN {7,5,3,R}, UP/DOWN {R,3,5,7,5,3} (len 6), DOWN/UP {7,5,3,R,3,5},
RANDOM (runtime shuffle of present slots), FINGERPICK (user/app-editable; default {R,5,3,5,7,5,3,5}
**assumption**). When a slot is absent in the current chord (triad has no 7th) the step plays the
nearest present slot below it **(assumption)**.

Rates (index order = HiChord): 1/1, 1/2, 1/4, 1/8, 1/16, 1/16T, 1/32, SWING8, SWING16. Swing = the
off-beat delayed by 1/3 of the step. Gate length = current Release time.

## 10. Drum loop format

```c
typedef struct { uint8_t step[16]; } beat_t;   /* bit0 kick, bit1 snare, bit2 closed hat,
                                                   bit3 tom, bit4 open hat, bit5 bell/cymbal */
beat_t DRUM_LOOPS[7][8];   /* 7 styles x 8 variations: ORIG, GHOST, BUSY, SYNC, FILL, HALF, DOUBLE, PERC */
```

Styles (3.0 list): ROCK, BREAK, DUB, FUNK, HOUSE, DEMBOW, SWING. The HiChord patterns themselves are
not public; author our own 56.

## 11. Envelope presets

| Index | Name | A ms | D ms | S % | R ms |
|---|---|---|---|---|---|
| 0 | LONG | 800 | 1000 | 70 | 2000 |
| 1 | SHORT | 100 | 100 | 100 | 1000 |
| 2 | SWELL | 800 | 300 | 80 | 2000 |
| 3 | PLUCK | 5 | 80 | 0 | 180 |
| 4 | TOUCH | 50 | 260 | 66 | 450 |
| 5 | SUSTAIN | 200 | 300 | 85 | 3000 |
| 6 | KEYS | 5 | 1200 | 55 | 700 |

## 12. Strum and repeat timing

Strum delays: SLOW 200 ms, MEDIUM 80 ms, FAST 40 ms between successive slots in order
ROOT → THIRD → FIFTH → BASS → EXT1 → EXT2 (slot order 1→6 per the manual). Repeat: 32-step gate at the
arp rate, 50 % duty, release clamped to 200 ms while active.

## 13. MIDI CC subset to honour (channel 1)

20/50 BPM, 21 key, 22 octave, 23 modifier mode, 24 sound, 25–28 ADSR, 29 cutoff, 31 reverb, 32 chorus,
37/38 delay, 41 reverb on, 42 chorus on, 43/44/55 glide, 45 bass, 46 play mode, 47 filter on, 48 LFO,
49 voice count, 56 inversion-all, 57/58/59 randomize, 60/61/62 FX modes, 91–93 tremolo, 103 scale,
104 voice leading, 106 stereo, 111 stereo detune. Echo on change. Everything else is optional.
