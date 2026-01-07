# Flash Multiple Partitions with Compression (Deflate) Example

## Overview

This example demonstrates how to flash an Espressif SoC (target) from another MCU (host) using `esp_serial_flasher` with compressed data transfer. Pre-compressed binaries are transferred using the FLASH_DEFL\_\* commands (zlib-compressed) and verified with MD5.

The following steps are performed in order to re-program the target's memory:

1. UART1 through which the new binary will be transferred is initialized.
2. The host puts the target device into boot mode and tries to connect and upload the flasher stub by calling `esp_loader_connect_with_stub()`. Change to connect_to_target(HIGHER_BAUDRATE) to use ROM bootloader in app_main().
3. `esp_loader_flash_deflate_start()` is called to enter flashing mode and erase the amount of memory to be flashed.
4. `esp_loader_flash_deflate_write()` function is called repeatedly until the whole compressed binary image is transferred.
5. Each flashed region is verified against the original binary using MD5.

## Hardware Required

- Two development boards with Espressif SoCs (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.).
- One or two USB cables for power supply and programming.
- Jumper cables to connect host to target according to table below.

## Hardware Connection

This example uses the **UART interface**. For detailed interface information and general hardware considerations, see the [Hardware Connections Guide](../../docs/hardware-connections.md#uartserial-interface).

**ESP32-to-Espressif SoC Pin Assignment:**

| ESP32 (host) | Espressif SoC (target) |
| :----------: | :--------------------: |
|     IO26     |          BOOT          |
|     IO25     |         RESET          |
|     IO4      |          RX0           |
|     IO5      |          TX0           |

## Prepare Target Firmware

Place the required target firmware binaries in the `target-firmware/` directory, You can use your own binaries, build them from the esp-idf examples, or build them from the source in the `test/target-example-src` directory, and compress them:

1. Place `bootloader.bin`, `partition-table.bin`, and `app.bin` in `target-firmware/`
2. Run the compression script:
   ```bash
   cd target-firmware
   python compress_firmware.py
   ```

This generates matching `*_deflated.bin` files for each input binary.

**Required binaries (after compression):**

- `bootloader.bin` and `bootloader_deflated.bin`
- `partition-table.bin` and `partition-table_deflated.bin`
- `app.bin` and `app_deflated.bin`

> [!NOTE]
> Both original `.bin` files (for MD5 verification) and compressed `*_deflated.bin` files (for flashing) are embedded into the host firmware.

## Build and Flash

To run the example, type the following command:

```CMake
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type `Ctrl-]`.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html) for full steps to configure and use ESP-IDF to build projects.

## Configuration

For details about available configuration options, please refer to the top level [README.md](../../README.md).

## Example Output

Here is the example's console output:

```text
...
I (398) main_task: Calling app_main()
I (398) gpio: GPIO[25]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (398) gpio: GPIO[26]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
Connected to target
Transmission rate changed.
I (2148) serial_flasher_deflate: Flashing bootloader via DEFLATE (compressed size: 16355, uncompressed: 26720)...
Progress: 100 %
I (3268) serial_flasher_deflate: Verifying bootloader via MD5...
I (3288) serial_flasher_deflate: bootloader MD5 matches.
I (3288) serial_flasher_deflate: Flashing partition table via DEFLATE (compressed size: 103, uncompressed: 3072)...
Progress: 100 %
I (3348) serial_flasher_deflate: Verifying partition table via MD5...
I (3358) serial_flasher_deflate: partition table MD5 matches.
I (3358) serial_flasher_deflate: Flashing application via DEFLATE (compressed size: 83489, uncompressed: 155600)...
Progress: 100 %
I (7928) serial_flasher_deflate: Verifying application via MD5...
I (8008) serial_flasher_deflate: application MD5 matches.
I (8008) serial_flasher_deflate: All flashes verified. Success!
I (8618) serial_flasher_deflate: ********************************************
I (8618) serial_flasher_deflate: *** Logs below are print from slave .... ***
I (8618) serial_flasher_deflate: *******************************************
```
