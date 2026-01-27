# ESP32 Zephyr Example

## Overview

This sample code demonstrates how to flash an Espressif SoC (target) from another MCU (host) using
`esp_serial_flasher`.
Binaries to be flashed from the host MCU to another Espressif SoC can be found in the [target-firmware](../target-firmware/) folder
and are converted into C arrays during the build process.

## How the Example Works

The example performs the following steps to flash the target device:

1. Initialization: Sets up the required peripherals (GPIO for BOOT and RESET pins, and UART)
2. Target Reset: Puts the target into bootloader mode using the BOOT and RESET pins
3. Connection: Establishes communication with the target bootloader using esp_loader_connect()
4. Baud Rate Change: Increases the transmission speed for faster flashing (except for ESP8266)
5. Flashing Process:
   - Calls esp_loader_flash_start() to begin flashing and erase the required memory
   - Repeatedly calls esp_loader_flash_write() to transfer the binary
   - Verifies the flash integrity if MD5 checking is enabled

> [!NOTE]
> In addition to the steps mentioned above, `esp_loader_change_transmission_rate()` is called after the connection is established in order to increase the flashing speed. This does not apply to ESP8266, as its bootloader does not support this command. However, ESP8266 is capable of detecting the baud rate during the connection phase; the rate can be changed before calling `esp_loader_connect()`, if necessary.

## Hardware Required

- `ESP32-DevKitC` board or another compatible Espressif development board as a host
- Any supported Espressif SoC development board as a target
- One or two USB cables for power supply and programming
- Jumper cables to connect the host to the target according to the table below.
- Alternatively, the `esp_threadbr` board integrates both host and target SoCs

## Hardware Connection

This example uses the **UART interface**. For detailed interface information and general hardware considerations, see the [Hardware Connections Guide](../../docs/hardware-connections.md#uartserial-interface).

**ESP32-to-Espressif SoC Pin Assignment:**

| ESP32 (host) | Espressif SoC (target) |
| :----------: | :--------------------: |
|     IO26     |          BOOT          |
|     IO25     |         RESET          |
|     IO4      |          RX0           |
|     IO5      |          TX0           |

> [!NOTE]
> Pin assignments can be modified in the device tree overlay.

## Prerequisites

Before building the example, you need to set up the Zephyr development environment as stated in [Zephyr's Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html). After this step, you can proceed to building and flashing the example.

## Prepare Target Firmware

Place the required target firmware binaries in the `target-firmware/` directory. You can use your own binaries, build them from the esp-idf examples, or build them from the source in the `test/target-example-src` directory.

**Example binaries:**

- `bootloader.bin` - ESP bootloader binary
- `partition-table.bin` - Partition table configuration
- `app.bin` - Main application binary

To build from the example source:

```bash
cd test/target-example-src/hello-world-ESP32-src
idf.py set-target esp32s3  # Set target chip (esp32, esp32s3, esp32c3, etc.)
idf.py fullclean
idf.py -D SDKCONFIG_DEFAULTS=sdkconfig.defaults.flash reconfigure build

# Copy binaries to example
cp build/bootloader/bootloader.bin ../../zephyr_example/target-firmware/
cp build/partition_table/partition-table.bin ../../zephyr_example/target-firmware/
cp build/hello_world.bin ../../zephyr_example/target-firmware/app.bin
```

To set the flash offsets for the binary images, create or copy the `images.csv` file from below.
Put each binary image name and its offset on one line as `file;offset` (semicolon-separated; offset in hex without `0x`).

```
# image_file;offset (hex, no 0x prefix)
bootloader.bin;0
partition-table.bin;8000
app.bin;10000
```

## Build and Flash

To run the example, type the following commands:

```bash
# Initialize west workspace if not done already
west init

# Update Zephyr modules
west update

# Build the example with ESP-Serial-Flasher module
west build -p -b <supported board from the boards folder> path/to/esp-serial-flasher/examples/zephyr_example

west flash
west espressif monitor
```

There is a board named `ESP ThreadBR` in the Zephyr sources that has ESP32-S3 as a host SoC and ESP32-H2 as a target SoC.
To build ESP Serial Flasher for that board, use:

```bash
# The sources path depends on how you set the entry in zephyr/submanifest
west build -p -b esp_threadbr/esp32s3/procpu ../modules/lib/esp_serial_flasher/examples/zephyr_example
```

## Configuration

For details about available configuration options, please refer to the top-level [README.md](../../README.md).
Compile definitions can be specified in the `prj.conf` file.

By default, the example will install provided images and read target serial output.
You can add `CONFIG_ESP_SERIAL_FLASHER_SHELL=y` to enable interactive shell console.

```bash
# Build the example with ESP-Serial-Flasher and interactive shell
west build -p -b <supported board from the boards folder> path/to/esp-serial-flasher/examples/zephyr_example -DCONFIG_ESP_SERIAL_FLASHER_SHELL=y
```

Flash your updated build to the target and observe the UART console.

## Example Output

Here is the example's console output without the interactive shell:

```text
Running ESP Flasher from Zephyr
Connected to target
Transmission rate changed.
Erasing flash (this may take a while)...
Start programming
100 %
Finished programming
Flash verified
Erasing flash (this may take a while)...
Start programming
100 %
Finished programming
Flash verified
Erasing flash (this may take a while)...
Start programming
100 %
Finished programming
Flash verified

DONE
********************************************
*** Logs below are printed from the slave .... ***
********************************************
Hello world!
```

And here with the interactive shell:

```text
Type 'esf help' to show serial flasher shell command options
*** Booting Zephyr OS build v4.4.0-1140-g808a0d08c689 ***
uart:~$
```

Enter `esf help` to display available `esf` commands:

```bash
esf - ESP Serial Flasher commands
Subcommands:
  reset     : Reset target
  info      : Show target info
  images    : List available images
  connect   : Connect target
  speed     : Change transmission speed
  flash     : Flash operations
  register  : Register operations
```
