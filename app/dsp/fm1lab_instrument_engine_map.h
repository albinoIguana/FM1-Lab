/* SPDX-License-Identifier: GPL-3.0-only
 * Fixed physical engine assignment for the four-slot FM1-Lab Instrument.
 *
 * Patch context 0..3 is the first sequencer bank; 4..7 is the second.
 * The bank offset never changes the physical slot's engine identity.
 */
#pragma once
#include <stdint.h>

typedef enum {
    FM1LAB_PATCH_ENGINE_FM6 = 0,
    FM1LAB_PATCH_ENGINE_WAVETABLE = 1,
    FM1LAB_PATCH_ENGINE_PSG = 2,
    FM1LAB_PATCH_ENGINE_SUBTRACTIVE = 3,
    FM1LAB_PATCH_ENGINE_INVALID = 0xff
} fm1lab_patch_engine_t;

#define FM1LAB_INSTRUMENT_SLOT_COUNT 4u
#define FM1LAB_INSTRUMENT_BANK_COUNT 2u
#define FM1LAB_PATCH_CONTEXT_COUNT (FM1LAB_INSTRUMENT_SLOT_COUNT * FM1LAB_INSTRUMENT_BANK_COUNT)

static inline fm1lab_patch_engine_t fm1lab_patch_engine_for_context(uint8_t patch_context)
{
    if (patch_context >= FM1LAB_PATCH_CONTEXT_COUNT)
        return FM1LAB_PATCH_ENGINE_INVALID;
    return (fm1lab_patch_engine_t)(patch_context % FM1LAB_INSTRUMENT_SLOT_COUNT);
}
