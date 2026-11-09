/* SPDX-License-Identifier: MPL-2.0
 * FM1-Lab sequencer recording quantization helpers.
 * Kept freestanding so the exact device-side duration math is host-testable. */
#ifndef FM1LAB_SEQUENCER_RECORD_H
#define FM1LAB_SEQUENCER_RECORD_H
#include <stdint.h>

/* Convert a held-note time into a quantized step duration. Notes are bounded
 * by the end of the active loop; zero-time taps still produce one step. */
static inline uint8_t fm1lab_recorded_note_duration(uint32_t elapsed_ms,
                                                    uint32_t step_ms,
                                                    uint32_t loop_length,
                                                    uint32_t first_step)
{
    if (step_ms == 0u) step_ms = 1u;
    if (loop_length < 2u) loop_length = 2u;
    if (loop_length > 64u) loop_length = 64u;
    if (first_step >= loop_length) return 0u;

    uint32_t duration = elapsed_ms / step_ms;
    if ((elapsed_ms % step_ms) != 0u) ++duration;
    if (duration < 1u) duration = 1u;

    const uint32_t remaining = loop_length - first_step;
    if (duration > remaining) duration = remaining;
    if (duration > 64u) duration = 64u;
    return (uint8_t)duration;
}
#endif
