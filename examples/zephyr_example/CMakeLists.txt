# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_flasher)

zephyr_compile_definitions_ifdef(CONFIG_SERIAL_FLASHER_MD5_ENABLED MD5_ENABLED)

target_sources(app PRIVATE
  ../common/example_common.c
  )

target_include_directories(app PRIVATE
  include
  ../common
  )

zephyr_linker_sources(SECTIONS sections-rom.ld)

include(${CMAKE_SOURCE_DIR}/include/bin_images_sections.cmake)
create_resources(${CMAKE_SOURCE_DIR}/target-firmware ${CMAKE_BINARY_DIR}/target_firmware_data.c)
set_property(SOURCE ${CMAKE_BINARY_DIR}/target_firmware_data.c PROPERTY GENERATED 1)

target_sources(app PRIVATE ${CMAKE_BINARY_DIR}/target_firmware_data.c)
target_sources(app PRIVATE src/main.c)

if(CONFIG_ESP_SERIAL_FLASHER_SHELL)
  target_sources(app PRIVATE src/shell.c)
endif()
