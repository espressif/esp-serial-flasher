# Example of Loading the Program into RAM Through SPI

## Overview

This example demonstrates how to upload an app to RAM of an Espressif MCU (target) with SPI download support from another MCU (host) using the `esp_serial_flasher`. In this case, another Espressif MCU is used as the host. Binaries to be uploaded to RAM from the host MCU to the target Espressif SoC can be found in `binaries/RAM_APP` folder and are converted into C-array during build process.

The following steps are performed in order to re-program the targets memory:

1. SPI2 through which the binary will be transferred is initialized.
2. The host puts the slave device into the SPI download mode tries to connect by calling `esp_loader_connect()`.
3. Then `esp_loader_mem_start()` is called for each segment in RAM.
4. `esp_loader_flash_write()` function is called repeatedly for every segment until the whole binary image is transferred.
5. `esp_loader_mem_finish()` is called with the binary entrypoint, telling the chip to start the uploaded program.
6. UART2 is initialized for the connection to the target.
7. Target output is continually read and printed out.

## Hardware Required

- **Host**: Any Espressif development board (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
- **Target**: Espressif MCU with SPI download support:
  - ESP32-C3, ESP32-C2, ESP32-C5, ESP32-C6
  - ESP32-S3, ESP32-S2
  - ESP32-H2
- One or two USB cables for power supply and programming.
- Jumper cables for host-to-target connections.

## Hardware Connection

This example uses the **SPI interface**. For detailed interface information, strapping pin requirements, and general hardware considerations, see the [Hardware Connections Guide](../../docs/hardware-connections.md#spi-interface).

**ESP32-S3-to-Espressif SoC Pin Assignment:**

| ESP32-S3 (host) | Espressif SoC (target) |
| :-------------: | :--------------------: |
|      IO_5       |         RESET          |
|      IO_12      |          CLK           |
|      IO_10      |           CS           |
|      IO_13      |          MISO          |
|      IO_11      |          MOSI          |
|      IO_14      |         QUADWP         |
|      IO_9       |         QUADHD         |
|      IO_13      |        STRAP_B0        |
|      IO_2       |        STRAP_B1        |
|      IO_3       |        STRAP_B2        |
|      IO_4       |        STRAP_B3        |
|      IO_6       |        UART0_RX        |
|      IO_7       |        UART0_TX        |

> [!NOTE]
>
> - **Strapping Pins**: Configuration varies by target chip - consult the target's Technical Reference Manual for the specific strapping pin values required
> - **UART Monitoring**: Optional 2-pin UART connection for real-time target output

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
I (682) spi_ram_loader: Loading app to RAM ...
Start loading
Downloading 7840 bytes at 0x3fc96e00...
Downloading 312 bytes at 0x3fca0020...
Downloading 93164 bytes at 0x40380000...
Finished loading
I (802) spi_ram_loader: ********************************************
I (802) spi_ram_loader: *** Logs below are print from slave .... ***
I (812) spi_ram_loader: ********************************************
Hello world!
Hello world!
...
```
