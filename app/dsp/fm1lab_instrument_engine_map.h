/* SPDX-License-Identifier: GPL-3.0-only
 * Fixed physical engine assignment for the four-slot FM1-Lab Instrument.
 *
 * The four runtime contexts are the four physical slots of one Instrument.
 */
#pragma once
#include <stdint.h>
#include "fm1lab_dsp_config.h"

typedef enum {
    FM1LAB_PATCH_ENGINE_FM6 = 0,
    FM1LAB_PATCH_ENGINE_WAVETABLE = 1,
    FM1LAB_PATCH_ENGINE_PSG = 2,
    FM1LAB_PATCH_ENGINE_SUBTRACTIVE = 3,
    FM1LAB_PATCH_ENGINE_INVALID = 0xff
} fm1lab_patch_engine_t;


static inline fm1lab_patch_engine_t fm1lab_patch_engine_for_context(uint8_t patch_context)
{
    if (patch_context >= FM1LAB_PATCH_CONTEXT_COUNT)
        return FM1LAB_PATCH_ENGINE_INVALID;
    switch (patch_context % FM1LAB_INSTRUMENT_SLOT_COUNT) {
    case FM1LAB_SLOT_FM6: return FM1LAB_PATCH_ENGINE_FM6;
    case FM1LAB_SLOT_WAVETABLE: return FM1LAB_PATCH_ENGINE_WAVETABLE;
    case FM1LAB_SLOT_PSG: return FM1LAB_PATCH_ENGINE_PSG;
    case FM1LAB_SLOT_SUBTRACTIVE: return FM1LAB_PATCH_ENGINE_SUBTRACTIVE;
    default: return FM1LAB_PATCH_ENGINE_INVALID;
    }
}

static inline const char *fm1lab_patch_engine_name(uint8_t patch_context)
{
    switch (fm1lab_patch_engine_for_context(patch_context)) {
    case FM1LAB_PATCH_ENGINE_FM6: return "FM6";
    case FM1LAB_PATCH_ENGINE_WAVETABLE: return "WAVETABLE";
    case FM1LAB_PATCH_ENGINE_PSG: return "PSG";
    case FM1LAB_PATCH_ENGINE_SUBTRACTIVE: return "SUBTRACTIVE";
    default: return "INVALID";
    }
}
static inline const char *fm1lab_patch_engine_short_name(uint8_t patch_context)
{
    switch (fm1lab_patch_engine_for_context(patch_context)) {
    case FM1LAB_PATCH_ENGINE_FM6: return "FM6";
    case FM1LAB_PATCH_ENGINE_WAVETABLE: return "WT";
    case FM1LAB_PATCH_ENGINE_PSG: return "PSG";
    case FM1LAB_PATCH_ENGINE_SUBTRACTIVE: return "SUB";
    default: return "--";
    }
}
