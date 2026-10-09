# 10 — Running in the FM-1 emulator (2026-10-08)

simonjohansson/fm1-emulator has two cores. Its only release so far (commit 81b9ed9, 2026-10-05) is
the Rust core, built for macOS, Linux and Windows. Its main branch since replaced that with a QEMU
core whose window, panel and audio are Cocoa-only, so it has no Windows or Linux build yet.

The released Rust core stops on our firmware with `unsupported instruction 0xeddc`, and stops the
same way on unchanged Felucca 1.0.1 (the emulator's compatibility table was made against 0.9-beta
and an older source build). Four patches in `tools/emulator/fm1-emulator-81b9ed9.patch` get both
running:

| Patch | What |
|---|---|
| `0xeddc` kind 1 | `h[++rB=rC] = rA`: the halfword pre-indexed store (the loads already existed) |
| `0xedd0..3` / `0xedd4..7` | the low two opcode bits extend the post-increment stride to a signed ten-bit value (`h[r8++=-2] = r9`) |
| `0xeb80..bf` | `[rA+off] += packed` with the ARM-style modified immediate the register forms already use, at a signed six-bit word offset (`[r0+0] += 0x40000000`) |
| USB SIE | five endpoints: Felucca 1.0.1's USB audio input is EP4 |

Each form was checked against the vendor disassembler and, for the immediates, against what the
toolchain's own encoder emits for `*p += constant`. The emulator's test suite passes with the patches.

`tools/emulator/run.sh [firmware.fwsc]` clones the emulator at that commit, applies the patch,
builds `fm1-ui` (needs cargo and the GUI packages from its README) and runs it. Under WSL the window
appears on the Windows desktop through WSLg; `LIBGL_ALWAYS_SOFTWARE=1` is set because WSLg has no
GPU driver for it. On Windows the same patched source builds natively with rustup and the Visual
Studio Build Tools (`cargo build --release --features gui --bin fm1-ui`). Verified: boot to the home
screen, the USB console on the terminal, a held white key showing its chord and degree colour.

## Knobs and audio (added to the patch set)

The released core had no rotary input and no audio output. The patch adds:

- **Knobs**: drag a knob up or down, or scroll over it. SELECT, ALGORITHM, PRESETS and KNOB1-4 are
  the matrix encoders (`panel.c` PANEL_DEFAULT: SELECT 0, ALGORITHM 1, PRESETS 6, KNOB1-4 2..5).
  Each click is a full quadrature cycle on the encoder's two matrix contacts, one state per 1.6
  million guest instructions so the firmware's scan sees every state twice. MASTER sets the ADC
  potentiometer (0..1023). Verified: four clicks on SELECT took the key from C major to E major.
- **Record WAV** (toolbar): the emulated DAC stream to a 16-bit stereo 44.1 kHz file beside the
  firmware. Felucca's output words are Q15 samples shifted left by OUT_SHIFT (7), so the recorder
  shifts them back; MASTER sets the level as on hardware.

Live playback is not offered on purpose: guest time comes from the instruction count, and the
interpreter runs about 20-30 million instructions a second against the 240 MHz part (600 M
instructions take 19 s in WSL, 28 s on Windows), so the firmware runs at a tenth of real time or
less and live audio would be stretched or gapped. The recording plays at pitch afterwards. The
author's QEMU core has host audio but is macOS-only so far.

A headless sweep (`fm1-emu boot --press COLUMN:ROW --limit 300000000` for all 66 matrix positions)
is the quick way to find further unsupported forms after firmware changes; `boot` now also prints
the emulated audio's word range on exit.
