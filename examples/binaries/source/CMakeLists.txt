# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Here we remove all unecessary components from the build to make the binary smaller
set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hello_world)

# if CONFIG_APP_BUILD_TYPE_ELF_RAM is set, execute the following code
if(CONFIG_APP_BUILD_TYPE_ELF_RAM)
    # This generates a binary file from the elf, as the RAM build doesn't do that automatically
    add_custom_command(TARGET ${PROJECT_NAME}.elf
        POST_BUILD
        COMMAND esptool.py --chip ${IDF_TARGET} elf2image --output app.bin ${PROJECT_NAME}.elf
    )
endif()
