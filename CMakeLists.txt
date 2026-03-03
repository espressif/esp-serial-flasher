cmake_minimum_required(VERSION 3.5)

# Core sources — always compiled; all protocols are available for runtime selection.
set(srcs
    src/md5_hash.c
    src/esp_loader.c
    src/esp_targets.c
    src/esp_stubs.c
    src/esp_sdio_stubs.c
    src/protocol_serial.c
    src/protocol_uart.c
    src/protocol_spi.c
    src/protocol_sdio.c
    src/slip.c
)
set(defs)

# Regular CMake builds are expected to set OPTION_NAME, while ESP-IDF's Kconfig system sets
# CONFIG_OPTION_NAME.
# This macro handles these differences and also a default value if neither is set.
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
add_option(SERIAL_FLASHER_RESET_INVERT false)
add_option(SERIAL_FLASHER_BOOT_INVERT false)

if (DEFINED ESP_PLATFORM)
    # Conditionally compile each ESP32 port based on Kconfig selection.
    # Each port exposes only static symbols, so multiple ports can coexist in the same build.
    set(priv_requires "esp_timer")
    set(public_requires "")

    if (CONFIG_SERIAL_FLASHER_PORT_UART)
        list(APPEND srcs port/esp32_port.c)
    endif()

    if (CONFIG_SERIAL_FLASHER_PORT_SPI)
        list(APPEND srcs port/esp32_spi_port.c)
    endif()

    if (CONFIG_SERIAL_FLASHER_PORT_SDIO)
        list(APPEND srcs port/esp32_sdio_port.c)
    endif()

    if (CONFIG_SERIAL_FLASHER_PORT_USB_CDC_ACM)
        list(APPEND srcs port/esp32_usb_cdc_acm_port.c)
    endif()

    # GPIO is needed by all ports for reset/boot pin control.
    list(APPEND priv_requires "esp_driver_gpio")
    # CMAKE_BUILD_EARLY_EXPANSION is defined during IDF's pre-scan phase, which runs before
    # Kconfig is loaded. Include all possible driver deps unconditionally in that phase so
    # they are captured in the dependency graph and their headers become available at build time.
    if (CONFIG_SERIAL_FLASHER_PORT_UART OR DEFINED CMAKE_BUILD_EARLY_EXPANSION)
        list(APPEND priv_requires "esp_driver_uart")
    endif()
    if (CONFIG_SERIAL_FLASHER_PORT_SPI OR DEFINED CMAKE_BUILD_EARLY_EXPANSION)
        list(APPEND priv_requires "esp_driver_spi")
    endif()
    if (CONFIG_SERIAL_FLASHER_PORT_SDIO OR DEFINED CMAKE_BUILD_EARLY_EXPANSION)
        # Public so the application can configure and pass the SDMMC host handle.
        list(APPEND public_requires "esp_driver_sdmmc")
    endif()

    # Register component to esp-idf build system
    idf_component_register(SRCS ${srcs}
                           INCLUDE_DIRS include port
                           PRIV_INCLUDE_DIRS private_include
                           PRIV_REQUIRES ${priv_requires}
                           REQUIRES ${public_requires})

    # USB CDC-ACM host support comes from a managed component; add it only when selected
    # so that targets without USB OTG (e.g. esp32) are not required to provide the header.
    if (CONFIG_SERIAL_FLASHER_PORT_USB_CDC_ACM)
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
    elseif(PORT STREQUAL "PI_PICO")
        target_link_libraries(flasher PUBLIC pico_stdlib)
        target_sources(flasher PRIVATE port/pi_pico_port.c)
    else()
        message(FATAL_ERROR "Selected port is not supported")
    endif()

    set(target flasher)

endif()

target_compile_definitions(${target} PUBLIC ${defs})

# This segment pulls the flasher stubs at a specified version at wish, and generates the
# esp_stubs.c/h files, overwriting the library provided ones.
# It is also possible to override stub generation to use a custom url or a local folder.
# Please check if the license under which the custom stub sources are released fits your usecase.
if (DEFINED SERIAL_FLASHER_STUB_PULL_VERSION OR DEFINED SERIAL_FLASHER_STUB_PULL_OVERRIDE_PATH)
    include(cmake/serial_flasher_pull_stubs.cmake)
    serial_flasher_pull_stubs(
        VERSION ${SERIAL_FLASHER_STUB_PULL_VERSION}
        SOURCE ${SERIAL_FLASHER_STUB_PULL_SOURCE}
        PATH_OVERRIDE ${SERIAL_FLASHER_STUB_PULL_OVERRIDE_PATH}
    )
endif()
