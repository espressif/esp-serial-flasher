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

#include <stdint.h>
#include "esp_loader_io.h"

#if defined STM32C0
#include "stm32c0xx_hal.h"
#elif defined STM32F0
#include "stm32f0xx_hal.h"
#elif defined STM32F1
#include "stm32f1xx_hal.h"
#elif defined STM32F2
#include "stm32f2xx_hal.h"
#elif defined STM32F3
#include "stm32f3xx_hal.h"
#elif defined STM32F4
#include "stm32f4xx_hal.h"
#elif defined STM32F7
#include "stm32f7xx_hal.h"
#elif defined STM32G0
#include "stm32g0xx_hal.h"
#elif defined STM32G4
#include "stm32g4xx_hal.h"
#elif defined STM32H5
#include "stm32h5xx_hal.h"
#elif defined STM32H7
#include "stm32h7xx_hal.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Concrete STM32 UART port instance.
 *
 * Declare one of these, fill the config fields, then pass &port.port to
 * esp_loader_init_uart(). Hardware initialisation is called automatically
 * inside esp_loader_init_uart() — no separate init step is needed.
 *
 * @code
 *   stm32_port_t port = {
 *       .port.ops    = &stm32_uart_ops,
 *       .huart       = &huart2,
 *       .port_boot    = TARGET_BOOT_GPIO_Port,
 *       .pin_num_boot = TARGET_BOOT_Pin,
 *       .port_rst    = TARGET_RESET_GPIO_Port,
 *       .pin_num_rst = TARGET_RESET_Pin,
 *   };
 *   esp_loader_t loader;
 *   esp_loader_init_uart(&loader, &port.port);
 * @endcode
 */
typedef struct {
    esp_loader_port_t   port;         /*!< Embedded port base */

    /* Configuration — fill before calling esp_loader_init_uart() */
    UART_HandleTypeDef *huart;
    GPIO_TypeDef       *port_boot;
    uint16_t            pin_num_boot;
    GPIO_TypeDef       *port_rst;
    uint16_t            pin_num_rst;

    /* Private runtime state — do not access directly */
    uint32_t _time_end;
} stm32_port_t;

/** Port operations vtable for the STM32 UART port. */
extern const esp_loader_port_ops_t stm32_uart_ops;

#ifdef __cplusplus
}
#endif
