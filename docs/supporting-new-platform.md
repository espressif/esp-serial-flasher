# Supporting New Host Platforms

This guide explains how to add support for new host microcontrollers or platforms to ESP Serial Flasher.

## Overview

ESP Serial Flasher uses a port layer abstraction to support different host platforms. To add support for a new host, implement the `esp_esp_loader_port_ops_t` vtable defined in `include/esp_loader_io.h` and expose it as a constant for application code to use.

## Port Operations Vtable

Port operations are split into per-protocol typed structs. Each struct embeds
`esp_esp_loader_port_ops_t` as its first member (`common`), which contains the
fields that are identical across all protocols. Provide the concrete
per-protocol type that matches the `esp_loader_init_*` function you intend to call.

```c
/* Common to all protocols (always the first member of the per-protocol structs) */
typedef struct {
    void               (*enter_bootloader)(void);
    void               (*reset_target)(void);
    void               (*start_timer)(uint32_t ms);
    uint32_t           (*remaining_time)(void);
    void               (*delay_ms)(uint32_t ms);
    void               (*debug_print)(const char *str);         /* NULL = no debug output */
    esp_loader_error_t (*change_transmission_rate)(uint32_t rate); /* NULL if not supported */
} esp_esp_loader_port_ops_t;

/* UART — pass to esp_loader_init_uart() */
typedef struct {
    esp_esp_loader_port_ops_t common;
    esp_loader_error_t (*write)(const uint8_t *data, uint16_t size, uint32_t timeout);
    esp_loader_error_t (*read)(uint8_t *data, uint16_t size, uint32_t timeout);
} esp_loader_uart_port_ops_t;

/* USB CDC-ACM — pass to esp_loader_init_usb() */
typedef struct {
    esp_esp_loader_port_ops_t common;
    esp_loader_error_t (*write)(const uint8_t *data, uint16_t size, uint32_t timeout);
    esp_loader_error_t (*read)(uint8_t *data, uint16_t size, uint32_t timeout);
} esp_loader_usb_port_ops_t;

/* SPI — pass to esp_loader_init_spi() */
typedef struct {
    esp_esp_loader_port_ops_t common;
    esp_loader_error_t (*write)(const uint8_t *data, uint16_t size, uint32_t timeout);
    esp_loader_error_t (*read)(uint8_t *data, uint16_t size, uint32_t timeout);
    void               (*spi_set_cs)(uint32_t level);
} esp_loader_spi_port_ops_t;

/* SDIO — pass to esp_loader_init_sdio() */
typedef struct {
    esp_esp_loader_port_ops_t common;
    esp_loader_error_t (*sdio_write)(uint32_t function, uint32_t addr,
                                     const uint8_t *data, uint16_t size, uint32_t timeout);
    esp_loader_error_t (*sdio_read)(uint32_t function, uint32_t addr,
                                    uint8_t *data, uint16_t size, uint32_t timeout);
    esp_loader_error_t (*sdio_card_init)(void);
} esp_loader_sdio_port_ops_t;
```

## Required Members

### `write` (UART, USB, SPI)

```c
esp_loader_error_t (*write)(const uint8_t *data, uint16_t size, uint32_t timeout);
```

Write bytes to the transport. Present in `esp_loader_uart_port_ops_t`,
`esp_loader_usb_port_ops_t`, and `esp_loader_spi_port_ops_t`; not present for
SDIO (use `sdio_write` instead).

- `data` — source buffer
- `size` — number of bytes to send
- `timeout` — timeout in milliseconds
- Returns `ESP_LOADER_SUCCESS` on success.

> [!NOTE]
> Should block until all bytes are queued/transmitted or an error occurs.

---

### `read` (UART, USB, SPI)

```c
esp_loader_error_t (*read)(uint8_t *data, uint16_t size, uint32_t timeout);
```

Read bytes from the transport. Present in `esp_loader_uart_port_ops_t`,
`esp_loader_usb_port_ops_t`, and `esp_loader_spi_port_ops_t`; not present for
SDIO (use `sdio_read` instead).

- `data` — destination buffer
- `size` — requested bytes
- `timeout` — maximum time to wait [ms]
- Returns `ESP_LOADER_SUCCESS` on success, `ESP_LOADER_ERROR_TIMEOUT` when the deadline expires before `size` bytes arrive.

> [!NOTE]
> Should block until at least 1 byte is read or `timeout` elapses. May be called repeatedly by higher layers until `size` is satisfied.

---

### `enter_bootloader`

```c
void (*enter_bootloader)(void);
```

Put the target device into ROM bootloader mode.

> [!NOTE]
> Typical UART sequence: assert BOOT low, hold RESET low for `SERIAL_FLASHER_RESET_HOLD_TIME_MS`, keep BOOT asserted for `SERIAL_FLASHER_BOOT_HOLD_TIME_MS`, then release. Respect `SERIAL_FLASHER_RESET_INVERT` and `SERIAL_FLASHER_BOOT_INVERT` settings when controlling GPIOs.

---

### `reset_target`

```c
void (*reset_target)(void);
```

Toggle the hardware reset pin to restart the target.

> [!NOTE]
> Reset pin should stay asserted for at least `SERIAL_FLASHER_RESET_HOLD_TIME_MS` milliseconds.

---

### `start_timer`

```c
void (*start_timer)(uint32_t ms);
```

Start a one-shot deadline timer.

- `ms` — timeout duration in milliseconds

> [!NOTE]
> Subsequent calls to `remaining_time` should reflect remaining time relative to this start.

---

### `remaining_time`

```c
uint32_t (*remaining_time)(void);
```

Return remaining milliseconds since the last `start_timer` call, or `0` if the timer has elapsed. Must never return a negative value.

---

### `delay_ms`

```c
void (*delay_ms)(uint32_t ms);
```

Block for the given number of milliseconds.

---

## Optional Members

Set to `NULL` when not needed.

### `debug_print`

```c
void (*debug_print)(const char *str);
```

Print a debug message string from the library. Route to your platform's logging/console facility. Pass `NULL` to suppress all debug output.

---

### `change_transmission_rate`

```c
esp_loader_error_t (*change_transmission_rate)(uint32_t rate);
```

Change the baud rate / peripheral clock speed. Used after initial sync to speed up transfers. Set to `NULL` for SDIO (rate is managed by the host driver).

---

## Interface-Specific Members

### SPI Interface

```c
void (*spi_set_cs)(uint32_t level);
```

Control the SPI chip-select pin. This field is only present in `esp_loader_spi_port_ops_t`.

---

### SDIO Interface

```c
esp_loader_error_t (*sdio_write)(uint32_t function, uint32_t addr,
                                 const uint8_t *data, uint16_t size, uint32_t timeout);

esp_loader_error_t (*sdio_read)(uint32_t function, uint32_t addr,
                                uint8_t *data, uint16_t size, uint32_t timeout);

esp_loader_error_t (*sdio_card_init)(void);
```

These fields are only present in `esp_loader_sdio_port_ops_t`.

- `sdio_write` / `sdio_read` handle SDIO function number and address routing.
- `sdio_card_init` initializes the SDIO bus (bus width, speed, interrupts).

---

## Implementation Steps

There are two main approaches to adding support for your platform:

### Option A: Contributing to the ESP Serial Flasher Repository

#### 1. Add Port Files

Place your implementation under `port/`:

```text
port/your_platform_port.c
port/your_platform_port.h
```

#### 2. Implement the Vtable

In `your_platform_port.c`, implement each required function as a `static` helper and assign it to a `const esp_esp_loader_port_ops_t`:

```c
#include "esp_loader_io.h"
#include "your_platform_port.h"

static esp_loader_error_t my_write(const uint8_t *data, uint16_t size, uint32_t timeout)
{
    // platform-specific write
}

static esp_loader_error_t my_read(uint8_t *data, uint16_t size, uint32_t timeout)
{
    // platform-specific read
}

static void my_enter_bootloader(void) { /* assert BOOT, toggle RESET */ }
static void my_reset_target(void)     { /* toggle RESET */ }
static void my_start_timer(uint32_t ms) { /* start deadline timer */ }
static uint32_t my_remaining_time(void) { /* return ms remaining */ }
static void my_delay_ms(uint32_t ms)    { /* blocking delay */ }
static void my_debug_print(const char *str) { printf("%s", str); }
static esp_loader_error_t my_change_rate(uint32_t rate) { /* reconfigure peripheral */ }

const esp_loader_uart_port_ops_t your_platform_port_ops = {
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

#### 3. Expose the Vtable in the Header

```c
// your_platform_port.h
#pragma once
#include "esp_loader_io.h"

typedef struct { /* your platform config */ } your_platform_config_t;

/** Port operations vtable for your platform (UART). */
extern const esp_loader_uart_port_ops_t your_platform_port_ops;

esp_loader_error_t loader_port_your_platform_init(const your_platform_config_t *config);
void               loader_port_your_platform_deinit(void);
```

#### 4. Update the Build System

Add your port to the main `CMakeLists.txt`:

```cmake
elseif(PORT STREQUAL "YOUR_PLATFORM")
    list(APPEND flasher_srcs "port/your_platform_port.c")
    # Add any platform-specific dependencies here
```

---

### Option B: Using ESP Serial Flasher as an External Library

This approach uses ESP Serial Flasher as a submodule or external dependency in your own project.

#### 1. Add ESP Serial Flasher to Your Project

```bash
git submodule add https://github.com/espressif/esp-serial-flasher.git external/esp-serial-flasher
git submodule update --init --recursive
```

#### 2. Implement the Port in Your Source Tree

```text
your_project/
├── main/
│   ├── main.c                # Your application code
│   ├── your_port.c           # Port implementation
│   ├── your_port.h           # Vtable export + init/deinit declarations
│   └── CMakeLists.txt
├── external/
│   └── esp-serial-flasher/   # Git submodule
├── CMakeLists.txt
└── .gitmodules
```

#### 3. Configure the Build System

**Root CMakeLists.txt:**

```cmake
cmake_minimum_required(VERSION 3.22)
project(your_project C)

set(PORT USER_DEFINED)

add_subdirectory(external/esp-serial-flasher)
add_subdirectory(main)
```

**main/CMakeLists.txt:**

```cmake
add_executable(your_app main.c)

target_sources(flasher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/your_port.c
)

target_include_directories(your_app PRIVATE
    ${CMAKE_SOURCE_DIR}/external/esp-serial-flasher/include
)

target_include_directories(flasher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/external/esp-serial-flasher/include
)

target_link_libraries(your_app PRIVATE flasher)
```

**Key Points:**

- Set `PORT=USER_DEFINED` to tell ESP Serial Flasher to expect an external port implementation.
- Add your port `.c` file to the `flasher` target via `target_sources(flasher PRIVATE ...)`.

#### 4. Use the Port in Application Code

```c
#include "esp_loader.h"
#include "your_port.h"

void app_main(void)
{
    const your_platform_config_t cfg = { /* ... */ };
    loader_port_your_platform_init(&cfg);

    esp_loader_t loader;
    esp_loader_init_uart(&loader, &your_platform_port_ops);

    esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
    esp_loader_connect(&loader, &args);

    // flash, read, etc. ...
}
```

---

## Submitting Your Port

### Before Submitting

1. Test thoroughly on your platform.
2. Follow coding standards (see [Contributing Guide](contributing.md)).
3. Add documentation for your platform.
4. Provide or reference a minimal example demonstrating usage.

### Pull Request

1. Open an issue first to discuss the new platform support.
2. Create a pull request with your implementation.
3. Include an example and documentation.
4. Provide testing results.

---

## Getting Help

### Resources

- Study existing port implementations in `port/` — `port/raspberry_port.c` is a good minimal example.
- See `include/esp_loader_io.h` for the vtable definition and member documentation.
- Review `examples/` for usage patterns.

### Support

- Open a [GitHub issue](https://github.com/espressif/esp-serial-flasher/issues) for questions.
