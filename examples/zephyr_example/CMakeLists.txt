# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_flasher)

zephyr_library_sources(
  ../common/example_common.c
  )

zephyr_library_include_directories(
  ../common
  )

set(target_firmware_dir ${CMAKE_SOURCE_DIR}/target-firmware)
include(${CMAKE_SOURCE_DIR}/../common/require_target_firmware.cmake)
esp_serial_flasher_require_target_firmware("zephyr_example" "${target_firmware_dir}"
    bootloader.bin
    partition-table.bin
    app.bin)

# Convert binary files to C source files using bin2array.cmake
include(${CMAKE_SOURCE_DIR}/../common/bin2array.cmake)
create_resources(${target_firmware_dir} ${CMAKE_BINARY_DIR}/target_firmware_data.c)
set_property(SOURCE ${CMAKE_BINARY_DIR}/target_firmware_data.c PROPERTY GENERATED 1)

target_sources(app PRIVATE
    src/main.c
    ${CMAKE_BINARY_DIR}/target_firmware_data.c
)
