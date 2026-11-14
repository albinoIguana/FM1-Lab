/* SPDX-License-Identifier: GPL-3.0-only
 * CPU2 startup self-test decision shared by the app and native regression test. */
#pragma once
#include <stdint.h>

static inline uint8_t fm1lab_cpu2_selftest_success(uint8_t started,
        uint8_t dispatched, int wait_result, uint32_t actual, uint32_t expected)
{
    return (uint8_t)(started && dispatched && wait_result == 0 && actual == expected);
}
