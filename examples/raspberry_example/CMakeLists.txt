cmake_minimum_required(VERSION 3.5)

set(FLASHER_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
set(PORT RASPBERRY_PI)

project(raspberry_flasher C)

add_compile_definitions(SERIAL_FLASHER_INTERFACE_UART)

# Convert binary files to C source files using bin2array.cmake
include(${CMAKE_SOURCE_DIR}/../common/bin2array.cmake)
create_resources(${CMAKE_SOURCE_DIR}/target-firmware ${CMAKE_CURRENT_BINARY_DIR}/target_firmware_data.c)
set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/target_firmware_data.c PROPERTY GENERATED 1)

add_executable(${CMAKE_PROJECT_NAME} ../common/example_common.c Src/main.c ${CMAKE_CURRENT_BINARY_DIR}/target_firmware_data.c)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ../common)

add_subdirectory(${FLASHER_DIR} ${CMAKE_BINARY_DIR}/flasher)

target_compile_options(flasher
PRIVATE
    -Wunused-parameter
    -Wshadow
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE flasher)
