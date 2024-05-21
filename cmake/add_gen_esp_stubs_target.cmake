macro(add_gen_esp_stubs_target)
    set(ONE_VALUE_KEYWORDS VERSION)
    cmake_parse_arguments(ESPTOOL "" "${ONE_VALUE_KEYWORDS}" "" "${ARGN}")

    # Don't make this mandatory, some users might not have Python installed
    find_package(Python COMPONENTS Interpreter)
    if(NOT Python_Interpreter_FOUND)
      message(WARNING "Python not found, gen_esp_stubs target not created")
      return()
    endif()

    # Run python script to generate esp_stubs.h and esp_stubs.c
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/stub.c
        COMMAND
            ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_esp_stubs.py
            ${ESPTOOL_VERSION} ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_esp_stubs.py)
    add_custom_target(gen_esp_stubs
                      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/stub.c)
endmacro()
