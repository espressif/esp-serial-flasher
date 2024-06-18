# Get target info example

## Overview

This example demonstrates how to get the target info including the flash chip size and the WIFI MAC address of an Espressif SoC from another (host) MCU using `esp-serial-flasher`. In this case, an ESP32 is used as the host MCU.

The following steps are performed:

1. UART1 through which new binary will be transfered is initialized.
2. The host puts target device into the boot mode and tries to connect by calling `esp_loader_connect()`.
3. The host attempts to read the target flash size and the WIFI MAC and prints them out.

Note: In addition, to steps mentioned above, `esp_loader_change_transmission_rate()`  is called after connection is established in order to increase communication speed. This does not apply for the ESP8266, as its bootloader does not support this command. However, the ESP8266 is capable of detecting the baud rate during connection phase, and can be changed before calling `esp_loader_connect()`, if necessary.

## Connection configuration

In the majority of cases `ESP_LOADER_CONNECT_DEFAULT` helper macro is used in order to initialize `loader_connect_args_t` data structure passed to `esp_loader_connect()`. Helper macro sets the maximum time to wait for a response and the number of retrials. For more detailed information refer to [serial protocol](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/serial-protocol.html).

## Hardware Required

* Two development boards with the ESP32 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.).
* One or two USB cables for power supply and programming.
* Cables to connect host to target according to table below.

## Hardware connection

Table below shows connection between two ESP32 devices.

| ESP32 (host) | ESP32 (target) |
|:------------:|:--------------:|
|    IO26      |      IO0       |
|    IO25      |     RESET      |
|    IO4       |      RX0       |
|    IO5       |      TX0       |

Note: interconnection is the same for ESP32, ESP32-S2 and ESP8266 targets.

## Build and flash

To run the example, type the following command:

```CMake
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example output

Here is the example's console output:

```
...
Connected to target
Transmission rate changed.
I (1341) serial_flasher: Target flash size [B]: 8388608
I (1341) serial_flasher: Target WIFI MAC:
I (1341) serial_flasher: e4 65 b8 7e 13 60
I (1341) main_task: Returned from app_main()
```
