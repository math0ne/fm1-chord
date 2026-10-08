#!/bin/sh
# Build (once) and run the FM-1 emulator with a patch set that lets it run this firmware.
#
#   tools/emulator/run.sh [firmware.fwsc]          (default: build/felucca.fwsc)
#
# The emulator is simonjohansson/fm1-emulator at its last released commit (81b9ed9, the Rust
# core, GPL-3.0). Four small patches (fm1-emulator-81b9ed9.patch) add CPU forms and the fifth
# USB endpoint that Felucca 1.0.1's toolchain output and USB audio input use. Needs cargo
# (https://rustup.rs) and, on Linux, the X11/Wayland dev packages from the emulator's README.
# Under WSL the window opens on the Windows desktop through WSLg.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
src=${FM1_EMULATOR:-$HOME/fm1-emulator-81b9ed9}
fw=${1:-$root/build/felucca.fwsc}
if [ ! -d "$src" ]; then
    git clone -q https://github.com/simonjohansson/fm1-emulator "$src"
    git -C "$src" checkout -q 81b9ed96e33b28fc401fb3e0faf46c5f904bd843
    git -C "$src" apply "$here/fm1-emulator-81b9ed9.patch"
fi
bin=$src/rust-emulator/target/release/fm1-ui
[ -x "$bin" ] || (cd "$src/rust-emulator" && cargo build --locked --release --features gui --bin fm1-ui)
echo "emulator: $bin"
echo "firmware: $fw   (keys: Z/C notes, X/V octave, P/O ENV/LFO, H HOME; the console is this terminal)"
LIBGL_ALWAYS_SOFTWARE=${LIBGL_ALWAYS_SOFTWARE:-1} exec "$bin" "$fw"
