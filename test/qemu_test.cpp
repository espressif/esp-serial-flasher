/*
 * SPDX-FileCopyrightText: 2018-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "catch.hpp"
#include "test_port.h"
#include "esp_loader.h"
#include "esp_loader_io.h"
#include <algorithm>
#include <iostream>
#include <fstream>

using namespace std;


#define ESP_ERR_CHECK(exp) REQUIRE( (exp) == ESP_LOADER_SUCCESS )

const uint32_t APP_START_ADDRESS = 0x10000;

static esp_loader_t g_loader;

static const esp_stub_t *stub_from_context(esp_loader_t *loader, target_chip_t chip, void *ctx)
{
    (void)loader;
    (void)chip;
    return static_cast<const esp_stub_t *>(ctx);
}


TEST_CASE( "Can connect " )
{
    esp_loader_init_serial(&g_loader, &test_tcp_port.port);

    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();

    ESP_ERR_CHECK( esp_loader_connect(&g_loader, &connect_config) );
    REQUIRE( esp_loader_get_target(&g_loader) == ESP32_CHIP );
}


TEST_CASE( "Can reject NULL flash stub provider" )
{
    esp_loader_t loader;
    ESP_ERR_CHECK( esp_loader_init_serial(&loader, &test_tcp_port.port) );

    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();

    REQUIRE( esp_loader_connect_with_stub_provider(&loader, &connect_config, nullptr, nullptr)
             == ESP_LOADER_ERROR_INVALID_PARAM );
}


TEST_CASE( "Can reject BYO flash stub with zero entrypoint" )
{
    esp_loader_t loader;
    ESP_ERR_CHECK( esp_loader_init_serial(&loader, &test_tcp_port.port) );

    const esp_loader_bin_segment_t segment = {};
    esp_stub_t external_stub = {
        .segments = &segment,
        .segment_count = 1,
    };
    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();

    REQUIRE( esp_loader_connect_with_stub_provider(&loader, &connect_config,
             stub_from_context, &external_stub)
             == ESP_LOADER_ERROR_INVALID_PARAM );
}


TEST_CASE( "Can reject BYO flash stub with NULL segments" )
{
    esp_loader_t loader;
    ESP_ERR_CHECK( esp_loader_init_serial(&loader, &test_tcp_port.port) );

    esp_stub_t external_stub = {};
    external_stub.header.entrypoint = 0x40080000;
    external_stub.segment_count = 1;
    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();

    REQUIRE( esp_loader_connect_with_stub_provider(&loader, &connect_config,
             stub_from_context, &external_stub)
             == ESP_LOADER_ERROR_INVALID_PARAM );
}


TEST_CASE( "Can reject BYO flash stub with no segments" )
{
    esp_loader_t loader;
    ESP_ERR_CHECK( esp_loader_init_serial(&loader, &test_tcp_port.port) );

    const esp_loader_bin_segment_t segment = {};
    esp_stub_t external_stub = {};
    external_stub.header.entrypoint = 0x40080000;
    external_stub.segments = &segment;
    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();

    REQUIRE( esp_loader_connect_with_stub_provider(&loader, &connect_config,
             stub_from_context, &external_stub)
             == ESP_LOADER_ERROR_INVALID_PARAM );
}


TEST_CASE( "Can reject BYO flash stub segment with NULL data" )
{
    esp_loader_t loader;
    ESP_ERR_CHECK( esp_loader_init_serial(&loader, &test_tcp_port.port) );

    const uint8_t valid_data[] = { 0, 0, 0, 0 };
    const esp_loader_bin_segment_t segments[] = {
        {
            .addr = 0x40080000,
            .size = sizeof(valid_data),
            .data = valid_data,
        },
        {
            .addr = 0x40090000,
            .size = 4,
            .data = nullptr,
        },
    };
    esp_stub_t external_stub = {
        .header = {
            .entrypoint = 0x40080000,
        },
        .segments = segments,
        .segment_count = sizeof(segments) / sizeof(segments[0]),
    };

    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();

    REQUIRE( esp_loader_connect_with_stub_provider(&loader, &connect_config,
             stub_from_context, &external_stub)
             == ESP_LOADER_ERROR_INVALID_PARAM );
}


TEST_CASE( "Can reject BYO flash stub with zero total segment size" )
{
    esp_loader_t loader;
    ESP_ERR_CHECK( esp_loader_init_serial(&loader, &test_tcp_port.port) );

    const esp_loader_bin_segment_t segments[] = {
        {},
        {},
    };
    esp_stub_t external_stub = {
        .header = {
            .entrypoint = 0x40080000,
        },
        .segments = segments,
        .segment_count = sizeof(segments) / sizeof(segments[0]),
    };

    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();

    REQUIRE( esp_loader_connect_with_stub_provider(&loader, &connect_config,
             stub_from_context, &external_stub)
             == ESP_LOADER_ERROR_INVALID_PARAM );
}


inline auto file_size_is(ifstream &file)
{
    uint32_t file_size;

    file.seekg(0, ios::end);
    file_size = file.tellg();
    file.seekg(0);

    return file_size;
}

void flash_application(esp_loader_t *loader, ifstream &image)
{
    uint8_t payload[1024];
    int32_t count = 0;
    size_t image_size = file_size_is(image);

    esp_loader_flash_cfg_t flash_cfg = {
        .offset     = APP_START_ADDRESS,
        .image_size = (uint32_t)image_size,
        .block_size = sizeof(payload),
        .skip_verify = false,
    };

    ESP_ERR_CHECK( esp_loader_flash_start(loader, &flash_cfg) );

    while (image_size > 0) {
        size_t to_read = min(image_size, sizeof(payload));

        image.read((char *)payload, to_read);

        ESP_ERR_CHECK( esp_loader_flash_write(loader, &flash_cfg, payload, (uint32_t)to_read) );

        cout << endl << "--- FLASH DATA PACKET: " << count++
             << " DATA WRITTEN: " << to_read << " ---" << endl;

        image_size -= to_read;
    }

    ESP_ERR_CHECK( esp_loader_flash_finish(loader, &flash_cfg) );
}

bool file_compare(ifstream &file_1, ifstream &file_2, size_t file_size)
{
    vector<char> file_data_1(file_size);
    vector<char> file_data_2(file_size);

    file_1.read((char *) &file_data_1[0], file_size);
    file_2.read((char *) &file_data_2[0], file_size);

    return file_data_1 == file_data_2;
}


TEST_CASE( "Can write application to flash" )
{
    ifstream new_image;
    ifstream qemu_image;

    new_image.open ("../hello-world.bin", ios::binary | ios::in);
    qemu_image.open ("empty_file.bin", ios::binary | ios::in);

    REQUIRE ( new_image.is_open() );
    REQUIRE ( qemu_image.is_open() );

    flash_application(&g_loader, new_image);

    auto new_image_size = file_size_is(new_image);
    qemu_image.seekg(APP_START_ADDRESS);
    new_image.seekg(0);

    REQUIRE ( file_compare(new_image, qemu_image, new_image_size) );
}

TEST_CASE( "Can write and read register" )
{
    uint32_t reg_value = 0;
    uint32_t SPI_MOSI_DLEN_REG = 0x60002000 + 0x28;

    ESP_ERR_CHECK( esp_loader_write_register(&g_loader, SPI_MOSI_DLEN_REG, 55) );
    ESP_ERR_CHECK( esp_loader_read_register(&g_loader, SPI_MOSI_DLEN_REG, &reg_value) );
    REQUIRE ( reg_value == 55 );
}
