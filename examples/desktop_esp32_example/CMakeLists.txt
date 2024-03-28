
cmake_minimum_required(VERSION 3.5)
project(esp-serial-flasher)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../../submodules)
find_package(WjwwoodSerial REQUIRED)

set(ESPSERIAL_PATH ${CMAKE_CURRENT_LIST_DIR}/../../)

# LIB esp_serial

include(${ESPSERIAL_PATH}examples/common/bin2array.cmake)
create_resources(${ESPSERIAL_PATH}examples/binaries/Hello-world ${CMAKE_BINARY_DIR}/binaries_1.c)
set_property(SOURCE ${CMAKE_BINARY_DIR}/binaries_1.c PROPERTY GENERATED 1)
create_resources(${ESPSERIAL_PATH}examples/binaries/RAM_APP ${CMAKE_BINARY_DIR}/binaries_2.c)
set_property(SOURCE ${CMAKE_BINARY_DIR}/binaries_2.c PROPERTY GENERATED 1)


add_library(esp_serial
    ${ESPSERIAL_PATH}port/wjwwood_serial_port.cpp
    ${ESPSERIAL_PATH}src/esp_loader.c
    ${ESPSERIAL_PATH}src/esp_targets.c
    ${ESPSERIAL_PATH}src/md5_hash.c
    ${ESPSERIAL_PATH}src/slip.c
    ${ESPSERIAL_PATH}src/protocol_uart.c
    ${ESPSERIAL_PATH}src/protocol_common.c
    ${CMAKE_BINARY_DIR}/binaries_1.c
    ${CMAKE_BINARY_DIR}/binaries_2.c
    ${ESPSERIAL_PATH}examples/common/example_common.c
)

target_include_directories(esp_serial PUBLIC
    ${ESPSERIAL_PATH}include
    ${ESPSERIAL_PATH}port
    ${ESPSERIAL_PATH}examples/common
)

target_include_directories(esp_serial PRIVATE
    ${ESPSERIAL_PATH}private_include
)

target_compile_definitions(esp_serial PUBLIC
    -DSERIAL_FLASHER_INTERFACE_USB
    -DMD5_ENABLED
    -DSERIAL_FLASHER_WRITE_BLOCK_RETRIES=4
    -DSERIAL_FLASHER_DEBUG_TRACE
)

target_link_libraries(esp_serial PUBLIC wjwwood_serial)

# EXECUTABLE desktop_esp32_example

add_executable(desktop_esp32_example
    main/main.cpp
)

target_link_libraries(desktop_esp32_example PRIVATE
    esp_serial
)