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

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr_port.h>
#include <esp_loader.h>

static const struct device *esp_uart_dev = DEVICE_DT_GET(...);

static const struct gpio_dt_spec esp_enable_spec = GPIO_DT_SPEC_GET(...);

static const struct gpio_dt_spec esp_boot_spec = GPIO_DT_SPEC_GET(...);

esp_loader_error_t flashESPBinary(const uint8_t* bin, size_t size, size_t address)
{
	esp_loader_error_t err;
	static uint8_t payload[1024];
	const uint8_t *bin_addr = bin;

	LOG_INF("Erasing ESP flash (this may take a while)...");
	err = esp_loader_flash_start(address, size, sizeof(payload));
	if (err != ESP_LOADER_SUCCESS) {
		LOG_ERR("Erasing ESP flash failed. Error %d", err);
		return err;
	}
	LOG_INF("Start programming ESP");

	while (size > 0) {
		size_t to_read = MIN(size, sizeof(payload));
		memcpy(payload, bin_addr, to_read);

		err = esp_loader_flash_write(payload, to_read);
		if (err != ESP_LOADER_SUCCESS) {
			LOG_ERR("ESP packet could not be written. Error %d", err);
			return err;
		}

		size -= to_read;
		bin_addr += to_read;
	};

	LOG_INF("ESP finished programming");

#if MD5_ENABLED
	err = esp_loader_flash_verify();
	if (err == ESP_LOADER_ERROR_UNSUPPORTED_FUNC) {
		LOG_INF("ESP8266 does not support flash verify command");
		return err;
	} else if (err != ESP_LOADER_SUCCESS) {
		LOG_ERR("ESP MD5 does not match. Error %d", err);
		return err;
	}
	LOG_INF("ESP flash verified");
#endif

	return ESP_LOADER_SUCCESS;
}

esp_loader_error_t espFlashBegin()
{
	loader_zephyr_config_t initArgs = {
		.uart_dev = esp_uart_dev,
		.enable_spec = esp_enable_spec,
		.boot_spec = esp_boot_spec
	};

	loader_port_zephyr_init(&initArgs);
	loader_port_change_baudrate(115200);

	esp_loader_connect_args_t connectArgs = ESP_LOADER_CONNECT_DEFAULT();
	esp_loader_error_t res = ESP_LOADER_SUCCESS;

	if (res == ESP_LOADER_SUCCESS) {
		res = esp_loader_connect(&connectArgs);
		if (res != ESP_LOADER_SUCCESS) {
			LOG_ERR("ESP loader connect failed. Error %d", (int)res);
		}
	}

	const int higherBaudrate = 230400;

	if (res == ESP_LOADER_SUCCESS) {
		res = esp_loader_change_baudrate(higherBaudrate);
		if (res != ESP_LOADER_SUCCESS) {
			LOG_ERR("ESP loader change baudrate failed. Error %d", (int)res);
		}
	}
	if (res == ESP_LOADER_SUCCESS) {
		res = loader_port_change_baudrate(higherBaudrate);
		if (res != ESP_LOADER_SUCCESS) {
			LOG_ERR("ESP loader: Change host UART baudrate failed. Error %d", (int)res);
		}
	}

	return res;
}

void espFlashFinish()
{
	gpio_pin_set_dt(&esp_enable_spec, false);
	loader_port_change_baudrate(115200);
}

void zephyr_example(const uint8_t* bin, size_t size, size_t address)
{
	if (!device_is_ready(esp_uart_dev)) {
		LOG_ERR("ESP UART not ready.");
		return;
	}
	if (!device_is_ready(esp_boot_spec.port)) {
		LOG_ERR("ESP boot GPIO not ready");
		return;
	}
	if (!device_is_ready(esp_enable_spec.port)) {
		LOG_ERR("Bluetooth Enable GPIO not ready");
		return;
	}
	gpio_pin_configure_dt(&esp_boot_spec, GPIO_OUTPUT_ACTIVE);
	gpio_pin_configure_dt(&esp_enable_spec, GPIO_OUTPUT_INACTIVE);

	if (espFlashBegin() == ESP_LOADER_SUCCESS) {
		esp_loader_error_t res = flashESPBinary(
			bin, size, address);
		if (res != ESP_LOADER_SUCCESS) {
			LOG_ERR("ESP loader flash failed. Error %d", (int)res);
		}
		espFlashFinish();
	}
}

