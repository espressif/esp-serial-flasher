/* Copyright 2020-2023 Espressif Systems (Shanghai) CO LTD
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
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Concrete ESP32 UART port instance.
 *
 * Declare one of these per target device, fill in the config fields,
 * then pass &my_port.port to esp_loader_init_uart(). Hardware initialisation
 * is called automatically inside esp_loader_init_uart() — there is no
 * separate init step.
 *
 * Multiple independent instances can coexist:
 * @code
 *   esp32_port_t port_a = {
 *       .port.ops          = &esp32_uart_ops,
 *       .baud_rate         = 115200,
 *       .uart_port         = UART_NUM_1,
 *       .uart_rx_pin       = GPIO_NUM_5,
 *       .uart_tx_pin       = GPIO_NUM_4,
 *       .reset_trigger_pin = GPIO_NUM_25,
 *       .gpio0_trigger_pin = GPIO_NUM_26,
 *   };
 *
 *   esp_loader_t loader;
 *   esp_loader_init_uart(&loader, &port_a.port);
 * @endcode
 */
typedef struct {
    esp_loader_port_t port;              /*!< Embedded port base — pass &port to esp_loader_init_* */

    /* Configuration — fill before calling esp_loader_init_uart() */
    uint32_t      baud_rate;             /*!< Initial baud rate */
    uint32_t      uart_port;             /*!< UART peripheral number (e.g. UART_NUM_1) */
    uint32_t      uart_rx_pin;           /*!< GPIO number for UART RX */
    uint32_t      uart_tx_pin;           /*!< GPIO number for UART TX */
    uint32_t      reset_trigger_pin;     /*!< GPIO used to reset the target */
    uint32_t      gpio0_trigger_pin;     /*!< GPIO used to control target IO0/BOOT */
    uint32_t      rx_buffer_size;        /*!< UART RX buffer size; 0 = use default (400) */
    uint32_t      tx_buffer_size;        /*!< UART TX buffer size; 0 = use default (400) */
    uint32_t      queue_size;            /*!< UART event queue depth; 0 = no queue */
    QueueHandle_t *uart_queue;           /*!< Written with the queue handle if non-NULL */
    bool          dont_initialize_peripheral; /*!< Skip UART driver init if already done */

    /* Private runtime state — do not access directly */
    int64_t       _time_end;
    bool          _peripheral_needs_deinit;
} esp32_port_t;

/** Port operations vtable for the ESP32 UART port. */
extern const esp_loader_port_ops_t esp32_uart_ops;

#ifdef __cplusplus
}
#endif
