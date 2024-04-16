cmake_minimum_required(VERSION 3.5)

set(srcs
    src/esp_targets.c
    src/md5_hash.c
    src/esp_loader.c
    src/protocol_common.c
)
set(defs)

# Regular CMake builds are expected to set OPTION_NAME, while ESP-IDF's Kconfig system sets
# CONFIG_OPTION_NAME.
# This macro handles these differences and also a default value if neither is set
macro(add_option option_name default_value)
    if (DEFINED CONFIG_${option_name})
        # Handle Kconfig booleans in a special manner
        if (CONFIG_${option_name} STREQUAL "")
            list(APPEND defs ${option_name}=false)
        elseif(CONFIG_${option_name} STREQUAL "y")
            list(APPEND defs ${option_name}=true)
        else()
            list(APPEND defs ${option_name}=${CONFIG_${option_name}})
        endif()
    elseif (DEFINED ${option_name})
        list(APPEND defs ${option_name}=${${option_name}})
    else()
        list(APPEND defs ${option_name}=${default_value})
    endif()
endmacro()

add_option(SERIAL_FLASHER_DEBUG_TRACE false)
add_option(SERIAL_FLASHER_RESET_HOLD_TIME_MS 100)
add_option(SERIAL_FLASHER_BOOT_HOLD_TIME_MS 50)
add_option(SERIAL_FLASHER_WRITE_BLOCK_RETRIES 3)


# Enforce default interface for non-ESP ports.
# This doesn't need to be done for the ESP port as the Kconfig system handles the default.
if (NOT DEFINED ESP_PLATFORM AND
    NOT DEFINED SERIAL_FLASHER_INTERFACE_UART AND
    NOT DEFINED SERIAL_FLASHER_INTERFACE_SPI AND
    NOT DEFINED SERIAL_FLASHER_INTERFACE_USB)

    set(SERIAL_FLASHER_INTERFACE_UART true)
endif()

if (DEFINED SERIAL_FLASHER_INTERFACE_UART OR CONFIG_SERIAL_FLASHER_INTERFACE_UART STREQUAL "y")
    list(APPEND srcs
        src/protocol_uart.c
        src/slip.c
    )
    list(APPEND defs
        SERIAL_FLASHER_INTERFACE_UART
    )
    add_option(MD5_ENABLED false)

elseif(DEFINED SERIAL_FLASHER_INTERFACE_USB OR CONFIG_SERIAL_FLASHER_INTERFACE_USB STREQUAL "y")
    list(APPEND srcs
        src/protocol_uart.c
        src/slip.c
    )
    list(APPEND defs
        SERIAL_FLASHER_INTERFACE_USB
    )
    add_option(MD5_ENABLED false)

elseif(DEFINED SERIAL_FLASHER_INTERFACE_SPI OR CONFIG_SERIAL_FLASHER_INTERFACE_SPI STREQUAL "y")
    list(APPEND srcs
        src/protocol_spi.c
    )
    list(APPEND defs
        SERIAL_FLASHER_INTERFACE_SPI
    )
endif()

if (DEFINED ESP_PLATFORM)
    if (${CONFIG_SERIAL_FLASHER_INTERFACE_UART})
        list(APPEND srcs
            port/esp32_port.c
        )
    elseif (${CONFIG_SERIAL_FLASHER_INTERFACE_SPI})
        list(APPEND srcs
            port/esp32_spi_port.c
        )
    elseif (${CONFIG_SERIAL_FLASHER_INTERFACE_USB})
        list(APPEND srcs
            port/esp32_usb_cdc_acm_port.c
        )
    endif()

    # Register component to esp-idf build system
    idf_component_register(SRCS ${srcs}
                           INCLUDE_DIRS include port
                           PRIV_INCLUDE_DIRS private_include
                           PRIV_REQUIRES driver esp_timer)

    if (${CONFIG_SERIAL_FLASHER_INTERFACE_USB})
        idf_component_set_property(${COMPONENT_NAME} PRIV_REQUIRES usb APPEND)
    endif()

    set(target ${COMPONENT_LIB})
    component_compile_options(-Wstrict-prototypes)

else()
    # Create traditional CMake target
    add_library(flasher ${srcs})

    target_include_directories(flasher PUBLIC include port PRIVATE private_include)

    if (NOT DEFINED PORT)
        message(WARNING "No port selected, default to user-defined")
        set(PORT "USER_DEFINED")
    elseif(PORT STREQUAL "USER_DEFINED")
        # The user has to manually link their port with the flasher target
    elseif(PORT STREQUAL "STM32")
        stm32_get_chip_info(${STM32_CHIP} FAMILY DEVICE_FAMILY DEVICE DEVICE_CODE)
        if(DEFINED CORE_USED)
            string(APPEND DEVICE_FAMILY ::${CORE_USED})
            string(APPEND DEVICE_CODE ::${CORE_USED})
        endif()
        target_link_libraries(flasher PRIVATE
            HAL::STM32::${DEVICE_FAMILY}::GPIO
            HAL::STM32::${DEVICE_FAMILY}::UART
            CMSIS::STM32::${DEVICE_CODE}
        )
        target_sources(flasher PRIVATE port/stm32_port.c)
    elseif(PORT STREQUAL "RASPBERRY_PI")
        find_library(pigpio_LIB pigpio)
        target_link_libraries(flasher PUBLIC ${pigpio_LIB})
        target_sources(flasher PRIVATE port/raspberry_port.c)
    else()
        message(FATAL_ERROR "Selected port is not supported")
    endif()

    set(target flasher)

endif()

target_compile_definitions(${target} PUBLIC ${defs})
