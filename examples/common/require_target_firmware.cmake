# Fail configure early if expected firmware binaries are missing (before bin2array runs).
# Usage:
#   include(${CMAKE_SOURCE_DIR}/../common/require_target_firmware.cmake)
#   esp_serial_flasher_require_target_firmware("my_example" "${CMAKE_SOURCE_DIR}/target-firmware"
#       bootloader.bin partition-table.bin app.bin)
function(esp_serial_flasher_require_target_firmware example_label firmware_dir)
    set(missing "")
    foreach(bin IN LISTS ARGN)
        if(NOT EXISTS "${firmware_dir}/${bin}")
            string(APPEND missing "  - ${bin}\n")
        endif()
    endforeach()
    if(missing)
        message(FATAL_ERROR
            "${example_label}: required target firmware binaries are missing from:\n"
            "  ${firmware_dir}\n"
            "Missing files:\n"
            "${missing}"
            "Copy or build them (see target-firmware/README.md and test/target-example-src).")
    endif()
endfunction()
