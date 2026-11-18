/* SPDX-License-Identifier: GPL-3.0-only
 * FM1-Lab DSP capacity and physical instrument-slot contract.
 *
 * Keep live capacities and compatibility context counts explicit. A future
 * single-bank runtime must change the runtime context count independently
 * from legacy persistence formats; do not silently reinterpret saved records.
 */
#pragma once

#define FM1LAB_PATCH_COUNT 4u
#define FM1LAB_MACRO_COUNT 4u
#define FM1LAB_VOICE_LIMIT 8u

#define FM1LAB_INSTRUMENT_SLOT_COUNT FM1LAB_PATCH_COUNT
#define FM1LAB_SLOT_FM6 0u
#define FM1LAB_SLOT_WAVETABLE 1u
#define FM1LAB_SLOT_PSG 2u
#define FM1LAB_SLOT_SUBTRACTIVE 3u

/* Compatibility/runtime context sizing: two historical banks still exist in
 * the current DSP implementation. These are not the desired live target. */
#define FM1LAB_INSTRUMENT_BANK_COUNT 2u
#define FM1LAB_PATCH_CONTEXT_COUNT \
    (FM1LAB_INSTRUMENT_SLOT_COUNT * FM1LAB_INSTRUMENT_BANK_COUNT)
#define FM6_ENGINE_PATCH_COUNT FM1LAB_PATCH_CONTEXT_COUNT
#define FM1LAB_VOICE_CONTEXT_COUNT \
    (FM6_ENGINE_PATCH_COUNT * FM1LAB_VOICE_LIMIT)

#if FM1LAB_PATCH_COUNT != 4u
#error "FM1-Lab Instruments must retain exactly four physical engine slots"
#endif
#if FM1LAB_SLOT_FM6 != 0u || FM1LAB_SLOT_WAVETABLE != 1u || \
    FM1LAB_SLOT_PSG != 2u || FM1LAB_SLOT_SUBTRACTIVE != 3u
#error "FM1-Lab physical engine slot order is part of the Instrument contract"
#endif
