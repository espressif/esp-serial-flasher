# Example of Loading the Program into RAM Through SDIO

## Overview

This example demonstrates how to upload an app to RAM of an Espressif MCU (target) with SDIO download support from another MCU (host) using the `esp_serial_flasher`. In this case, another Espressif MCU is used as the host. Binaries to be uploaded to RAM from the host MCU to the target Espressif SoC can be found in `binaries/RAM_APP` folder and are converted into C-array during build process.

The following steps are performed in order to load the program into target's RAM:

1. SDIO interface in slot 1 through which the binary will be transferred is initialized.
2. The host puts the slave device into joint download mode and tries to connect by calling `esp_loader_connect()`.
3. Then `esp_loader_mem_start()` is called for each segment in RAM.
4. `esp_loader_mem_write()` function is called repeatedly for every segment until the whole binary image is transferred.
5. `esp_loader_mem_finish()` is called with the binary entrypoint, telling the chip to start the uploaded program.
6. UART2 is initialized for the connection to the target.
7. Target output is continually read and printed out.

## Hardware Required

- **Host**: Espressif SoC development board with SDIO Host support
- **Target**: Espressif SoC development board with SDIO download support (e.g., ESP32-C6)
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

You can find the target SDIO pins for each target [here](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/sdio_slave.html).

> [!NOTE]
> SDIO pins CMD and DAT0-3 may require pullup resistors depending on your hardware setup. Please take a look at the [SD Pull-up Requirements](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/sd_pullup_requirements.html) for more info.

## Build and Flash

To run the example, type the following command:

```CMake
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type `Ctrl-]`.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

Here is the example's console output:

```text
Connected to target
I (701) sdio_ram_loader: Loading app to RAM ...
Start loading
Downloading 52296 bytes at 0x40800000...
Downloading 312 bytes at 0x4080d990...

Finished loading
I (741) sdio_ram_loader: ********************************************
I (741) sdio_ram_loader: *** Logs below are print from slave .... ***
I (751) sdio_ram_loader: ********************************************
Hello world!
...
```
