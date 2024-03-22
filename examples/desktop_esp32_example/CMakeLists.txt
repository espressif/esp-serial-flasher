set(ESPSERIAL_PATH ${CMAKE_CURRENT_LIST_DIR}/../../)

# LIB serial

SET(serial_path ${ESPSERIAL_PATH}submodules/serial)
SET(serial_SRCS ${serial_path}/src/serial.cc)

if(APPLE)
    # If OSX
    list(APPEND serial_SRCS ${serial_path}/src/impl/unix.cc)
    list(APPEND serial_SRCS ${serial_path}/src/impl/list_ports/list_ports_osx.cc)
elseif(UNIX)
    # If unix
    list(APPEND serial_SRCS ${serial_path}/src/impl/unix.cc)
    list(APPEND serial_SRCS ${serial_path}/src/impl/list_ports/list_ports_linux.cc)
else()
    # If windows
    list(APPEND serial_SRCS ${serial_path}/src/impl/win.cc)
    list(APPEND serial_SRCS ${serial_path}/src/impl/list_ports/list_ports_win.cc)
endif()

add_library(serial
    ${serial_SRCS}
)

target_include_directories(serial PUBLIC
    ${serial_path}/include
)

# LIB esp_serial

include(${ESPSERIAL_PATH}examples/common/bin2array.cmake)
create_resources(${ESPSERIAL_PATH}examples/binaries/Hello-world ${CMAKE_BINARY_DIR}/binaries_1.c)
set_property(SOURCE ${CMAKE_BINARY_DIR}/binaries_1.c PROPERTY GENERATED 1)
create_resources(${ESPSERIAL_PATH}examples/binaries/RAM_APP ${CMAKE_BINARY_DIR}/binaries_2.c)
set_property(SOURCE ${CMAKE_BINARY_DIR}/binaries_2.c PROPERTY GENERATED 1)


add_library(esp_serial
    ${ESPSERIAL_PATH}port/serial_port.cpp
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
)

target_link_libraries(esp_serial PUBLIC
    serial
)

# EXECUTABLE desktop_esp32_example

add_executable(desktop_esp32_example
    main/main.cpp
)

target_link_libraries(desktop_esp32_example PRIVATE
    esp_serial
)