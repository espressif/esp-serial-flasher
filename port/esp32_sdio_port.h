/* Copyright 2020-2024 Espressif Systems (Shanghai) CO LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "esp_loader_io.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SDIO_4BIT = 0,
    SDIO_1BIT,
} sdio_bus_width_t;

/**
 * @brief Concrete ESP32 SDIO port instance.
 *
 * Declare one of these, fill the config fields, then pass &port.port to
 * esp_loader_init_sdio(). Hardware initialisation is called automatically
 * inside esp_loader_init_sdio() — no separate init step is needed.
 *
 * @code
 *   esp32_sdio_port_t port = {
 *       .port.ops              = &esp32_sdio_ops,
 *       .slot                  = SDMMC_HOST_SLOT_1,
 *       .max_freq_khz          = SDMMC_FREQ_DEFAULT,
 *       .reset_trigger_pin     = GPIO_NUM_54,
 *       .boot_pin              = GPIO_NUM_53,
 *       .bus_width             = SDIO_4BIT,
 *   };
 *   esp_loader_t loader;
 *   esp_loader_init_sdio(&loader, &port.port);
 * @endcode
 */
typedef struct {
    esp_loader_port_t port;           /*!< Embedded port base */

    /* Configuration — fill before calling esp_loader_init_sdio() */
    int              slot;
    uint32_t         max_freq_khz;
    uint8_t          sdio_clk_pin;
    uint8_t          sdio_d0_pin;
    uint8_t          sdio_d1_pin;
    uint8_t          sdio_d2_pin;
    uint8_t          sdio_d3_pin;
    uint8_t          sdio_cmd_pin;
    uint8_t          reset_trigger_pin;
    uint8_t          boot_pin;
    bool             dont_initialize_host_driver;
    sdio_bus_width_t bus_width;

    /* Private runtime state — do not access directly */
    sdmmc_card_t  _card;
    sdmmc_host_t  _card_config;
    int64_t       _time_end;
    bool          _host_driver_needs_deinit;
} esp32_sdio_port_t;

/** Port operations vtable for the ESP32 SDIO port. */
extern const esp_loader_port_ops_t esp32_sdio_ops;

/**
 * @brief Wait for an SDIO interrupt from the target.
 *
 * @param port    Pointer to the SDIO port instance.
 * @param timeout Timeout in milliseconds.
 */
esp_loader_error_t loader_port_wait_int(esp32_sdio_port_t *port, uint32_t timeout);

#ifdef __cplusplus
}
#endif
