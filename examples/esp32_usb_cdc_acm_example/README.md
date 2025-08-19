# Flashing Multiple Partitions over USB CDC ACM Interface

## Overview

This example demonstrates how to flash an ESP32-S3 (target) from another ESP32-S3 or an ESP32-S2 MCU (host) using the `esp_serial_flasher`. Binaries to be flashed from the host MCU to the Espressif SoC can be found in [binaries](../binaries/) folder and are converted into C-array during build process.

> [!NOTE]
> The `esp32_usb_cdc_acm` port requires ESP-IDF v4.4 or newer to build.

The following steps are performed in order to re-program memory:

1. The system is started.
2. The USB CDC ACM driver is initialized and a task to handle USB events is created.
3. As soon as an ESP32-S3 is connected to the bus, a connection is opened with it using `loader_port_esp32_usb_cdc_acm_init()`. If the target USB Serial/JTAG peripheral is not active (e.g the device firmware is using the USB OTG peripheral), it is necessary to manually put it in download mode.
4. The flasher connection is started with the connected ESP32-S3 by calling `esp_loader_connect()`.
5. The binary file is opened and its size is acquired, as it has to be known before flashing.
6. Then `esp_loader_flash_start()` is called to enter the flashing mode and erase amount of the memory to be flashed.
7. `esp_loader_flash_write()` function is called repeatedly until the whole binary image is transferred.
8. After completion, the device can be manually reset and another ESP32-S3 can be connected to perform the flashing.

> [!NOTE]
> The USB CDC ACM device of the ESP32-S3 does not support changing the baudrate, so the argument to `esp_loader_connect()` is irrelevant.

## USB Host Driver Usage

This example makes use of the Espressif [USB Host Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb_host.html).
In addition to initializing the host driver, a FreeRTOS task needs to be created to handle host usb events.

A binary semaphore is used as a lock for the connected device and a callback is registered with the loader port which releases the lock, so that a new device can be connected only after disconnection.

## Hardware Required

- **Host**: Espressif SoC development board with USB Host capability
- **Target**: Espressif SoC development board with USB Serial/JTAG peripheral (e.g., ESP32-S3)
- USB OTG adapter for the host board
- Quality USB cables for communication and programming
- **Separate power supply** for target board (USB cannot provide sufficient power)

> [!NOTE]
> The USB connector on most ESP32-S3 and ESP32-S2 boards cannot supply sufficient power to the target, so a separate power connection is required.

## Hardware Connection

This example uses the **USB CDC ACM interface**. For detailed interface information, power requirements, and general hardware considerations, see the [Hardware Connections Guide](../../docs/hardware-connections.md#usb-cdc-acm-interface).

**Connection Setup:**

1. **USB Connection**: Host board → USB OTG adapter → Target board (USB cable only)
2. **Power Supply**: Independent power source for target board
3. **Programming**: Separate USB connection to PC for host programming

**No additional wiring required** - communication is entirely over USB.

## Building and Flashing

To run the example, type the following command:

```CMake
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type `Ctrl-]`.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Configuration

For details about available configuration option, please refer to the top level [README.md](../../README.md).
Compile definitions can be specified in the command line when running `idf.py`, for example:

```bash
idf.py build -DMD5_ENABLED=1
```

Binaries to be flashed are placed in a separate folder (binaries.c) for each possible target and converted to C-array. Without explicitly enabling MD5 check, flash integrity verification is disabled by default.

## Example Output

Here is the example's console output:

```text
...
I (541) main_task: Calling app_main()
I (541) usb_flasher: Installing USB Host
I (571) usb_flasher: Installing the USB CDC-ACM driver
I (571) usb_flasher: Opening CDC ACM device 0x303A:0x1001...
Connected to target
I (1121) usb_flasher: Loading bootloader...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
I (1681) usb_flasher: Loading partition table...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
I (1771) usb_flasher: Loading app...
Erasing flash (this may take a while)...
Start programming
Progress: 100 %
Finished programming
I (5011) usb_flasher: Done!
```
