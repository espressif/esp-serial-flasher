set(srcs main.c ../../common/example_common.c)
set(include_dirs . ../../common)

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs})
set(target ${COMPONENT_LIB})

set(target_firmware_dir ${CMAKE_SOURCE_DIR}/target-firmware)
include(${CMAKE_SOURCE_DIR}/../common/require_target_firmware.cmake)
esp_serial_flasher_require_target_firmware("esp32_deflate_example" "${target_firmware_dir}"
    bootloader.bin
    partition-table.bin
    app.bin
    bootloader_deflated.bin
    partition-table_deflated.bin
    app_deflated.bin)

# Embed binaries into the app using bin2array.cmake which is able to generate md5sum of the binary.
include(${CMAKE_SOURCE_DIR}/../common/bin2array.cmake)
create_resources(${target_firmware_dir} ${CMAKE_BINARY_DIR}/target_firmware_data.c)
set_property(SOURCE ${CMAKE_BINARY_DIR}/target_firmware_data.c PROPERTY GENERATED 1)
target_sources(${target} PRIVATE ${CMAKE_BINARY_DIR}/target_firmware_data.c)
