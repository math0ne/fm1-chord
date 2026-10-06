# 07 — Phase 2 implementation notes (2026-10-05)

Phase 2 of `03-firmware-spec.md` is implemented and tested on the host: the HiChord's settings, play
modes, effects, stereo voices, sound list and the whole colour UI with its three menus and presets.
Nothing has been flashed. Firmware image: 508,660 B (Felucca stock 476,996 B; the window is ~568 KiB).
RAM: 93.5 KB of 98.3 KB (4.8 KB headroom), pool 333 KB of 344 KB.

## What exists now

| File | Role |
|---|---|
| `firmware/src/hichord.c` | `hc_trk_t` (every HiChord setting of a track), `hc_apply` (projects them onto Felucca's parameters and buses), the play modes PLAY / STRUM / LEAD / DRONE / ARP / REPEAT on a sample clock (`hc_tick`), the arp patterns and rates, the gestures |
| `firmware/src/hcfx.c` | the master stage after the master level: FILTER wheel (low-pass), HI-PASS, FLANGER (4 modes), TAPE (LOFI / VINYL / TAPE), OUT LEVEL |
| `firmware/src/voice.c`, `fx.c` | STEREO partner voices (a detuned twin per note, the two sides mixed mid/side, width 0.7); bit-identical when off (the 87 golden renders are unchanged) |
| `firmware/src/hui.c` | the UI: HOME, the KEY (grey) / SOUND (yellow) / MODE (red) menus, PRESETS (green), tap tempo, randomize, the sound list, the hand-over to Felucca's UI |
| `firmware/src/settings_persist.c` | PER5: the four presets' HiChord state travels in the settings record |
| `tools/gen_aa_font.py`, `gfx.c` | a 44 px face "X" for the chord name (32 glyphs, 7.7 KB) |
| `tests/hichord_test.c` | 137 checks: chords, gestures, play modes, stereo, the master stage |
| `tests/hui_test.c` | 76 checks: every menu row, the knobs, tap tempo, presets, drawing, the hand-over |
| `tests/hui_shot.c` | renders 12 screens (`build/hui_shots/*.png`) |

## The controls (HiChord gesture → FM-1)

| HiChord | FM-1 |
|---|---|
| 7 chord buttons | the white keys, C4 = I (two octaves of degrees) |
| joystick 8 directions | C#4 ↑, D#4 ↗, F#4 →, G#4 ↘, A#4 ↓, C#5 ↙, D#5 ←, F#5 ↖ |
| chord + Yellow (inversion), chord + stick + Yellow (lock) | F#3 INVERT, G#3 LOCK; A#3 HOLD (latch) |
| Gray menu (Key & Settings) | **SCL**: KEY, OCTAVE, SCALE, LAYOUT, JOYSTICK, BASS, VOICES, VOICE LEAD, RANDOMIZE ALL |
| Yellow menu (Sounds & Effects) | **FX** (or GLO): SOUND, ENVELOPE, ATTACK, RELEASE, FILTER WHEEL, CUTOFF, HI-PASS, REVERB, DELAY, CHORUS, FLANGER, TREMOLO, LFO, GLIDE, DRIVE, TAPE, STEREO, SPEAKER, OUT LEVEL, MIDI IN, RANDOMIZE SOUND |
| Red menu (Modes & Tempo) | **EDIT**: MODE, TEMPO, STRUM SPEED, ARP PATTERN, ARP RATE, ARP LAYER, RANDOMIZE PATTERN. EDIT tapped 3× = tap tempo |
| Yellow + Red (presets) | **SAVE**: P1..P4 (OCT+ loads, SAVE saves) |
| joystick U/D in a menu, L/R value | SELECT = the row, OCT− / OCT+ = the value |
| joystick click (randomize) | OCT− + OCT+ together |
| Gray + U/D (octave) | OCT− / OCT+ on HOME |
| Red + wheel (cutoff), Gray + wheel (attack), Yellow + wheel (release) | KNOB 1, KNOB 2, KNOB 3 |
| Red + L/R (mode), Yellow + L/R (sound) | ALGORITHM, PRESETS knobs |
| BPM | KNOB 4 |
| ADSR preset cycle, LFO cycle | ENV, LFO buttons |
| Red + 5 (Arp) | ARP button toggles ARP mode |

HOME held one second opens Felucca's full synth UI underneath; SAVE with HOME held returns.

## What a setting does (the projection)

ENVELOPE → ATK/DEC/SUS/REL from the seven HiChord presets; GLIDE → Felucca GLIDE 0/30/55/85; LFO →
LFO→PITCH 0/2/4/7; TREMOLO → LFO→AMP 90 at the tempo division (one LFO per track: vibrato and tremolo share its
rate when both are on); DRIVE → DIST 0/28/56/90/127; CHORUS → the chorus send 40/60/85/110 and the bus rate/depth;
DELAY → the delay send 55 and DLY TIME; REVERB → the reverb send and the bus (ROOM/HALL/PLATE/AMBIENT on
the ROOM model with size/damp sets, SPRING on the spring model); MODE LEAD → LEGATO voice mode, else POLY;
STEREO → partner voices; FILTER / HI-PASS / FLANGER / TAPE / OUT LEVEL → the master stage of the live track.

## Sounds (36, our own names, Felucca engines)

SINE, SAW, TRIANGLE, SQUARE, E.PIANO, FM PLUCK, BELL, FM ORGAN, FM BRASS, STRINGS, CLARINET, CELLOS,
ACOUSTIC, BRASS, PIANO, VIBES, VIOLINS, VOX AHH, SAX, HARP, HUMMING, SYNTH BASS, ARCADE, FLUTE, SAW SQUARE,
JUNO POLY, OCEAN PAD, WOBBLE BASS, BUZZ ORGAN, ORGAN, HORNS, E GUITAR, KALIMBA, SAW BASS, SHIMMER, GOSPEL ORGAN.
Each carries a default envelope preset (KEYS for the keyboard-like ones).

## Deviations from the HiChord, deliberate

- No mic features (vocoder, tuner, mic sample): no mic. No TRS OUT (no TX). No USB AUDIO setting (Felucca's UAC is always on).
- SPEAKER is FLAT / LOWCUT / BASS+ (Felucca's speaker EQ) instead of a mute: the codec cannot mute the speaker in software.
- OG joystick mode = DEFAULT (the 2.8 doubled-3rd 9th voicing is not reproduced).
- TEMPO 40..240 (Felucca's range; the HiChord goes to 300).
- The looper, sequencer, drums, drum loops, mixer and the games are phase 3/4: PLAY / REC / SEQ say "LOOPER: SOON".

## Verified

`tests/run_tests.sh`: every Felucca test, the 87 golden renders bit-identical, plus `hichord_test` and
`hui_test`. The firmware compiles for the chip. Known cost: none measured on hardware yet; the host CPU
regression (instructions per sample) is unchanged for the stock presets.

## Next (phase 3)

The six-track event looper on PLAY / REC with per-track sound snapshots, the 16-step chord sequencer, drum
mode and the 56 drum loops on Felucca's DRUM engine, the mixer; then Chord Hiro, Ear Trainer. And on the
device: CPU headroom with 12 paired voices + reverb + delay, which decides whether NVOICE grows.
