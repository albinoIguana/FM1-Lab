/* SPDX-License-Identifier: GPL-3.0-only
 * Fixed-point candidate engines for FM1-Lab mixed-synthesis evaluation.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef enum {
    FM1LAB_SYNTH_WAVETABLE = 0,
    FM1LAB_SYNTH_PSG = 1,
    FM1LAB_SYNTH_SUBTRACTIVE = 2,
    FM1LAB_SYNTH_ENGINE_COUNT = 3
} fm1lab_synth_engine_t;

typedef struct {
    uint32_t phase;
    uint32_t phase_b;
    uint32_t phase_inc;
    uint32_t phase_inc_b;
    uint32_t noise;
    int32_t filter_state;
    uint16_t envelope_q15;
    uint16_t filter_alpha_q15;
    uint8_t engine;
    uint8_t note;
    uint8_t velocity;
    uint8_t gate;
    uint8_t active;
} fm1lab_synth_voice_t;

/* One static state object per voice. All render calls are allocation-free,
 * fixed-point, and deterministic at the 44.1 kHz device sample rate. */
void fm1lab_synth_init(fm1lab_synth_voice_t *voice, fm1lab_synth_engine_t engine);
void fm1lab_synth_note_on(fm1lab_synth_voice_t *voice, uint8_t note, uint8_t velocity);
/* Update pitch without resetting oscillator phase; suitable for bend/tuning. */
int fm1lab_synth_set_pitch(fm1lab_synth_voice_t *voice, int16_t pitch256);
void fm1lab_synth_note_off(fm1lab_synth_voice_t *voice);
int fm1lab_synth_render(fm1lab_synth_voice_t *voice, int16_t *output, uint32_t frames);
size_t fm1lab_synth_voice_bytes(void);
