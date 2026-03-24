cmake_minimum_required(VERSION 3.22)

set(FLASHER_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
set(PORT LINUX)

project(linux_flasher C)

add_executable(${CMAKE_PROJECT_NAME}
    ../common/example_common.c
    main.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ../common)

add_subdirectory(${FLASHER_DIR} ${CMAKE_BINARY_DIR}/flasher)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE flasher)
