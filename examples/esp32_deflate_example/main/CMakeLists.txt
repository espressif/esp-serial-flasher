set(srcs main.c ../../common/example_common.c)
set(include_dirs . ../../common)

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs})
set(target ${COMPONENT_LIB})

# Embed binaries into the app using bin2array.cmake which is able to generate md5sum of the binary.
include(${CMAKE_SOURCE_DIR}/../common/bin2array.cmake)
create_resources(${CMAKE_SOURCE_DIR}/target-firmware ${CMAKE_BINARY_DIR}/target_firmware_data.c)
set_property(SOURCE ${CMAKE_BINARY_DIR}/target_firmware_data.c PROPERTY GENERATED 1)
target_sources(${target} PRIVATE ${CMAKE_BINARY_DIR}/target_firmware_data.c)
