# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)

if (CONFIG_ESP_SERIAL_FLASHER)

    zephyr_interface_library_named(esp_serial_flasher)
    zephyr_library_link_libraries(esp_serial_flasher)

    zephyr_compile_definitions_ifdef(CONFIG_SERIAL_FLASHER_WRITE_BLOCK_RETRIES
        SERIAL_FLASHER_WRITE_BLOCK_RETRIES=${CONFIG_SERIAL_FLASHER_WRITE_BLOCK_RETRIES}
    )

    zephyr_compile_definitions(
        SERIAL_FLASHER_LOG_LEVEL=${CONFIG_SERIAL_FLASHER_LOG_LEVEL}
    )

    zephyr_include_directories(
        ${ZEPHYR_CURRENT_MODULE_DIR}/include
        ${ZEPHYR_CURRENT_MODULE_DIR}/port
        ${ZEPHYR_CURRENT_MODULE_DIR}/private_include
    )

    set(zephyr_stub_sources
        ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stubs_table.c
    )

    if (CONFIG_SERIAL_FLASHER_BUNDLE_ALL_STUBS)
        list(APPEND zephyr_stub_defs ESP_STUB_BUNDLE_ALL)
        list(APPEND zephyr_stub_sources
            ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp8266.c
            ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32.c
            ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32s2.c
            ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32c3.c
            ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32s3.c
            ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32c2.c
            ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32c5.c
            ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32h2.c
            ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32c6.c
            ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32p4.c
            ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32p4rev1.c
            ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32c61.c
        )
    else()
        if (CONFIG_SERIAL_FLASHER_BUNDLE_STUB_ESP8266)
            list(APPEND zephyr_stub_defs ESP_STUB_BUNDLE_ESP8266)
            list(APPEND zephyr_stub_sources ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp8266.c)
        endif()
        if (CONFIG_SERIAL_FLASHER_BUNDLE_STUB_ESP32)
            list(APPEND zephyr_stub_defs ESP_STUB_BUNDLE_ESP32)
            list(APPEND zephyr_stub_sources ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32.c)
        endif()
        if (CONFIG_SERIAL_FLASHER_BUNDLE_STUB_ESP32S2)
            list(APPEND zephyr_stub_defs ESP_STUB_BUNDLE_ESP32S2)
            list(APPEND zephyr_stub_sources ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32s2.c)
        endif()
        if (CONFIG_SERIAL_FLASHER_BUNDLE_STUB_ESP32C3)
            list(APPEND zephyr_stub_defs ESP_STUB_BUNDLE_ESP32C3)
            list(APPEND zephyr_stub_sources ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32c3.c)
        endif()
        if (CONFIG_SERIAL_FLASHER_BUNDLE_STUB_ESP32S3)
            list(APPEND zephyr_stub_defs ESP_STUB_BUNDLE_ESP32S3)
            list(APPEND zephyr_stub_sources ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32s3.c)
        endif()
        if (CONFIG_SERIAL_FLASHER_BUNDLE_STUB_ESP32C2)
            list(APPEND zephyr_stub_defs ESP_STUB_BUNDLE_ESP32C2)
            list(APPEND zephyr_stub_sources ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32c2.c)
        endif()
        if (CONFIG_SERIAL_FLASHER_BUNDLE_STUB_ESP32C5)
            list(APPEND zephyr_stub_defs ESP_STUB_BUNDLE_ESP32C5)
            list(APPEND zephyr_stub_sources ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32c5.c)
        endif()
        if (CONFIG_SERIAL_FLASHER_BUNDLE_STUB_ESP32H2)
            list(APPEND zephyr_stub_defs ESP_STUB_BUNDLE_ESP32H2)
            list(APPEND zephyr_stub_sources ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32h2.c)
        endif()
        if (CONFIG_SERIAL_FLASHER_BUNDLE_STUB_ESP32C6)
            list(APPEND zephyr_stub_defs ESP_STUB_BUNDLE_ESP32C6)
            list(APPEND zephyr_stub_sources ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32c6.c)
        endif()
        if (CONFIG_SERIAL_FLASHER_BUNDLE_STUB_ESP32P4)
            list(APPEND zephyr_stub_defs ESP_STUB_BUNDLE_ESP32P4)
            list(APPEND zephyr_stub_sources
                ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32p4.c
                ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32p4rev1.c
            )
        endif()
        if (CONFIG_SERIAL_FLASHER_BUNDLE_STUB_ESP32C61)
            list(APPEND zephyr_stub_defs ESP_STUB_BUNDLE_ESP32C61)
            list(APPEND zephyr_stub_sources ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stub_esp32c61.c)
        endif()
    endif()

    zephyr_compile_definitions(${zephyr_stub_defs})

    zephyr_library_sources(
        ${ZEPHYR_CURRENT_MODULE_DIR}/port/zephyr_port.c
        ${ZEPHYR_CURRENT_MODULE_DIR}/src/md5_hash.c
        ${ZEPHYR_CURRENT_MODULE_DIR}/src/esp_loader.c
        ${ZEPHYR_CURRENT_MODULE_DIR}/src/protocol_serial.c
        ${ZEPHYR_CURRENT_MODULE_DIR}/src/esp_targets.c
        ${zephyr_stub_sources}
        ${ZEPHYR_CURRENT_MODULE_DIR}/src/protocol_uart.c
        ${ZEPHYR_CURRENT_MODULE_DIR}/src/slip.c
    )

endif()
