# 06 — Phase 1 implementation notes (2026-10-05)

Phase 1 of `03-firmware-spec.md` is implemented and tested on the host. Nothing has been flashed; the
FM-1 has not arrived. The firmware image builds for the chip at 481,168 B (Felucca 1.0.1 stock: 476,996 B).

## What exists

| File | Role |
|---|---|
| `firmware/src/harmony.c` | pure tables and functions: 29 chord qualities, the DEFAULT / EXTEND / CHROM / BORROW / OG modifier tables, scale-degree harmonisation, six-slot voicing, inversions, voice leading, naming |
| `firmware/src/chordmachine.c` | the key layer: DEGREE / PIANO layouts, modifier directions on the black keys, INVERT / LOCK / HOLD, BASS OFF / ROOT / SLASH, VOICES 8 / 4 / 2 / 1, re-voicing of held chords, the `hc` state struct |
| `tests/chordmachine_test.c` | 95 checks against the real keyboard, voice and MIDI code (in `tests/run_tests.sh`) |
| `tests/chordmachine_demo.c` | renders an 8 s progression through the real DSP to a WAV (`build/chordmachine_demo.wav`) |
| `tests/chordmachine_shot.c` | renders HOME and the CHORD page with a chord held to PPM/PNG |

## How it hooks into Felucca (the whole diff to upstream is 11 files, ~70 lines)

- `chord.c`: a new value of the CHRD parameter, **HI** (`CH_HI`, appended after POW so stored projects keep their meaning). `chord_make` diverts to `hc_make` for it. `CHORD_MAX` 4 → 6 (the fixed shapes keep their own `SHAPE_MAX 4`, so DIA/MAJ/… are bit-identical: the chord regression test passes). `chord_last` gains a `name[12]`.
- `seq.c` `keyboard_block`: in HI mode a black key is a modifier (`hc_black`), a white key's note is `hc_root_note` and `hc.cur_key` tells `chord_make` which key it builds for; `hc_block()` after the edges re-voices held chords and ends HOLD. Everything downstream (voice allocation, MIDI OUT, recording, the arp, LEDs, releases) is untouched Felucca code.
- `params.c`: `N_CHRD` gets "HI". `ui_graph.c`: the CHORD page shows the HI name. `ui_draw.c`: HOME's footer title shows the chord played last. `main.c`: `hc_init()` at power-on.
- `web/editor.html`, `web/test_web.mjs`: the editor's mirror of the CHRD names.
- `tests/chord_test.c`: one line, its expectations are 4-note arrays and `CHORD_MAX` is now 6.

Key and scale are Felucca's own ROOT and SCALE (all ten chord machine scales are in its 16). OCT± and TRN apply. MONO / LEGATO / UNISON voice modes give chord machine LEAD mode (root only) for free.

## Using it on the device (once flashed)

SCL twice → CHORD page → KNOB 1 CHRD = HI. Pick a POLY sound (the power-on ACID is LEGATO and will play roots only). White keys play degrees, C4 = I. Black keys: C#4 ↑, D#4 ↗, F#4 →, G#4 ↘, A#4 ↓, C#5 ↙, D#5 ←, F#5 ↖; F#3 INVERT, G#3 LOCK, A#3 HOLD. The `hc` settings (modifier mode, bass, voices, voice leading, layout) have **no UI yet** and are not saved: defaults are DEFAULT, OFF, 8, off, DEGREE.

## Verified

- `tests/run_tests.sh`: ALL HOST TESTS PASSED, including the 87 golden renders unchanged, the chord regression test, the UI layout lint over every screen and palette, and the web editor tests.
- `chordmachine_test`: every direction of every table against the chord machine manual's key-of-C examples; re-voicing while held (the notes that stay are not retriggered, MIDI OUT balanced); INVERT / LOCK / HOLD; BASS ROOT and SLASH ("Em/C"); VOICES; VOICE LEADING; PIANO layout; MIDI IN plays the degree's chord; DIA3 unchanged.
- `chordmachine_demo.wav`: C → Am → F → G → G7 (direction after the key) → Cmaj9 (direction first) → Dm → Dm/F (INVERT) → Gsus4 → G (direction let go) → Caug → C, peak −5.1 dBFS, 8 voices at most.

## Assumptions baked in (revisit against a real chord machine)

- Non-major scales: triads stacked on the scale degree. Pentatonics and blues: the roots walk the scale, the triad comes from the parent major or natural minor scale.
- VOICES 8 doubles the root and fifth an octave up on triads. MIN11 drops its 9th (two extension slots). DOM7alt = ♭5 + ♭9. OG = DEFAULT (no separate 9th voicing).
- Both layouts sound an octave below the printed key (`HC_BASE 48`: C4 key = C3 root), so BASS ROOT lands at C1 like the chord machine.
- SLASH: the first key held keeps its own chord; the next keys take its root as bass.
- Re-voicing restarts only the slots whose pitch changed.

## Known gaps, next

- No UI or persistence for the `hc` settings: phase 2 adds them as track parameters just before `P_E0` (Felucca maps stores by count) with a CHORD page.
- LEAD (MONO) shows no chord name on HOME (`chord_last` only records chords of more than one note).
- No stereo partner voices yet (`voice_t.pan`), no Strum / Drone / Repeat, no chord arp patterns, no event looper: phases 2–3.
- `NVOICE` is still 8: a BASS ROOT + VOICES 8 chord is six voices, two chords layered steal.
- Felucca's `fm1_input` debounce and the 10 kHz scan are unchanged; the black-key modifier order-independence depends on both edges arriving in the same or consecutive blocks, which the HAL guarantees.

## Commands (WSL, `~/fm1/fm1-chord`)

```sh
./build.sh                                   # firmware: build/felucca.fwsc (identity FM-1_900)
AC79_SDK=$HOME/fw-AC79_AIoT_SDK sh tests/run_tests.sh
cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/chordmachine_test tests/chordmachine_test.c -lm && build/host/chordmachine_test
cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/chordmachine_demo tests/chordmachine_demo.c -lm && build/host/chordmachine_demo build/chordmachine_demo.wav
cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/chordmachine_shot tests/chordmachine_shot.c -lm && mkdir -p build/chordmachine_shots && build/host/chordmachine_shot build/chordmachine_shots
```
