# ESP32 Get target info example

## Overview

This example demonstrates how to get the target info including the flash chip size and the WIFI MAC address of an Espressif SoC from another (host) MCU using `esp-serial-flasher`. In this case, an ESP32 is used as the host MCU.

Following steps are performed:

1. UART1 through which new binary will be transfered is initialized.
2. Host puts target device into boot mode and tries to connect by calling `esp_loader_connect()`.
3. Host attempts to read the target flash size and WIFI MAC and prints them out

Note: In addition, to steps mentioned above, `esp_loader_change_transmission_rate()`  is called after connection is established in order to increase communication speed. This does not apply for ESP8266, as its bootloader does not support this command. However, ESP8266 is capable of detecting baud rate during connection phase, and can be changed before calling `esp_loader_connect()`, if necessary.

## Hardware Required

* Two development boards with ESP32 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.).
* One or two USB cables for power supply and programming.

## Hardware connection

Table below shows connection between two ESP32 devices.

| ESP32 (host) | ESP32 (target) |
|:------------:|:--------------:|
|    IO26      |      IO0       |
|    IO25      |     RESET      |
|    IO4       |      RX0       |
|    IO5       |      TX0       |

## Build and flash

To run the example, type the following command:

```CMake
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example output

Here is the example's console output:

```
...
Connected to target
Transmission rate changed changed
I (744) serial_flasher: Target flash size [B]: 8388608
I (744) serial_flasher: Target WIFI MAC:
I (744) serial_flasher: 84 f7 03 80 01 c4 
I (744) main_task: Returned from app_main()
```
