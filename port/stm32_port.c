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

#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/param.h>
#include <stdio.h>
#include "stm32_port.h"

static UART_HandleTypeDef *uart;
static GPIO_TypeDef *gpio_port_io0, *gpio_port_rst;
static uint16_t gpio_num_io0, gpio_num_rst;

#if SERIAL_FLASHER_DEBUG_TRACE
static void transfer_debug_print(const uint8_t *data, uint16_t size, bool write)
{
    static bool write_prev = false;

    if (write_prev != write) {
        write_prev = write;
        printf("\n--- %s ---\n", write ? "WRITE" : "READ");
    }

    for (uint32_t i = 0; i < size; i++) {
        printf("%02x ", data[i]);
    }
}
#endif

static uint32_t s_time_end;

static esp_loader_error_t stm32_uart_write(esp_loader_port_t *port, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    (void)port;
    HAL_StatusTypeDef err = HAL_UART_Transmit(uart, (uint8_t *)data, size, timeout);

    if (err == HAL_OK) {
#if SERIAL_FLASHER_DEBUG_TRACE
        transfer_debug_print(data, size, true);
#endif
        return ESP_LOADER_SUCCESS;
    } else if (err == HAL_TIMEOUT) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}


static esp_loader_error_t stm32_uart_read(esp_loader_port_t *port, uint8_t *data, uint16_t size, uint32_t timeout)
{
    (void)port;
    HAL_StatusTypeDef err = HAL_UART_Receive(uart, data, size, timeout);

    if (err == HAL_OK) {
#if SERIAL_FLASHER_DEBUG_TRACE
        transfer_debug_print(data, size, false);
#endif
        return ESP_LOADER_SUCCESS;
    } else if (err == HAL_TIMEOUT) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}

void loader_port_stm32_init(loader_stm32_config_t *config)
{
    uart = config->huart;
    gpio_port_io0 = config->port_io0;
    gpio_port_rst = config->port_rst;
    gpio_num_io0 = config->pin_num_io0;
    gpio_num_rst = config->pin_num_rst;
}

static void stm32_uart_delay_ms(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    HAL_Delay(ms);
}


static void stm32_uart_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    s_time_end = HAL_GetTick() + ms;
}


static uint32_t stm32_uart_remaining_time(esp_loader_port_t *port)
{
    (void)port;
    int32_t remaining = s_time_end - HAL_GetTick();
    return (remaining > 0) ? (uint32_t)remaining : 0;
}


static void stm32_uart_reset_target(esp_loader_port_t *port)
{
    (void)port;
    HAL_GPIO_WritePin(gpio_port_rst, gpio_num_rst, SERIAL_FLASHER_RESET_INVERT ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_Delay(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    HAL_GPIO_WritePin(gpio_port_rst, gpio_num_rst, SERIAL_FLASHER_RESET_INVERT ? GPIO_PIN_RESET : GPIO_PIN_SET);
}


static void stm32_uart_enter_bootloader(esp_loader_port_t *port)
{
    (void)port;
    HAL_GPIO_WritePin(gpio_port_io0, gpio_num_io0, SERIAL_FLASHER_BOOT_INVERT ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(gpio_port_rst, gpio_num_rst, SERIAL_FLASHER_RESET_INVERT ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_Delay(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    HAL_GPIO_WritePin(gpio_port_rst, gpio_num_rst, SERIAL_FLASHER_RESET_INVERT ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_Delay(SERIAL_FLASHER_BOOT_HOLD_TIME_MS);
    HAL_GPIO_WritePin(gpio_port_io0, gpio_num_io0, SERIAL_FLASHER_BOOT_INVERT ? GPIO_PIN_RESET : GPIO_PIN_SET);
}


static void stm32_uart_debug_print(esp_loader_port_t *port, const char *str)
{
    (void)port;
    printf("DEBUG: %s\n", str);
}

static esp_loader_error_t stm32_uart_change_rate(esp_loader_port_t *port, uint32_t baudrate)
{
    (void)port;
    uart->Init.BaudRate = baudrate;

    if ( HAL_UART_Init(uart) != HAL_OK ) {
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}

static const esp_loader_port_ops_t stm32_uart_ops = {
    .enter_bootloader         = stm32_uart_enter_bootloader,
    .reset_target             = stm32_uart_reset_target,
    .start_timer              = stm32_uart_start_timer,
    .remaining_time           = stm32_uart_remaining_time,
    .delay_ms                 = stm32_uart_delay_ms,
    .debug_print              = stm32_uart_debug_print,
    .change_transmission_rate = stm32_uart_change_rate,
    .write                    = stm32_uart_write,
    .read                     = stm32_uart_read,
};

esp_loader_port_t stm32_uart_port = { .ops = &stm32_uart_ops };
