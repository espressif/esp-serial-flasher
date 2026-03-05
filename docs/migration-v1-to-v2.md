# Migration Guide: v1 → v2

This guide describes the breaking changes introduced in v2 and explains how to update existing code.

## Overview of Breaking Changes

| Area                        | v1                                                            | v2                                                                                                     |
| --------------------------- | ------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| Library state               | Global / static variables                                     | Caller-owned `esp_loader_t` context                                                                    |
| Initialization              | Not required                                                  | `esp_loader_init()` required                                                                           |
| Function signatures         | No context parameter                                          | First parameter is `esp_loader_t *loader`                                                              |
| Flash / mem operation state | Stored inside `esp_loader_t`                                  | Separate `esp_loader_flash_cfg_t` / `esp_loader_flash_deflate_cfg_t` / `esp_loader_mem_cfg_t` contexts |
| Port layer                  | Free functions (`loader_port_write`, etc.)                    | `esp_esp_loader_port_ops_t` vtable                                                                     |
| Protocol selection          | Compile-time `SERIAL_FLASHER_INTERFACE_*`                     | Runtime `esp_loader_protocol_t` in `esp_loader_init()`                                                 |
| Kconfig options             | `SERIAL_FLASHER_INTERFACE_*` choice                           | `SERIAL_FLASHER_PORT_*` booleans (multiple can be enabled)                                             |
| Error type header           | Defined in `esp_loader.h`                                     | Separate `esp_loader_error.h` (auto-included by `esp_loader.h`)                                        |
| Conditionally-compiled APIs | Many functions guarded by `#ifdef SERIAL_FLASHER_INTERFACE_*` | All functions always available; return `ESP_LOADER_ERROR_UNSUPPORTED_FUNC` if not applicable           |
| `esp_loader_flash_finish()` | Accepted `reboot` bool; not recommended due to ROM issues     | `reboot` parameter removed; **must now be called** — performs MD5 verification                         |

---

## 1. Add `esp_loader_t` Context and `esp_loader_init()`

The library no longer uses global state. All state is stored in an `esp_loader_t` struct that you allocate and own.

**v1:**

```c
loader_port_esp32_init(&config);

esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
esp_loader_connect(&args);
```

**v2:**

```c
loader_port_esp32_init(&config);

esp_loader_t loader;
esp_loader_init_uart(&loader, &esp32_uart_port_ops);

esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
esp_loader_connect(&loader, &args);
```

The per-protocol init functions (`esp_loader_init_uart`, `esp_loader_init_usb`,
`esp_loader_init_spi`, `esp_loader_init_sdio`) bind the protocol and port vtable
to the context. Call the appropriate one after the platform-specific hardware
init but before any other `esp_loader_*` call.

---

## 2. Pass `esp_loader_t *` to Every API Call

All public functions now take the loader context as their first argument.

| v1                                                           | v2                                                                                                                                                                  |
| ------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `esp_loader_connect(&args)`                                  | `esp_loader_connect(&loader, &args)`                                                                                                                                |
| `esp_loader_connect_with_stub(&args)`                        | `esp_loader_connect_with_stub(&loader, &args)`                                                                                                                      |
| `esp_loader_connect_secure_download_mode(&args, size, chip)` | `esp_loader_connect_secure_download_mode(&loader, &args, size, chip)`                                                                                               |
| `esp_loader_get_target()`                                    | `esp_loader_get_target(&loader)`                                                                                                                                    |
| `esp_loader_flash_start(offset, size, block)`                | `esp_loader_flash_start(&loader, &flash_cfg)` — see [Section 3](#3-use-operation-context-structs-for-flash-and-ram-operations)                                      |
| `esp_loader_flash_write(buf, size)`                          | `esp_loader_flash_write(&loader, &flash_cfg, buf, size)`                                                                                                            |
| `esp_loader_flash_finish(reboot)`                            | `esp_loader_flash_finish(&loader, &flash_cfg)`                                                                                                                      |
| `esp_loader_flash_deflate_write(buf, size)`                  | `esp_loader_flash_deflate_write(&loader, &defl_cfg, buf, size)`                                                                                                     |
| `esp_loader_flash_deflate_finish(reboot)`                    | `esp_loader_flash_deflate_finish(&loader, &defl_cfg)`                                                                                                               |
| `esp_loader_flash_detect_size(&size)`                        | `esp_loader_flash_detect_size(&loader, &size)`                                                                                                                      |
| `esp_loader_flash_read(buf, addr, len)`                      | `esp_loader_flash_read(&loader, buf, addr, len)`                                                                                                                    |
| `esp_loader_flash_erase()`                                   | `esp_loader_flash_erase(&loader)`                                                                                                                                   |
| `esp_loader_flash_erase_region(offset, size)`                | `esp_loader_flash_erase_region(&loader, offset, size)`                                                                                                              |
| `esp_loader_flash_verify()`                                  | Removed — MD5 verification now runs automatically inside `esp_loader_flash_finish()` by default; set `skip_verify = true` in `esp_loader_flash_cfg_t` to disable it |
| `esp_loader_mem_start(offset, size, block)`                  | `esp_loader_mem_start(&loader, &mem_cfg)` — see [Section 3](#3-use-operation-context-structs-for-flash-and-ram-operations)                                          |
| `esp_loader_mem_write(buf, size)`                            | `esp_loader_mem_write(&loader, &mem_cfg, buf, size)`                                                                                                                |
| `esp_loader_mem_finish(entry)`                               | `esp_loader_mem_finish(&loader, &mem_cfg, entry)`                                                                                                                   |
| `esp_loader_read_mac(&mac)`                                  | `esp_loader_read_mac(&loader, &mac)`                                                                                                                                |
| `esp_loader_write_register(addr, val)`                       | `esp_loader_write_register(&loader, addr, val)`                                                                                                                     |
| `esp_loader_read_register(addr, &val)`                       | `esp_loader_read_register(&loader, addr, &val)`                                                                                                                     |
| `esp_loader_change_transmission_rate(rate)`                  | `esp_loader_change_transmission_rate(&loader, rate)`                                                                                                                |
| `esp_loader_change_transmission_rate_stub(old, new)`         | `esp_loader_change_transmission_rate_stub(&loader, old, new)`                                                                                                       |
| `esp_loader_get_security_info(&info)`                        | `esp_loader_get_security_info(&loader, &info)`                                                                                                                      |
| `esp_loader_reset_target()`                                  | `esp_loader_reset_target(&loader)`                                                                                                                                  |

## 3. Use Operation Context Structs for Flash and RAM Operations

Flash, compressed flash, and RAM load operations each now require a dedicated context struct that is allocated by the caller and passed to every function in the operation sequence. This removes per-operation state from `esp_loader_t` (which is now connection-only state) and makes the operation parameters explicit and inspectable.

### Flash write

**v1:**

```c
esp_loader_flash_start(0x10000, image_size, 4096);
esp_loader_flash_write(buf, chunk_size);
esp_loader_flash_finish(false);
```

**v2:**

```c
esp_loader_flash_cfg_t flash_cfg = {
    .offset     = 0x10000,
    .image_size = image_size,
    .block_size = 4096,
};

esp_loader_flash_start(&loader, &flash_cfg);

while (remaining > 0) {
    size_t chunk = MIN(flash_cfg.block_size, remaining);
    esp_loader_flash_write(&loader, &flash_cfg, buf, chunk);
    // ...
}

esp_loader_flash_finish(&loader, &flash_cfg);
```

The `_state` sub-struct inside `esp_loader_flash_cfg_t` is managed entirely by the library. Do not read or write its fields.

### Compressed flash write (deflate)

```c
esp_loader_flash_deflate_cfg_t defl_cfg = {
    .offset          = 0x10000,
    .image_size      = uncompressed_size,
    .compressed_size = compressed_size,
    .block_size      = 4096,
};

esp_loader_flash_deflate_start(&loader, &defl_cfg);

while (remaining > 0) {
    size_t chunk = MIN(defl_cfg.block_size, remaining);
    esp_loader_flash_deflate_write(&loader, &defl_cfg, buf, chunk);
    // ...
}

esp_loader_flash_deflate_finish(&loader, &defl_cfg);
```

### RAM load

```c
esp_loader_mem_cfg_t mem_cfg = {
    .offset     = segment_addr,
    .size       = segment_size,
    .block_size = 0x1800,
};

esp_loader_mem_start(&loader, &mem_cfg);

while (remaining > 0) {
    size_t chunk = MIN(mem_cfg.block_size, remaining);
    esp_loader_mem_write(&loader, &mem_cfg, buf, chunk);
    // ...
}

esp_loader_mem_finish(&loader, &mem_cfg, entrypoint);
```

> [!NOTE]
> When loading multiple RAM segments (e.g. text + data), call `esp_loader_mem_start()` and `esp_loader_mem_write()` for each segment, then call `esp_loader_mem_finish()` once after all segments with the binary entrypoint.

---

## 4. Always Call `esp_loader_flash_finish()` — Reboot Flag Removed

In v1, `esp_loader_flash_finish(reboot)` accepted a boolean to optionally reboot the target after flashing. Due to issues with the ROM bootloader (it does not respond to further commands after the flash end command, and strapping pins are not re-read on the resulting reboot), the function was **not recommended** for use.

In v2, the `reboot` parameter has been **removed** and `esp_loader_flash_finish()` **must always be called** after flashing — it performs MD5 verification of the written data unless `skip_verify = true` is set in `esp_loader_flash_cfg_t`.

To reset the target after flashing, call `esp_loader_reset_target()` explicitly.

**v1:**

```c
esp_loader_flash_finish(true);   // not recommended — reboot via flash end command
```

**v2:**

```c
esp_loader_flash_finish(&loader, &flash_cfg);   // required — runs MD5 verification
esp_loader_reset_target(&loader);               // reset explicitly if needed
```

---

## 5. Update Port Implementations

The port layer contract changed from individual free functions to a single `esp_esp_loader_port_ops_t` vtable.

**v1** — port implementations provided a set of required free functions:

```c
esp_loader_error_t loader_port_write(const uint8_t *data, uint16_t size, uint32_t timeout);
esp_loader_error_t loader_port_read(uint8_t *data, uint16_t size, uint32_t timeout);
void               loader_port_enter_bootloader(void);
void               loader_port_reset_target(void);
void               loader_port_delay_ms(uint32_t ms);
void               loader_port_start_timer(uint32_t ms);
uint32_t           loader_port_remaining_time(void);
esp_loader_error_t loader_port_change_transmission_rate(uint32_t rate);
void               loader_port_debug_print(const char *str);
```

**v2** — port implementations provide a `const esp_esp_loader_port_ops_t` struct:

```c
#include "esp_loader_io.h"

static esp_loader_error_t my_write(const uint8_t *data, uint16_t size, uint32_t timeout) { ... }
static esp_loader_error_t my_read(uint8_t *data, uint16_t size, uint32_t timeout)         { ... }
static void               my_enter_bootloader(void)              { ... }
static void               my_reset_target(void)                  { ... }
static void               my_delay_ms(uint32_t ms)               { ... }
static void               my_start_timer(uint32_t ms)            { ... }
static uint32_t           my_remaining_time(void)                { ... }
static esp_loader_error_t my_change_rate(uint32_t rate)          { ... }
static void               my_debug_print(const char *str)        { ... }

const esp_loader_uart_port_ops_t my_platform_port_ops = {
    .common = {
        .enter_bootloader         = my_enter_bootloader,
        .reset_target             = my_reset_target,
        .start_timer              = my_start_timer,
        .remaining_time           = my_remaining_time,
        .delay_ms                 = my_delay_ms,
        .debug_print              = my_debug_print,
        .change_transmission_rate = my_change_rate,
    },
    .write = my_write,
    .read  = my_read,
};
```

Use the per-protocol struct type that matches the interface:

| Interface   | Struct type                  | Init function            |
| ----------- | ---------------------------- | ------------------------ |
| UART        | `esp_loader_uart_port_ops_t` | `esp_loader_init_uart()` |
| USB CDC-ACM | `esp_loader_usb_port_ops_t`  | `esp_loader_init_usb()`  |
| SPI         | `esp_loader_spi_port_ops_t`  | `esp_loader_init_spi()`  |
| SDIO        | `esp_loader_sdio_port_ops_t` | `esp_loader_init_sdio()` |

See `port/esp32_sdio_port.c` for a reference SDIO implementation.

---

## 6. Update Interface / Protocol Selection

### CMake (non-ESP-IDF) Builds

The compile-time `SERIAL_FLASHER_INTERFACE_*` CMake variables are **removed**. Remove them from your build scripts. The protocol is selected by calling the appropriate per-protocol init function.

**v1:**

```bash
cmake -DSERIAL_FLASHER_INTERFACE_SPI=1 ..
```

**v2:**

```bash
cmake ..   # no interface flag needed
```

In code, call the per-protocol init:

```c
esp_loader_init_spi(&loader, &my_spi_port_ops);
```

### ESP-IDF (Kconfig) Builds

The `SERIAL_FLASHER_INTERFACE_*` Kconfig choice is replaced by individual `SERIAL_FLASHER_PORT_*` booleans. Multiple ports can be enabled simultaneously. But custom port can be implemented without these options anyway.

**v1 (`sdkconfig`):**

```ini
CONFIG_SERIAL_FLASHER_INTERFACE_UART=y
```

**v2 (`sdkconfig`):**

```ini
CONFIG_SERIAL_FLASHER_PORT_UART=y
# Optionally enable additional ports:
# CONFIG_SERIAL_FLASHER_PORT_SPI=y
# CONFIG_SERIAL_FLASHER_PORT_SDIO=y
# CONFIG_SERIAL_FLASHER_PORT_USB_CDC_ACM=y
```

Each enabled port compiles its `*_port.c` file and exposes a `*_port_ops` vtable (e.g. `esp32_uart_port_ops`, `esp32_spi_port_ops`).

---

## 7. Conditionally-Compiled APIs are Now Always Available

In v1, several functions were only declared when the matching interface macro was defined (e.g. `esp_loader_flash_read` was only available with `SERIAL_FLASHER_INTERFACE_UART`). In v2 these guards are removed — every function is always declared. Calling a function on an unsupported protocol returns `ESP_LOADER_ERROR_UNSUPPORTED_FUNC` instead of causing a compile error.

This means you no longer need `#ifdef SERIAL_FLASHER_INTERFACE_UART` guards around calls to `esp_loader_connect_with_stub()`, `esp_loader_flash_deflate_*()`, `esp_loader_flash_read()`, etc.

---

## 8. `esp_loader_error_t` Header Change

`esp_loader_error_t` is now defined in `include/esp_loader_error.h` (previously it was part of `esp_loader.h`). Including `esp_loader.h` still works — it automatically includes `esp_loader_error.h`. No source changes are required unless you included `esp_loader_io.h` independently to get the error type; in that case add `#include "esp_loader_error.h"` to your port code.

---

## 9. Multiple Instances

Because all state is in `esp_loader_t`, you can now run multiple independent loader instances in the same program for example, only not simultaneously. Each instance needs its own `esp_loader_t` and its own `loader_port_*_init()` call.

---

## Complete Migration Example

### Before (v1)

```c
#include "esp_loader.h"
#include "esp32_port.h"

void app_main(void)
{
    const loader_esp32_config_t config = { .baud_rate = 115200, /* ... */ };
    loader_port_esp32_init(&config);

    esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
    if (esp_loader_connect(&args) != ESP_LOADER_SUCCESS) return;

    esp_loader_flash_start(0x10000, image_size, 4096);
    esp_loader_flash_write(image_data, image_size);
    esp_loader_flash_finish(true);
    esp_loader_reset_target();
}
```

### After (v2)

```c
#include "esp_loader.h"
#include "esp32_port.h"

void app_main(void)
{
    const loader_esp32_config_t config = { .baud_rate = 115200, /* ... */ };
    loader_port_esp32_init(&config);

    esp_loader_t loader;
    esp_loader_init_uart(&loader, &esp32_uart_port_ops);

    esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
    if (esp_loader_connect(&loader, &args) != ESP_LOADER_SUCCESS) return;

    esp_loader_flash_cfg_t flash_cfg = {
        .offset     = 0x10000,
        .image_size = image_size,
        .block_size = 4096,
    };
    if (esp_loader_flash_start(&loader, &flash_cfg) != ESP_LOADER_SUCCESS) return;

    size_t offset = 0;
    while (offset < image_size) {
        size_t chunk = MIN(flash_cfg.block_size, image_size - offset);
        if (esp_loader_flash_write(&loader, &flash_cfg, image_data + offset, chunk) != ESP_LOADER_SUCCESS) return;
        offset += chunk;
    }

    esp_loader_flash_finish(&loader, &flash_cfg);
    esp_loader_reset_target(&loader);
}
```
