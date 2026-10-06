# 09 — The CHORD engine: getting the sounds closer (2026-10-06)

The HiChord's companion app (public, unminified JavaScript) spells out how each factory sound is built:
every chord slot is one plain oscillator, or a two-operator FM pair, or a sample, and the bass slot and
the right-channel layer can take a different wave. That structure is reproduced by a new Felucca engine,
**CHORD** (`firmware/src/eng_hc.c`, engine 14):

| Parameter | What |
|---|---|
| WAVE | SINE, TRI, SAW, SQR (band-limited), FM, NOISE |
| BASS | the bass slot's wave: SAME, SINE, TRI, SAW, SQR (the HiChord: TRIANGLE under SINE, SINE under SAW) |
| LAYR2 | the stereo partner's wave (the HiChord's "layer 2"): SAME, a wave, or OFF (OCEAN PAD plays its left layer alone) |
| RATIO, DEPTH, DECAY | the FM modulator: its ratio in halves, its depth, how fast the depth decays |
| TONE | a one-pole low-pass on the top |

The engine knows which note is the chord's bass (`eng_hc_bass[]`, set by `hichord.c` for every chord it
builds), so BASS applies to that slot alone.

The sound list now puts the HiChord's own voices on it: SINE, SAW, TRIANGLE, SQUARE, E.PIANO, HX7 PIANO,
FM BELL, FM ORGAN, FM BRASS, SAW SQUARE, JUNO POLY, OCEAN PAD, WOBBLE BASS, PURE SINE. The sampled and
physical instruments (strings, piano, harp, vibes, organ, …) stay on Felucca's engines.

## What is still approximate

- The FM presets use ratios and depths chosen by ear from the names (E.PIANO 1:1, BELL 3.5:1, ORGAN 2:1).
  The HiChord's exact operator settings are not public; tune them against a real unit, one line each in
  `HC_ENG_PRESETS`.
- The sampled instruments are Felucca's CC0 samples or synthesised stand-ins. Getting them closer means
  recording or licensing single-note samples (Versilian's CC0 sets have strings, clarinet, cello, brass,
  harp, flute) and making room in flash by dropping Felucca engines the sound list does not use. Pocket
  Audio's own samples are inside their firmware image and are not to be copied.

The demo `tests/hichord_demo.c` now renders on E.PIANO.
