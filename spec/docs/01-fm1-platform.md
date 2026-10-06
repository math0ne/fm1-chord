# 01 — The M-VAVE FM-1 platform

Research date: 2026-10-05. The product is the **M-VAVE FM-1** (not "M Wave FM1"). Pre-release listings
sometimes spell it "M-WAVE FM-1". It shipped mid-June 2026 at roughly $60–75 and is marketed as a
6-operator, 32-algorithm DX7-style FM synth. The stock engine is a port of Google's `msfa` (the core of Dexed).

## 1. Hardware

| Item | Finding |
|---|---|
| SoC | JieLi (杰理) **AC791N**, "WL82" family (SDK target `cpu/wl82`). Likely part marking AC7911B8, LQFP48. |
| CPU | 2 × **pi32v2** cores (JieLi proprietary 32-bit DSP ISA, saturating/SIMD ops, FPU). Stock runs UI/MIDI/USB/BLE on cpu0 and voice rendering on cpu1. **All open firmwares run on cpu0 only.** |
| Clock | **240 MHz** (24 MHz crystal; family max 320 MHz). |
| RAM | **578 KB on-chip SRAM**, ≈514 KB usable after 32 KB I-cache + 32 KB D-cache. Mapped at `0x01C00000`. No SDRAM. |
| Flash | **1 MiB SPI NOR** (Puya) on SPI0, XIP at `0x02000000`. No SD card. |
| Audio out | External I2S codec on ALNK0, 24-bit stereo, **44,117.6 Hz** (TIMER4 24 MHz / 544). Codec part number not identified. |
| Display | **1.54" ST7789V TFT, 240×240 RGB565**, SPI1 at ~12 MHz. Stock flushes in ten 240×24 strips. |
| Keys | **27 silicone keys (F3–G5) + 14 buttons** in an 11×6 diode matrix behind two 74HC595s. **No velocity, no aftertouch.** Each key has an LED. |
| Knobs | **7 rotary encoders** (quadrature decoded in software via the matrix) + **1 master volume pot** (SARADC, 10-bit). |
| Battery | Li-Po 3.7 V 2000 mAh, ~12 h. Voltage sensed on SARADC. |
| USB | USB-C, **full-speed device only** (MUSB-derived). Stock: composite USB-MIDI + UAC1 24-bit audio. **No USB host.** |
| MIDI | 3.5 mm **TRS MIDI IN only** (UART1 RX, optocoupler). **No TRS MIDI out.** |
| Wireless | BT 5.0 dual-mode on the die (stock uses BLE-MIDI). Wi-Fi 4 present, unused. No open firmware uses either. |
| Speaker | Mono, auto-muted by the headphone jack. |
| Debug | **No JTAG/SWD, no UART header, no recovery button.** USB only. |

### Boot, flash layout, flashing

| Offset | Content |
|---|---|
| `0x00000` | flash header |
| `0x000A0` | SPL `uboot.boot` |
| `0x038D0` | `isd_config.ini` (contains the chip key) |
| `0x04000` | app area (stock: JLFS container with chip-key-encrypted `app.bin`), ~0x8E59C |
| `0x94000` | VM key-value store |
| `0xEA000` | user patch storage |

- **Stock update path**: the M-UPGRADE desktop app sends a `.fwsc` package over USB-MIDI SysEx (header `F0 00 32 45 …`, 8-to-7-bit encoding). Metadata check, soft reset into the OTA loader (re-enumerates as `4D4A:4155`), 4 KiB sectors streamed. Integrity is **CRC-16/CCITT only, no signature**. This is why custom firmware is possible at all.
- **Bootloader is not locked.** The mask-ROM `WL80UBOOT1.00` is reachable by a USB_KEY bit-bang on D+/D- at power-on, or from stock V15 with SysEx `F0 22 24 35 7D F7` (dangerous: black screen until a valid image is written).
- **Custom firmwares never write `0x0000–0x3FFF`.** Felucca's loader writes only `0x4000–0x93000`.
- **Recovery**: [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter) (MIT): a Seeed XIAO RP2040 wired to D+/D-/GND dumps or writes the whole 1 MiB flash via `jl-uboot-tool` in about 3 s. There is at least one real brick report (Felucca issue #61, stuck in `WL80UBOOT` after a 1.0.1 update). **Build the dongle before flashing anything.**

## 2. Stock firmware

FreeRTOS-derived SMP kernel plus JieLi SDK libraries, USB composite stack, BLE-MIDI GATT, a UI menu framework and a fixed-point msfa/Dexed port: 6-op, 32 algorithms, 12 voices, 64-sample blocks. 128 presets, DX7 SysEx bank import, 6 FX, 7-mode arp, 16-step sequencer, USB + BLE + TRS MIDI.

Versions: V13 (early units), V14 (2026-07-06), V15 (2026-07-30). **Closed source.** M-VAVE publishes only `.fwsc` files, release notes and MIDI CC guides. No SDK, schematics or sources. The chip vendor's SDK ([fw-AC79_AIoT_SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK), Apache-2.0) is public, but the **pi32v2 compiler (clang 4.0.1) is closed** and is downloaded from JieLi.

## 3. Custom firmware ecosystem (as of 2026-10-05)

Everything is C99 bare-metal or JieLi-SDK, built with JieLi's closed clang for pi32v2. No Rust/Arduino/PlatformIO ports exist. The scene is days to weeks old.

| Firmware | Author | Repo | Base | License | Notes |
|---|---|---|---|---|---|
| FM-1+VA | Baud Girl | [baudgirl.com/work/FM-1+VA](https://baudgirl.com/work/FM-1+VA) | patched stock V15 binary | closed, free | first CFW, proved the installer path |
| **Felucca** | Leo Kuroshita (Hügelton) | [hugelton/Felucca](https://github.com/hugelton/Felucca) | **from-scratch bare metal** | **GPL-3.0** | v1.0.1 Oct 5; 13 engines; the platform everyone forks |
| Melodee | keremimo | [keremimo/melodee](https://github.com/keremimo/melodee) | Felucca fork | GPL-3.0 | added USB audio, TRS MIDI, clock, FM6, 808 kit |
| SLOOP | 3dSam | [isod89/sloop-fm1](https://github.com/isod89/sloop-fm1) | Felucca fork | GPL-3.0 | 4-track groovebox, 37 drum kits, Windows build via WSL `.bat` |
| X0X | Charles Vestal | [charlesvestal/fm1-x0x](https://github.com/charlesvestal/fm1-x0x) | Felucca fork | GPL-3.0 | "keeps the hardware layer, USB MIDI, update loader, web installer, flash storage; replaces the instrument". Browser emulator. Worst case ~102 % CPU. |
| Jangada | zednaked | [zednaked/jangada](https://github.com/zednaked/jangada) | Felucca fork | GPL-3.0 | supersaw VA; "8 voices of SUPER SAW: 55 % on the FM-1" |
| Groove OS | Peter Gombos | [groove-os.com](https://www.groove-os.com) | stock V15 | proprietary, $29 | 8 tracks, FM 12 + VA 8 voices |
| Lunar Modulator | ip2k | [ip2k/lunar-modulator](https://github.com/ip2k/lunar-modulator) | JieLi SDK (RTOS) | MIT | research stage, nothing has run on hardware; best written hardware docs |
| fm1-nes | Keitark | [Keitark/fm1-nes](https://github.com/Keitark/fm1-nes) | JieLi SDK | Apache-2.0 | board-support + NES example; depends on a private flasher |

Tooling and research repos: [AL-255/FM-1-RE](https://github.com/AL-255/FM-1-RE) (Ghidra RE, architecture doc, OTA protocol doc, `fm1_ota.py`), [aroum/fm1-custom-fw](https://github.com/aroum/fm1-custom-fw), [kurogedelic/FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter), [simonjohansson/fm1-emulator](https://github.com/simonjohansson/fm1-emulator) (Rust pi32v2 + LCD/keys emulator, boots Felucca; no audio yet), [kagaimiq/jielie](https://github.com/kagaimiq/jielie), [kagaimiq/jl-uboot-tool](https://github.com/kagaimiq/jl-uboot-tool).

## 4. Why Felucca is the framework

Nobody ships a thing named an SDK. Felucca is the de facto one: four independent forks (SLOOP, X0X, Melodee, Jangada) have swapped the instrument and UI while keeping its hardware layer. What it provides:

**HAL** (`firmware/hal/`): `fm1_audio.h` (I2S double-buffer DMA, `fm1_audio_init(buf, half_words, isr, prio)`), `fm1_input.h` (matrix scan at 10 kHz from TIMER5, `fm1_input_edges()`, `fm1_input_note_edges()`, `fm1_enc_take(e)`, `fm1_led_key()`), `fm1_lcd_hw.h` (SPI1 + DMA), `fm1_usb.h` (register-level MUSB device), `fm1_adc.h`, `fm1_flash.h`, `fm1_xip.h`, `fm1_uart.h`, `fm1_sys.h` (watchdog, `fm1_enter_update()`), `fm1_guard.h`, `crt0.S`, `app.ld`.

**Engine interface** (`src/core.h`):

```c
typedef struct {
    const char *name;
    const char *page_title[2];
    param_desc_t edit[8];            /* P_E0..P_E7 */
    const preset_t *presets; uint8_t npresets;
    uint8_t knob[4];
    uint8_t poly, sampled, keep, oneshot;
    void (*note_on)(struct track *t, voice_t *v);
    void (*render)(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m);
    int32_t (*amp)(struct track *t, voice_t *v, int32_t adsr);
    const param_desc_t *(*desc)(const struct track *t, uint32_t k);
    void (*block)(struct track *t);
    int32_t (*keys)(const struct track *t, uint32_t k);
    uint8_t ownenv;
    int (*done)(struct track *t, voice_t *v);
} engine_t;
```

Constants: `NVOICE 8` (shared budget across all parts), `NPART 4`, `NSTEP 64`, `HALF_FRAMES 128` (2.9 ms), Q15 fixed point. Engines render **mono** per voice; voices carry `ph[3]` phase accumulators and `s[8]` state. Registered in `src/engines.c` `ENGINES[]` ("append, never reorder": indices are persisted). `eng_noise.c` is the minimal example.

**Voice allocation** (`src/voice.c`): `trk_note_on(track_t*, note, vel)`, `trk_note_off(track_t*, note)`, `trk_all_off()`. POLY steals oldest released, then oldest extra UNISON voice, then oldest held non-lowest. ROTATE round-robin. UNISON spreads fine detune linearly (up to ±40 cents); **no pan spread**. Control rate: once per 32-sample block.

**Chords** (`src/chord.c`): already has `CH_OFF, CH_DIA3, CH_DIA7, CH_MAJ, CH_MIN, CH_DOM7, CH_MAJ7, CH_MIN7, CH_SUS4, CH_POW`, voicings `VC_CLOSE, VC_OPEN, VC_INV1, VC_INV2, VC_BASS`, diatonic building from the track's scale, `chord_make()`, `chord_name()` (e.g. "Cm7", "F#m7b5"). Max 4 tones. Driven by `P_CHRD` / `P_VOIC`.

**Audio** (`src/audio.c`): per-half-buffer ISR → `mix_block` (events → parts → dist → level/pan → sends → buses → master). CPU meter `cpu_q8`; **voice shedding when two consecutive halves exceed 85 %**.

**FX** (`src/fx.c`): per-track distortion; three mono send buses: chorus (2048-sample modulated delay), tempo delay (65,536 samples ≈1.49 s, feedback LPF), reverb (Freeverb-style room; spring). Master peak limiter, DC blocker, low-cut, BASS+ enhancer.

**Display** (`src/gfx.c`, `lcd.c`): no full framebuffer; strip canvas up to 240×124 (`cv_begin/cv_rect/cv_text/cv_rrect/cv_blit`), palettes/themes, antialiased Inter Tight font pipeline.

**UI** (`src/ui*.c`, `panel.c`): declarative `PAGES[]` table, ~60 Hz main loop with 15 ms pacing. `ui_input()` reads `fm1_input_edges(0)` and `fm1_input_note_edges()`; `kb_map(t, k)` maps key index to a MIDI note; `btn_hold()` with a 700 ms hold threshold.

**Panel names** (`src/panel.c`): buttons `FX, SCL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC, OCT-, OCT+`; encoders `SELECT, ALGORITHM, PRESETS, KNOB 1..4`. Physical positions are a "best guess, overridable by HARDWARE CALIBRATION" (hold OCT buttons at power-on).

**MIDI** (`src/midi_control.c`, `midi_uart.c`, `midi_clock.c`, `usb.c`): USB-MIDI + TRS in; notes/CC/bend/pressure/RPN/clock; channel→track map.

**Storage/OTA** (`storage.c`, `ota.c`, `loader/`): RAM-resident update loader speaking the stock M-UPGRADE SysEx protocol, writes only app sectors, watchdog-guarded, resumable.

**Editor protocol** (`web/EDITOR_PROTOCOL.md`): SysEx `F0 7D 46 4C <cmd>…`, 67 commands, versioned.

**Build**: `./build.sh` → `felucca.bin` + `loader/ota.bin` + `felucca.fwsc`. Needs the JieLi toolchain (`tools/get_toolchain.sh`), three blobs from fw-AC79_AIoT_SDK tag `AC79NN_SDK_V1.2.1_2023-12-13` (`uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`), Python 3 + Pillow + fontTools. Linux x86-64 native; macOS via Docker; **Windows via WSL** (SLOOP ships `INSTALL-SLOOP.bat`). Host-side test suite; X0X and SLOOP add a host simulator with WAV/PNG output.

**Install**: Web MIDI installer (Chrome/Edge) or `tools/fm1_install.py`. Return to stock with M-VAVE's `.fwsc`.

**Memory budget** (`app.ld`): XIP app window ~568 KiB; RAM 96 KiB + POOL 336 KiB + RAMTEXT 24 KiB + NOINIT 15 KiB.

## 5. Known limits and headroom

- Single core at 240 MHz for the Felucca family → 8 shared voices by default, 85 % overload threshold. Reference points: Jangada 8 supersaw voices = 55 %; X0X all parts dense ≈102 %.
- Stock gets 12 FM voices by offloading to cpu1. **No open project uses cpu1 yet.** That is the big untapped resource.
- 578 KB SRAM and 1 MiB flash (≈568 KiB app) are hard ceilings. Samples: 3 × 80 KiB IMA-ADPCM slots in Felucca.
- Latency: ~1–2 ms key debounce, 2.9 ms audio halves, 60 Hz UI.
- No Discord found. Support is [Felucca GitHub Discussions](https://github.com/hugelton/Felucca/discussions), per-fork issues, the [Gearspace thread](https://gearspace.com/threads/m-vave-fm-1.1465371/), and the [Piano & Synth custom firmware collection](https://pianoandsynth.com/m-vave-fm-1-custom-firmware-collection/).

## 6. Not found

Codec IC part number, TRS MIDI polarity, an official AC791N datasheet, any vendor SDK/schematic, any Discord, stock latency measurements.
