# 02 — Chord machine functional reference

What a chord-machine style instrument does, as the target for this firmware. Compiled 2026-10-05
from public manuals, a public companion-app source and strings in public firmware binaries. The raw
extracts stay in `research/` (local only, not in the repo).

**Corrections to common assumptions**

- The chord machine is **closed source**. There is no firmware repo. The only public repo (github.com/chord machine/updater) is an Electron DFU wrapper. Everything here is behaviour observed from manuals and the app protocol, so an FM-1 port is a **clean-room re-implementation**.
- The MCU is **not a Teensy**. It is an Electro-Smith **Daisy Seed2 DFM**: STM32H750 Cortex-M7 at 400/480 MHz, PCM3060 codec, **64 MB SDRAM**, 8 MB QSPI flash. 32-bit float DSP at 48 kHz. That SDRAM is what makes its 6×20 s audio looper possible, and it is the main thing the FM-1 does not have.
- The maker of the reference instrument claims a "patent-pending chord mapping". See the licensing note in `03-firmware-spec.md`.

## 1. Hardware

| Item | Spec |
|---|---|
| Display | 64×32 OLED (~0.49"). Chord name, key, menus, looper state, battery, animated sound icons, oscilloscope (3.0). |
| Chord buttons | 7, two rows: bottom 1-3-5-7, top 2-4-6. Batch 1–3 analog, Batch 4+ I²C. Not velocity-sensitive. |
| Function buttons | 3: **Gray** (Key & Settings), **Yellow** (Sounds & Effects), **Red** (Modes & Tempo). |
| Joystick | 8 directions + click. |
| Wheel | One volume pot; doubles as cutoff / attack / release / mic gain / track volume while a function button is held. |
| Audio | Mono speaker (2 W), 3.5 mm stereo out. Batch 4+: jack switchable to TRS-A MIDI out. |
| USB-C | Class-compliant USB-MIDI + USB audio (48 kHz; 24-bit simultaneous in 3.0). |
| Mic | Batch 4+ only (vocoder, tuner, mic sampler). |
| Battery | 1000 mAh (B1–3) / 2400 mAh (B4+). Deep sleep at ~3 %. |

## 2. Chord engine

### 2.1 Buttons are scale degrees

Nashville number system, diatonic to the selected key and scale. Major scale, key of C:

| Button | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| Degree | I | ii | iii | IV | V | vi | vii° |
| Chord | C | Dm | Em | F | G | Am | Bdim |

### 2.2 Key, scale, octave, tempo

- **Key**: 12 keys, Gray + joystick L/R.
- **Scale** (10, CC 103 values 0–9 in this order): MAJOR, MINOR, HARM MIN, MELOD MIN, MAJ PEN, MIN PEN, BLUES, DORIAN, MIXOLYD, LYDIAN. "Each scale changes which 7 chords the buttons play." The per-degree chord qualities for non-major scales are **not published**; `04-chord-engine-tables.md` derives them.
- **Global octave**: −2…+2 (Gray + joystick U/D).
- **Per-button octave**: hold chord button + Gray = down (to −2), + Red = up (to +2). Saved in presets.
- **BPM**: 40–300, Red + Up then L/R. Tap tempo = tap Red 3+ times.

### 2.3 Joystick modifications (hold chord + push; momentary)

Five joystick modes (CC 23 = 0..4): DEFAULT, EXTEND, CHROM, BORROW, OG.

**DEFAULT**

| Dir | Result |
|---|---|
| ↑ | Maj ↔ Min |
| ↗ | Dom7 |
| → | Maj7 on major, Min7 on minor |
| ↘ | 9th (Maj9 / Min9; m7♭5 on the dim chord). In 2.8 this was add9. |
| ↓ | Sus4 |
| ↙ | Maj6 on major, Sus2 on minor |
| ← | Dim on major and minor; Min on the dim chord (3.0). |
| ↖ | Aug |

**EXTENDED**: ↑ Maj↔Min, ↓ Dom7#9, ← Sus4+7, → Add11, ↖ Half-dim7, ↗ Dom9, ↙ Add9, ↘ Min11.

**CHROMATIC**: ↑ Min(Maj7), ↓ Maj13, ← key −1 semitone, → key +1 semitone, ↖ Dom7♭9, ↗ Dom7alt, ↙ 6/9, ↘ ø7.

**BORROW** (changes root too): ↑ secondary dominant (same root, dom7), ↓ parallel Maj↔Min, ← flat-side major (root −1, major), → passing dim7 (root +1, dim7), ↖ tritone sub (root +6, dom7), ↗ backdoor dom7 (root −2, dom7), ↙ borrowed 7ths (major family → min7, minor family → maj7), ↘ half-dim7 (same root).

**OG**: as DEFAULT but 9ths use the Rev 2.8 doubled-3rd voicing.

28 chord types total: Major, Minor, Dim, ♭5, Aug, Sus4, Sus2, Maj7, Min7, Dom7, Maj6, Min6, Maj9, Min9, Dom7#9, Half-dim7, Dom9, Add9, Add11, Min11, Sus4+7, Min(Maj7), Maj13, 6/9, Dom7♭9, Dom7alt, Maj7#11, Dom13. Marketing: "84,000+ voicings = 28 types × 12 keys × 10 scales × inversions".

### 2.4 Inversions, voice leading, chord lock, slash chords

- **Inversions**: hold chord + Yellow cycles Root → 1st → 2nd per button, saved in presets. CC 56 sets all.
- **Voice Leading** (V.LEAD, CC 104): "Chord Buttons auto-pick the smoothest inversion — notes stay close instead of jumping."
- **Chord Lock**: hold chord + joystick direction + Yellow → "LOCKED"; that button permanently plays the modified chord. Repeat to unlock. 3.0: a locked chord stays pinned to the key it was locked in.
- **Bass** setting (CC 45): OFF → ROOT ("bass note 2 octaves below the chord root") → SLASH ("hold one Chord Button for the bass, press another for the chord. Screen shows Em/C").
- **Display**: chord name ("C", "Dm", "Em/C"), key, LOCKED, mode names.

### 2.5 Voice architecture ("Under the Hood")

12 oscillators as 6 stereo pairs. "Each main voice (oscillators 0–5) has a detuned partner (oscillators 6–11) panned opposite for stereo width" (±5–15 cents).

| Voice | Role |
|---|---|
| 1 | Root |
| 2 | Third |
| 3 | Fifth |
| 4 | Bass (root −2 oct, or slash note) |
| 5 | Extension (6th, 7th…) |
| 6 | Additional harmonics / fill (9th, 11th+) |

Companion-app default for C major 9: BASS C1 (24), ROOT C2 (36), 3RD E2 (40), 5TH G2 (43), EXT1 B2 (47), EXT2 D3 (50).

**Voice Count** (CC 49): 8 "thick, full chord with doubled notes", 4 "lighter chord", 2 "root + one other note", 1 "single note (monophonic)". Stereo OFF sums to mono. Multiple chord buttons held in Play mode all sound (layered); stealing policy unknown.

## 3. Synth engine

Engines, selectable per oscillator via the app (SysEx engineType 0–3):

- **Analog**: SINE, SAW, TRIANGLE, SQUARE (band-limited from 3.0).
- **FM**: 2-operator. Presets EPIANO, HX7, BELL, ORGAN, BRASS. Ratio/depth editable in the app.
- **Sample**: 16-bit PCM single-note recordings pitch-shifted ±24 semitones: STRINGS, CLARINET, CELLOS, ACOUSTIC (guitar), BRASS, PIANO, VIBRAPHONE, VIOLINS, VOX, WURLI, HARP, VIOLA, HUMMING, ROBBO, SHUTTER, FLUTE, COMBO/BUZZ ORG, ORGAN, TINE EP, HORNS, E GUITAR, GRANDPNO, plus MIC and USER.
- **Noise**: white/pink, filtered.
- **Hybrid**: per-oscillator mix (SawSquare, Ocean Pad = noise + FM, Juno Poly, Wobble Bass, Saw Bass = saw + sub-octave + glide).

Factory sound list (39 entries, firmware enum order): Sine, Saw (default), Triangle, Square, FM E.Piano, HX7 Piano, FM Bell, FM Organ, FM Brass, Strings, Clarinet, Cellos, Acoustic, Brass, Piano, Vibraphone, Violins, Vox Ahh, Wurli, Harp, Viola, Humming, Robbo, Shutter, Flute, Mic Sample, User Sample, SawSquare, Juno Poly, Ocean Pad, Wobble Bass, Pure Sine, RANDOM/MANUAL, Buzz Org, Organ, Tine EP, Horns, E Guitar, GrandPno; 3.0 adds Saw Bass. Basic-waveform presets use a different wave on the bass voice (SINE preset → bass is TRIANGLE). Quick select: hold Yellow + button 1–7.

**Envelope**: one global ADSR (CC 25–28). Presets:

| Name | A | D | S | R (ms) |
|---|---|---|---|---|
| LONG | 800 | 1000 | 70 % | 2000 |
| SHORT | 100 | 100 | 100 % | 120 (3.0: 1000) |
| SWELL | 800 | 300 | 80 % | 2000 |
| PLUCK | 5 | 80 | 0 % | 180 |
| TOUCH | 25 (3.0: 50) | 260 | 66 % | 450 |
| SUSTAIN | 200 | 300 | 85 % | 3000 |
| KEYS (3.0) | 5 | 1200 | 55 % | 700 |

Gray + wheel = attack 1–2000 ms (3.0: 0.1–5000), Yellow + wheel = release 1–5000 ms.

**Filter**: low-pass with on/off (CC 47), Red + wheel = cutoff 20 Hz–20 kHz; resonance and drive app-only; separate high-pass toggle.

**Effects** (device = toggle/cycle, depth via app CCs): Reverb OFF/ROOM/HALL/PLATE/SPRING/AMBIENT; Delay OFF/1/4/1/8/1/16/1/16T/1/32 BPM-synced; Chorus OFF/LIGHT/WARM/WIDE/LUSH; Flanger OFF/SLOW/JET/DEEP/METAL; Tremolo OFF + 5 rates; LFO vibrato OFF/LOW/MED/HIGH; Glide OFF/SHORT/MEDIUM/LONG; Drive OFF/TUBE/DRIVE/DIST/FUZZ (3.0); Tape OFF/LOFI/VINYL/TAPE (3.0); Stereo ON/OFF; Bass OFF/ROOT/SLASH; Vocoder OFF/MIC/LOOP; OSC View.

**Randomize**: joystick click in a menu. Sound menu = random waveform/osc config + FX. Key menu = everything incl. key, BPM, arp. Mode menu = random arp/sequencer pattern.

**Signal flow (3.0)**: 6 voices → ADSR → mix + bass → filter → delay → tremolo → vocoder → chorus → flanger → trance gate → drive → tape → reverb → looper → output.

**CPU budgeting on the real device**: reverb refuses to run with delay or flanger, nor with a drum loop playing, nor with chorus in Strum/Drum. "CPU PROTECT / FX BYPASSED" shutdown order: flanger, chorus, delay, tremolo, reverb, vocoder. Even the Daisy runs out of CPU, so the FM-1 will need the same discipline.

## 4. Play modes

Mode list (Red + L/R, 3.0 order): Play → Strum → Lead → Drone → Arp → Repeat → Sequencer → Drum → Drum Loop → Chord Hiro → Ear Trainer → Tuner → Mic Sample → Mixer. CC 46: 0 ONESHOT, 1 STRUM, 2 LEAD, 3 DRONE, 4 ARPEGGIO, 5 REPEAT, 6 DRUM. Quick select: hold Red + button 1–7.

- **Play**: polyphonic; press = chord, release = fade at Release; buttons layer.
- **Strum**: "Notes roll through all active oscillators (1→2→3→4→5→6) with the selected delay": Slow 120 ms (3.0: 200), Medium 80 ms (default), Fast 40 ms.
- **Lead**: monophonic, plays only the chord **root**; new note cuts previous; glide recommended; centred (no detune) in 3.0.
- **Drone**: chord sustains until mode change; new button crossfades.
- **Arpeggio**: 6 patterns (UP, DOWN, UP/DOWN, DOWN/UP, RANDOM, FINGERPICK), 9 rates (1/1, 1/2, 1/4, 1/8 default, 1/16, 1/16T, 1/32, Swing 8th, Swing 16th), 3 layering modes (ARP ONLY; CHORD+ARP = "full chord sustains in the left channel, arp in the right"; RHYTHM+ARP = chord pulses rhythmically under the arp). Gate = Release time. App pattern format: `fingerpickPattern[8][2]` note indices 0–5 (ROOT, 3rd, 5th, 7th, 9th, 11th) or −1 rest; `octaveMultipliers[8][2]` 0=−1 oct, 1=0, 2=+1, 3=+2; `amplitudeLevels[8][2]` 0.0–1.5; `patternLength` 2–8. Presets: UP = R,3,5,7; DOWN = 7,5,3,R; UP/DN len 6; PICK; RAND.
- **Repeat**: "32-step trance gate"; held chord stutters at the arp rates; release capped at 200 ms.
- **Drum**: buttons 1–7 = Kick, Kick alt, Snare, Closed HH, Tom, Bell/Ride, Open HH/Cymbal. Kits (Yellow + 1–6): Tight Kit, x0x Box (808), x9x Box (909), Lynn Kit, KR-78, Trap Box, + User Kit (7 pads ≤0.5 s). GM notes on ch 10: kick 36, snare 38, closed hat 42, tom 45, open hat 46, bell 56.
- **Auto-Drum**: hold a pad + joystick direction = retrigger at ↑1/4, →1/8, ↓1/16, ←1/32, ↗ swing 8th, ↘ swing 16th, ↙ 1/16T.
- **Drum Loops**: 7 styles × 8 variations = 56 patterns. Styles (3.0): Rock, Break, Dub, Funk, House, Dembow, Swing. Variations: Orig, Ghost, Busy hats, Syncopated, Fill, Half-time, Double-time, Perc. Button = style; hold + U/D = variation; L/R = kit; click = bounce to looper (1 bar). SysEx beat format: 16 step bytes, bit0 kick … bit5 cymbal.
- **Sequencer**: up to 16 chord steps (starts at 4; ±4 via joystick); each step stores root, chord type (hold 300 ms + joystick for mods) and bass mode; click = start/stop; leaving mode auto-bounces to the next empty looper track; sends MIDI.
- **Chord Hiro**: rhythm game, 10 songs (3.0), timing windows EASY ±200 / MEDIUM ±150 / HARD ±100 / EXPERT ±50 ms, PERFECT/GREAT/OK/MISS scoring, practice 50–100 % speed. Charts are Nashville strings like `1:4 5:4 6m:4 4:4` (degree+quality:beats).
- **Ear Trainer**: 6 levels (single triad; 4-chord progression; with modifications; intervals). Gray replays root. Streak tracking.
- **Tuner**, **Mic Sample**: mic-based, Batch 4+ (not portable to the FM-1).
- **Mixer**: buttons 1–6 mute/unmute looper tracks; hold + wheel = track volume; 7 = metronome; hold + Gray = solo; click = pause/resume all; Filter Wheel = master filter.

## 5. Looper and presets

Audio looper, **6 tracks × ~20 s in SDRAM**, no overdub, no undo. Joystick click cycles OFF → WAITING → RECORD → LOOP → OFF. In WAITING, L/R sets bar count 0 (free) or 1–8 (fixed, 4-beat count-in). Track 1 sets loop length; tracks 2–6 start at Track 1's next loop point and auto-sync; after a take the device advances to the next empty track. 3.0: hold click 0.7 s to clear; a transport "stop" slot pauses all (tape-stop). Loops are lost at power-off. From 3.0 each track also replays its **MIDI on channel = track number**.

Presets P1–P4 (Yellow + Red menu) store "100+ parameters": sound, effects, key, mode, arp pattern, octave shifts, inversions, chord locks, cutoff, kit, sequencer pattern, FM params, samples. Sound Library (3.0): 32 named slots.

## 6. UI flow

Boot: logo → firmware rev → current key. Idle: chord name large, key, mode, looper shapes, tempo. Auto-sleep after 30 s (not while charging/looping/droning).

Three menus (press to open, press again / play / timeout to close):

- **Gray**: L/R key; U/D octave; click = Randomize ALL. Combos: chord + Gray = per-button octave down; Gray + wheel = attack; Gray + 1–6 (in Arp) = pattern, + 7 = layering; Gray + Red = battery; Gray + Red 5 s = factory reset; Gray + Yellow + Red 5 s = DFU.
- **Yellow**: L/R browse sounds; Up = effects list, then settings (Voice, ADSR, Bass, Glide, Scale, V.Lead, Stereo, Joystick, Speaker, Out Lvl, MIDI Out, MIDI In, USB Audio, TRS Out, OSC View); click = Randomize sound; Yellow + chord = quick sound; chord + Yellow = inversion; chord + joystick + Yellow = lock; Yellow + wheel = release; Yellow + Red = presets.
- **Red**: L/R mode; Down = mode params; Up = BPM; ×3 taps = tap tempo; click = randomize pattern; Red + chord = quick mode; chord + Red = per-button octave up; Red + wheel = cutoff.
- Joystick click outside menus = looper.

Display strings seen in firmware: WAITING, RECORDING, PLAYING, HOLD=CLEAR, ALL CLEARED, REC ARMED, IN LOOP n, FX LIMIT / TURN 1 OFF, CPU PROTECT, FX BYPASSED, LOCKED, RANDOMIZED, SOLO Tn, TRACK n, BPM n, PRESET n, LOW BATTERY.

## 7. MIDI

**Out**: "Every chord sends real notes: the full voicing, bass included, in every mode." Up to 12 simultaneous notes. **Velocity fixed at 100**, note-off velocity 0. Channel: live playing follows the layer being built (ch 1 with empty looper, then the next empty track's channel, ch 7 when all six are full); looper tracks replay on ch 1–6; drums on ch 10. MIDI clock 24 PPQN always; Start/Stop with looper/sequencer. Follows incoming clock when MIDI IN is on.

**In** (3.0, default OFF): notes on any channel play the current sound with velocity in Play/Lead/Strum/Drone/Arp/Repeat; drum notes hit the kit in Drum mode; sustain CC 64; CC 120/123 panic; parameter CCs honoured.

**CC map** (channel 1): 20+50 BPM (hi<<7|lo), 21 Key 0–11, 22 Octave (0=+1, 1=0, 2=−1, 3=+2), 23 Joystick mode 0–4, 24 Sound index, 25–28 ADSR, 29 Cutoff, 30 Resonance, 31 Reverb amt, 32 Chorus amt, 33/34/51/52 load P1–4, 35/36/53/54 save P1–4, 37 Delay on, 38 Delay rate 0–5, 39 Flanger on, 40 Flanger depth, 41 Reverb on, 42 Chorus on, 43 Glide on, 44 Glide rate, 45 Bass mode 0–2, 46 Play mode 0–6, 47 Filter on, 48 LFO 0–3, 49 Voice count 0–3, 55 Glide mode, 56 Inversion-all 0–2, 57/58/59 Random sound/all/pattern, 60 Reverb mode, 61 Chorus mode, 62 Flanger mode, 89 Filter drive, 90 Hi-pass, 91–93 Tremolo on/depth/rate, 94–96 chorus/flanger LFO, 100 Reverb feedback, 101 Delay feedback, 102 Bass on, 103 Scale 0–9, 104 Voice leading, 105 Master gain, 106 Stereo, 107–110 chorus/flanger feedback+delay, 111 Stereo detune, 127 handshake. Device echoes CCs on change.

**SysEx** (manufacturer 0x7D): `01` oscillator config (slot, engine, source, pan, gain, detune), `02/03` state request/dump, `03–07` user sample INIT/DATA/COMMIT/DELETE, `0A/0B` arp pattern write/read (`len, mode, 16 notes, 16 octaves, 16 velocities`), `0B–0F` user kit, `13/14` hardware info, `1A` drum beat dump, `30–3F` sound library.

## 8. Firmware and versions

C++ on libDaisy (USB descriptor strings "DAISY_SEED MIDI Config"), arm-none-eabi-gcc, Daisy bootloader DFU. Versions: 1.4 (Mar 2025), 1.7 (MIDI overhaul, Strum, Lead, Chorus, Flanger, multi-track looper), 1.8, 1.9, 2.01, 2.4 stable (Jan 2026), 2.6.9, 2.7.1 beta (arp editor SysEx), 2.8 beta (Mar 2026), 2.9.1 beta (Aug 2026: six recorded instruments, true 9ths, BORROW, hold-to-clear), 3.0 beta rc68 (Sep 2026: multitimbral MIDI looping, MIDI in/clock follow, Drive/Tape, OSC View, sound library, Saw Bass, KEYS envelope). No third-party forks or mods exist.

## 9. Explicitly unknown

Per-degree chord qualities for the nine non-major scales; exact octave placement and doublings for 8-OSC mode; voice-stealing policy; FM operator ratios; reverb/chorus algorithm details; factory sample lengths; Chord Hiro charts beyond the Nashville strings found; the "HI-1" model code.

## Sources

- https://manual.chordmachine.shop/ (Rev 2.8 BETA manual), https://chordmachine.github.io/chordmachine-beta-updater/manual/ (Rev 3.0 BETA manual), https://images.equipboard.com/uploads/item/manual/159320/pocket-audio-chordmachine-manual.pdf (Rev 1.4)
- https://updater.chordmachine.shop/ , https://chordmachine.github.io/chordmachine-beta-updater/ (changelogs, firmware binaries)
- https://app.chordmachine.shop/ (Companion App source: `app.js`, `arp-editor.js`, `voice-cards.js`, `piano-roll.js`, `chord-visualizer-simple.js`)
- https://github.com/chord machine/updater
- https://chordmachine.shop/ , https://chordmachine.shop/pages/synth-enthusiasts
- https://daisy.audio/blogs/seeds-n-circuits/daisy-in-the-wild-chordmachine , https://docs.daisy.audio/hardware/Seed2-DFM/
- https://synthanatomy.com/2025/10/pocket-audio-chordmachine-a-pocket-sized-chord-machine.html , https://sonicstate.com/news/2025/10/20/chordmachine-synth-looper-chord-machine/ , https://pianoandsynth.com/chordmachine-the-delightful-pocket-sized-chord-synth/
- https://www.kicktraq.com/projects/chordmachine/chordmachine-pocket-chord-synthesizer/
- YouTube: BrendenAV full video manual (CUAIOcvDnLA), Blue Sirens hands-on (j_oVhePMZ0w)
