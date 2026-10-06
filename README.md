# fm1-chord — a HiChord-style chord instrument firmware for the M-VAVE FM-1

Specification and research for a custom firmware that brings the Pocket Audio HiChord's one-key
diatonic chord paradigm to the M-VAVE FM-1 pocket FM synth, built on the Felucca open firmware.

Status (2026-10-05): **phases 1 to 4 implemented** (every HiChord mode the hardware allows, the full colour UI) in the Felucca fork at `~/fm1/fm1-chord` (WSL). Nothing flashed; the FM-1 unit has not arrived.

## Documents

| File | Contents |
|---|---|
| [docs/01-fm1-platform.md](spec/docs/01-fm1-platform.md) | FM-1 hardware (JieLi AC791N), stock firmware, every custom firmware and tool, why Felucca is the framework, its APIs and limits |
| [docs/02-hichord-reference.md](spec/docs/02-hichord-reference.md) | What the HiChord does: chord engine, modifiers, voicing, synth engines, modes, looper, menus, MIDI/SysEx, versions |
| [docs/03-firmware-spec.md](spec/docs/03-firmware-spec.md) | **The spec**: control mapping, architecture, module map, required Felucca changes, budgets, feature-by-feature behaviour, phases, risks, fidelity gaps |
| [docs/04-chord-engine-tables.md](spec/docs/04-chord-engine-tables.md) | Implementation data: scales, per-degree chords, 29 chord types, modifier tables, voicing slots, inversions, voice-leading algorithm, arp/drum/envelope tables, CC subset |
| [docs/05-dev-setup.md](spec/docs/05-dev-setup.md) | Toolchain (WSL), build, flash, recovery dongle, first-day checklist |
| [docs/06-phase1-implementation.md](spec/docs/06-phase1-implementation.md) | **Phase 1 done**: what was built, how it hooks into Felucca, how to use it, verification, assumptions, gaps |
| [docs/07-phase2-implementation.md](spec/docs/07-phase2-implementation.md) | **Phase 2 done**: settings, play modes, effects, stereo, the sound list, the colour UI with the three menus and presets; the control map; deviations |
| [docs/08-phase3-implementation.md](spec/docs/08-phase3-implementation.md) | **Phases 3 and 4 done**: the event looper, the sequencer, drums and drum loops, the mixer, Chord Hiro and the Ear Trainer |
| `spec/research/hichord/` (not in git: Pocket Audio material, local only) | Raw extracts: HiChord manuals (1.4 PDF, 2.8 and 3.0 text), updater changelogs, firmware string dumps, Companion App JS (CC/SysEx protocol) |

## The short version

- The product is the **M-VAVE FM-1**, not "M Wave". JieLi AC791N, two pi32v2 cores at 240 MHz, 578 KB RAM, 1 MiB flash, 27 keys, 14 buttons, 7 encoders, 240×240 TFT, USB-MIDI, TRS MIDI in only.
- **Felucca** (GPL-3.0, bare metal, from scratch) is the framework. SLOOP, X0X, Melodee and Jangada are all Felucca forks that swapped the instrument and UI. Fork it the way X0X did.
- The **HiChord is closed source** on an Electro-Smith Daisy (STM32H750, 64 MB SDRAM). This is a clean-room re-implementation from its public manuals and companion-app protocol.
- Mapping: **white keys = scale degrees** (C4 = I, two octaves of chord keys), **black keys = the eight joystick directions** plus INVERT, LOCK and HOLD. Encoders carry key/scale/mode/sound/cutoff/attack/release/BPM.
- The one thing the FM-1 cannot replicate is the 6 × 20 s **audio** looper; the spec replaces it with a six-track **event** looper that stores a sound snapshot per track.
- Build the **recovery dongle** (RP2040) and dump the stock flash before flashing anything.
