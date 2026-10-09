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

## Numbers

| | |
|---|---|
| CPU at idle (console `status`) | 28 % |
| Late audio blocks over the first minute | 2 (at boot) |
| USB audio tap, idle | RMS 0.6, peak 13 of 32767: silent |
| Install time | about 2 minutes over USB-MIDI |
