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
GPU driver for it. Verified: boot to the home screen, the USB console on the terminal, a held white
key showing its chord and degree colour. The Rust core plays no audio and has no rotary input, so
the encoders and the sound are hardware-only for now.

A headless sweep (`fm1-emu boot --press COLUMN:ROW --limit 300000000` for all 66 matrix positions)
is the quick way to find further unsupported forms after firmware changes.
