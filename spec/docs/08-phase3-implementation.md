# 08 — Phase 3 implementation notes (2026-10-05)

Phases 3 and 4 of `03-firmware-spec.md`: the looper, the sequencer, the drums and drum loops, the mixer
and the two games. With these every mode of the chord machine's list that the hardware allows is in:
PLAY, STRUM, LEAD, DRONE, ARP, REPEAT, SEQUENCER, DRUM, DRUM LOOP, CHORD HIRO, EAR TRAINER, MIXER
(TUNER and MIC SAMPLE need the mic). Nothing has been flashed. Firmware image: 523,536 B; RAM 93.9 KB
of 98.3 KB; pool 335.7 KB of 344 KB.

## What exists

| File | Role |
|---|---|
| `firmware/src/hclooper.c` | the event looper: 4 layers (Felucca's parts), free or 1..8-bar first layer, the next layers from the loop's start, REC cycles OFF → ARMED → REC → PLAY → OFF, REC held clears, PLAY pauses, the live instrument moves to the next empty layer (its sound and settings copied), a metronome while recording, replay with MIDI OUT on the layer's channel |
| `firmware/src/hcseq.c` | SEQUENCER (16 chord steps of a beat, keys write, F#3 rest, PLAY runs, leaving while it runs bounces it into the looper), DRUM (7 pads, AUTO-DRUM on a held direction, KIT), DRUM LOOP (7 styles × 8 variations derived from each style's ORIG), MIXER keys |
| `firmware/src/hcgame.c` | CHORD HIRO (10 charts, 4 difficulties, practice speed, PERFECT/GREAT/OK/MISS, score, combo) and EAR TRAINER (6 levels, streak, replay on F#3) |
| `firmware/src/hui.c` | the LOOPER screen (SEQ button), HOME bodies for every mode, the MODE rows SEQ LENGTH, DRUM KIT, LOOP STYLE, LOOP VARIATION, HIRO SONG, DIFFICULTY, PRACTICE SPEED, EAR LEVEL, the drum engine swap on entering a drum mode |
| `tests/hui_test.c` | 160+ checks over everything above |

## How the looper differs from the chord machine's

The chord machine records audio into 64 MB of SDRAM. The FM-1 has 578 KB, so each layer holds note events
(160 a layer, 2.5 KB in all) and replays them on its own part with the sound it was recorded with. Four
layers rather than six, because Felucca's project format fixes four parts. Each layer is a loop of
the first layer's length (the chord machine allows multiples). The controls are the chord machine's: REC (the
joystick click) cycles the layer, REC held clears it, PLAY is the transport, BARS is set on the LOOPER
screen, the metronome in the MIXER (key 7).

## Verified

- `tests/run_tests.sh`: every Felucca test and ours. The 87 golden renders are still bit-identical.
- Screens: `build/hui_shots/` has HOME in every mode, the menus, the looper.

## Still open

- The chord machine's Sound Library (32 app-made sounds) has no equivalent without the Companion App.
- Chord Hiro's charts are our own ten progressions (the chord machine's are not public).
- CPU on the device: unmeasured. Four layers replaying four-note chords with stereo partners is up
  to 32 voices against NVOICE 8; the shedding will bite. Raising NVOICE and using cpu1 are the first
  hardware tasks.
