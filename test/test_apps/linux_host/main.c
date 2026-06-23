/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "linux_port.h"
#include "esp_loader.h"
#include "esp_loader_io.h"
#include "esp_targets.h"
#include "loader_log.h"
#include "test_common.h"

#define DEFAULT_BAUD_RATE      115200

/* ------------------------- levelled logging mock ------------------------- */

/*
 * A standalone port that captures the arguments of the log / log_hex callbacks
 * so the levelled-logging dispatch can be verified without a real target.
 * esp_loader_port_t is embedded so container_of() recovers the capture struct.
 */
typedef struct {
    esp_loader_port_t port;
    int log_calls;
    esp_loader_log_level_t last_level;
    char last_msg[128];
    int log_hex_calls;
    esp_loader_log_level_t last_hex_level;
    uint8_t last_hex_data[64];
    size_t last_hex_size;
} log_capture_t;

static void capture_log(esp_loader_port_t *port, esp_loader_log_level_t level,
                        const char *fmt, va_list args)
{
    log_capture_t *c = container_of(port, log_capture_t, port);
    c->log_calls++;
    c->last_level = level;
    vsnprintf(c->last_msg, sizeof(c->last_msg), fmt, args);
}

static void capture_log_hex(esp_loader_port_t *port, esp_loader_log_level_t level,
                            const char *label, const uint8_t *data, size_t size)
{
    (void)label;
    log_capture_t *c = container_of(port, log_capture_t, port);
    c->log_hex_calls++;
    c->last_hex_level = level;
    if (size > sizeof(c->last_hex_data)) {
        size = sizeof(c->last_hex_data);
    }
    memcpy(c->last_hex_data, data, size);
    c->last_hex_size = size;
}

static const esp_loader_port_ops_t capture_ops = {
    .log     = capture_log,
    .log_hex = capture_log_hex,
};

static const esp_loader_port_ops_t silent_ops = {
    .log     = NULL,
    .log_hex = NULL,
};

/* ------------------------- parallel flashing setup ----------------------- */

#define NUMBER_OF_TARGETS 2

#define INITIAL_BAUD_RATE   115200
#define HIGHER_BAUD_RATE    460800
#define FLASH_BLOCK_SIZE    1024

#define TEST_FLASH_OFFSET   0x10000
#define TEST_IMAGE_SIZE     (128 * 1024)

typedef struct {
    const char *name;     /* Human readable label for logging */
    const char *device;   /* Serial port, e.g. "/dev/ttyUSB0" */
    esp_loader_t *loader; /* Loader instance */
    bool        success;  /* Result, filled by the thread */
} target_t;

/* Serial ports for the two targets, set from argv in main(). */
static const char *g_port1;
static const char *g_port2;

/*
 * The app is compiled with SERIAL_FLASHER_LOG_LEVEL=DEBUG so the logging test
 * cases can exercise every LOADER_LOG* macro, which would otherwise make the
 * real-hardware cases print the whole protocol trace and bury the RUN/PASS/FAIL
 * markers. So the hardware cases run on linux_uart_ops with the log callbacks
 * stripped, and --verbose puts them back. The logging cases use their own
 * capture port and are unaffected either way.
 */
static esp_loader_port_ops_t g_uart_ops;

/*
 * The flashed payload is plain random data, so it is generated in process
 * instead of being prepared by the caller.
 */
static uint8_t *generate_random_image(size_t size)
{
    uint8_t *buf = malloc(size);
    if (!buf) {
        fprintf(stderr, "Cannot allocate %zu bytes for the test image\n", size);
        return NULL;
    }

    FILE *rand_file = fopen("/dev/urandom", "rb");
    if (!rand_file || fread(buf, 1, size, rand_file) != size) {
        fprintf(stderr, "Cannot read %zu bytes from /dev/urandom\n", size);
        if (rand_file) {
            fclose(rand_file);
        }
        free(buf);
        return NULL;
    }

    fclose(rand_file);
    return buf;
}

static esp_loader_error_t flash_image(esp_loader_t *loader, const target_t *t)
{
    const size_t size = TEST_IMAGE_SIZE;
    uint8_t *bin = generate_random_image(size);
    if (!bin) {
        return ESP_LOADER_ERROR_FAIL;
    }

    printf("[%s] Flashing %zu random bytes at 0x%x\n", t->name, size, TEST_FLASH_OFFSET);

    esp_loader_flash_cfg_t cfg = {
        .offset     = TEST_FLASH_OFFSET,
        .image_size = size,
        .block_size = FLASH_BLOCK_SIZE,
    };

    esp_loader_error_t err = esp_loader_flash_start(loader, &cfg);
    if (err == ESP_LOADER_SUCCESS) {
        for (size_t off = 0; off < size && err == ESP_LOADER_SUCCESS; off += FLASH_BLOCK_SIZE) {
            uint32_t chunk = (size - off < FLASH_BLOCK_SIZE) ? (uint32_t)(size - off) : FLASH_BLOCK_SIZE;
            err = esp_loader_flash_write(loader, &cfg, bin + off, chunk);
        }
    }
    free(bin);

    if (err == ESP_LOADER_SUCCESS) {
        err = esp_loader_flash_finish(loader, &cfg);
    }
    if (err != ESP_LOADER_SUCCESS) {
        fprintf(stderr, "[%s] Failed to flash the test image (error %d)\n", t->name, err);
    }
    return err;
}

static void *flash_target(void *arg)
{
    target_t *t = (target_t *)arg;

    esp_loader_t *loader = t->loader;
    linux_port_t port = {
        .port.ops  = &g_uart_ops,
        .device    = t->device,
        .baudrate  = INITIAL_BAUD_RATE,
        .gpio_mode = LINUX_GPIO_DTR_RTS,
    };

    if (esp_loader_init_serial(loader, &port.port) != ESP_LOADER_SUCCESS) {
        fprintf(stderr, "[%s] Cannot open %s\n", t->name, t->device);
        return NULL;
    }

    esp_loader_connect_args_t connect_cfg = ESP_LOADER_CONNECT_DEFAULT();
    if (esp_loader_connect_with_stub(loader, &connect_cfg) != ESP_LOADER_SUCCESS) {
        fprintf(stderr, "[%s] Cannot connect on %s\n", t->name, t->device);
        esp_loader_deinit(loader);
        return NULL;
    }
    printf("[%s] Connected on %s\n", t->name, t->device);

    if (esp_loader_change_transmission_rate(loader, HIGHER_BAUD_RATE) == ESP_LOADER_SUCCESS) {
        printf("[%s] Transmission rate changed to %d\n", t->name, HIGHER_BAUD_RATE);
    }

    if (flash_image(loader, t) == ESP_LOADER_SUCCESS) {
        printf("[%s] All done! Resetting target...\n", t->name);
        esp_loader_reset_target(loader);
        t->success = true;
    }

    esp_loader_deinit(loader);
    return NULL;
}

/* ------------------------------ test cases ------------------------------- */

/* The test app is built with SERIAL_FLASHER_LOG_LEVEL=DEBUG so every macro is active. */

static test_result_t test_log_levels(void)
{
    log_capture_t cap = {0};
    cap.port.ops = &capture_ops;
    esp_loader_t loader = {0};
    loader._port = &cap.port;

    LOADER_LOGE(&loader, "value=%d", 1);
    CHECK_EQ(cap.log_calls, 1);
    CHECK_EQ(cap.last_level, ESP_LOADER_LOG_LEVEL_ERROR);
    CHECK(strcmp(cap.last_msg, "value=1") == 0, "Message not printf-formatted: '%s'", cap.last_msg);

    LOADER_LOGW(&loader, "warn");
    CHECK_EQ(cap.last_level, ESP_LOADER_LOG_LEVEL_WARN);

    LOADER_LOGI(&loader, "info");
    CHECK_EQ(cap.last_level, ESP_LOADER_LOG_LEVEL_INFO);

    LOADER_LOGD(&loader, "debug");
    CHECK_EQ(cap.last_level, ESP_LOADER_LOG_LEVEL_DEBUG);

    CHECK_EQ(cap.log_calls, 4);

    return TEST_PASS;
}

static test_result_t test_log_hex(void)
{
    log_capture_t cap = {0};
    cap.port.ops = &capture_ops;
    esp_loader_t loader = {0};
    loader._port = &cap.port;

    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    LOADER_LOG_HEX(&loader, "payload", data, sizeof(data));

    CHECK_EQ(cap.log_hex_calls, 1);
    CHECK_EQ(cap.last_hex_level, ESP_LOADER_LOG_LEVEL_DEBUG);
    CHECK_EQ(cap.last_hex_size, sizeof(data));
    CHECK(memcmp(cap.last_hex_data, data, sizeof(data)) == 0, "log_hex forwarded wrong data");

    return TEST_PASS;
}

static test_result_t test_log_null_callbacks(void)
{
    log_capture_t cap = {0};
    cap.port.ops = &silent_ops;
    esp_loader_t loader = {0};
    loader._port = &cap.port;

    const uint8_t data[] = {0x01, 0x02};
    LOADER_LOGI(&loader, "ignored");
    LOADER_LOG_HEX(&loader, "ignored", data, sizeof(data));

    CHECK_EQ(cap.log_calls, 0, "Dispatch must be a no-op when callbacks are NULL");
    CHECK_EQ(cap.log_hex_calls, 0, "Dispatch must be a no-op when callbacks are NULL");

    return TEST_PASS;
}

/*
 * Split from its wrapper so the CHECK macros, which return on the spot, cannot
 * skip the esp_loader_deinit() that closes the port: the wrapper owns the
 * loader and deinits it on every path out of here.
 */
static test_result_t register_read_write(esp_loader_t *loader)
{
    esp_loader_connect_args_t connect_cfg = ESP_LOADER_CONNECT_DEFAULT();
    CHECK_EQ(esp_loader_connect_with_stub(loader, &connect_cfg), ESP_LOADER_SUCCESS,
             "Cannot connect on %s", g_port1);

    const target_registers_t *regs = get_esp_target_data(esp_loader_get_target(loader));
    if (regs->mosi_dlen == 0) {
        return TEST_SKIP;
    }

    const uint32_t test_value = 55;
    uint32_t reg_value = 0;

    CHECK_EQ(esp_loader_write_register(loader, regs->mosi_dlen, test_value), ESP_LOADER_SUCCESS);
    CHECK_EQ(esp_loader_read_register(loader, regs->mosi_dlen, &reg_value), ESP_LOADER_SUCCESS);
    CHECK_EQ(reg_value, test_value);

    return TEST_PASS;
}

static test_result_t test_register_read_write(void)
{
    esp_loader_t loader;
    linux_port_t port = {
        .port.ops  = &g_uart_ops,
        .device    = g_port1,
        .baudrate  = DEFAULT_BAUD_RATE,
        .gpio_mode = LINUX_GPIO_DTR_RTS,
    };

    CHECK_EQ(esp_loader_init_serial(&loader, &port.port), ESP_LOADER_SUCCESS,
             "Cannot open %s", g_port1);

    const test_result_t result = register_read_write(&loader);

    esp_loader_deinit(&loader);
    return result;
}

static test_result_t test_parallel_flashing(void)
{
    if (!g_port2) {
        TEST_PRINT_MSG("--port2 not given, nothing to flash in parallel");
        return TEST_SKIP;
    }

    esp_loader_t loader1;
    esp_loader_t loader2;

    target_t targets[NUMBER_OF_TARGETS] = {
        { .name = "Target 1", .device = g_port1, .loader = &loader1 },
        { .name = "Target 2", .device = g_port2, .loader = &loader2 },
    };

    pthread_t threads[NUMBER_OF_TARGETS];
    for (int i = 0; i < NUMBER_OF_TARGETS; i++) {
        pthread_create(&threads[i], NULL, flash_target, &targets[i]);
    }
    for (int i = 0; i < NUMBER_OF_TARGETS; i++) {
        pthread_join(threads[i], NULL);
    }

    for (int i = 0; i < NUMBER_OF_TARGETS; i++) {
        CHECK(targets[i].success, "%s flashing failed", targets[i].name);
    }

    return TEST_PASS;
}

static const test_case_t test_cases[] = {
    { "register_read_write", test_register_read_write },
    { "log_levels", test_log_levels },
    { "log_hex", test_log_hex },
    { "log_null_callbacks", test_log_null_callbacks },
    { "parallel_flashing", test_parallel_flashing },
};

/* -------------------------------- main ----------------------------------- */

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s -p|--port <device> [-p2|--port2 <device>] [-v|--verbose]\n"
            "\n"
            "-p/--port is the serial port of the target under test and is required.\n"
            "-p2/--port2 is the second serial port, used only by the parallel_flashing test\n"
            "case, which flashes a random blob to -p/--port and -p2/--port2 in parallel. The images\n"
            "are plain random data generated at runtime, so any two ESP chips can be used.\n"
            "Without -p2/--port2 that case is skipped.\n"
            "-v/--verbose prints the DEBUG-level protocol trace of the cases that talk to real\n"
            "hardware. Off by default, so the output is just the per-case result markers.\n",
            prog);
}

int main(int argc, char *argv[])
{
    const char *device  = NULL;
    const char *device2 = NULL;
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) && i + 1 < argc) {
            device = argv[++i];
        } else if ((strcmp(argv[i], "-p2") == 0 || strcmp(argv[i], "--port2") == 0) && i + 1 < argc) {
            device2 = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else {
            fprintf(stderr, "Unknown argument: %s\n\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!device) {
        fprintf(stderr, "Missing required argument: -p/--port\n\n");
        print_usage(argv[0]);
        return 1;
    }

    g_port1 = device;
    g_port2 = device2;

    g_uart_ops = linux_uart_ops;
    if (!verbose) {
        g_uart_ops.log     = NULL;
        g_uart_ops.log_hex = NULL;
    }

    return RUN_TEST_CASES(test_cases);
}
