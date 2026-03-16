/* Copyright 2020-2023 Espressif Systems (Shanghai) CO LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_loader_error.h"
#include "esp_loader_io.h"
#if MD5_ENABLED
#include "md5_ctx.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Macro which can be used to check the error code,
 * and return in case the code is not ESP_LOADER_SUCCESS.
 */
#define RETURN_ON_ERROR(x) do {         \
    esp_loader_error_t _err_ = (x);     \
    if (_err_ != ESP_LOADER_SUCCESS) {  \
        return _err_;                   \
    }                                   \
} while(0)

/**
 * @brief Supported targets
 */
typedef enum {
    ESP8266_CHIP = 0,
    ESP32_CHIP   = 1,
    ESP32S2_CHIP = 2,
    ESP32C3_CHIP = 3,
    ESP32S3_CHIP = 4,
    ESP32C2_CHIP = 5,
    ESP32C5_CHIP = 6,
    ESP32H2_CHIP = 7,
    ESP32C6_CHIP = 8,
    ESP32P4_CHIP = 9,
    ESP_MAX_CHIP = 10,
    ESP_UNKNOWN_CHIP = 10
} target_chip_t;

/**
 * @brief Forward declarations for internal types used only as pointers inside
 * struct esp_loader.  Full definitions live in private headers.
 */
struct esp_loader_protocol_ops_s;
struct target_registers_t;

/**
 * @brief Application binary image header (8-byte common header).
 */
typedef struct {
    uint8_t magic;
    uint8_t segments;
    uint8_t flash_mode;
    uint8_t flash_size_freq;
    uint32_t entrypoint;
} esp_loader_bin_header_t;

/**
 * @brief Segment descriptor within an application binary.
 */
typedef struct {
    uint32_t addr;
    uint32_t size;
    const uint8_t *data;
} esp_loader_bin_segment_t;

typedef struct {
    target_chip_t target_chip;
    uint32_t eco_version; // Not present on ESP32-S2
    bool secure_boot_enabled;
    bool secure_boot_aggressive_revoke_enabled;
    bool secure_download_mode_enabled;
    bool secure_boot_revoked_keys[3];
    bool jtag_software_disabled;
    bool jtag_hardware_disabled;
    bool usb_disabled;
    bool flash_encryption_enabled;
    bool dcache_in_uart_download_disabled;
    bool icache_in_uart_download_disabled;
} esp_loader_target_security_info_t;

/**
 * @brief Connection arguments
 */
typedef struct {
    uint32_t sync_timeout;  /*!< Maximum time to wait for response from serial interface. */
    int32_t trials;         /*!< Number of trials to connect to target. If greater than 1,
                               100 millisecond delay is inserted after each try. */
} esp_loader_connect_args_t;

#define ESP_LOADER_CONNECT_DEFAULT() { \
  .sync_timeout = 100, \
  .trials = 10, \
}

/**
 * @brief Flash operation context.
 *
 * Allocate on the stack, fill the public fields, then pass to
 * esp_loader_flash_start() / esp_loader_flash_write() / esp_loader_flash_finish().
 * Fields inside _state are managed by the library; do not access them directly.
 */
typedef struct {
    uint32_t offset;      /*!< Flash address to write to. Must be 4-byte aligned. */
    uint32_t image_size;  /*!< Total size of the image. Must be 4-byte aligned. */
    uint32_t block_size;  /*!< Size of each block passed to esp_loader_flash_write(). */
    struct {
        uint32_t          _sequence_number;
#if MD5_ENABLED
        struct MD5Context _md5_context;
#endif
    } _state;
} esp_loader_flash_cfg_t;

/**
 * @brief Compressed flash operation context (DEFLATE/zlib stream).
 *
 * Allocate on the stack, fill the public fields, then pass to
 * esp_loader_flash_deflate_start() / esp_loader_flash_deflate_write() / esp_loader_flash_deflate_finish().
 * Fields inside _state are managed by the library; do not access them directly.
 */
typedef struct {
    uint32_t offset;           /*!< Flash address to write to. Must be 4-byte aligned. */
    uint32_t image_size;       /*!< Size of the uncompressed image in bytes. */
    uint32_t compressed_size;  /*!< Size of the compressed data in bytes. */
    uint32_t block_size;       /*!< Size of each compressed block. */
    struct {
        uint32_t _sequence_number;
    } _state;
} esp_loader_flash_deflate_cfg_t;

/**
 * @brief RAM load operation context.
 *
 * Allocate on the stack, fill the public fields, then pass to
 * esp_loader_mem_start() / esp_loader_mem_write() / esp_loader_mem_finish().
 * Fields inside _state are managed by the library; do not access them directly.
 */
typedef struct {
    uint32_t offset;      /*!< RAM address to load to. */
    uint32_t size;        /*!< Total size of the data to load. */
    uint32_t block_size;  /*!< Size of each block passed to esp_loader_mem_write(). */
    struct {
        uint32_t _sequence_number;
    } _state;
} esp_loader_mem_cfg_t;

/**
 * @brief Protocol type stored in the loader context.
 *
 * Used internally to gate protocol-specific behaviour (e.g. baud-rate changes
 * are not available on SDIO; SPI does not support flash operations).
 */
typedef enum {
    ESP_LOADER_PROTOCOL_UART, /*!< UART */
    ESP_LOADER_PROTOCOL_USB,  /*!< USB CDC-ACM */
    ESP_LOADER_PROTOCOL_SPI,  /*!< SPI */
    ESP_LOADER_PROTOCOL_SDIO, /*!< SDIO */
} esp_loader_protocol_t;

/**
 * @brief Loader context.
 *
 * Declare on the stack or as a static and pass its address to every
 * esp_loader_*() call.  All fields are prefixed with _ and must not be
 * accessed directly — they are private implementation details.
 */
typedef struct esp_loader {
    const struct esp_loader_protocol_ops_s *_protocol;
    esp_loader_port_t                      *_port;
    esp_loader_protocol_t                   _protocol_type;
    target_chip_t                           _target;
    const struct target_registers_t        *_reg;
    uint32_t  _target_flash_size;
    bool      _stub_running;
} esp_loader_t;

/**
  * @brief Initialize the loader context for UART protocol.
  *
  * Call the port-specific hardware init function (e.g. @c loader_port_esp32_init)
  * before calling this function.
  *
  * @code
  *   loader_port_esp32_init(&config);
  *   esp_loader_t loader;
  *   esp_loader_init_uart(&loader, &esp32_uart_port);
  *   esp_loader_connect(&loader, &connect_args);
  *
  *   // Two UART devices in parallel
  *   esp_loader_init_uart(&loader_a, &esp32_uart_port_a);
  *   esp_loader_init_uart(&loader_b, &esp32_uart_port_b);
  * @endcode
  *
  * @param loader[in]  Pointer to a caller-allocated esp_loader_t instance.
  * @param port[in]    Pointer to the port handle (its @c ops must be populated).
  *
  * @return ESP_LOADER_SUCCESS on success.
  */
esp_loader_error_t esp_loader_init_uart(esp_loader_t *loader, esp_loader_port_t *port);

/**
  * @brief Initialize the loader context for USB CDC-ACM protocol.
  *
  * @param loader[in]  Pointer to a caller-allocated esp_loader_t instance.
  * @param port[in]    Pointer to the port handle.
  *
  * @return ESP_LOADER_SUCCESS on success.
  */
esp_loader_error_t esp_loader_init_usb(esp_loader_t *loader, esp_loader_port_t *port);

/**
  * @brief Initialize the loader context for SPI protocol.
  *
  * @param loader[in]  Pointer to a caller-allocated esp_loader_t instance.
  * @param port[in]    Pointer to the port handle.
  *
  * @return ESP_LOADER_SUCCESS on success.
  */
esp_loader_error_t esp_loader_init_spi(esp_loader_t *loader, esp_loader_port_t *port);

/**
  * @brief Initialize the loader context for SDIO protocol.
  *
  * @param loader[in]  Pointer to a caller-allocated esp_loader_t instance.
  * @param port[in]    Pointer to the port handle.
  *
  * @return ESP_LOADER_SUCCESS on success.
  */
esp_loader_error_t esp_loader_init_sdio(esp_loader_t *loader, esp_loader_port_t *port);

/**
  * @brief Connects to the target
  *
  * @param loader[in]       Pointer to initialized loader context.
  * @param connect_args[in] Timing parameters to be used for connecting to target.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_connect(esp_loader_t *loader, esp_loader_connect_args_t *connect_args);

/**
  * @brief   Returns attached target chip.
  *
  * @warning This function can only be called after connection with target
  *          has been successfully established by calling esp_loader_connect().
  *
  * @param loader[in] Pointer to initialized loader context.
  *
  * @return  One of target_chip_t
  */
target_chip_t esp_loader_get_target(esp_loader_t *loader);

/**
  * @brief Connects to the target while using the flasher stub
  *
  * @note  Only supported on UART and USB interfaces.
  *
  * @param loader[in]       Pointer to initialized loader context.
  * @param connect_args[in] Timing parameters to be used for connecting to target.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol
  */
esp_loader_error_t esp_loader_connect_with_stub(esp_loader_t *loader, esp_loader_connect_args_t *connect_args);

/**
  * @brief Connects to the target running in secure download mode
  *
  * @note  Only supported on UART interface.
  *
  * @param loader[in]       Pointer to initialized loader context.
  * @param connect_args[in] Timing parameters to be used for connecting to target.
  * @param flash_size Flash size of the target chip.
  * @param target_chip Target chip. Used for the ESP32 and ESP8266, which do not support the
  *                    GET_SECURITY_INFO command required to identify the target in secure
  *                    download mode. Leave as ESP_UNKNOWN_CHIP for autodetection of newer chips.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol
  */
esp_loader_error_t esp_loader_connect_secure_download_mode(esp_loader_t *loader,
        esp_loader_connect_args_t *connect_args,
        uint32_t flash_size, target_chip_t target_chip);

/**
  * @brief Initiates flash operation
  *
  * @param loader[in]  Pointer to initialized loader context.
  * @param cfg[in,out] Flash operation context. Caller fills offset, image_size and block_size
  *                    before calling. The _state sub-struct is initialized by this function
  *                    and must not be modified by the caller.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_start(esp_loader_t *loader, esp_loader_flash_cfg_t *cfg);

/**
  * @brief Writes supplied data to target's flash memory.
  *
  * @param loader[in,out]  Pointer to initialized loader context.
  * @param cfg[in,out]     Flash operation context initialized by esp_loader_flash_start().
  * @param payload[in]     Data to be flashed into target's memory.
  * @param size[in]        Size of payload in bytes.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_write(esp_loader_t *loader, esp_loader_flash_cfg_t *cfg, void *payload, uint32_t size);

/**
  * @brief Ends flash operation.
  *
  * @param loader[in]  Pointer to initialized loader context.
  * @param cfg[in]     Flash operation context initialized by esp_loader_flash_start().
  * @param reboot[in]  Reboot the target if true. Has no effect when MD5 verification fails.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_finish(esp_loader_t *loader, esp_loader_flash_cfg_t *cfg, bool reboot);

/**
  * @brief Initiates compressed flash operation (DEFLATE/zlib stream).
  *
  * @note  Only supported on UART and USB interfaces.
  *
  * @note  The deflate path does not accumulate an MD5 digest internally (the compressed
  *        data stream cannot be hashed to match the plaintext MD5). Use
  *        esp_loader_flash_verify_known_md5() after flashing to verify the uncompressed
  *        content.
  *
  * @param loader[in]  Pointer to initialized loader context.
  * @param cfg[in,out] Compressed flash operation context. Caller fills offset, image_size,
  *                    compressed_size and block_size before calling. The _state sub-struct
  *                    is initialized by this function and must not be modified by the caller.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol
  */
esp_loader_error_t esp_loader_flash_deflate_start(esp_loader_t *loader, esp_loader_flash_deflate_cfg_t *cfg);

/**
  * @brief Writes a compressed data block to target flash memory.
  *
  * @param loader[in,out]  Pointer to initialized loader context.
  * @param cfg[in,out]     Compressed flash context initialized by esp_loader_flash_deflate_start().
  * @param payload[in]     Data buffer containing a zlib-compressed block.
  * @param size[in]        Size of payload in bytes.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_deflate_write(esp_loader_t *loader, esp_loader_flash_deflate_cfg_t *cfg, void *payload, uint32_t size);

/**
  * @brief Ends compressed flash operation.
  *
  * @param loader[in]  Pointer to initialized loader context.
  * @param cfg[in]     Compressed flash context initialized by esp_loader_flash_deflate_start().
  * @param reboot[in]  Reboot the target if true.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_deflate_finish(esp_loader_t *loader, esp_loader_flash_deflate_cfg_t *cfg, bool reboot);

/**
  * @brief Detects the size of the flash chip used by target
  *
  * @param loader[in]       Pointer to initialized loader context.
  * @param flash_size[out] Flash size detected in bytes
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_UNSUPPORTED_CHIP The target flash chip is not known
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol
  */
esp_loader_error_t esp_loader_flash_detect_size(esp_loader_t *loader, uint32_t *flash_size);

/**
  * @brief Reads from the target flash.
  *
  * @note  Only supported on UART and USB interfaces.
  *
  * @param loader[in]  Pointer to initialized loader context.
  * @param buf[out] Buffer to read into
  * @param address[in] Flash address to read from.
  * @param length[in] Read length in bytes.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol
  */
esp_loader_error_t esp_loader_flash_read(esp_loader_t *loader, uint8_t *buf, uint32_t address, uint32_t length);

/**
  * @brief Erase the whole flash chip
  *
  * @param loader[in] Pointer to initialized loader context.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_erase(esp_loader_t *loader);

/**
  * @brief Erase a region of the flash
  *
  * @param loader[in]  Pointer to initialized loader context.
  * @param offset[in]  The offset of the region to erase (must be 4096 byte aligned)
  * @param size[in]    The size of the region to erase (must be 4096 byte aligned)
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_erase_region(esp_loader_t *loader, uint32_t offset, uint32_t size);

/**
  * @brief Change baud rate of the stub running on the target
  *
  * @note  Only supported on UART and USB interfaces with stub running.
  *
  * @param loader[in]                Pointer to initialized loader context.
  * @param old_transmission_rate[in] The baudrate to be changed
  * @param new_transmission_rate[in] The new baud rate to be set.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol or stub not running
  */
esp_loader_error_t esp_loader_change_transmission_rate_stub(esp_loader_t *loader,
        uint32_t old_transmission_rate,
        uint32_t new_transmission_rate);

/**
  * @brief Get the security info of the target chip
  *
  * @note  Only supported on UART and USB interfaces.
  *
  * @param loader[in]        Pointer to initialized loader context.
  * @param security_info[out] The security info structure
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol
  */
esp_loader_error_t esp_loader_get_security_info(esp_loader_t *loader,
        esp_loader_target_security_info_t *security_info);

/**
  * @brief Initiates RAM load operation.
  *
  * @param loader[in]  Pointer to initialized loader context.
  * @param cfg[in,out] RAM load context. Caller fills offset, size and block_size before
  *                    calling. The _state sub-struct is initialized by this function and
  *                    must not be modified by the caller.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_mem_start(esp_loader_t *loader, esp_loader_mem_cfg_t *cfg);

/**
  * @brief Writes supplied data to target's RAM.
  *
  * @param loader[in,out]  Pointer to initialized loader context.
  * @param cfg[in,out]     RAM load context initialized by esp_loader_mem_start().
  * @param payload[in]     Data to be loaded into target's memory.
  * @param size[in]        Size of data in bytes.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_mem_write(esp_loader_t *loader, esp_loader_mem_cfg_t *cfg, const void *payload, uint32_t size);

/**
  * @brief Finishes RAM load operation and transfers control to the loaded program.
  *
  * @param loader[in]       Pointer to initialized loader context.
  * @param cfg[in]          RAM load context initialized by esp_loader_mem_start().
  * @param entrypoint[in]   Entry point address of the loaded program.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_mem_finish(esp_loader_t *loader, esp_loader_mem_cfg_t *cfg, uint32_t entrypoint);

/**
  * @brief Reads the MAC address of the connected chip.
  *
  * @param loader[in] Pointer to initialized loader context.
  * @param mac[out] 6 byte MAC address of the chip
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_read_mac(esp_loader_t *loader, uint8_t *mac);

/**
  * @brief Writes register.
  *
  * @param loader[in]       Pointer to initialized loader context.
  * @param address[in]      Address of register.
  * @param reg_value[in]    New register value.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_write_register(esp_loader_t *loader, uint32_t address, uint32_t reg_value);

/**
  * @brief Reads register.
  *
  * @param loader[in]       Pointer to initialized loader context.
  * @param address[in]      Address of register.
  * @param reg_value[out]   Register value.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_read_register(esp_loader_t *loader, uint32_t address, uint32_t *reg_value);

/**
  * @brief Change baud rate.
  *
  * @note  Not supported on SDIO interface.
  *
  * @param loader[in]              Pointer to initialized loader context.
  * @param transmission_rate[in]   new baud rate to be set.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol
  */
esp_loader_error_t esp_loader_change_transmission_rate(esp_loader_t *loader, uint32_t transmission_rate);

#if MD5_ENABLED
/**
  * @brief Verify target's flash integrity by checking with a known MD5 checksum
  * for a specified offset and length.
  *
  * @param loader[in] Pointer to initialized loader context.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_INVALID_MD5 MD5 does not match
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Unsupported on the target
  *     - ESP_LOADER_ERROR_IMAGE_SIZE Flash region specified is beyond the flash end
  */
esp_loader_error_t esp_loader_flash_verify_known_md5(esp_loader_t *loader,
        uint32_t address,
        uint32_t size,
        const uint8_t *expected_md5);

/**
  * @brief Verify target's flash integrity by checking MD5.
  *
  * @param loader[in] Pointer to initialized loader context.
  * @param cfg[in]    Flash operation context previously used with esp_loader_flash_start()
  *                   and esp_loader_flash_write(). The accumulated MD5 context stored in
  *                   cfg->_state is used to compute the expected digest.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_INVALID_MD5 MD5 does not match
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Unsupported on the target
  *     - ESP_LOADER_ERROR_IMAGE_SIZE Flash region specified is beyond the flash end
  */
esp_loader_error_t esp_loader_flash_verify(esp_loader_t *loader, esp_loader_flash_cfg_t *cfg);
#endif /* MD5_ENABLED */

/**
  * @brief Toggles reset pin.
  *
  * @param loader[in] Pointer to initialized loader context.
  */
void esp_loader_reset_target(esp_loader_t *loader);

#ifdef __cplusplus
}
#endif
