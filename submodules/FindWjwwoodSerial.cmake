SET(DEPENDENCY_NAME "WjwwoodSerial")
SET(GIT_URL "https://github.com/wjwwood/serial")

if(NOT EXISTS "${CMAKE_BINARY_DIR}/${DEPENDENCY_NAME}")
    # Clone the repository if it's not already present
    message(STATUS "Cloning ${DEPENDENCY_NAME} from ${GIT_URL}")
    execute_process(COMMAND git clone ${GIT_URL} ${CMAKE_BINARY_DIR}/${DEPENDENCY_NAME})
else()
    message(STATUS "${DEPENDENCY_NAME} already cloned")
endif()

SET(serial_path ${CMAKE_BINARY_DIR}/${DEPENDENCY_NAME})
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

add_library(wjwwood_serial
    ${serial_SRCS}
)

target_include_directories(wjwwood_serial PUBLIC
    ${serial_path}/include
)
