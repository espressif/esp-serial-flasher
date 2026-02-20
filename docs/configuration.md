# Configuration Reference

This document provides a comprehensive reference for all configuration options available in ESP Serial Flasher.

## Configuration Types

ESP Serial Flasher supports various configuration options that can be set as **CMake cache variables**. These options control the behavior and features of the library.

## Configuration Options

### Debug and Diagnostics

#### `SERIAL_FLASHER_DEBUG_TRACE`

- **Type**: CMake cache variable / Kconfig (`CONFIG_SERIAL_FLASHER_DEBUG_TRACE`)
- **Default**: Disabled
- **Description**: Enables debug tracing output (transfer data tracing).

### Protocol / Interface Selection

Starting from v2, the communication protocol is **selected at runtime** by calling the appropriate per-protocol init function. There are no compile-time interface flags.

```c
esp_loader_t loader;
esp_loader_init_uart(&loader, &esp32_uart_port_ops);   // UART
esp_loader_init_usb(&loader, &esp32_usb_cdc_acm_port_ops); // USB CDC-ACM
esp_loader_init_spi(&loader, &esp32_spi_port_ops);    // SPI
esp_loader_init_sdio(&loader, &esp32_sdio_port_ops);  // SDIO
```

Each init function accepts the matching per-protocol port ops type:

| Init function            | Port ops type                | Interface   | Notes                                  |
| ------------------------ | ---------------------------- | ----------- | -------------------------------------- |
| `esp_loader_init_uart()` | `esp_loader_uart_port_ops_t` | UART        | Default; full feature set              |
| `esp_loader_init_usb()`  | `esp_loader_usb_port_ops_t`  | USB CDC-ACM | Full feature set                       |
| `esp_loader_init_spi()`  | `esp_loader_spi_port_ops_t`  | SPI         | RAM download only                      |
| `esp_loader_init_sdio()` | `esp_loader_sdio_port_ops_t` | SDIO        | Experimental; limited platform support |

Functions not supported by a given protocol return `ESP_LOADER_ERROR_UNSUPPORTED_FUNC`.

### Port Compilation (ESP-IDF / Kconfig only)

For ESP-IDF builds, you choose which port implementations to compile into the library. Multiple ports can be enabled simultaneously, enabling runtime switching between interfaces in the same firmware image.

#### `CONFIG_SERIAL_FLASHER_PORT_UART`

- **Type**: Kconfig (`bool`)
- **Default**: Enabled (`y`)
- **Description**: Compile the ESP32 UART port (`esp32_port.c`). Exposes `esp32_uart_port_ops`.

#### `CONFIG_SERIAL_FLASHER_PORT_SPI`

- **Type**: Kconfig (`bool`)
- **Default**: Disabled
- **Description**: Compile the ESP32 SPI port (`esp32_spi_port.c`). Exposes `esp32_spi_port_ops`. Only supports RAM download operations.

#### `CONFIG_SERIAL_FLASHER_PORT_SDIO`

- **Type**: Kconfig (`bool`)
- **Default**: Disabled
- **Description**: Compile the ESP32 SDIO port (`esp32_sdio_port.c`). Exposes `esp32_sdio_port_ops`. Experimental; only available on targets with SDIO host support.

#### `CONFIG_SERIAL_FLASHER_PORT_USB_CDC_ACM`

- **Type**: Kconfig (`bool`)
- **Default**: Disabled
- **Depends on**: `SOC_USB_OTG_SUPPORTED`
- **Description**: Compile the ESP32 USB CDC-ACM port (`esp32_usb_cdc_acm_port.c`). Exposes `esp32_usb_cdc_acm_port_ops`. Requires the `espressif/usb_host_cdc_acm` managed component.

### Flash Verification

#### `MD5_ENABLED`

- **Type**: CMake cache variable / Kconfig (see mapping below)
- **Default**:
  - Plain CMake: Disabled (only enabled if you pass `-DMD5_ENABLED=1`)
  - ESP-IDF/Zephyr (Kconfig): Enabled by default (`CONFIG_SERIAL_FLASHER_MD5_ENABLED=y`)
- **Description**: Enable MD5 checksum verification of flash content after writing.
- **Warning**: Must be disabled for ESP8266 targets as the ROM bootloader does not support MD5 verification.

### Retry and Timing Configuration

#### `SERIAL_FLASHER_WRITE_BLOCK_RETRIES`

- **Type**: CMake cache variable
- **Default**: 3
- **Description**: Number of retry attempts for writing blocks to target flash or RAM.

#### `SERIAL_FLASHER_RESET_HOLD_TIME_MS`

- **Type**: CMake cache variable
- **Default**: 100 (milliseconds)
- **Description**: Duration for which the reset pin is asserted during a hard reset.

#### `SERIAL_FLASHER_BOOT_HOLD_TIME_MS`

- **Type**: CMake cache variable
- **Default**: 50 (milliseconds)
- **Description**: Duration for which the boot pin is asserted during a hard reset.

### GPIO Pin Inversion

#### `SERIAL_FLASHER_RESET_INVERT`

- **Type**: CMake cache variable
- **Default**: Disabled
- **Description**: Invert the output of the reset GPIO pin.
- **Use case**: Useful when hardware has an inverting connection between host and target reset pin.
- **Availability**: UART interface only.

#### `SERIAL_FLASHER_BOOT_INVERT`

- **Type**: CMake cache variable
- **Default**: Disabled
- **Description**: Invert the output of the boot GPIO pin.
- **Use case**: Useful when hardware has an inverting connection between host and target boot pin.
- **Availability**: UART interface only.

## Setting Configuration Options

Configuration options can be set when running CMake using the `-D` flag:

```bash
cmake -DMD5_ENABLED=1 -DSERIAL_FLASHER_WRITE_BLOCK_RETRIES=5 .. && cmake --build .
```

### Multiple Options Example

```bash
cmake \
  -DMD5_ENABLED=0 \
  -DSERIAL_FLASHER_WRITE_BLOCK_RETRIES=5 \
  -DSERIAL_FLASHER_RESET_HOLD_TIME_MS=200 \
  .. && cmake --build .
```

### ESP-IDF and Zephyr Configuration

When using ESP-IDF or Zephyr, configuration options can be set using **Kconfig** instead of CMake flags. Many options have corresponding Kconfig names with the `CONFIG_` prefix:

- `SERIAL_FLASHER_DEBUG_TRACE` → `CONFIG_SERIAL_FLASHER_DEBUG_TRACE`
- `MD5_ENABLED` → `CONFIG_SERIAL_FLASHER_MD5_ENABLED`
- Port compilation → `CONFIG_SERIAL_FLASHER_PORT_UART`, `CONFIG_SERIAL_FLASHER_PORT_SPI`, etc.

You can configure these options using:

**ESP-IDF:**

```bash
idf.py menuconfig
```

**Zephyr:**

```bash
west build -t menuconfig
```

Alternatively, you can set them directly in your `sdkconfig` (ESP-IDF) or `prj.conf` (Zephyr) files:

```ini
# ESP-IDF sdkconfig or Zephyr prj.conf
CONFIG_SERIAL_FLASHER_DEBUG_TRACE=y
CONFIG_SERIAL_FLASHER_MD5_ENABLED=n
CONFIG_SERIAL_FLASHER_PORT_UART=y
CONFIG_SERIAL_FLASHER_PORT_SPI=y
```

## Platform-Specific Configuration

Some platforms may have additional configuration requirements. See the platform-specific setup guides:

- [STM32 Setup](platform-setup.md#stm32-setup)
- [Zephyr Setup](platform-setup.md#zephyr-setup)
- [Raspberry Pi Pico Setup](platform-setup.md#raspberry-pi-pico-setup)
