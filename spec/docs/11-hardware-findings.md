# 11 — Findings on the unit (2026-10-08)

The first hours on hardware, and what they changed. Tooling: the USB console (`hc` dumps the master
effects' state and the live track's voices with their sides; `hc enc ROLE STEPS` turns a panel knob),
and `probe_device.py` in the Windows folder: MIDI notes in, the unit's own USB audio stream captured
(the master output, as the DAC gets it), per-segment levels.

## Panning that followed the key

Report: notes panned left or right depending on the key. Cause: a paired part mixes its voices as
mid = (A + B) / 2 and side = (A − B) · 0.72, with each note's main voice on side A or B by note parity
and its detuned partner on the other. A note **without** a live partner then sits at about 86 % on
its side: the partner is not allocated when the 8-voice budget is spent (the console showed eight
main voices and no partner for a 5-note chord), is stolen first (steal class 2), or never exists for
a one-shot engine (the drum kit). Fix: `voice.c` renders a voice with no sounding partner into a
third buffer (`render_mono`) that `fx.c` adds to the mid only. Verified: equal L/R on the USB tap.

## Noise with the FILTER wheel

Report: the filter wheel adds a lot of noise. On the host the filter was clean at full level, and on
the unit the coefficients and states matched the host. The cause is precision: the master chain is
Q15, the master level is applied before the HiChord master effects, and the wheel used perform.c's
`pf_svf`, which shifts its input down 2 bits and truncates its state updates at Q13. At a quiet
MASTER the signal is a few hundred LSB and the filter's steps of 4 LSB (plus truncation limit cycles)
sit 30-40 dB under it: a hiss that follows the wheel. Fix: `hcfx.c` `hc_svf`, the same filter at Q21
with int64 products and rounding (states within ±2^24, inputs up to ±2^17 so master_out's soft clip
still sees everything). Host probe (`tests/hcfx_probe.c OUT MASTER_Q12`) at MASTER 256: highs cut
to a third at 64 and a thirtieth at 20 with no floor; the unit's USB tap shows the same proportions.

## Slash chords sounded as two chords

Report: two keys at once did not behave as a slash chord. With BASS set to SLASH the second key's chord
did take the first key's root as its bass and the name read "Em/C", but the first key's own chord kept
sounding under it: two chords. The HiChord's diagram of Am/C is C A C E, the bass and the chord only.
Now (`hc_slash_take`, hichord.c): when the chord key comes down, the held bass key's chord stops and
only its bass note remains; letting the chord key go while the bass key is held brings that key's
own chord back; letting the bass key go first re-voices the chord over its own root; a further key
is the new chord over the same bass. The home screen names the chord key, not the bass key.
BASS is OFF by default, as on the HiChord: KEY menu → BASS → SLASH turns it on.

## Inversions on the display

INVERT (F#3, tapped while a key is held) cycles the key's inversion; it now shows: the chord is named
over its lowest note when no bass voice is on (C, C/E, C/G, as written), and the degree line carries
"1ST INV" / "2ND INV" in every bass mode. With BASS ROOT or SLASH the bass voice is the lowest note
and names the chord (none, or /bass).

## The chord settings across power-off

The HiChord keeps its sound, effects, mode, inversions and chord locks across power-off and resets
only key, scale, octave and tempo. Ours started from the defaults at every boot, so every reflash
(a reboot) also dropped BASS back to OFF, which is what the unit reported while slash chords seemed
not to work. Now the live state is packed like a preset into the settings record (PER6, `hcl`),
saved 3 s after the last change (one flash erase per edit session) and restored at boot
(`hui_live_restore`, main.c). Key, scale, octave and tempo are Felucca's song and still reset.

## Drum loops were clicks

Report: DRUM LOOP mode gave only clicks. Reproduced on the host (`tests/drum_probe.c`: peaks near
18000 with an RMS of 440 per quarter, a crest factor of 40). The loop sent each hit's note-off in the
same instant as its note-on. A Felucca voice starts with its ADSR at zero and the drum's own hit only
takes over in `drum_amp` during the first rendered block, so a release before that block ends the
voice at once: a few samples. Pads were fine because a key is held. Now the loop's note-offs go out
one block later (`hcd_loop_release`); the host render then shows the hits ringing (RMS 1500-4100).

## Owner's choices and small UI fixes

- BASS defaults to SLASH (the HiChord: OFF): slash chords from the first power-on.
- The footer's action words sat 3 px under the keycaps' centre line (Felucca's cv_key_hint puts the
  word at the cap's y − 1); they now sit on it.
- The red keycaps (EDIT, REC) used near-white text; the panel LCD washes that out, so they use the
  dark ink like the yellow and green caps.

## Effect amounts on KNOB 4 (beyond the HiChord's device UI)

The HiChord device only cycles each effect's type; the amounts are its app's CCs, which we do not
speak. Now, in the SOUND menu with the REVERB, DELAY, CHORUS, FLANGER or TREMOLO row selected,
KNOB 4 sets the amount (the reverb, delay and chorus sends, the flanger's wet, the tremolo's depth;
1..127, 4 a click); the row reads "HALL 65", the footer shows the K4 "AMOUNT" hint, and a knob
turn on an OFF effect turns it on at its first type. Elsewhere KNOB 4 stays the tempo. The amounts
live in `hc_trk_t` (preset format HCP2: earlier P1..P4 and the live state read as empty once) and
travel with the presets and across power-off.

## Chords over drums

As on the HiChord: record the drum loop into a looper layer, play on the next. DRUM LOOP mode,
REC (armed), REC again (recording one loop; BARS on the LOOPER screen, or REC a third time to close a
free first layer), the layer plays and the live instrument moves to layer 2; ALGORITHM knob to PLAY
(the chord sound returns) and play. `tests/hui_test.c test_drums_under_chords` runs this path.

## The looper on the drum screens

To time the bounce, the DRUM and DRUM LOOP screens show a looper strip in place of the "REC: BOUNCE
INTO THE LOOPER" line: the four layers' dots (green playing, red recording, yellow armed; the live
one ringed), the live layer's state, and a bar of the loop with its bars ticked. While recording it
reads "REC  BAR n" and fills red; a free first recording fills against BARS (or the bar it is in).
`hui_shot` renders it as `mode_drumloop_rec`.

## Lists instead of cycling (owner's choice)

ENV and LFO no longer cycle: they open a page (ENVELOPE, LFO) listing the choices, SELECT moves the
choice and applies it as it moves, the same button or HOME closes it. The PRESETS and ALGORITHM
knobs show their list (SOUND, MODE) while they turn; it closes 1.5 s after the last turn or on any
button, which then acts as on HOME. One screen (`HU_PICK`, `hui_pick_*`, `hui_draw_pick`); shots
`pick_env`, `pick_sound`.

## Keys from the console

`hc key K 0|1` holds or releases key K (0 = F3 .. 26 = G5) through the keyboard scan (`hc_dbg_notes`
in hichord.c, ORed into the scan in seq.c keyboard_block), so chord gestures can be played on the
unit from a script and read back with `hc`. Checked after a reboot: `hc key 7 1` gives C with its
bass (24 48 52 55 60 67), `hc key 11 1` on top gives "Em/C" with 24 52 55 59 64 71 and no 48.

## Inversions stuck to the key

Report: an inversion stayed on the key after letting go, LOCK or not. The HiChord's rule for a
change sticking to a button is Chord Lock, so now a key's inversion is forgotten when the key is let
go (or when HOLD releases it) unless the key is locked. LOCK without a joystick direction on an
inverted key locks the plain chord with its inversion; LOCK again unlocks, and the inversion goes
with the next release.

## The chord on a piano (owner's choice)

While a chord sounds, HOME's bottom graphic shows the chord's notes on a four-octave piano (the
window starts at the C at or below the lowest note, moved up when the top note would not fit; a note
beyond it is a dot at that edge) in the degree's colour, the root notes dotted. At rest the FM-1 key
layout with its degree colours is back. `hui_draw_piano`; the README's home shot shows C7.

## The keyboard at rest is a legend

The key strip shown when nothing sounds now says what each key does: the degree's number on every
white key (over its colour bar), I, L and H on the INVERT, LOCK and HOLD keys, and a dot placed in
the joystick direction on each of the eight direction keys.

## KNOB 1-4: filter, resonance, attack, release (owner's choice)

KNOB 4 was the tempo (the MODE menu's TEMPO row and tap tempo remain). Now the four knobs are the
filter wheel, RESONANCE (new: the master SVF's damping k from the table's 1 down to 0.1, computed
from the k = 1 coefficient table, `hcfx_coef`; also a SOUND menu row; the HiChord has it only as
its app's CC 30), attack and release, and each turn shows its name and value in the header bar
("ATTACK 790ms"). KNOB 4 on an effect row still sets the amount. Preset format HCP3.

## Louder after the panning fix: 6 dB, clipping at half volume

Report: much louder, clipping at MASTER near half. The panning fix mixes a lone voice whole where
it used to be mixed at half, and in real playing most voices are lone: big chords spend the 8-voice
budget on notes, not partners, and MIDI-driven chords have no partners at all (a host probe with a
triad, mostly paired, had hidden this: +0.4 dB). The unit's USB tap showed the chord at 2.4× its
earlier level (RMS 436 vs 180). Now the chord layer's parts are mixed 6 dB down (`trk_hc`, fx.c
mix_part), so eight voices sum to twice full scale at most and a full chord stays clean with the
MASTER at half; the tap reads 218 again. The drum modes also cap the part level at 96 (−5 dB).
The looper replays every note at velocity 100, so bounced drum loops lose their accents: a known gap
(the event word has no room for velocity).

Note for the record: the knob remap commit did not link on the real toolchain (a 64-bit division in
the resonance coefficients needs a runtime helper the part lacks; the host has it), so the unit kept
the previous build and the knobs looked unchanged; fixed with 32-bit divisions. The build step's
output must be checked for the app line, not only for errors.

## The strumplate (owner's request, 2026-10-09)

The chord keys moved to the seven white keys at the left (F3..E4: the first is the tonic, sounding at
C3 as the C4 key did), and the nine white keys to their right (F4..G5) are a strumplate after the
Omnichord's, chords left and strum right: each plays one note of the chord last built, rising from
its root at C4, octave after octave (a triad over three octaves). A swipe strums the chord. The
owner's first cut had the chords in the middle (C4..B4, the HiChord's) with the plate split around
them; "the chord keys should be on the left". The DRUM pads, the loop styles and the step entry
count from the first key too. The plate follows the chord as it changes
and keeps the last chord once the keys are up (the piano then shows it dim, the plate note white);
before any chord it plays the tonic's. HOLD does not latch plate notes, DRONE does not keep them;
OCT- / OCT+ move them with the chords. The other modes (SEQ, DRUM, the games, the mixer) keep the
keys as they were. The HOME legend marks the plate keys with a bar that rises with the note.
hichord.c: hc_plate_of_key / hc_plate_note / hc_plate_on, tested in hichord_test plate().

## LEAD is a scale keyboard (owner's request, 2026-10-09)

First the display: in LEAD the HOME screen kept naming the chord and drawing all its notes while
only the root sounded (chord.c's one-voice branch never built the chord; hc.cur stale). Then the
owner's call: "in lead mode the strum thing should be disabled and it should just play quantized
notes from the scale all the way up". LEAD now has no chords and no strumplate: all sixteen white
keys walk the track's scale from the tonic at C4 (an octave above the chords), one note each, wrapping
by the scale's length (a pentatonic repeats every five keys). HOME names the note (D4) with its degree
(seven-note scales) and shows it alone on the piano; the legend numbers every key by its degree.
hc_lead_note in hichord.c; hc.lead_note / lead_deg carry the display. Then "i should be able to play
multiple notes at one time and it should display that": LEAD is polyphonic (the track POLY, not
LEGATO), HOME names every held note low to high (D4 F4 A4) and lights them all; the piano window
stands on the first key's octave so a note climbing the keys climbs the piano.

## ACOUSTIC GTR (owner's request)

A plucky acoustic guitar for leads: a new PHYS preset on the string model (STRC 38, just above the
curved-bridge zone: a nearly pure string; BRIT 66; DAMP 90, a 1.5 s ring; POS 22, picked near the
bridge; ACC 104; EXC 26, the pick's click), appended to the engine (10 presets) and to the end of the
sound list (37, so the saved sound indices keep). The physical models sit ~12 dB under Felucca's
other engines, so their sounds load at LEVEL 127 and skip the chord layer's 6 dB trim (three voices
cannot sum to eight). `tests/gtr_probe.c` renders a lead line on any sound to a WAV.

## Numbers

| | |
|---|---|
| CPU at idle (console `status`) | 28 % |
| Late audio blocks over the first minute | 2 (at boot) |
| USB audio tap, idle | RMS 0.6, peak 13 of 32767: silent |
| Install time | about 2 minutes over USB-MIDI |
