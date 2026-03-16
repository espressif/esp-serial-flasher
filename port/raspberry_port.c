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

#include "protocol.h"
#include <pigpio.h>
#include "raspberry_port.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#if SERIAL_FLASHER_DEBUG_TRACE
static void transfer_debug_print(const uint8_t *data, uint16_t size, bool write)
{
    static bool write_prev = false;

    if (write_prev != write) {
        write_prev = write;
        printf("\n--- %s ---\n", write ? "WRITE" : "READ");
    }

    for (uint32_t i = 0; i < size; i++) {
        printf("%02x ", data[i]);
    }
}
#endif

static int serial;
static int64_t s_time_end;
static int32_t s_reset_trigger_pin;
static int32_t s_gpio0_trigger_pin;


static speed_t convert_baudrate(int baud)
{
    switch (baud) {
    case 50: return B50;
    case 75: return B75;
    case 110: return B110;
    case 134: return B134;
    case 150: return B150;
    case 200: return B200;
    case 300: return B300;
    case 600: return B600;
    case 1200: return B1200;
    case 1800: return B1800;
    case 2400: return B2400;
    case 4800: return B4800;
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    case 460800: return B460800;
    case 500000: return B500000;
    case 576000: return B576000;
    case 921600: return B921600;
    case 1000000: return B1000000;
    case 1152000: return B1152000;
    case 1500000: return B1500000;
    case 2000000: return B2000000;
    case 2500000: return B2500000;
    case 3000000: return B3000000;
    case 3500000: return B3500000;
    case 4000000: return B4000000;
    default: return -1;
    }
}

static int serialOpen(const char *device, uint32_t baudrate)
{
    struct termios options;
    int status, fd;

    if ((fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK)) == -1) {
        printf("Error occurred while opening serial port !\n");
        return -1;
    }

    fcntl(fd, F_SETFL, O_RDWR);

    // Get and modify current options:

    tcgetattr(fd, &options);
    speed_t baud = convert_baudrate(baudrate);

    if (baud < 0) {
        printf("Invalid baudrate!\n");
        return -1;
    }

    cfmakeraw(&options);
    cfsetispeed(&options, baud);
    cfsetospeed(&options, baud);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;
    options.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable any special handling of received bytes

    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 10; // 1 Second

    tcsetattr(fd, TCSANOW, &options);

    ioctl(fd, TIOCMGET, &status);

    status |= TIOCM_DTR;
    status |= TIOCM_RTS;

    ioctl(fd, TIOCMSET, &status);

    usleep(10000);  // 10mS

    return fd;
}

static esp_loader_error_t change_baudrate(int file_desc, int baudrate)
{
    struct termios options;
    speed_t baud = convert_baudrate(baudrate);

    if (baud < 0) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    tcgetattr(file_desc, &options);

    cfmakeraw(&options);
    cfsetispeed(&options, baud);
    cfsetospeed(&options, baud);

    tcsetattr(file_desc, TCSANOW, &options);

    return ESP_LOADER_SUCCESS;
}

static void set_timeout(uint32_t timeout)
{
    struct termios options;

    timeout /= 100;
    timeout = MAX(timeout, 1);

    tcgetattr(serial, &options);
    options.c_cc[VTIME] = timeout;
    tcsetattr(serial, TCSANOW, &options);
}

static esp_loader_error_t read_char(char *c, uint32_t timeout)
{
    set_timeout(timeout);
    int read_bytes = read(serial, c, 1);

    if (read_bytes == 1) {
        return ESP_LOADER_SUCCESS;
    } else if (read_bytes == 0) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}

static uint32_t raspi_remaining_time_ms(void)
{
    int64_t remaining = (s_time_end - clock()) / 1000;
    return (remaining > 0) ? (uint32_t)remaining : 0;
}

static esp_loader_error_t read_data(char *buffer, uint32_t size)
{
    for (int i = 0; i < size; i++) {
        uint32_t remaining_time = raspi_remaining_time_ms();
        RETURN_ON_ERROR(read_char(&buffer[i], remaining_time));
    }

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_raspberry_init(const loader_raspberry_config_t *config)
{
    s_reset_trigger_pin = config->reset_trigger_pin;
    s_gpio0_trigger_pin = config->gpio0_trigger_pin;

    serial = serialOpen(config->device, config->baudrate);
    if (serial < 0) {
        printf("Serial port could not be opened!\n");
        return ESP_LOADER_ERROR_FAIL;
    }

    if (gpioInitialise() < 0) {
        printf("pigpio initialisation failed\n");
        return ESP_LOADER_ERROR_FAIL;
    }

    gpioSetMode(config->reset_trigger_pin, PI_OUTPUT);
    gpioSetMode(config->gpio0_trigger_pin, PI_OUTPUT);

    return ESP_LOADER_SUCCESS;
}

void loader_port_deinit(void)
{
    close(serial);
    gpioTerminate();
}

static esp_loader_error_t raspi_uart_write(esp_loader_port_t *port, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    (void)port;
    int written = write(serial, data, size);

    if (written < 0) {
        return ESP_LOADER_ERROR_FAIL;
    } else if (written < size) {
#if SERIAL_FLASHER_DEBUG_TRACE
        transfer_debug_print(data, written, true);
#endif
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
#if SERIAL_FLASHER_DEBUG_TRACE
        transfer_debug_print(data, written, true);
#endif
        return ESP_LOADER_SUCCESS;
    }
}


static esp_loader_error_t raspi_uart_read(esp_loader_port_t *port, uint8_t *data, uint16_t size, uint32_t timeout)
{
    (void)port;
    RETURN_ON_ERROR(read_data(data, size));

#if SERIAL_FLASHER_DEBUG_TRACE
    transfer_debug_print(data, size, false);
#endif

    return ESP_LOADER_SUCCESS;
}


static void raspi_uart_delay_ms(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    usleep(ms * 1000);
}


static void raspi_uart_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    s_time_end = clock() + (ms * (CLOCKS_PER_SEC / 1000));
}

static uint32_t raspi_uart_remaining_time(esp_loader_port_t *port)
{
    (void)port;
    return raspi_remaining_time_ms();
}

static void raspi_uart_reset_target(esp_loader_port_t *port)
{
    (void)port;
    gpioWrite(s_reset_trigger_pin, SERIAL_FLASHER_RESET_INVERT ? 1 : 0);
    usleep(SERIAL_FLASHER_RESET_HOLD_TIME_MS * 1000);
    gpioWrite(s_reset_trigger_pin, SERIAL_FLASHER_RESET_INVERT ? 0 : 1);
}


static void raspi_uart_enter_bootloader(esp_loader_port_t *port)
{
    (void)port;
    gpioWrite(s_gpio0_trigger_pin, SERIAL_FLASHER_BOOT_INVERT ? 1 : 0);
    gpioWrite(s_reset_trigger_pin, SERIAL_FLASHER_RESET_INVERT ? 1 : 0);
    usleep(SERIAL_FLASHER_RESET_HOLD_TIME_MS * 1000);
    gpioWrite(s_reset_trigger_pin, SERIAL_FLASHER_RESET_INVERT ? 0 : 1);
    usleep(SERIAL_FLASHER_BOOT_HOLD_TIME_MS * 1000);
    gpioWrite(s_gpio0_trigger_pin, SERIAL_FLASHER_BOOT_INVERT ? 0 : 1);
}


static void raspi_uart_debug_print(esp_loader_port_t *port, const char *str)
{
    (void)port;
    printf("DEBUG: %s\n", str);
}

static esp_loader_error_t raspi_uart_change_rate(esp_loader_port_t *port, uint32_t baudrate)
{
    (void)port;
    return change_baudrate(serial, baudrate);
}

static const esp_loader_port_ops_t raspi_uart_ops = {
    .enter_bootloader         = raspi_uart_enter_bootloader,
    .reset_target             = raspi_uart_reset_target,
    .start_timer              = raspi_uart_start_timer,
    .remaining_time           = raspi_uart_remaining_time,
    .delay_ms                 = raspi_uart_delay_ms,
    .debug_print              = raspi_uart_debug_print,
    .change_transmission_rate = raspi_uart_change_rate,
    .write                    = raspi_uart_write,
    .read                     = raspi_uart_read,
};

esp_loader_port_t raspi_uart_port = { .ops = &raspi_uart_ops };
