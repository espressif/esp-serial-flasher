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
#include "protocol_prv.h"
#include "esp_loader.h"
#include "esp_loader_protocol.h"
#include "esp_targets.h"
#include "md5_hash.h"
#include "slip.h"
#include <stddef.h>
#include <string.h>

#define UART_DEFAULT_TIMEOUT 1000

static esp_loader_error_t uart_check_response(esp_loader_t *loader, const send_cmd_config *config);

static esp_loader_error_t uart_initialize_conn(esp_loader_t *loader, esp_loader_connect_args_t *connect_args)
{
    esp_loader_error_t err;
    int32_t trials = connect_args->trials;

    do {
        loader->_port->ops->start_timer(loader->_port, connect_args->sync_timeout);
        err = loader_sync_cmd(loader);
        if (err == ESP_LOADER_ERROR_TIMEOUT) {
            if (--trials == 0) {
                return ESP_LOADER_ERROR_TIMEOUT;
            }
            loader->_port->ops->delay_ms(loader->_port, 100);
        } else if (err != ESP_LOADER_SUCCESS) {
            return err;
        }
    } while (err != ESP_LOADER_SUCCESS);

    return err;
}

static esp_loader_error_t uart_spi_attach(esp_loader_t *loader, uint32_t config)
{
    return loader_spi_attach_cmd(loader, config);
}

static esp_loader_error_t uart_send_cmd(esp_loader_t *loader, const send_cmd_config *config)
{
    RETURN_ON_ERROR(SLIP_send_delimiter(loader));

    RETURN_ON_ERROR(SLIP_send(loader, (const uint8_t *)config->cmd, config->cmd_size));

    if (config->data != NULL && config->data_size != 0) {
        RETURN_ON_ERROR(SLIP_send(loader, (const uint8_t *)config->data, config->data_size));
    }

    RETURN_ON_ERROR(SLIP_send_delimiter(loader));

    command_t command = ((const command_common_t *)config->cmd)->command;
    const uint8_t response_cnt = command == SYNC ? 8 : 1;

    for (uint8_t recv_cnt = 0; recv_cnt < response_cnt; recv_cnt++) {
        RETURN_ON_ERROR(uart_check_response(loader, config));
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t uart_check_response(esp_loader_t *loader, const send_cmd_config *config)
{
    uint8_t buf[sizeof(common_response_t) + sizeof(response_status_t) + MAX_RESP_DATA_SIZE];

    common_response_t *response = (common_response_t *)&buf[0];
    command_t command = ((const command_common_t *)config->cmd)->command;

    // If the command has fixed response data size, require all of it to be received
    uint32_t minimum_packet_recv = sizeof(common_response_t) + sizeof(response_status_t);
    if (config->resp_data_recv_size == NULL) {
        minimum_packet_recv += config->resp_data_size;
    }

    size_t packet_recv = 0;
    do {
        RETURN_ON_ERROR(SLIP_receive_packet(loader, buf,
                                            sizeof(common_response_t) + sizeof(response_status_t) + config->resp_data_size,
                                            &packet_recv));
    } while ((response->direction != READ_DIRECTION) || (response->command != command) ||
             packet_recv < minimum_packet_recv);

    response_status_t *status = (response_status_t *)&buf[packet_recv - sizeof(response_status_t)];

    if (status->failed) {
        log_loader_internal_error(loader, status->error);
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    if (config->reg_value != NULL) {
        *config->reg_value = response->value;
    }

    if (config->resp_data != NULL) {
        const size_t resp_data_size = packet_recv - sizeof(common_response_t) - sizeof(response_status_t);

        memcpy(config->resp_data, &buf[sizeof(common_response_t)], resp_data_size);

        if (config->resp_data_recv_size != NULL) {
            *config->resp_data_recv_size = resp_data_size;
        }
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t uart_flash_read_stub(esp_loader_t *loader, uint8_t *dest, uint32_t address, uint32_t length)
{
    uint8_t buf[256];
    size_t recv_size = 0;
    struct MD5Context md5_context;
    MD5Init(&md5_context);

    const uint32_t seek_back_len = address % 4;
    address -= seek_back_len;
    length += seek_back_len;

    const uint32_t overread_len = ROUNDUP(length, 4) - length;
    length += overread_len;

    loader->_port->ops->start_timer(loader->_port, UART_DEFAULT_TIMEOUT);
    RETURN_ON_ERROR(loader_flash_read_stub_cmd(loader, address, length, sizeof(buf)));

    uint32_t copy_dest_start = 0;
    int32_t remaining = length;
    while (remaining > 0) {
        loader->_port->ops->start_timer(loader->_port, UART_DEFAULT_TIMEOUT);
        const uint32_t to_receive = MIN(remaining, sizeof(buf));
        RETURN_ON_ERROR(SLIP_receive_packet(loader, buf, to_receive, &recv_size));

        if (recv_size != to_receive) {
            return ESP_LOADER_ERROR_INVALID_RESPONSE;
        }

        MD5Update(&md5_context, buf, recv_size);

        uint32_t copy_start = 0;
        uint32_t copy_length = recv_size;

        const bool first_read = remaining == (int32_t)length;
        if (first_read) {
            copy_start += seek_back_len;
            copy_length -= seek_back_len;
        }

        const bool last_read = remaining - (int32_t)recv_size <= 0;
        if (last_read) {
            copy_length -= overread_len;
        }

        memcpy(&dest[copy_dest_start], &buf[copy_start], copy_length);
        copy_dest_start += copy_length;

        remaining -= recv_size;

        const uint32_t bytes_recv = length - remaining;
        loader->_port->ops->start_timer(loader->_port, UART_DEFAULT_TIMEOUT);
        RETURN_ON_ERROR(SLIP_send_delimiter(loader));
        RETURN_ON_ERROR(SLIP_send(loader, (const uint8_t *)&bytes_recv, sizeof(bytes_recv)));
        RETURN_ON_ERROR(SLIP_send_delimiter(loader));
    }

    uint8_t md5_calc[16];
    MD5Final(md5_calc, &md5_context);

    loader->_port->ops->start_timer(loader->_port, UART_DEFAULT_TIMEOUT);
    uint8_t md5_recv[16];
    RETURN_ON_ERROR(SLIP_receive_packet(loader, md5_recv, sizeof(md5_recv), &recv_size));

    if (recv_size != sizeof(md5_recv) || memcmp(md5_calc, md5_recv, sizeof(md5_calc))) {
        return ESP_LOADER_ERROR_INVALID_MD5;
    }

    return ESP_LOADER_SUCCESS;
}


static esp_loader_error_t uart_mem_begin_cmd(esp_loader_t *loader, uint32_t offset,
        uint32_t size, uint32_t blocks_to_write,
        uint32_t block_size)
{
    if (loader->_stub_running) {
        esp_loader_connect_args_t connect_args = ESP_LOADER_CONNECT_DEFAULT();
        loader->_port->ops->enter_bootloader(loader->_port);
        RETURN_ON_ERROR(loader->_protocol->initialize_conn(loader, &connect_args));
        loader->_stub_running = false;
    }

    uint32_t seq = 0;
    return loader_mem_begin_cmd(loader, &seq, offset, size, blocks_to_write, block_size);
}

const esp_loader_protocol_ops_t uart_protocol_ops = {
    .initialize_conn = uart_initialize_conn,
    .send_cmd        = uart_send_cmd,
    .spi_attach      = uart_spi_attach,
    .flash_read_stub = uart_flash_read_stub,
    .mem_begin_cmd   = uart_mem_begin_cmd,
    .mem_data_cmd    = NULL,
    .mem_end_cmd     = NULL,
};

const esp_loader_protocol_ops_t *esp_loader_get_uart_ops(void)
{
    return &uart_protocol_ops;
}

const esp_loader_protocol_ops_t *esp_loader_get_usb_ops(void)
{
    return &uart_protocol_ops;
}
