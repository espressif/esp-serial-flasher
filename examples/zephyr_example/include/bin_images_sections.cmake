# Creates C resources file from binary files listed in images.csv inside input_dir.
# CSV format: image_file;offset (semicolon delimiter). Offsets are hexadecimal (no 0x prefix).
#
# create_resources(input_dir output_c_file)
#
# Example images.csv:
#   bootloader.bin;0
#   partition-table.bin;8000
#   app.bin;10000

function(create_resources input_dir output)
    set(csv_path "${input_dir}/images.csv")
    message(STATUS "[bin2array] Input directory: ${input_dir}")
    message(STATUS "[bin2array] CSV: ${csv_path}")
    message(STATUS "[bin2array] Output file: ${output}")

    if(NOT IS_DIRECTORY "${input_dir}")
        message(FATAL_ERROR "[bin2array] Input directory does not exist: ${input_dir}")
    endif()
    if(NOT EXISTS "${csv_path}")
        message(FATAL_ERROR "[bin2array] CSV not found: ${csv_path} (expected images.csv inside input directory)")
    endif()

    file(STRINGS "${csv_path}" _csv_lines)


    file(WRITE "${output}" "#include <stdint.h>\n")
    file(APPEND "${output}" "#include <bin_image.h>\n\n")

    set(_img_n 0)
    foreach(_line IN LISTS _csv_lines)
        string(STRIP "${_line}" _line)
        if(NOT _line)
            continue()
        endif()
        string(SUBSTRING "${_line}" 0 1 _first)
        if(_first STREQUAL "#")
            continue()
        endif()
        string(REGEX MATCH "^([^;]+);([^;]*)$" _dummy "${_line}")
        if(NOT CMAKE_MATCH_1)
            message(WARNING "[bin2array] Skipping invalid line: ${_line}")
            continue()
        endif()
        string(STRIP "${CMAKE_MATCH_1}" _file)
        string(STRIP "${CMAKE_MATCH_2}" _offset)
        string(REGEX REPLACE "^0[xX]" "" _offset_hex "${_offset}")
        if(NOT _offset_hex)
            set(_offset_hex "0")
        endif()
        set(_path "${input_dir}/${_file}")
        if(NOT EXISTS "${_path}")
            message(WARNING "[bin2array] File not found: ${_path}")
            continue()
        endif()

        string(REGEX REPLACE "[\\./-]" "_" _varname "${_file}")
        set(_dataname "img_${_img_n}_${_varname}")
        message(STATUS "[bin2array] Processing: ${_file} -> offset ${_offset} (${_dataname})")

        file(READ "${_path}" _filedata HEX)
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _filedata "${_filedata}")
        execute_process(COMMAND md5sum "${_path}" OUTPUT_VARIABLE _md5_out)
        string(REGEX REPLACE " .*" "" _md5_hash "${_md5_out}")
        string(REPLACE "\\" "\\\\" _file_esc "${_file}")
        string(REPLACE "\"" "\\\"" _file_esc "${_file_esc}")

        file(APPEND "${output}"
            "const uint8_t ${_dataname}[] = {${_filedata}};\n"
            "DEFINE_IMAGE(${_varname}, ${_dataname}, 0x${_offset_hex}, sizeof(${_dataname}), \"${_md5_hash}\", \"${_file_esc}\");\n"
        )
        math(EXPR _img_n "${_img_n} + 1")
    endforeach()
endfunction()
