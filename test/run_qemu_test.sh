#!/bin/bash

set -e

# Generate a test binary to be flashed (content doesn't matter, the test verifies write integrity)
dd if=/dev/urandom bs=1024 count=64 of="$(dirname "$0")/hello-world.bin"

mkdir -p build && cd build

# QEMU_PATH environment variable has to be defined, pointing to qemu-system-xtensa
# Example: export QEMU_PATH=/home/user/esp/qemu/xtensa-softmmu/qemu-system-xtensa
if [ -z "${QEMU_PATH}" ]; then
    echo "QEMU_PATH environment variable needs to be set"
    exit 1
fi

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
