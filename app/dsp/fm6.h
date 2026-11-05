/* SPDX-License-Identifier: GPL-3.0-only
 * FM1-Lab: four-patch Instrument API. FM6 core provenance is retained in fm6.c. */
#pragma once
#include <stdint.h>

#define FM1LAB_PATCH_COUNT 4u
#define FM1LAB_MACRO_COUNT 4u
#define FM1LAB_VOICE_LIMIT 8u /* voices per patch; four independent pools */

enum {
    FM1LAB_MACRO_BRIGHTNESS = 0,
    FM1LAB_MACRO_BODY,
    FM1LAB_MACRO_MOTION,
    FM1LAB_MACRO_EDGE,
    /* Source compatibility for older preset metadata. */
    FM1LAB_MACRO_DIRT = FM1LAB_MACRO_EDGE
};

void fm6_init(void);
void fm6_panic(void);
void fm6_note_on(uint8_t note, uint8_t velocity);
void fm6_note_on_mask(uint8_t note, uint8_t velocity, uint8_t patch_mask);
void fm6_note_on_tuned_mask(uint8_t note, uint8_t velocity, uint8_t patch_mask, int16_t tune16);
void fm6_note_off(uint8_t note);
void fm6_note_off_mask(uint8_t note, uint8_t patch_mask);
void fm6_pitch_bend(uint16_t value);
void fm6_render(int32_t *stereo, uint32_t frames, uint32_t master_q12);

uint32_t fm6_patch_count(void);
const char *fm6_patch_name(uint32_t patch);
uint8_t fm6_patch_enabled(uint32_t patch);
void fm6_patch_set_enabled(uint32_t patch, uint8_t enabled);
int fm6_patch_set_voice(uint32_t patch, const uint8_t *voice);
void fm6_patch_set_level(uint32_t patch, uint8_t level);
uint8_t fm6_patch_algorithm(uint32_t patch);
uint8_t fm6_operator_is_carrier(uint32_t patch, uint32_t operator_1_to_6);
const uint8_t *fm6_patch_data(uint32_t patch);
void fm6_patch_set_byte(uint32_t patch, uint32_t byte_index, uint8_t value);
void fm6_macro_set(uint32_t patch, uint32_t macro, uint8_t value);
uint8_t fm6_macro_get(uint32_t patch, uint32_t macro);
uint32_t fm6_active_voices(void);
