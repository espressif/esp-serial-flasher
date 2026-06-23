/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "test_common.h"

int run_test_cases(const test_case_t *cases, size_t count)
{
    unsigned passed = 0;
    unsigned failed = 0;
    unsigned skipped = 0;

    for (size_t i = 0; i < count; i++) {
        const test_case_t *tc = &cases[i];

        printf("RUN  : %s\n", tc->name);

        switch (tc->fn()) {
        case TEST_PASS:
            printf("PASS : %s\n", tc->name);
            passed++;
            break;
        case TEST_SKIP:
            printf("SKIP : %s\n", tc->name);
            skipped++;
            break;
        case TEST_FAIL:
        default:
            printf("FAIL : %s\n", tc->name);
            failed++;
            break;
        }
    }

    printf("Tests finished: %u passed, %u failed, %u skipped\n", passed, failed, skipped);

    return failed == 0 ? 0 : 1;
}
