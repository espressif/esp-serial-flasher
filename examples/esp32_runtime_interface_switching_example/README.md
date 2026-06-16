# Runtime Interface Switching Example

## Overview

This example demonstrates how to flash multiple Espressif SoCs (targets) from a single host MCU using `esp_serial_flasher`, switching between communication interfaces at runtime. The host is an ESP32-P4 that flashes an ESP32-C6 over **SDIO**, **UART**, and **USB CDC ACM** back to back.

Although the example is wired for ESP32-P4 and ESP32-C6, any Espressif SoC with the required host peripherals can serve as the host, and any SoC that supports a given download interface can serve as the target for that session. The three interfaces do not need to share the same target chip — you can use a different board for each interface if you adjust the connections and configuration accordingly.

Binaries to be flashed are placed in the `target-firmware/` directory and are converted into C-arrays during the build process.

The following steps are performed for each target:

1. The appropriate port is initialized by calling `esp_loader_init_sdio()` or `esp_loader_init_serial()` with the corresponding port configuration.
2. The host puts the target into download mode and connects by calling `esp_loader_connect()`.
3. Bootloader, partition table, and application binaries are flashed using `esp_loader_flash_start()` and `esp_loader_flash_write()`.
4. The target is reset with `esp_loader_reset_target()`.
5. When switching to a different serial-based interface, `esp_loader_deinit()` is called to release hardware resources before the next `esp_loader_init_*()` call.

> [!NOTE]
> The same `esp_loader_t` instance is reused across all three flashing sessions. After UART or USB flashing completes, call `esp_loader_deinit()` before initializing the loader with a different port. The USB CDC ACM interface does not support baud rate changes, so the connection arguments passed to `esp_loader_connect()` are irrelevant for the USB session.

## USB Host Driver Usage

The USB CDC ACM session requires the Espressif [USB Host Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/usb_host.html). The USB host library and CDC-ACM driver are installed once at startup, and a FreeRTOS task handles USB host events for the duration of the example.

## Hardware Required

- **Host**: ESP32-P4 development board with SDIO host and USB host capability (e.g., ESP32-P4-Function-EV-Board)
- **Target**: ESP32-C6 development board with SDIO, UART, and USB Serial/JTAG support
- USB OTG adapter for the host board (required for USB flashing)
- Quality USB cables for communication and programming
- Jumper cables for the UART and SDIO connections
- **Separate power supply** for the target board

> [!NOTE]
> Please check if your board has [possible issues](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/sd_pullup_requirements.html) regarding SDIO pullup requirements.

## Hardware Connection

This example uses three interfaces in sequence. For detailed interface information and general hardware considerations, see the [Hardware Connections Guide](../../docs/hardware-connections.md).

### ESP32-C6 over SDIO

This session uses the **SDIO interface**. See the [SDIO section](../../docs/hardware-connections.md#sdio-interface) of the hardware connections guide for pullup requirements and additional details.

**ESP32-P4 (host) to ESP32-C6 (target) pin assignment:**

| ESP32-P4 (host) | ESP32-C6 (target) |
| :-------------: | :---------------: |
|      IO_54      |       RESET       |
|      IO_53      |       BOOT        |
|      IO_50      |        D0         |
|      IO_49      |        D1         |
|      IO_48      |        D2         |
|      IO_47      |        D3         |
|      IO_51      |        CLK        |
|      IO_52      |        CMD        |

### ESP32-C6 over UART

This session uses the **UART interface**. See the [UART section](../../docs/hardware-connections.md#uartserial-interface) of the hardware connections guide for additional details.

**ESP32-P4 (host) to ESP32-C6 (target) pin assignment:**

| ESP32-P4 (host) | ESP32-C6 (target) |
| :-------------: | :---------------: |
|      IO_24      |       RESET       |
|      IO_25      |       BOOT        |
|      IO_5       |        TX0        |
|      IO_6       |        RX0        |

### ESP32-C6 over USB CDC ACM

This session uses the **USB CDC ACM interface**. See the [USB CDC ACM section](../../docs/hardware-connections.md#usb-cdc-acm-interface) of the hardware connections guide for power requirements and additional details.

**Connection setup:**

1. **USB connection**: Host board → USB OTG adapter → ESP32-C6 (USB cable only)
2. **Power supply**: Independent power source for the target board
3. **Programming**: Separate USB connection to PC for host programming

**No additional wiring required** — communication is entirely over USB.

## Prepare Target Firmware

Place the required target firmware binaries in the `target-firmware/` directory. You can use your own binaries, build them from the esp-idf examples, or build them from the source in the `test/target-example-src` directory.

**Required binaries:**

- `bootloader.bin` - ESP bootloader binary
- `partition-table.bin` - Partition table configuration
- `app.bin` - Main application binary

## Build and Flash

To run the example, type the following command:

```CMake
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type `Ctrl-]`.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/index.html) for full steps to configure and use ESP-IDF to build projects.

## Configuration

This example requires UART, SDIO, and USB CDC ACM ports to be enabled simultaneously. These options are pre-set in `sdkconfig.defaults`:

- `CONFIG_SERIAL_FLASHER_PORT_SDIO=y`
- `CONFIG_SERIAL_FLASHER_PORT_UART=y`
- `CONFIG_SERIAL_FLASHER_PORT_USB_CDC_ACM=y`

### Pin and USB peripheral configuration

The host GPIO pins and the USB host peripheral are configurable through `idf.py menuconfig` under **Runtime Interface Switching Example Configuration**. The default values target the [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html), so you can adapt the example to your own board without editing the source:

- **SDIO interface**: reset, boot, D0–D3, CLK and CMD GPIOs
- **UART interface**: TX, RX, reset and boot GPIOs
- **USB interface**: `EXAMPLE_USB_PERIPHERAL_INDEX` selects which USB OTG peripheral the host driver uses. It is converted to the `peripheral_map` bitmask as `BIT(EXAMPLE_USB_PERIPHERAL_INDEX)`, so `0` maps to `BIT0` (default) and `1` maps to `BIT1`.

To override these values without going through the menu, list them in an additional config file and pass it to the build, e.g.:

```CMake
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.ci" build
```

For details about other available configuration options, please refer to the top level [README.md](../../README.md).

## Example Output

Here is the example's console output:

```text
I (332) runtime_interface_switching: Installing USB Host
I (362) runtime_interface_switching: Installing USB CDC-ACM driver
I (362) runtime_interface_switching: Waiting for target over USB...
Connected to target
I (932) runtime_interface_switching: Flashing target over USB...
I (932) runtime_interface_switching: Loading bootloader...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
I (1132) runtime_interface_switching: Loading partition table...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
I (1172) runtime_interface_switching: Loading app...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
I (1822) runtime_interface_switching: target over USB flashing done!
W (1902) usb_cdc_acm_port: RX stream buffer full: dropped 59 bytes
W (1902) cdc_acm: RX buffer append is not supported on this target!
W (1912) usb_cdc_acm_port: RX stream buffer full: dropped 86 bytes
W (1912) cdc_acm: RX buffer append is not supported on this target!
W (1922) usb_cdc_acm_port: RX stream buffer full: dropped 85 bytes
W (1922) cdc_acm: RX buffer append is not supported on this target!
W (1932) usb_cdc_acm_port: RX stream buffer full: dropped 87 bytes
W (1932) cdc_acm: RX buffer append is not supported on this target!
I (1942) SD_HOST: src_freq_hz: 160000000
Connected to target
I (2162) runtime_interface_switching: Flashing target over SDIO...
I (2162) runtime_interface_switching: Loading bootloader...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
I (2292) runtime_interface_switching: Loading partition table...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
I (2312) runtime_interface_switching: Loading app...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
I (2542) runtime_interface_switching: target over SDIO flashing done!
Connected to target
I (3032) runtime_interface_switching: Flashing target over UART...
I (3032) runtime_interface_switching: Loading bootloader...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
I (5162) runtime_interface_switching: Loading partition table...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
I (5502) runtime_interface_switching: Loading app...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
I (12182) runtime_interface_switching: target over UART flashing done!
I (12292) runtime_interface_switching: All targets flashed.

```
