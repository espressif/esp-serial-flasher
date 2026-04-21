/*
 * SPDX-FileCopyrightText: 2018-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// #define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_RUNNER

#include "catch.hpp"
#include "esp_loader_io.h"
#include "test_port.h"


int main( int argc, char *argv[] )
{

    // global setup...
    if ( esp_loader_port_test_init(&test_tcp_port) != ESP_LOADER_SUCCESS ) {
        std::cout << "Serial initialization failed";
        return 0;
    }

    int result = Catch::Session().run( argc, argv );

    // global clean-up...
    esp_loader_port_test_deinit(&test_tcp_port);

    return result;
}
