/* SPDX-License-Identifier: GPL-3.0-only
 * Four-operator fixed-point FM candidate for mixed-synthesis evaluation.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t phase[4];
    uint32_t phase_inc[4];
    int16_t feedback_sample;
    uint16_t envelope_q15;
    uint8_t note;
    uint8_t velocity;
    uint8_t algorithm;
    uint8_t gate;
    uint8_t active;
} fm1lab_fm4_voice_t;

void fm1lab_fm4_init(fm1lab_fm4_voice_t *voice);
void fm1lab_fm4_note_on(fm1lab_fm4_voice_t *voice, uint8_t note, uint8_t velocity, uint8_t algorithm);
void fm1lab_fm4_note_off(fm1lab_fm4_voice_t *voice);
int fm1lab_fm4_render(fm1lab_fm4_voice_t *voice, int16_t *output, uint32_t frames);
size_t fm1lab_fm4_voice_bytes(void);
