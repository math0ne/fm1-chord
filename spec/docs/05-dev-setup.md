# 05 — Development setup, flashing, recovery

Everything here is the Felucca workflow; verify against Felucca's `BUILDING.md` at the time you start,
because the project moves daily.

## 0. Building without the device

Almost everything is possible on the host:

- The JieLi toolchain is Linux x86-64 and runs natively in WSL (Ubuntu is already installed on this machine).
- [charlesvestal/fm1-x0x](https://github.com/charlesvestal/fm1-x0x) ships `host/x0x_host`, a native build of the whole firmware driven by a script (buttons, keys, knobs, MIDI in) that writes WAV and PNG screenshots, plus a browser emulator of the same firmware. Take X0X's `host/` harness and `tests/scenarios/` pattern into this fork.
- Felucca's and X0X's `tests/run_tests.sh` compare engine output and render hashes against golden files.
- [simonjohansson/fm1-emulator](https://github.com/simonjohansson/fm1-emulator) runs the real pi32v2 binary with LCD and keys (no audio). Needs Rust inside WSL.

What needs hardware: CPU load on pi32v2 (voice budget, FX headroom, looper limits), flashing and the update loader, the recovery dongle, encoder calibration, latency and speaker behaviour.

## 0b. First flash (2026-10-08)

The unit arrived already on Felucca (identity FM-1_900), so the stock dump of section 2 was moot;
the way back to stock is M-VAVE's official `FM-1.fwsc` through the same installer. v0.1 went on
from Windows: Python 3.12 (3.13 has no python-rtmidi wheel), `pip install mido python-rtmidi`,
`fm1_install.py fm1-chord-0.1.fwsc --yes` (about two minutes, identity FM-1_901 after the restart).
The console is a USB serial device (COM6, 115200): `status` showed 28 % CPU at idle, audio_late 2,
batt_raw 602. Recovery without a dongle: the boot guard enters uboot after two failed boots, and
OCT- + OCT+ held 5 s does the same; the installer then finishes the write.

**Factory reset** (added after the first flash): Felucca has none, and the settings record
(0xFC000, with the panel calibration and the P1..P4 presets), the 4 projects (0x97000..), the user
preset banks (0xDC000..) and the FM6 bank survive reinstalls. `factory_reset()` in main.c erases
both copies of every storage object (`st_wipe`, storage.c), clears the .noinit panel table and
reboots. Entry: HOME + SAVE held at power-on (3 s countdown on screen, letting go cancels) or the
console command `factory yes`. The uploaded samples (0xA0000..0xDBFFF) are left alone.

## 0a. Phase 0 status (done 2026-10-05, in WSL Ubuntu 24.04)

Layout in WSL (canonical from now on; the Windows folder is a mirror):

```
~/fm1/fm1-chord/        this repo (spec, later the firmware fork)
~/fm1/Felucca/          upstream, tag v1.0.1 (20c275e)
~/fm1/fm1-x0x/          upstream X0X, for the host simulator
~/fw-AC79_AIoT_SDK/     JieLi SDK, tag AC79NN_SDK_V1.2.1_2023-12-13 (1.1 GB)
~/.jieli/toolchain/     jieli-linux-toolchains-20250324.1, clang 4.0.1 for pi32v2
```

Results:

| Step | Result |
|---|---|
| `tools/get_toolchain.sh` | 25 MB download, no login needed |
| Felucca `./build.sh` | 6.5 s; `felucca.bin` 476,996 B; RAM 88,884 / 98,304; pool 329,952 / 344,064 |
| Felucca `tests/run_tests.sh` | ALL HOST TESTS PASSED, 2 min 10 s (needs `AC79_SDK=$HOME/fw-AC79_AIoT_SDK`) |
| `vendor/ac79/` (2026-10-06) | the three SDK files the package needs, vendored; `AC79_SDK=$PWD/vendor/ac79` works without the 1.1 GB checkout (CI uses it) |
| X0X `./build.sh` | 2.4 s; `x0x.bin` 580,068 B |
| X0X host simulator | builds and runs; `getting_started.x0x` passes every `expect` |

Gotchas:

- X0X `host/build_host.sh` uses `-std=c99`, which hides `CLOCK_MONOTONIC` on glibc. Build it with `CC="cc -D_POSIX_C_SOURCE=200809L" sh host/build_host.sh`. Worth a one-line upstream PR.
- `x0x_host SCRIPT OUTDIR` does not create OUTDIR; `mkdir -p` it first or the PNG/WAV writes fail silently.
- Python packages were installed with `pip install --user --break-system-packages` (Ubuntu 24.04 PEP 668).

Simulator script commands (top of `host/x0x_host.c`): `wait MS`, `press/release/tap BTN`, `key K down|up`, `tapkey K` (K = 0..26, `w0..w15` white, `b0..b10` black), `turn ENC N`, `spin ENC N`, `master N`, `midi B0 B1 B2`, `wav FILE` / `wavstop`, `shot FILE.png`, `leds`, `expect WHAT VALUE`, `reboot`.

## 1. Before the FM-1 arrives

1. **WSL** (Ubuntu) is already installed. Felucca builds natively on Linux x86-64; SLOOP's `INSTALL-SLOOP.bat` shows the WSL wrapper pattern.
2. (Done, see 0a.) Clone Felucca and fetch the toolchain:
   ```sh
   git clone https://github.com/hugelton/Felucca
   cd Felucca
   git checkout <latest tag>        # 1.0.1 on 2026-10-05
   ./tools/get_toolchain.sh         # downloads JieLi's closed clang 4.0.1 for pi32v2
   pip install pillow fonttools mido python-rtmidi
   ./build.sh                       # -> felucca.bin, loader/ota.bin, felucca.fwsc
   ```
   The build also needs three blobs from the JieLi SDK tag `AC79NN_SDK_V1.2.1_2023-12-13`: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`. `build.sh` documents where they go.
3. Run the host test suite and, if you base on X0X or SLOOP, their host simulator (WAV/PNG output).
4. Clone [simonjohansson/fm1-emulator](https://github.com/simonjohansson/fm1-emulator) (Rust). It boots Felucca builds with LCD and keys, no audio. Good for UI iteration.
5. **Order parts for the recovery dongle**: a Seeed XIAO RP2040 and a sacrificial USB-C cable. [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter) wires D+, D−, GND and talks to the mask-ROM bootloader.

## 2. First day with the unit

1. Play it stock. Note the firmware version (V15 expected).
2. Build the transporter dongle. **Dump the full 1 MiB flash** and keep it: `stock_v15_full.bin`. This is your way back no matter what.
3. Install stock Felucca with the Web MIDI installer (Chrome/Edge) or `tools/fm1_install.py`. Confirm it boots and plays.
4. Run Felucca's panel calibration (hold OCT buttons at power-on) if any encoder or button is mis-mapped on your unit.
5. Re-install M-VAVE V15 from `.fwsc` once, to prove the round trip works.
6. Practise one recovery with the dongle while nothing is at stake.

## 3. Daily loop

- Edit in `firmware/src/`, `./build.sh`, flash `.fwsc` with `fm1_install.py`. The loader writes `0x4000–0x93000` only, resumes after power loss, and is watchdog-guarded.
- Felucca's `fm1_guard.h` crash records survive reset; read them with the console SysEx after a hang.
- Keep CPU meter `cpu_q8` on screen in debug builds.

## 4. Flash safety rules

- Never write below `0x4000` (SPL and chip config).
- Never send `F0 22 24 35 7D F7` to stock V15 unless you intend to enter the mask-ROM bootloader.
- Keep the stock dump and the last known-good `.fwsc` on disk.

## 5. Useful references

- Felucca: README, BUILDING.md, `firmware/hal/*.h` (the HAL headers are the documentation), `web/EDITOR_PROTOCOL.md`.
- [AL-255/FM-1-RE](https://github.com/AL-255/FM-1-RE): `docs/architecture.md`, `docs/io/11-ota-protocol.md`.
- [ip2k/lunar-modulator](https://github.com/ip2k/lunar-modulator): `docs/01-hardware.md`, `DEVELOPERS.md` (best pin and register write-ups).
- [kagaimiq/jielie](https://github.com/kagaimiq/jielie) chip wiki, [jl-uboot-tool](https://github.com/kagaimiq/jl-uboot-tool).
- JieLi SDK: https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK
