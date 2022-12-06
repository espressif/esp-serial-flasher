/*
 * Copyright (c) 2022 KT-Elektronik, Klaucke und Partner GmbH
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

#include "serial_io.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <stdint.h>

typedef struct {
#ifdef __cplusplus
extern "C" {
#endif

    const struct device *uart_dev;
    const struct gpio_dt_spec enable_spec;
    const struct gpio_dt_spec boot_spec;
} loader_zephyr_config_t;

void loader_port_zephyr_init(loader_zephyr_config_t *config);

#ifdef __cplusplus
}
#endif

