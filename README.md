# esp-serial-flasher

`esp-serial-flasher` is a portable C library for flashing or loading apps to RAM of Espressif SoCs from other host microcontrollers.

`esp-serial-flasher` supports a variety of host/target/interface combinations:

Supported **host** microcontrollers:

- STM32
- Raspberry Pi SBC
- ESP32 Series
- Any MCU running Zephyr OS
- Raspberry Pi Pico

Supported **target** microcontrollers:

- ESP32
- ESP8266
- ESP32-S2
- ESP32-S3
- ESP32-C3
- ESP32-C2
- ESP32-H2
- ESP32-C6

Supported hardware interfaces:
- UART
- SPI (only for RAM download)
- USB CDC ACM

For example usage check the [examples](/examples) directory.

## Configuration

These are the configuration toggles available to the user:

* `SERIAL_FLASHER_INTERFACE_UART`/`SERIAL_FLASHER_INTERFACE_SPI`/`SERIAL_FLASHER_INTERFACE_USB`

This defines the hardware interface to use.

Default: SERIAL_FLASHER_INTERFACE_UART

* `MD5_ENABLED`

If enabled, `esp-serial-flasher` is capable of verifying flash integrity after writing to flash.

Default: Enabled
> Warning: As ROM bootloader of the ESP8266 does not support MD5_CHECK, this option has to be disabled!

* `SERIAL_FLASHER_WRITE_BLOCK_RETRIES`

This configures the amount of retries for writing blocks either to target flash or RAM.

Default: 3

* `SERIAL_FLASHER_RESET_HOLD_TIME_MS`

This is the time for which the reset pin is asserted when doing a hard reset in milliseconds.

Default: 100

* `SERIAL_FLASHER_BOOT_HOLD_TIME_MS`

This is the time for which the boot pin is asserted when doing a hard reset in milliseconds.

Default: 50

* `SERIAL_FLASHER_RESET_INVERT`

This inverts the output of the reset gpio pin. Useful if the hardware has inverting connection
between the host and the target reset pin. Implemented only for UART interface.

Default: n

* `SERIAL_FLASHER_BOOT_INVERT`
This inverts the output of the boot (IO0) gpio pin. Useful if the hardware has inverting connection
between the host and the target boot pin. Implemented only for UART interface.

Default: n

Configuration can be passed to `cmake` via command line:

```
cmake -DMD5_ENABLED=1 .. && cmake --build .
```

### ESP support

#### Supported ESP-IDF versions
- v4.3 or later

### STM32 support

The STM32 port makes use of STM32 HAL libraries, and these do not come with CMake support. In order to compile the project, `stm32-cmake` (a `CMake` support package) has to be pulled as submodule.

```
git clone --recursive https://github.com/espressif/esp-serial-flasher.git
```

If you have cloned this repository without the `--recursive` flag, you can initialize the submodule using the following command:

```
git submodule update --init
```

In addition to the configuration parameters mentioned above, the following definitions have to be set:

- STM32_TOOLCHAIN_PATH: path to arm toolchain (i.e /home/user/gcc-arm-none-eabi-9-2019-q4-major)
- STM32_CUBE_<CHIP_FAMILY>_PATH: path to STM32 Cube libraries (i.e /home/user/STM32Cube/Repository/STM32Cube_FW_F4_V1.25.0)
- STM32_CHIP: name of STM32 for which project should be compiled (i.e STM32F407VG)
- CORE_USED: core used on multicore devices (i.e. M7 or M4 on some STM32H7 chips)
- PORT: STM32

This can be achieved by passing definitions to the command line, such as:

```
cmake -DSTM32_TOOLCHAIN_PATH="path_to_toolchain" -DSTM32_CUBE_<CHIP_FAMILY>_PATH="path_to_cube_libraries" -DSTM32_CHIP="STM32F407VG" -DPORT="STM32" .. && cmake --build .
```

Alternatively, those variables can be set in the top level `cmake` directory:

```
set(STM32_TOOLCHAIN_PATH path_to_toolchain)
set(STM32_CUBE_H7_PATH path_to_cube_libraries)
set(STM32_CHIP STM32H743VI)
set(CORE_USED M7)
set(PORT STM32)
```

### Zephyr support

The Zephyr port is ready to be integrated into Zephyr apps as a Zephyr module. In the manifest file (west.yml), add:

```
    - name: esp-flasher
      url: https://github.com/espressif/esp-serial-flasher
      revision: master
      path: modules/lib/esp_flasher
```

And add

```
CONFIG_ESP_SERIAL_FLASHER=y
CONFIG_CONSOLE_GETCHAR=y
CONFIG_SERIAL_FLASHER_MD5_ENABLED=y
```

to the project configuration `prj.conf`.

For the C/C++ source code, the example code provided in `examples/zephyr_example` can be used as a starting point.

## Supporting a new host target

The port layer for the given host microcontroller can be implemented if not available, in order to support a new target, following functions have to be implemented by user:

- `loader_port_read()`
- `loader_port_write()`
- `loader_port_enter_bootloader()`
- `loader_port_delay_ms()`
- `loader_port_start_timer()`
- `loader_port_remaining_time()`

For the SPI interface ports
- `loader_port_spi_set_cs()`
needs to be implemented as well.

The following functions are part of the [io.h](include/io.h) header for convenience, however, the user does not have to strictly follow function signatures, as there are not called directly from library.

- `loader_port_change_transmission_rate()`
- `loader_port_reset_target()`
- `loader_port_debug_print()`

Prototypes of all functions mentioned above can be found in [io.h](include/io.h).

After that, the target implementing these functions should be linked with the `flasher` target and the `PORT` CMake variable should be set to `USER_DEFINED`.

## Contributing

We welcome contributions to this project in the form of bug reports, feature requests and pull requests.

Issue reports and feature requests can be submitted using [Github Issues](https://github.com/espressif/esp-serial-flasher/issues). Please check if the issue has already been reported before opening a new one.

Contributions in the form of pull requests should follow ESP-IDF project's [contribution guidelines](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html) and use the [conventional commit message style](https://www.conventionalcommits.org/en/v1.0.0/).

To automatically enforce these rules, use [pre-commit](https://pre-commit.com/) and install hooks with the following commands:
```
pre-commit install
pre-commit install -t commit-msg
```

## Licence

Code is distributed under Apache 2.0 license.

## Known limitations

Size of new binary image has to be known before flashing.
