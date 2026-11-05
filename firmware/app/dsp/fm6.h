/* SPDX-License-Identifier: GPL-3.0-only
 * FM1-Lab: four-patch Instrument API. FM6 core provenance is retained in fm6.c. */
#pragma once
#include <stdint.h>

#define FM1LAB_PATCH_COUNT 4u
#define FM1LAB_MACRO_COUNT 4u
#define FM1LAB_VOICE_LIMIT 8u
#define FM1LAB_INSTRUMENT_COUNT 16u
#define FM1LAB_INSTRUMENT_NAME_LEN 12u
#define FM1LAB_PACKED_PATCH_BYTES 128u

typedef struct {
    uint8_t valid;
    uint8_t enabled_mask;
    uint8_t name[FM1LAB_INSTRUMENT_NAME_LEN];
    uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT];
    uint8_t patches[FM1LAB_PATCH_COUNT][FM1LAB_PACKED_PATCH_BYTES];
} fm1lab_instrument_record_t;

enum {
    FM1LAB_MACRO_BRIGHTNESS = 0,
    FM1LAB_MACRO_BODY,
    FM1LAB_MACRO_MOTION,
    FM1LAB_MACRO_DIRT
};

void fm6_init(void);
void fm6_panic(void);
void fm6_note_on(uint8_t note, uint8_t velocity);
void fm6_note_off(uint8_t note);
void fm6_render(int32_t *stereo, uint32_t frames, uint32_t master_q12);

uint32_t fm6_patch_count(void);
const char *fm6_patch_name(uint32_t patch);
uint8_t fm6_patch_enabled(uint32_t patch);
void fm6_patch_set_enabled(uint32_t patch, uint8_t enabled);
uint8_t fm6_patch_algorithm(uint32_t patch);
uint8_t fm6_operator_is_carrier(uint32_t patch, uint32_t operator_1_to_6);
const uint8_t *fm6_patch_data(uint32_t patch);
void fm6_patch_set_byte(uint32_t patch, uint32_t byte_index, uint8_t value);
void fm6_macro_set(uint32_t patch, uint32_t macro, uint8_t value);
uint8_t fm6_macro_get(uint32_t patch, uint32_t macro);
uint32_t fm6_active_voices(void);

void fm6_instrument_load(uint32_t slot, const fm1lab_instrument_record_t *record);
void fm6_instrument_export_current(fm1lab_instrument_record_t *record);
void fm6_select_instrument(uint32_t slot);
uint32_t fm6_current_instrument(void);
const char *fm6_instrument_name(uint32_t slot);
uint8_t fm6_instrument_valid(uint32_t slot);
uint8_t fm6_instrument_dirty(uint32_t slot);
void fm6_instrument_set_name_char(uint32_t position, uint8_t character);
void fm6_instrument_mark_saved(uint32_t slot);
uint8_t fm6_patch_set_algorithm(uint32_t patch, uint8_t algorithm);
