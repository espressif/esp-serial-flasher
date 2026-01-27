# Copyright (c) 2026 Espressif Systems (Shanghai) Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)

if (CONFIG_ESP_SERIAL_FLASHER)

    zephyr_interface_library_named(esp_flasher)
    zephyr_library_link_libraries(esp_flasher)

    # Align all compile definitions to Kconfig
    zephyr_compile_definitions(SERIAL_FLASHER_INTERFACE_UART=1)

    zephyr_compile_definitions_ifdef(CONFIG_SERIAL_FLASHER_MD5_ENABLED
        MD5_ENABLED=1
    )

    zephyr_compile_definitions_ifdef(CONFIG_SERIAL_FLASHER_DEBUG_TRACE
        SERIAL_FLASHER_DEBUG_TRACE=1
    )

    zephyr_compile_definitions_ifdef(CONFIG_SERIAL_FLASHER_WRITE_BLOCK_RETRIES
        SERIAL_FLASHER_WRITE_BLOCK_RETRIES=${CONFIG_SERIAL_FLASHER_WRITE_BLOCK_RETRIES}
    )

    if (CONFIG_SERIAL_FLASHER_RESET_INVERT)
        zephyr_compile_definitions(
            SERIAL_FLASHER_RESET_INVERT=1
        )
    else()
        zephyr_compile_definitions(
            SERIAL_FLASHER_RESET_INVERT=0
        )
    endif()

    if (CONFIG_SERIAL_FLASHER_BOOT_INVERT)
        zephyr_compile_definitions(
            SERIAL_FLASHER_BOOT_INVERT=1
        )
    else()
        zephyr_compile_definitions(
            SERIAL_FLASHER_BOOT_INVERT=0
        )
    endif()

    zephyr_include_directories(
        ${ZEPHYR_CURRENT_MODULE_DIR}/include
        ${ZEPHYR_CURRENT_MODULE_DIR}/port
        ${ZEPHYR_CURRENT_MODULE_DIR}/private_include
    )

    zephyr_library_sources(
        ${ZEPHYR_CURRENT_MODULE_DIR}/port/zephyr_port.c
        ${ZEPHYR_CURRENT_MODULE_DIR}/src/md5_hash.c
        ${ZEPHYR_CURRENT_MODULE_DIR}/src/esp_loader.c
        ${ZEPHYR_CURRENT_MODULE_DIR}/src/protocol_serial.c
    )

    zephyr_library_sources(
        ${ZEPHYR_CURRENT_MODULE_DIR}/src/esp_targets.c
        ${ZEPHYR_CURRENT_MODULE_DIR}/src/esp_stubs.c
        ${ZEPHYR_CURRENT_MODULE_DIR}/src/protocol_uart.c
        ${ZEPHYR_CURRENT_MODULE_DIR}/src/slip.c
    )

endif()
