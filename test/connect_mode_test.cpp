/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "catch.hpp"
#include "esp_loader.h"
#include "esp_loader_protocol.h"

static int s_enter_bootloader_calls;
static int s_initialize_calls;

static void fake_enter_bootloader(esp_loader_port_t *port)
{
    (void)port;
    s_enter_bootloader_calls++;
}

static esp_loader_error_t fake_initialize_conn(esp_loader_t *loader, esp_loader_connect_args_t *args)
{
    (void)loader;
    (void)args;
    s_initialize_calls++;
    return ESP_LOADER_ERROR_TIMEOUT;
}

static const esp_loader_port_ops_t s_port_ops = {
    nullptr,
    nullptr,
    fake_enter_bootloader,
};

static const esp_loader_protocol_ops_t s_protocol_ops = {
    fake_initialize_conn,
};

static esp_loader_t make_loader()
{
    static esp_loader_port_t port = { &s_port_ops };
    esp_loader_t loader = {};
    loader._protocol = &s_protocol_ops;
    loader._port = &port;
    loader._protocol_type = ESP_LOADER_PROTOCOL_SERIAL;
    return loader;
}

static void reset_counters()
{
    s_enter_bootloader_calls = 0;
    s_initialize_calls = 0;
}

TEST_CASE("Reset mode enters bootloader before connecting", "[connect-mode]")
{
    reset_counters();
    esp_loader_t loader = make_loader();
    esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();

    REQUIRE(esp_loader_connect(&loader, &args) == ESP_LOADER_ERROR_TIMEOUT);
    REQUIRE(s_enter_bootloader_calls == 1);
    REQUIRE(s_initialize_calls == 1);
}

TEST_CASE("No-reset mode never enters bootloader", "[connect-mode]")
{
    reset_counters();
    esp_loader_t loader = make_loader();
    esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
    args.mode = ESP_LOADER_CONNECT_NO_RESET;

    REQUIRE(esp_loader_connect(&loader, &args) == ESP_LOADER_ERROR_TIMEOUT);
    REQUIRE(s_enter_bootloader_calls == 0);
    REQUIRE(s_initialize_calls == 1);
}

TEST_CASE("Caller can retry with reset after no-reset failure", "[connect-mode]")
{
    reset_counters();
    esp_loader_t loader = make_loader();
    esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
    args.mode = ESP_LOADER_CONNECT_NO_RESET;

    REQUIRE(esp_loader_connect(&loader, &args) == ESP_LOADER_ERROR_TIMEOUT);

    args.mode = ESP_LOADER_CONNECT_RESET;
    REQUIRE(esp_loader_connect(&loader, &args) == ESP_LOADER_ERROR_TIMEOUT);
    REQUIRE(s_enter_bootloader_calls == 1);
    REQUIRE(s_initialize_calls == 2);
}

TEST_CASE("Invalid connect mode is rejected", "[connect-mode]")
{
    reset_counters();
    esp_loader_t loader = make_loader();
    esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
    args.mode = static_cast<esp_loader_connect_mode_t>(-1);

    REQUIRE(esp_loader_connect(&loader, &args) == ESP_LOADER_ERROR_INVALID_PARAM);
    REQUIRE(s_enter_bootloader_calls == 0);
    REQUIRE(s_initialize_calls == 0);
}
