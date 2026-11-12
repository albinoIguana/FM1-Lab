/* SPDX-License-Identifier: MPL-2.0
 * Backported from hugelton/felucca-bsp e36cb7c578fd90e2e83672194144507dd348c82c.
 *
 * Completion wait for the FM1 SPI NOR driver. The includer supplies RAMINL,
 * fl_rdsr(), T4_CNT and fl_reset_busy(). Never return to XIP while BUSY.
 */
#pragma once
#define FL_ERR_LATE_READY (-4)

RAMINL int fl_wait_issued(uint32_t issued, uint32_t normal_us,
                          uint32_t extended_us, uint32_t *took_us)
{
    for (;;) {
        uint32_t status = fl_rdsr(0x05);
        uint32_t elapsed = (T4_CNT - issued) / 24u;
        if (!(status & 1u)) {
            *took_us = elapsed;
            return elapsed <= normal_us ? 0 : FL_ERR_LATE_READY;
        }
        if (elapsed >= extended_us)
            fl_reset_busy();
    }
}
