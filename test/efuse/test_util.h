/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tiny in-repo test harness for the eFuse host tests. No external framework:
 * each TEST() is a function auto-collected into a table by the REGISTER() call
 * in its definition; main() (via TEST_MAIN) runs them all and prints a summary.
 *
 * A failed check returns false from its test/helper; successful tests return
 * true. There is no shared per-test failure state.
 *
 * Usage:
 *   #include "test_util.h"
 *   TEST(does_a_thing) {
 *       CHECK(some_condition);
 *       CHECK(other, "chip=%d key %u was not staged", chip, i);
 *       CHECK_EQ(actual, expected);
 *   }
 *   TEST_MAIN()
 *
 * Checks inside a sweep all report the same __FILE__:__LINE__, so give those
 * an explicit message naming the chip/block — otherwise a failure can't be
 * traced back to the iteration that caused it.
 */

#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef bool (*test_fn_t)(void);

typedef struct test_case {
    const char       *name;
    test_fn_t         fn;
    struct test_case *next;
} test_case_t;

/* An intrusive list: each TEST() owns its node, so the registry has no capacity
 * to overflow (a cap here would mean silently running fewer tests than were
 * written, and still exiting green). Appending via a tail pointer keeps the run
 * order the same as the source order. */
static test_case_t  *g_test_head;
static test_case_t **g_test_tail = &g_test_head;

static inline void test_register(test_case_t *tc)
{
    *g_test_tail = tc;
    g_test_tail  = &tc->next;
}

/* Each TEST(x) defines the body plus a constructor that registers it before
 * main() runs, so suites never maintain a manual list. This relies on
 * __attribute__((constructor)); rather than silently registering nothing on a
 * compiler that lacks it (a green build that ran zero tests), fail loudly. */
#if !defined(__GNUC__) && !defined(__clang__)
#error "efuse test harness requires __attribute__((constructor)) (GCC or Clang)"
#endif

#define TEST(test_name)                                                        \
    static bool test_name(void);                                               \
    static test_case_t _tc_##test_name = { #test_name, test_name, NULL };      \
    __attribute__((constructor)) static void register_##test_name(void)        \
    {                                                                          \
        test_register(&_tc_##test_name);                                       \
    }                                                                          \
    static bool test_name(void)

/* Prints a check's optional message, indented under the failure line it
 * explains. A macro calling printf directly, rather than a varargs helper: the
 * compiler checks a printf call against its arguments on its own, so every
 * message is verified without __attribute__((format)) or any other GCC/Clang
 * extension. That checking matters here because these strings are only ever
 * evaluated on failure — an unchecked wrong argument list would compile clean
 * and stay hidden until the day the check finally fires. */
#define TEST_PRINT_MSG(...)                                                    \
    do {                                                                       \
        printf("               ");                                             \
        printf(__VA_ARGS__);                                                   \
        printf("\n");                                                          \
    } while (0)

/*
 * A failed check ends the current test/helper: it reports and returns false.
 * enclosing function. Continuing past a failure would run on state the check
 * just proved wrong, and in a sweep would re-report the same defect once per
 * remaining iteration — the first failure is the one worth reading. Note this
 * Helpers return bool too, so callers propagate their result explicitly.
 *
 * Each takes an optional printf-style message after the condition, for when
 * the condition text alone doesn't say what went wrong:
 *   CHECK(staged_bit(blk0, bit));
 *   CHECK(staged_bit(blk0, bit), "key %u should be wr-protected", key_idx);
 * No trailing newline needed. Omitting the message emits no print call at all,
 * so plain CHECK(cond) call sites keep working as-is.
 */
#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("    CHECK failed: %s  (%s:%d)\n", #cond, __FILE__,         \
                   __LINE__);                                                  \
            __VA_OPT__(TEST_PRINT_MSG(__VA_ARGS__);)                           \
            return false;                                                      \
        }                                                                      \
    } while (0)

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
            return false;                                                      \
        }                                                                      \
    } while (0)

/* Compare two byte buffers; reports the first differing index. */
#define CHECK_MEM_EQ(actual, expected, len, ...)                               \
    do {                                                                       \
        const uint8_t *_ap = (const uint8_t *)(actual);                        \
        const uint8_t *_ep = (const uint8_t *)(expected);                      \
        size_t _n = (size_t)(len);                                             \
        for (size_t _i = 0; _i < _n; _i++) {                                   \
            if (_ap[_i] != _ep[_i]) {                                          \
                printf("    CHECK_MEM_EQ failed at byte %zu: got 0x%02x, "     \
                       "want 0x%02x  (%s:%d)\n",                               \
                       _i, _ap[_i], _ep[_i], __FILE__, __LINE__);              \
                __VA_OPT__(TEST_PRINT_MSG(__VA_ARGS__);)                       \
                return false;                                                  \
            }                                                                  \
        }                                                                      \
    } while (0)

/* Report a failure that has no condition to quote (a duplicate name, a
 * mismatched vector byte) and end the test, same contract as CHECK. Message is
 * required here; no trailing newline needed. */
#define TEST_FAIL(...)                                                         \
    do {                                                                       \
        printf("    test failure  (%s:%d)\n", __FILE__, __LINE__);             \
        TEST_PRINT_MSG(__VA_ARGS__);                                           \
        return false;                                                          \
    } while (0)

#define TEST_MAIN()                                                            \
    int main(void)                                                             \
    {                                                                          \
        int failed_cases = 0;                                                  \
        int total_cases = 0;                                                   \
        for (const test_case_t *tc = g_test_head; tc != NULL; tc = tc->next) { \
            total_cases++;                                                     \
            printf("[ RUN  ] %s\n", tc->name);                                 \
            if (tc->fn()) {                                                    \
                printf("[  OK  ] %s\n", tc->name);                             \
            } else {                                                           \
                printf("[ FAIL ] %s\n", tc->name);                             \
                failed_cases++;                                                \
            }                                                                  \
        }                                                                      \
        printf("\n%d/%d cases passed\n", total_cases - failed_cases,           \
               total_cases);                                                   \
        return failed_cases == 0 ? 0 : 1;                                      \
    }
