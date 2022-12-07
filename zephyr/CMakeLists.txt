if (CONFIG_ESP32_SERIAL_FLASHER)
    set(PORT ZEPHYR)
    add_subdirectory(${ZEPHYR_CURRENT_MODULE_DIR})
endif()