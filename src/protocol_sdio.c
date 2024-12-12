/* Copyright 2020-2024 Espressif Systems (Shanghai) CO LTD
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

#include "protocol.h"
#include "esp_loader_io.h"
#include "esp_loader.h"
#include "esp_targets.h"
#include "sip.h"
#include <stddef.h>
#include <assert.h>
#include <string.h>

/* SDIO CCR registers - common to all SDIO devices */
#define SD_IO_CCCR_FN_ENABLE 0x02
#define SD_IO_CCR_FN_ENABLE_FUNC1_EN (1 << 1)
#define SD_IO_CCCR_FN_READY 0x03

typedef struct {
    bool sdio_supported;
    /* SLC Host Registers. Some of these are reserved for host/slave communication without
       on-target concurrency issues */
    /* The host writes into CONF registers to send data to the slave, and reads back from STATE registers */
    /* Uses a different target base address from SLC slave registers, read or write with regular transactions */
    uint32_t slchost_date_addr;
    uint32_t slchost_date_expected_val;
    uint32_t slchost_state_w0_addr;
    uint32_t slchost_conf_w5_addr;
    uint32_t slchost_win_cmd_addr;
    uint32_t slchost_packet_space_end;

    /* SLC Registers */
    /* Use host registers for writing/reading to these using slave_read/write_register() */
    uint32_t slc_conf1_addr;
    uint32_t slc_len_conf_addr;
    uint32_t slc_conf1_tx_stitch_en;
    uint32_t slc_conf1_rx_stitch_en;
    uint32_t slc_len_conf_tx_packet_load_en;
} esp_target_t;

static const esp_target_t esp_target[ESP_MAX_CHIP] = {

    // ESP8266
    {},

    // ESP32
    {
        .sdio_supported = true,
        .slchost_date_addr = 0x178,
        .slchost_date_expected_val = 0x16022500,
        .slchost_state_w0_addr = 0x64,
        .slchost_conf_w5_addr = 0x80,
        .slchost_win_cmd_addr = 0x84,
        .slchost_packet_space_end = 0x1f800,
        .slc_conf1_addr = 0x60,
        .slc_len_conf_addr = 0xE4,
        .slc_conf1_tx_stitch_en = (1 << 5),
        .slc_conf1_rx_stitch_en = (1 << 6),
        .slc_len_conf_tx_packet_load_en = (1 << 24),
    },

    // ESP32S2
    {},

    // ESP32C3
    {},

    // ESP32S3
    {},

    // ESP32C2
    {},

    // Reserved for future use
    {},

    // ESP32H2
    {},

    // ESP32C6
    {
        .sdio_supported = true,
        .slchost_date_addr = 0x178,
        .slchost_date_expected_val = 0x21060700,
        .slchost_state_w0_addr = 0x64,
        .slchost_conf_w5_addr = 0x80,
        .slchost_win_cmd_addr = 0x84,
        .slchost_packet_space_end = 0x1f800,
        .slc_conf1_addr = 0x70,
        .slc_len_conf_addr = 0xF4,
        .slc_conf1_tx_stitch_en = (1 << 5),
        .slc_conf1_rx_stitch_en = (1 << 6),
        .slc_len_conf_tx_packet_load_en = (1 << 24),
    },
};

static uint8_t s_sip_buf[SIP_PACKET_SIZE] __attribute__((aligned(4)));
static uint32_t s_sip_seq_tx;
static uint32_t s_sip_current_transaction_addr;
static target_chip_t s_target_chip = ESP_UNKNOWN_CHIP;

static esp_loader_error_t slave_read_register(const uint32_t addr, uint32_t *reg)
{
    assert(addr >> 2 <= 0x7F);

    uint8_t buf[4] __attribute__((aligned(4))) = {0};
    buf[0] = (addr >> 2) & 0x7F;
    buf[1] = 0x80;

    RETURN_ON_ERROR(loader_port_write(1,
                                      esp_target[s_target_chip].slchost_win_cmd_addr,
                                      buf,
                                      sizeof(buf),
                                      loader_port_remaining_time()));

    return loader_port_read(1,
                            esp_target[s_target_chip].slchost_state_w0_addr,
                            (uint8_t *)reg,
                            sizeof(uint32_t),
                            loader_port_remaining_time());
}

static esp_loader_error_t slave_write_register(const uint32_t addr, uint32_t reg_val)
{
    assert(addr >> 2 <= 0x7F);

    uint8_t buf[8] __attribute__((aligned(4))) = {0};
    memcpy(buf, &reg_val, 4);
    buf[4] = (addr >> 2) & 0x7F;
    buf[5] = 0xC0;

    return loader_port_write(1,
                             esp_target[s_target_chip].slchost_conf_w5_addr,
                             buf,
                             sizeof(buf),
                             loader_port_remaining_time());
}

static esp_loader_error_t slave_wait_ready(const uint32_t timeout)
{
    uint8_t reg = 0;

    loader_port_start_timer(timeout);
    while ((reg & SD_IO_CCR_FN_ENABLE_FUNC1_EN) == 0) {
        if (loader_port_remaining_time() == 0) {
            return ESP_LOADER_ERROR_TIMEOUT;
        }

        RETURN_ON_ERROR(loader_port_read(0, SD_IO_CCCR_FN_READY,
                                         &reg, sizeof(reg),
                                         loader_port_remaining_time()));
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t slave_init_io(void)
{
    RETURN_ON_ERROR(slave_wait_ready(100));

    // Enable function 1, we will use it for upload
    // The alignment requirement comes from the esp port DMA requirements
    uint8_t reg __attribute__((aligned(4)));
    RETURN_ON_ERROR(loader_port_read(0, SD_IO_CCCR_FN_ENABLE,
                                     &reg, sizeof(reg),
                                     loader_port_remaining_time()));

    reg |= SD_IO_CCR_FN_ENABLE_FUNC1_EN;
    uint8_t expected_val = reg;
    RETURN_ON_ERROR(loader_port_write(0, SD_IO_CCCR_FN_ENABLE,
                                      &reg, sizeof(reg),
                                      loader_port_remaining_time()));

    // Read back to verify
    RETURN_ON_ERROR(loader_port_read(0, SD_IO_CCCR_FN_ENABLE,
                                     &reg, sizeof(reg),
                                     loader_port_remaining_time()));

    if (reg != expected_val) {
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t slave_detect_chip(void)
{
    RETURN_ON_ERROR(slave_wait_ready(100));

    for (int chip = 0; chip < ESP_MAX_CHIP; chip++) {
        if (!esp_target[chip].sdio_supported) {
            continue;
        }

        uint32_t reg;
        RETURN_ON_ERROR(loader_port_read(1,
                                         esp_target[chip].slchost_date_addr,
                                         (uint8_t *)&reg,
                                         sizeof(uint32_t),
                                         loader_port_remaining_time()));

        if (reg == esp_target[chip].slchost_date_expected_val) {
            s_target_chip = (target_chip_t)chip;
            return ESP_LOADER_SUCCESS;
        }
    }

    return ESP_LOADER_ERROR_INVALID_TARGET;
}

static esp_loader_error_t slave_init_link(void)
{
    RETURN_ON_ERROR(slave_wait_ready(100));

    uint32_t reg = 0;

    // Configure stitching
    RETURN_ON_ERROR(slave_read_register(esp_target[s_target_chip].slc_conf1_addr, &reg));
    reg |= esp_target[s_target_chip].slc_conf1_tx_stitch_en |
           esp_target[s_target_chip].slc_conf1_rx_stitch_en;
    uint32_t expected_val = reg;
    RETURN_ON_ERROR(slave_write_register(esp_target[s_target_chip].slc_conf1_addr, reg));

    RETURN_ON_ERROR(slave_read_register(esp_target[s_target_chip].slc_conf1_addr, &reg));
    if (reg != expected_val) {
        return ESP_LOADER_ERROR_FAIL;
    }

    RETURN_ON_ERROR(slave_wait_ready(100));

    // Configure the tx packet load enable
    // This bit does not stay set, so reading it back to check for success is pointless
    RETURN_ON_ERROR(slave_read_register(esp_target[s_target_chip].slc_len_conf_addr, &reg));
    reg |= esp_target[s_target_chip].slc_len_conf_tx_packet_load_en;
    expected_val = reg;
    RETURN_ON_ERROR(slave_write_register(esp_target[s_target_chip].slc_len_conf_addr, reg));

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_initialize_conn(esp_loader_connect_args_t *connect_args)
{
    for (uint8_t trial = 0; trial < connect_args->trials; trial++) {

        if (loader_port_sdio_card_init() != ESP_LOADER_SUCCESS) {
            loader_port_debug_print("Retrying card connection...");
            loader_port_delay_ms(100);
        } else {
            break;
        }
    }

    RETURN_ON_ERROR(slave_init_io());

    RETURN_ON_ERROR(slave_detect_chip());

    RETURN_ON_ERROR(slave_init_link());

    s_sip_seq_tx = 0;

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_mem_begin_cmd(const uint32_t offset, const uint32_t size,
                                        uint32_t blocks_to_write, uint32_t block_size)
{
    // This function only sets up global variables to be used by the loader_mem_data_cmd() function.
    // This is because the sip protocol requires no set up for block size or number of blocks.
    (void) blocks_to_write;
    (void) block_size;
    (void) size;

    s_sip_current_transaction_addr = offset;

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_mem_data_cmd(const uint8_t *data, uint32_t size)
{
    RETURN_ON_ERROR(slave_wait_ready(100));

    int32_t remaining = size;
    while (remaining > 0) {

        const uint32_t nondata_size = sizeof(sip_header_t) + sizeof(sip_cmd_write_memory);
        const uint32_t data_size = ROUNDUP(MIN(remaining, SIP_PACKET_SIZE - nondata_size), 4);

        const sip_header_t header = {
            .fc[0] = SIP_PACKET_TYPE_CTRL & SIP_TYPE_MASK,
            .fc[1] = 0x00,
            .len = data_size + nondata_size,
            .sequence_num = s_sip_seq_tx,
            .u.tx_info.u.cmdid = SIP_CMD_ID_WRITE_MEMORY,
        };

        const sip_cmd_write_memory cmd = {
            .addr = s_sip_current_transaction_addr + size - remaining,
            .len = data_size,
        };

        memcpy(&s_sip_buf[0], &header, sizeof(header));
        memcpy(&s_sip_buf[sizeof(header)], &cmd, sizeof(cmd));
        memcpy(
            &s_sip_buf[sizeof(header) + sizeof(cmd)],
            &data[size - remaining],
            data_size
        );

        RETURN_ON_ERROR(loader_port_write(1,
                                          esp_target[s_target_chip].slchost_packet_space_end - header.len,
                                          s_sip_buf,
                                          header.len,
                                          loader_port_remaining_time()));

        remaining -= data_size;
        s_sip_seq_tx++;
    }

    s_sip_current_transaction_addr += size;

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_mem_end_cmd(uint32_t entrypoint)
{
    RETURN_ON_ERROR(slave_wait_ready(100));

    const sip_header_t header = {
        .fc[0] = SIP_PACKET_TYPE_CTRL & SIP_TYPE_MASK,
        .fc[1] = SIP_HDR_F_SYNC,
        .len = sizeof(sip_header_t) + sizeof(sip_cmd_write_memory),
        .sequence_num = 0,
        .u.tx_info.u.cmdid = SIP_CMD_ID_BOOTUP,
    };

    const sip_cmd_bootup cmd = { .boot_addr = entrypoint, .discard_link = 1};

    memcpy(&s_sip_buf[0], &header, sizeof(header));
    memcpy(&s_sip_buf[sizeof(header)], &cmd, sizeof(cmd));

    return loader_port_write(1,
                             esp_target[s_target_chip].slchost_packet_space_end - header.len,
                             s_sip_buf,
                             header.len,
                             loader_port_remaining_time()
                            );

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_detect_chip(target_chip_t *target_chip, const target_registers_t **target_data)
{
    (void) target_data;
    *target_chip = s_target_chip;

    return ESP_LOADER_SUCCESS;
}
