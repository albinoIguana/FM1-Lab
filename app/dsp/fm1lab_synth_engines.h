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

/* Compact, engine-native patch payload. The physical Instrument slot supplies
 * the engine type; these values are the actual sound recipe, not display data. */
typedef struct {
    uint8_t waveform;          /* 0 sine, 1 triangle, 2 saw, 3 pulse */
    uint8_t secondary_waveform;/* wavetable morph target / secondary tone */
    uint8_t duty;              /* pulse width, 5..95 */
    uint8_t noise_mix;         /* PSG noise mix / subtractive filter-envelope depth */
    uint8_t osc_mix;           /* subtractive oscillator B mix, 0..100 */
    uint8_t detune;            /* subtractive oscillator detune / PSG noise clock rate, 0..100 */
    uint8_t cutoff;            /* subtractive low-pass cutoff, 0..100 */
    uint8_t attack;            /* 0..100 */
    uint8_t sustain;           /* 0..100 */
    uint8_t release;           /* 0..100 */
} fm1lab_synth_patch_t;

typedef struct {
    uint32_t phase;
    uint32_t phase_b;
    uint32_t phase_inc;
    uint32_t phase_inc_b;
    uint32_t noise;
    uint32_t noise_phase;
    int32_t filter_state;
    int32_t filter_state2;
    uint16_t envelope_q15;
    uint16_t envelope_attack_step;
    uint16_t envelope_decay_step;
    uint16_t envelope_release_step;
    uint16_t filter_alpha_q15;
    uint8_t envelope_stage;
    fm1lab_synth_patch_t patch;
    uint8_t engine;
    uint8_t note;
    uint8_t velocity;
    uint8_t gate;
    uint8_t active;
} fm1lab_synth_voice_t;

/* Engine-native defaults, validation and patch application are allocation-free. */
void fm1lab_synth_patch_default(fm1lab_synth_engine_t engine, fm1lab_synth_patch_t *patch);
int fm1lab_synth_patch_valid(fm1lab_synth_engine_t engine, const fm1lab_synth_patch_t *patch);
int fm1lab_synth_set_patch(fm1lab_synth_voice_t *voice, const fm1lab_synth_patch_t *patch);

/* One static state object per voice. All render calls are allocation-free,
 * fixed-point, and deterministic at the 44.1 kHz device sample rate. */
void fm1lab_synth_init(fm1lab_synth_voice_t *voice, fm1lab_synth_engine_t engine);
void fm1lab_synth_note_on(fm1lab_synth_voice_t *voice, uint8_t note, uint8_t velocity);
/* Update pitch without resetting oscillator phase; suitable for bend/tuning. */
int fm1lab_synth_set_pitch(fm1lab_synth_voice_t *voice, int16_t pitch256);
void fm1lab_synth_note_off(fm1lab_synth_voice_t *voice);
int fm1lab_synth_render(fm1lab_synth_voice_t *voice, int16_t *output, uint32_t frames);
size_t fm1lab_synth_voice_bytes(void);
