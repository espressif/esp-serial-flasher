# Linux example

Flash an ESP chip from any Linux host. Two hardware setups are supported:

**Option 1 — USB connection (standard PC / laptop)**
Connect your ESP chip via a USB-to-UART adapter (CP2102, CH340, FT232, …)
or a native USB JTAG Serial device (ESP32-C3/S3/C6/H2/P4). Reset and
boot-mode entry are handled automatically through the **DTR/RTS** lines —
the same auto-reset circuit used by esptool and the Arduino IDE. No extra
GPIO wiring is required.

**Option 2 — GPIO connection (SBC: Raspberry Pi, BeagleBone, …)**
Wire the ESP chip directly to the SBC's UART and GPIO pins. Reset and
boot-mode entry are driven via the Linux **GPIO character-device API**
(libgpiod ≥ 2.0). This requires the `LINUX_PORT_GPIO=ON` CMake
option and adds the `libgpiod` dependency.

A third mode (`-m none`) is available for both setups when you prefer to
put the target into download mode manually.

## Prerequisites

- CMake ≥ 3.22 and a C compiler (gcc / clang)

- An ESP chip connected via USB (e.g. `/dev/ttyUSB0` or `/dev/ttyACM0`)

- Your user in the `dialout` group (or run as root):

  ```bash
  sudo usermod -aG dialout $USER   # logout and back in
  ```

- For GPIO character device mode (Raspberry Pi / SBC): also add yourself to the `gpio` group and install libgpiod **≥ 2.0**:

  ```bash
  sudo usermod -aG gpio $USER
  # Debian 13 (Trixie) / Ubuntu 24.04 or later — libgpiod 2.x is in the main repos:
  sudo apt-get install libgpiod-dev
  # Older distros: build from source (https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git)
  ```

## Build

```
cd examples/linux_example
mkdir -p build && cd build
cmake ..
make
```

No firmware files need to be present at build time — binaries are passed
as command-line arguments at run time.

## Run

```
./linux_flasher [OPTIONS] <addr1> <file1> [<addr2> <file2> ...]

Options:
  -p, --port <device>   Serial device (default: /dev/ttyUSB0)
  -b, --baud <rate>     Baud rate     (default: 115200)
  -m, --mode <mode>     GPIO mode: dtr-rts | gpio | none (default: dtr-rts)
  -h, --help
```

Addresses can be decimal or hexadecimal (with `0x` prefix).

### USB-to-UART adapter

```
./linux_flasher -p /dev/ttyUSB0 -b 115200 \
    0x1000  bootloader.bin \
    0x8000  partition-table.bin \
    0x10000 app.bin
```

### USB JTAG Serial

These devices appear as `/dev/ttyACM*` and are **auto-detected** within
`dtr-rts` mode — no extra flag is needed. After the reset sequence the
device re-enumerates, and the flasher waits for the port to reappear
automatically.

```
./linux_flasher -p /dev/ttyACM0 -b 115200 \
    0x1000  bootloader.bin \
    0x8000  partition-table.bin \
    0x10000 app.bin
```

### GPIO character device (Raspberry Pi, BeagleBone, …)

Requires gpio support compiled in (see Prerequisites for libgpiod installation):

```
cmake -DLINUX_PORT_GPIO=ON ..
make
```

If `-m gpio` is used without the compile-time flag, the flasher will print a clear
error and exit.

**Raspberry Pi 4 wiring**:

| Raspberry Pi (host) | Espressif SoC (target) |
| :-----------------: | :--------------------: |
|        GPIO2        |         RESET          |
|        GPIO3        |          BOOT          |
|    GPIO14 (TXD)     |          RX0           |
|    GPIO15 (RXD)     |          TX0           |
|         GND         |          GND           |

On RPi 4, UART0 is used by Bluetooth by default. Enable it first:

```
sudo raspi-config   # Interfacing Options → Serial → No (login shell) → Yes (hardware port)
```

Then run:

```
./linux_flasher -p /dev/ttyS0 -m gpio \
    0x1000  bootloader.bin \
    0x8000  partition-table.bin \
    0x10000 app.bin
```

The GPIO chip path (`/dev/gpiochip0`) and pin numbers (RESET=2, BOOT=3) are hardcoded
in `main.c` to match the above wiring. Adjust them there if your wiring differs.

### Manual reset (no auto-reset circuit)

```
./linux_flasher -p /dev/ttyUSB0 -m none \
    0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 app.bin
```

Put the target into download mode manually before running.
