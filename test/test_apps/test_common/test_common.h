/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Minimal, portable test-case harness shared by all test apps.
 *
 * Each test is a parameterless function that returns one of the results below.
 * A test that needs a loader (or any other resource) sets it up itself. A test
 * app declares an array of `test_case_t` and runs them with RUN_TEST_CASES().
 * The runner prints a clear marker per case so it is obvious which one failed:
 *
 *   RUN  : <name>
 *   PASS : <name>
 *   FAIL : <name>
 *   SKIP : <name>
 *
 * and a final summary line:
 *
 *   Tests finished: <p> passed, <f> failed, <s> skipped
 *
 * The harness only uses printf/fprintf, so it works both on the Linux host and
 * on ESP-IDF targets.
 * ------------------------------------------------------------------------- */

typedef enum {
    TEST_PASS,
    TEST_FAIL,
    TEST_SKIP,
} test_result_t;

typedef test_result_t (*test_fn_t)(void);

typedef struct {
    const char *name;
    test_fn_t   fn;
} test_case_t;

/*
 * Print an indented, printf-style note about the running test case. Goes to
 * stdout like every other line the harness and the flasher print, so the log
 * stays in order when it is piped into a CI job log.
 */
#define TEST_PRINT_MSG(...) do {                \
         printf("    " __VA_ARGS__);            \
         printf("\n");                          \
     } while (0)

/*
 * Return TEST_FAIL from the current test case unless the condition holds. The
 * optional trailing arguments are a printf-style note explaining the failure.
 */
#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("    CHECK failed: %s  (%s:%d)\n", #cond, __FILE__,         \
                   __LINE__);                                                  \
            __VA_OPT__(TEST_PRINT_MSG(__VA_ARGS__);)                           \
            return TEST_FAIL;                                                  \
        }                                                                      \
    } while (0)

/* CHECK for equality, reporting both operands. @p actual is evaluated once. */
#define CHECK_EQ(actual, expected, ...)                                        \
    do {                                                                       \
        long long _a = (long long)(actual);                                    \
        long long _e = (long long)(expected);                                  \
        if (_a != _e) {                                                        \
            printf("    CHECK_EQ failed: %s == %s  (got %lld, want %lld)"      \
                   "  (%s:%d)\n",                                              \
                   #actual, #expected, _a, _e, __FILE__, __LINE__);            \
            /* Register values and bit masks are unreadable in decimal; small  \
             * numbers (error enums, counts, indices) are unreadable in hex.   \
             * Print hex only once a value is big enough to be the former. */  \
            if (_a > 0xFFFF || _e > 0xFFFF) {                                  \
                printf("      hex: got 0x%llx, want 0x%llx\n", _a, _e);        \
            }                                                                  \
            __VA_OPT__(TEST_PRINT_MSG(__VA_ARGS__);)                           \
            return TEST_FAIL;                                                  \
        }                                                                      \
    } while (0)

/**
 * @brief Run an array of test cases.
 *
 * @param cases   Array of test cases.
 * @param count   Number of entries in @p cases.
 *
 * @return 0 if every case passed or was skipped, 1 if any case failed.
 */
int run_test_cases(const test_case_t *cases, size_t count);

/* Convenience wrapper that derives the case count from a static array. */
#define RUN_TEST_CASES(cases) \
     run_test_cases((cases), sizeof(cases) / sizeof((cases)[0]))

#ifdef __cplusplus
}
#endif
