#pragma once

#include "serial_io.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <stdint.h>

typedef struct {
    const struct device *uart_dev;
    const struct gpio_dt_spec enable_spec;
    const struct gpio_dt_spec boot_spec;
} loader_zephyr_config_t;

void loader_port_zephyr_init(loader_zephyr_config_t *config);
