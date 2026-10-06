# 03 — Firmware spec: HiChord-style chord instrument on the M-VAVE FM-1

Working name: `fm1-chord`. A Felucca fork that keeps Felucca's hardware layer, loader, storage, USB-MIDI
and FX, and replaces the instrument and UI with a HiChord-style chord-button paradigm.

Status: specification, nothing built. Everything below is scoped to what the FM-1 can actually do;
Section 9 lists where fidelity to the HiChord is reduced and why.

## 1. Goals and non-goals

**Goal.** Play the HiChord way on the FM-1: press one key, get the right diatonic chord in the current
key and scale; modify it momentarily with a second gesture; strum, arpeggiate, drone, repeat, sequence
and loop it; all of it as USB-MIDI notes too.

**Non-goals.** Mic features (vocoder, tuner, mic sampler), USB audio in phase 1, BLE-MIDI, TRS MIDI out
(the FM-1 has no TX), bit-exact HiChord sound. Not a HiChord emulator; a HiChord-paradigm instrument
built on FM-1 strengths (bigger screen, more keys, seven encoders, Felucca's engines).

## 2. Base and licensing

- **Base**: fork [hugelton/Felucca](https://github.com/hugelton/Felucca) at the latest tag (1.0.1 on 2026-10-05), the way X0X did: keep `firmware/hal/`, `loader/`, `usb.c`, `storage*.c`, `ota*.c`, `settings_persist.c`, `gfx.c`, `lcd.c`, `panel.c`, `fx.c`, `dsp.c`, `voice.c`, `mod.c`, `midi_*.c`, and the engines you want; replace `ui*.c`, `engines.c` selection, `seq.c`, `song_chain.c`, `perform.c`, `chord.c`.
- **License**: Felucca is GPL-3.0-only, so this firmware is GPL-3.0. Fine for a free release.
- **HiChord IP**: the firmware is closed and Pocket Audio claims a patent-pending chord mapping. Everything here is re-implemented from the public manual and the public companion-app protocol. Do not copy their firmware binaries, samples, sound names, or UI artwork into the repo. Degree-to-button chord playing itself has prior art going back decades (Omnichord, Suzuki QChord, Casio Chordana), but if this becomes public, keep the naming distinct and do not call it a HiChord clone.
- **Upstream etiquette**: Felucca issues are open; keep the `hal/` untouched so upstream fixes merge cleanly, and mirror `ENGINES[]` indices ("append, never reorder").

## 2a. What Felucca already covers versus what this fork adds

Felucca stock (1.0.1) already does: one-key in-key chords (`CHRD` = OFF / scale triads / scale sevenths /
fixed maj, min, 7, maj7, m7, sus4, power; max 4 tones), 16 scales with ROOT and a WHITE-keys quantize
mode, voicings CLOSE / OPEN / INV1 / INV2 / BASS, chord naming, a full arp (mode, rate, octaves, gate,
swing, probability, hold, order), a 64-step sequencer with chords, ties, accents, slides, chance, live
overdub and motion recording, song chain A–D, MONO / LEGATO / UNISON with glide, ADSR + LFO + 4-slot mod
matrix, chorus / delay / reverb sends, per-track distortion, limiter, performance FX (repeat, reverse,
filter sweeps, tape stop, freeze, harmonizer), an 8-lane drum engine, 13 sound engines, 32 presets and
4 projects, USB + TRS MIDI in with clock. Set `CHRD` = sevenths, `QNT` = WHITE, `VOIC` = BASS and you can
already play HiChord-style diatonic chords.

What this fork adds, in order of musical importance:

1. **Momentary chord modifiers** (the joystick layer): DEFAULT / EXTENDED / CHROMATIC / BORROW tables, 29 chord types, root-shifting borrowed chords, re-voicing while held, per-key inversion, chord lock, slash chords from two held keys. Felucca has no equivalent.
2. **Six-slot role voicing** (root, third, fifth, bass −2 oct, ext1, ext2) with voice-count levels, used by strum order, arp roles and the display. Felucca builds ≤4 close tones.
3. **Stereo partner voices** (per-voice pan on UNISON pairs). Felucca has detune but no pan spread.
4. **Voice leading** (smoothest-inversion search).
5. **Play modes** as a keyboard-level switch: Strum, Lead, Drone, Repeat, arp layering. Pieces exist, the mode concept does not.
6. **Six-track event looper** with a sound snapshot per track. Felucca records into one 64-step pattern per track.
7. **Degree-based chord sequencer** steps that re-harmonise on key change (thin layer over Felucca's sequencer).
8. **Drum loops** (56 patterns, data only) and auto-drum retrigger.
9. **UI rewrite**: HiChord home screen, three-menu structure, gesture combos. Largest by line count, smallest by risk.
10. **Games and randomize** (Chord Hiro, Ear Trainer): pure UI over the harmony module.

Roughly 70 % of HiChord functionality exists in Felucca in some form. Item 1 is what makes it feel like a
HiChord and is about a thousand lines plus tables; items 6 and 9 are where the hours go.

## 3. Control mapping

The FM-1 has 27 keys, 14 buttons, 7 encoders, a volume pot and a 240×240 colour screen. The HiChord has 7 chord buttons, 3 menu buttons, an 8-way joystick with click, and a wheel. The mapping below keeps every HiChord gesture reachable with the same number of hands.

### 3.1 The 27 keys

**White keys = chord buttons (scale degrees).** The physical layout is a piano, so make C4 degree 1 and
let the octave fall out of position. This gives 16 chord keys spanning two octaves and replaces the
HiChord's per-button octave feature with geometry.

| Key | F3 | G3 | A3 | B3 | C4 | D4 | E4 | F4 | G4 | A4 | B4 | C5 | D5 | E5 | F5 | G5 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Degree | 4 | 5 | 6 | 7 | **1** | 2 | 3 | 4 | 5 | 6 | 7 | 1 | 2 | 3 | 4 | 5 |
| Octave | −1 | −1 | −1 | −1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | +1 | +1 | +1 | +1 | +1 |

Degree is *always* relative to the selected key (C4 plays the I chord in any key), exactly like
HiChord button 1. A second layout, **PIANO**, where the white key's own letter name is the root and the
quality comes from the key/scale, is a setting for people who think in note names. Key LEDs show the
degree colour (I/IV/V one colour, ii/iii/vi another, vii° a third) in DEGREE layout.

**Black keys = joystick directions and chord gestures.**

| Key | C#4 | D#4 | F#4 | G#4 | A#4 | C#5 | D#5 | F#5 | F#3 | G#3 | A#3 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Function | ↑ | ↗ | → | ↘ | ↓ | ↙ | ← | ↖ | INVERT | LOCK | HOLD |

The eight directions run clockwise from Up, left to right across the central black keys, so the
modifier tables in `04-chord-engine-tables.md` apply unchanged. Behaviour:

- A modifier is **momentary** while held, like the joystick. Order-independent: hold modifier then
  press a chord key, or hold a chord key then press the modifier. Changing modifiers while a chord is
  held re-voices it: slots whose pitch is unchanged keep sounding, changed slots retrigger.
- Two modifiers held at once: the most recent wins (joystick can only point one way).
- **INVERT** (F#3): tap while a chord key is held cycles that key's inversion Root → 1st → 2nd. Stored per key in the preset.
- **LOCK** (G#3): tap while a chord key + modifier are held locks the modification into that key ("LOCKED" on screen). Tap again with the key held to unlock. Locked chords are pinned to the key they were locked in (3.0 behaviour).
- **HOLD** (A#3): sustain latch. Not a HiChord feature but the FM-1 has the key and it makes one-handed modifier play possible. Off by default in presets.
- **Slash chords**: with Bass = SLASH, the first held chord key is the bass, the second is the chord, as on the HiChord.

### 3.2 The 14 buttons

Felucca logical names (physical silkscreen may differ; use Felucca's panel calibration if a button is in the wrong place).

| Felucca name | HiChord equivalent | Function |
|---|---|---|
| HOME | — | Play screen. Press again toggles the big-chord view and the piano-roll view. |
| SCL | Gray | Key & scale menu. Hold + encoder: key / scale / octave. |
| FX | Yellow | Sounds & effects menu. |
| EDIT | Red | Mode & tempo menu. Triple-tap = tap tempo. |
| ENV | Gray+wheel / Yellow+wheel | Cycle envelope presets; hold + KNOB 2/3 = attack/release. |
| LFO | LFO setting | Cycle vibrato OFF/LOW/MED/HIGH. |
| GLO | settings | System settings (voices, stereo, bass, glide, V.LEAD, MIDI, speaker). |
| SAVE | Yellow+Red | Preset menu: hold = save to selected slot, tap = load. |
| ARP | Red+5 | Jump to Arp mode (tap again returns to Play). Hold + white keys 1–6 = pattern, 7 = layering. |
| SEQ | Red+7 | Jump to Sequencer mode. |
| PLAY | joystick click (transport) | Looper: play/pause all. In Sequencer: start/stop. |
| REC | joystick click (track) | Looper: arm → record → loop → off cycle on the selected track. Hold 0.7 s = clear track. |
| OCT− / OCT+ | Gray + U/D | Global octave −2…+2. Both together = randomize (sound in FX menu, all in SCL menu, pattern in EDIT menu). |

Hold-gesture equivalents of HiChord combos: SCL + ENC = key; EDIT + white key = quick mode (Play, Strum, Lead, Drone, Arp, Repeat, Drum); FX + white key = quick sound 1–7; SCL + EDIT = battery; SCL + EDIT held 5 s = factory reset; SCL + FX + EDIT held 5 s = enter Felucca's update loader.

### 3.3 Encoders and pot

| Felucca name | Home screen | In a menu |
|---|---|---|
| SELECT | select looper track 1–6 | navigate list / change value |
| ALGORITHM | play mode (Play, Strum, Lead, Drone, Arp, Repeat, Seq, Drum, Drum Loop, Mixer, games) | mode parameter (strum speed, arp rate, difficulty) |
| PRESETS | sound (the 39-ish list) | sound |
| KNOB 1 | filter cutoff (HiChord Red + wheel) | context |
| KNOB 2 | attack | context |
| KNOB 3 | release | context |
| KNOB 4 | BPM | context |
| Master pot | volume (hardware) | volume |

Encoders have no push; everything the HiChord does with "click" lives on REC/PLAY/OCT±.

### 3.4 Display (240×240)

Home screen regions, drawn with Felucca's strip canvas:

- Top band (0–40): key, scale, mode, BPM, battery, MIDI/USB indicator.
- Centre (40–160): chord name in the largest font ("Dm7", "Em/C"), degree numeral below, LOCKED badge, modifier arrow glyph while a modifier is held.
- Lower band (160–200): a 2-octave piano strip with sounding notes highlighted and the bass note marked.
- Bottom band (200–240): six looper track glyphs (empty outline / armed / recording / playing / muted), sequencer position when running.

Menus are full-height lists, 7 rows visible. Every menu times out to Home after 3 s of no input (HiChord behaviour).

## 4. Architecture

### 4.1 Module map

| Module | Status | Responsibility |
|---|---|---|
| `hal/*`, `loader/*`, `usb.c`, `ota*.c`, `storage*.c`, `settings_persist.c` | keep | hardware, update, flash |
| `gfx.c`, `lcd.c`, `icons.c`, `panel.c` | keep | drawing, panel mapping |
| `audio.c`, `voice.c`, `mod.c`, `dsp.c`, `fx.c` | keep + small changes (§4.3) | render, allocation, buses |
| `eng_analog.c`, `eng_fm6.c`, `eng_sample.c`, `eng_drum.c`, `drum_voice.c`, `eng_noise.c`, `eng_phys.c` | keep | sound sources |
| `eng_fm2.c` | new | cheap 2-op FM for the EPIANO/BELL/ORGAN/BRASS family |
| `eng_chord.c` | new | thin wrapper engine that owns a sound preset = {source engine, bass-slot wave, stereo detune} so one "sound" can use different waves on the bass slot like the HiChord |
| `harmony.c` | new | scales, chord types, modifier tables, voicing, inversions, voice leading, naming (data in `04`) |
| `chordkb.c` | new | key layout, modifier/INVERT/LOCK/HOLD gestures, slash logic, per-key octave and inversion state; emits chord events |
| `play_modes.c` | new | Play, Strum, Lead, Drone, Repeat state machines on top of chord events |
| `carp.c` | new | chord arpeggiator with HiChord pattern struct |
| `cseq.c` | new (replaces `seq.c`) | 16-step chord sequencer |
| `eloop.c` | new | 6-track event looper |
| `drumloop.c` | new | 56 beat patterns, auto-drum retrigger |
| `mixer.c` | new | track mute/solo/volume, metronome |
| `games.c` | new, phase 4 | Chord Hiro, Ear Trainer |
| `ui_*.c` | rewrite | screens above |
| `midi_control.c` | modify | HiChord CC map, chord-note output, per-track channels |
| `preset.c` | new | HiChord-style preset = full state snapshot |

### 4.2 Data flow

```
keys/buttons/encoders (ui_input, 60 Hz)
   └─> chordkb: (degree, octave, modifier, inversion, lock, slash) ─> harmony: chord_t {root, type, slots[6], name}
         └─> play_modes / carp / cseq / eloop  ─> note events (note, slot role, track)
               └─> voice.c trk_note_on/off on the track's part  ─> audio ISR render ─> fx buses ─> master
               └─> midi_control: USB-MIDI note out on the track's channel
```

A **chord event** is `{track, key_index, on/off, chord_t, timestamp}`. Everything downstream consumes
chord events, so the looper, sequencer and MIDI output never re-derive harmony.

### 4.3 Required changes to Felucca core

1. **Stereo partner voices.** HiChord width comes from each voice having a detuned, opposite-panned twin. Felucca UNISON already spreads fine detune; add an optional per-voice pan (`voice_t.pan`, Q15) applied in `track_render` and a `P_UPAN` spread parameter so UNISON×2 pans ±. Partner = Felucca unison voice. Cost: one multiply per sample per voice.
2. **Voice budget.** `NVOICE 8 → 16` shared. HiChord's full chord is 6 slots × 2 = 12 voices; the looper needs more. The 85 % shedding stays and is the safety net. Measure before raising further.
3. **Parts.** `NPART 4 → 7`: six looper/live tracks plus a fixed drum part. `track_t` grows linearly; it is a few KB each, well within the 336 KiB pool.
4. **Tempo-synced tremolo and flanger** in `fx.c` (chorus exists; flanger is the chorus delay line with shorter range and feedback). Reverb HALL/PLATE/AMBIENT = parameter sets over the existing room and spring algorithms, not new algorithms.
5. **Per-track MIDI channel** = track index + 1, drums on 10, fixed velocity 100 on output.

Nothing in `hal/` changes.

### 4.4 Memory and CPU budget (estimates, to be measured on hardware in phase 0)

| Item | Estimate | Basis |
|---|---|---|
| 12 analog voices (6 slots × 2) | ~40–55 % | Jangada: 8 supersaw voices = 55 %; a single saw/sine voice is far cheaper than a supersaw |
| 12 FM-2 voices | ~35–45 % | 2 sine lookups + envelope per sample |
| 12 sample voices | ~25–40 % | Felucca `eng_sample` with linear interpolation |
| chorus + delay + reverb buses | ~15–20 % | Felucca already runs all three |
| UI + input + USB | ~5 % | 60 Hz, strips only on change |
| Six looper tracks replaying different sounds | over budget | this is the gap, see §9 |

Flash: Felucca 1.0.1 fits the 568 KiB app window with 13 engines; drop the engines you do not use
(formant, grain, wheel, trio, phase, lofi, slice) to make room for sample data. RAM: event looper ≈ 6 ×
512 events × 8 bytes = 24 KiB; drum patterns < 1 KiB; chord tables < 2 KiB.

## 5. Functional spec by feature

### 5.1 Harmony (`harmony.c`)

- `chord_t harmony_build(key, scale, degree, octave, modifier_mode, direction, inversion, voice_count, bass_mode, slash_degree)` returns six slot pitches (−1 = silent), the type index and a name string. Tables in `04`.
- V.LEAD ON applies the voice-leading search against the last chord played on the same track.
- CHROMATIC ←/→ modify the global key by ±1 while held and restore on release; the display shows the transient key.
- Names use Felucca's `chord_name()` style but extended to the 29 types.

### 5.2 Keyboard and gestures (`chordkb.c`)

- Reads `fm1_input_note_edges()` for the 27 keys and the three gesture black keys; debounce is the HAL's.
- State per white key: inversion (0–2), lock {on, root offset, type}, octave shift from layout.
- Emits chord-on when a white key goes down; chord-off when it comes up unless HOLD is latched or mode is Drone.
- Re-voices on modifier change while held.
- In Drum mode the white keys become seven pads (C4–B4 = pads 1–7, octaves duplicate), black keys become auto-drum rates.

### 5.3 Play modes (`play_modes.c`)

| Mode | Behaviour |
|---|---|
| PLAY | poly; chords layer; release follows envelope |
| STRUM | slots triggered in order ROOT, THIRD, FIFTH, BASS, EXT1, EXT2 with 200/80/40 ms gaps; a release during the roll stops it |
| LEAD | mono, root only, no stereo partner, legato with glide; new key cuts |
| DRONE | chord-off ignored; new chord crossfades over 300 ms; leaving the mode stops |
| REPEAT | 32-step gate at the arp rate, release forced to ≤200 ms |
| ARP | `carp.c`; layering ARP ONLY / CHORD+ARP (chord panned left, arp right) / RHYTHM+ARP (chord gated at the rate) |

### 5.4 Chord arpeggiator (`carp.c`)

Pattern struct and built-ins from `04 §9`. Rates from `04`. Clock source: internal BPM or incoming MIDI
clock. Each arp step emits a note event on the track with slot role so MIDI output and the looper see
it. Editable via a SysEx command mirroring HiChord `0A/0B` so the existing Companion App arp editor
could drive it (optional).

### 5.5 Sequencer (`cseq.c`)

16 steps max (start at 4; KNOB 4 or ENC changes length in steps of 4). Step = {degree, octave, type,
root offset, inversion, bass mode, length in beats}. Record by pressing white keys (and modifiers) on the
step; SEQ + PLAY runs it; leaving the mode while running bounces it to the next empty looper track
(HiChord behaviour). Sends MIDI.

### 5.6 Event looper (`eloop.c`)

Replaces the HiChord's audio looper with a six-track **event** looper, which the FM-1 can afford. Each
track = one part with its own sound snapshot (engine, preset, envelope, FX sends) and a list of
timestamped chord/note events.

- States OFF → ARMED → REC → LOOP, cycled by REC on the selected track; REC held 0.7 s clears; PLAY pauses/resumes all; OCT−+OCT+ held = clear all (confirmed on screen).
- ARMED + SELECT sets bar count 0 (free, length set at the end of the take) or 1–8 (fixed, four-beat count-in with metronome).
- Track 1 defines the loop length; other tracks start at track 1's next loop point and are forced to a multiple of its length.
- Playback replays events into that track's play-mode state machine, so arp/strum/repeat settings are re-applied, and MIDI goes out on channel = track.
- Recording the **sound** with the take: the snapshot is stored with the track so changing the live sound does not change old layers (the audio-looper property that matters most).
- Polyphony is the real constraint: when the sum of sounding voices exceeds the budget Felucca sheds the oldest; the Mixer lets you mute tracks. Voice Count per track defaults to 4 for looper tracks.

### 5.7 Drums (`drumloop.c`, Felucca `eng_drum`)

Drum part is fixed (part 7). Seven pads on white keys with Felucca/SLOOP kits remapped to the HiChord
pad order (Kick, Kick alt, Snare, CHH, Tom, Bell, OHH). Auto-drum: hold pad + black key rate. Drum Loop
mode: white key = style, OCT± = variation, SELECT = kit, REC = bounce one bar to a looper track. GM
notes out on channel 10.

### 5.8 Sounds and FX

- Sound list: one `sound_t` per entry = {source engine, engine params, bass-slot wave override, stereo detune cents, envelope preset, default FX}. Ship ~30 entries covering the HiChord categories (basic waves, 2-op FM family, sampled instruments from whatever samples you record or license yourself, hybrids). Felucca's three 80 KiB ADPCM sample slots limit how many sampled instruments ship in flash; most can be FM or analog.
- Global ADSR with the seven presets; KNOB 2/3 = attack/release.
- Filter: LP cutoff + resonance (Felucca per-track filter), separate HP toggle.
- FX menu: Reverb mode, Delay rate, Chorus mode, Flanger mode, Tremolo rate, LFO, Glide, Drive (Felucca DIST), Stereo, Bass, V.LEAD, Voice Count, Scale, Joystick (modifier) mode, Speaker, MIDI in/out, Layout (DEGREE/PIANO).
- CPU discipline copied from HiChord: a soft rule table (reverb refuses with delay + flanger while a drum loop plays, etc.) plus Felucca's hard shedding. Show "FX LIMIT" rather than glitch.

### 5.9 Presets

Eight slots (HiChord has four). A preset is the complete state: sound, FX, key, scale, octave, mode,
arp pattern, per-key inversions and locks, cutoff, kit, sequencer pattern, layout. Stored through
Felucca's `storage.c` in the VM area. Randomize: sound / all / pattern.

### 5.10 MIDI

- **Out**: every sounding note as USB-MIDI, velocity 100, on channel = live track (1 with empty looper, next empty track as layers fill, 7 when full), looper tracks on 1–6, drums on 10; clock 24 PPQN always; Start/Stop with looper/sequencer. Chord name as SysEx text (optional, for a future companion page).
- **In** (USB and TRS): notes play the current sound with velocity in chord modes, drum notes in Drum mode; CC 64 sustain; CC 120/123 panic; the CC subset in `04 §13`; follow external clock when present.
- Keep Felucca's editor SysEx (`F0 7D 46 4C …`) so the Felucca web installer/updater still works.

## 6. Build, flash, test (details in `05-dev-setup.md`)

- Build in WSL on Windows with Felucca's `build.sh`; produces `fm1-chord.fwsc`.
- Flash with Felucca's Web MIDI installer or `tools/fm1_install.py`. The loader writes app sectors only.
- **Before first flash**: build the FM-1-transporter recovery dongle and take a full 1 MiB dump of the stock flash.
- Unit tests on the host for `harmony.c` (the manual's key-of-C examples are the fixtures: button 1 + ↘ = Cmaj9, button 2 + ↘ = Dm9, button 7 + ↘ = Bm7♭5, BORROW button 5 + ↖ = D♭7, etc.).
- Host simulator (X0X/SLOOP style) rendering WAV for strum/arp timing and PNG for screens.
- The pi32v2 emulator boots Felucca builds for UI iteration without hardware (no audio).

## 7. Phased plan

| Phase | Deliverable | Exit criterion |
|---|---|---|
| 0 | Toolchain in WSL, stock Felucca built and flashed, recovery dongle, stock flash dump, CPU meter baseline | Felucca 1.0.1 rebuilt from source runs on your unit; recovery exercised once |
| 1 | `harmony.c` + `chordkb.c` + Play mode + home screen + one analog sound + MIDI out | Press C4 → C major chord sounds and appears as MIDI; modifiers, inversions, lock, slash work; host tests green |
| 2 | Strum, Lead, Drone, Repeat, Arp; ADSR presets; FX menu; stereo partners; sound list; presets | All six chord play modes; CPU under 70 % with 12 voices + reverb + delay |
| 3 | Sequencer, drums, drum loops, event looper, mixer, MIDI in/clock | Six-track jam with drums; looper replays with per-track sounds |
| 4 | Chord Hiro, Ear Trainer, randomize, polish, cpu1 investigation | Games playable; measured headroom documented |

Phase 1 is the proof that the paradigm works on this keyboard; everything else is additive.

## 8. Risks

| Risk | Mitigation |
|---|---|
| Bricking (single flash bank, CRC-only updater, one real brick report) | Recovery dongle before first flash; never touch `0x0000–0x3FFF`; keep Felucca's watchdog-guarded loader |
| CPU for six looper tracks with distinct sounds | Voice Count default 4 on looper tracks; cheap engines by default; shedding; cpu1 offload as a phase-4 research item |
| Toolchain is closed (JieLi clang 4.0.1) | Felucca's `get_toolchain.sh`; keep a copy of the toolchain archive |
| Upstream Felucca moves fast (daily pushes, 24 open issues) | Fork at a tag; keep `hal/` pristine; rebase monthly at most |
| Non-major-scale chord tables are assumptions | Verify against the real HiChord when it arrives, or accept the diatonic harmonisation as the design |
| Patent claim on chord mapping | Private project; if published, distinct naming and prior-art note |

## 9. Fidelity gaps versus the HiChord

| HiChord feature | FM-1 version | Reason |
|---|---|---|
| 6 × 20 s audio looper | 6-track event looper with per-track sound snapshots | 578 KB SRAM vs 64 MB SDRAM |
| 12 oscillators at 48 kHz float | 16 Q15 fixed-point voices at 44.1 kHz | pi32v2 at 240 MHz, single core in use |
| Mic, vocoder, tuner, mic sampler | none | no mic |
| TRS MIDI out | none (USB-MIDI out only) | no UART TX routed |
| 7 chord buttons + 8-way joystick | 16 chord keys + 8 modifier keys + 3 gesture keys | different, arguably better for two-handed play |
| 64×32 OLED | 240×240 colour TFT | better |
| ~20 sampled instruments in 8 MB flash | a few ADPCM samples in ~200 KiB; rest FM/analog | 1 MiB flash |
| Reverb HALL/PLATE/AMBIENT | parameter sets over room/spring | CPU and flash |
| USB audio | optional later (Melodee has UAC on Felucca) | scope |
| Companion App | not supported directly; Felucca's editor SysEx retained | different protocol |
| Battery life ~10 h | ~12 h | similar |
