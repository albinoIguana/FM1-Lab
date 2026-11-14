/* SPDX-License-Identifier: GPL-3.0-only
 * Resettable held maxima for the AUDIO DIAG measurement window. */
#pragma once
#include <stdint.h>

static inline void fm1lab_audio_peak_window_reset(volatile uint32_t *pre_peak,
        volatile uint32_t *limiter_reduction)
{
    *pre_peak=0u;
    *limiter_reduction=0u;
}

static inline void fm1lab_audio_peak_window_update(volatile uint32_t *pre_peak,
        volatile uint32_t *limiter_reduction, uint32_t pre_value, uint32_t reduction_value)
{
    if(pre_value>*pre_peak)*pre_peak=pre_value;
    if(reduction_value>*limiter_reduction)*limiter_reduction=reduction_value;
}
