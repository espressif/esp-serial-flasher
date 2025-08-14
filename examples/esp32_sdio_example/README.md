# Example of flashing through SDIO

## Overview

This example demonstrates how to flash an Espressif MCU (target) with SDIO download support from another MCU (host) using the `esp_serial_flasher`. In this case, another Espressif MCU is used as the host. Binaries to be flashed from the host MCU to the target Espressif SoC can be found in `binaries` folder and are converted into C-array during build process.

The following steps are performed in order to re-program the targets memory:

1. SDIO interface in slot 1 through which the binary will be transferred is initialized.
2. The host puts the slave device into joint download mode and tries to connect by calling `esp_loader_connect()`.
3. The binary file is opened and its size is acquired, as it has to be known before flashing.
4. Then `esp_loader_flash_start()` is called to enter the flashing mode and erase the amount of memory to be flashed.
5. `esp_loader_flash_write()` function is called repeatedly until the whole binary image is transferred.

## Hardware Required

- **Host**: ESP32-P4 development board (with SDIO support)
- **Target**: ESP32-C6 development board (with SDIO download support)
- One or two USB cables for power supply and programming.
- Jumper cables for host-to-target connections.
- **Pullup resistors**: 10kΩ to 47kΩ resistors for SDIO signal lines (may be required - see connection details below).

> [!NOTE]
> Please check if your board has [possible issues](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/sd_pullup_requirements.html) regarding SDIO requirements.

## Hardware Connection

This example uses the **SDIO interface**. For detailed interface information, pullup requirements, and general hardware considerations, see the [Hardware Connections Guide](../../docs/hardware-connections.md#sdio-interface).

**ESP32-P4-to-Espressif SoC Pin Assignment:**

| ESP32-P4 (host) | Espressif SoC (target) |
| :-------------: | :--------------------: |
|      IO_54      |         RESET          |
|      IO_4       |          BOOT          |
|      IO_14      |           D0           |
|      IO_15      |           D1           |
|      IO_16      |           D2           |
|      IO_17      |           D3           |
|      IO_18      |          CLK           |
|      IO_19      |          CMD           |
|      IO_5       |        UART0_RX        |
|      IO_6       |        UART0_TX        |

You can find target SDIO pin mappings [here](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/sdio_slave.html).

> [!NOTE]
> SDIO pins CMD and DAT0-3 may require pullup resistors depending on your hardware setup. Please take a look at the [SD Pull-up Requirements](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/sd_pullup_requirements.html) for more info.

## Build and Flash

To run the example, type the following command:

```CMake
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type `Ctrl-]`.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Configuration

For details about available configuration options, please refer to the top level [README.md](../../README.md).
Compile definitions can be specified in the command line when running `idf.py`, for example:

```bash
idf.py build -DMD5_ENABLED=1
```

Binaries to be flashed are placed in a separate folder (binaries.c) for each possible target and converted to C-array. Without explicitly enabling MD5 check, flash integrity verification is disabled by default.

## Example output

Here is the example's console output:

```text
Connected to target
I (1866) sdio_ram_loader: Loading bootloader...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
I (2226) sdio_ram_loader: Loading partition table...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
I (2276) sdio_ram_loader: Loading app...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
Flash verified
I (2906) sdio_ram_loader: Done!
I (3506) sdio_ram_loader: ********************************************
I (3506) sdio_ram_loader: *** Logs below are print from slave .... ***
I (3506) sdio_ram_loader: ********************************************
Hello world!
...
```
