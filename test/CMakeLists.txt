cmake_minimum_required(VERSION 3.22)
project(serial_flasher_test)

add_executable( ${PROJECT_NAME}
	test_main.cpp
	../src/esp_loader.c
	../src/esp_targets.c
	../src/stubs/esp_stubs_table.c
	../src/stubs/esp_stub_esp8266.c
	../src/stubs/esp_stub_esp32.c
	../src/stubs/esp_stub_esp32s2.c
	../src/stubs/esp_stub_esp32c3.c
	../src/stubs/esp_stub_esp32s3.c
	../src/stubs/esp_stub_esp32c2.c
	../src/stubs/esp_stub_esp32c5.c
	../src/stubs/esp_stub_esp32h2.c
	../src/stubs/esp_stub_esp32c6.c
	../src/stubs/esp_stub_esp32p4.c
	../src/stubs/esp_stub_esp32p4rev1.c
	../src/esp_sdio_stubs.c
	../src/md5_hash.c
	../src/protocol_serial.c
	../src/protocol_uart.c
	../src/protocol_spi.c
	../src/protocol_sdio.c
	../src/slip.c)

target_include_directories(${PROJECT_NAME} PRIVATE ../include ../private_include ../test)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Werror -O3)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)

target_sources(${PROJECT_NAME} PRIVATE test_tcp_port.cpp qemu_test.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	SERIAL_FLASHER_DEBUG_TRACE
	SERIAL_FLASHER_WRITE_BLOCK_RETRIES=3
)
