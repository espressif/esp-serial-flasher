macro(add_gen_flasher_stub_target)
    set(ONE_VALUE_KEYWORDS VERSION)
    cmake_parse_arguments(ESPTOOL "" "${ONE_VALUE_KEYWORDS}" "" "${ARGN}")

    # Don't make this mandatory, some users might not have Python installed
    find_package(Python COMPONENTS Interpreter)
    if(NOT Python_Interpreter_FOUND)
      message(WARNING "Python not found, gen_flasher_stub target not created")
      return()
    endif()

    # Run python script to generate esp_stubs.h and esp_stubs.c
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/stub.c
        COMMAND
            ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_flasher_stub.py
            ${ESPTOOL_VERSION} ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_flasher_stub.py)
    add_custom_target(gen_flasher_stub
                      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/stub.c)
endmacro()