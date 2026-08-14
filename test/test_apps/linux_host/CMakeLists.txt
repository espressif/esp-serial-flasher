cmake_minimum_required(VERSION 3.22)

set(FLASHER_DIR ${CMAKE_CURRENT_LIST_DIR}/../../..)
set(PORT LINUX)

# Build the flasher with DEBUG logging so the levelled-logging test cases are active.
set(SERIAL_FLASHER_LOG_LEVEL DEBUG)

project(linux_host C)

add_compile_options(-Wall -Wextra -Werror)

add_executable(${CMAKE_PROJECT_NAME}
    main.c
    ../test_common/test_common.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ../test_common
    ${FLASHER_DIR}/private_include   # loader_log.h: levelled-logging macros under test
)

add_subdirectory(${FLASHER_DIR} ${CMAKE_BINARY_DIR}/flasher)

find_package(Threads REQUIRED)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE flasher Threads::Threads)
