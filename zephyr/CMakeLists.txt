cmake_minimum_required(VERSION 3.22)

if (CONFIG_ESP_SERIAL_FLASHER)
    zephyr_include_directories(
        "${ZEPHYR_CURRENT_MODULE_DIR}/include"
        "${ZEPHYR_CURRENT_MODULE_DIR}/port"
        "${ZEPHYR_CURRENT_MODULE_DIR}/private_include"
    )

    zephyr_interface_library_named(esp_flasher)

    zephyr_library()

    zephyr_library_sources(
                ${ZEPHYR_CURRENT_MODULE_DIR}/src/esp_loader.c
                ${ZEPHYR_CURRENT_MODULE_DIR}/src/esp_targets.c
                ${ZEPHYR_CURRENT_MODULE_DIR}/src/stubs/esp_stubs_table.c
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
                ${ZEPHYR_CURRENT_MODULE_DIR}/src/esp_sdio_stubs.c
                ${ZEPHYR_CURRENT_MODULE_DIR}/src/protocol_serial.c
                ${ZEPHYR_CURRENT_MODULE_DIR}/src/protocol_uart.c
                ${ZEPHYR_CURRENT_MODULE_DIR}/src/protocol_spi.c
                ${ZEPHYR_CURRENT_MODULE_DIR}/src/protocol_sdio.c
                ${ZEPHYR_CURRENT_MODULE_DIR}/src/slip.c
                ${ZEPHYR_CURRENT_MODULE_DIR}/src/md5_hash.c
                ${ZEPHYR_CURRENT_MODULE_DIR}/port/zephyr_port.c
    )

    zephyr_library_link_libraries(esp_flasher)

    if(DEFINED SERIAL_FLASHER_DEBUG_TRACE OR CONFIG_SERIAL_FLASHER_DEBUG_TRACE)
        target_compile_definitions(esp_flasher INTERFACE SERIAL_FLASHER_DEBUG_TRACE=1)
    endif()

    target_compile_definitions(esp_flasher
    INTERFACE
        SERIAL_FLASHER_WRITE_BLOCK_RETRIES=${CONFIG_SERIAL_FLASHER_WRITE_BLOCK_RETRIES}
    )

    if((DEFINED SERIAL_FLASHER_RESET_INVERT AND SERIAL_FLASHER_RESET_INVERT) OR CONFIG_SERIAL_FLASHER_RESET_INVERT)
        target_compile_definitions(esp_flasher INTERFACE SERIAL_FLASHER_RESET_INVERT=1)
    else()
        target_compile_definitions(esp_flasher INTERFACE SERIAL_FLASHER_RESET_INVERT=0)
    endif()

    if((DEFINED SERIAL_FLASHER_BOOT_INVERT AND SERIAL_FLASHER_BOOT_INVERT) OR CONFIG_SERIAL_FLASHER_BOOT_INVERT)
        target_compile_definitions(esp_flasher INTERFACE SERIAL_FLASHER_BOOT_INVERT=1)
    else()
        target_compile_definitions(esp_flasher INTERFACE SERIAL_FLASHER_BOOT_INVERT=0)
    endif()

endif()
