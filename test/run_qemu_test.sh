#!/bin/bash

set -e

# Generate a test binary to be flashed (content doesn't matter, the test verifies write integrity)
dd if=/dev/urandom bs=1024 count=64 of="$(dirname "$0")/hello-world.bin"

mkdir -p build && cd build

# qemu-system-xtensa is expected to be available on PATH.
# See https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/qemu.html
QEMU_PATH="${QEMU_PATH:-qemu-system-xtensa}"

# Generate empty file into which application will be flashed and compared against
dd if=/dev/zero bs=1024 count=4096 of="empty_file.bin"

# Run qemu in background, capture PID for cleanup
${QEMU_PATH} \
    -display none \
    -machine esp32 \
    -drive file=empty_file.bin,if=mtd,format=raw \
    -global driver=esp32.gpio,property=strap_mode,value=0x0f \
    -serial tcp::5555,server,nowait &
QEMU_PID=$!
trap 'kill $QEMU_PID 2>/dev/null || true' EXIT

cmake ..
cmake --build .
./serial_flasher_test
