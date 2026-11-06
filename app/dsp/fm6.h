/* SPDX-License-Identifier: GPL-3.0-only
 * FM1-Lab: four-patch Instrument API. FM6 core provenance is retained in fm6.c. */
#pragma once
#include <stdint.h>

#define FM6_SAMPLE_RATE_HZ 44100u /* fm1-base device/main_fm1.c: FS */
#define FM1LAB_PATCH_COUNT 4u
#define FM1LAB_MACRO_COUNT 4u
#define FM1LAB_VOICE_LIMIT 8u /* voices per patch; four independent pools */
enum { FM6_FX_CHORUS=0u, FM6_FX_DELAY, FM6_FX_REVERB, FM6_FX_PHASER, FM6_FX_COUNT };

/* Persistent musical/effect controls. Transport playback state and delay buffers
 * are intentionally not serialized; a restored pattern always boots stopped. */
typedef struct {
    uint16_t tempo_bpm;
    uint8_t seq_length;
    uint8_t lane_patch[2];
    uint8_t seq_enabled[2][64];
    uint8_t seq_note[2][64];
    uint8_t seq_velocity[2][64];
    uint8_t seq_gate[2][64];
    uint8_t arp_mode;
    uint8_t fx_enabled[4];
    uint8_t fx_mix[4];
    uint8_t fx_time[4];
    uint8_t fx_feedback[4];
    uint8_t fx_send[4][4];
    uint8_t fx_depth[4];
} fm6_performance_state_t;

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
void fm6_overload(void); /* gracefully shed one oldest voice after an audio deadline warning */
uint32_t fm6_overload_shed_count(void);
void fm6_note_on(uint8_t note, uint8_t velocity);
void fm6_note_on_mask(uint8_t note, uint8_t velocity, uint8_t patch_mask);
void fm6_note_on_tuned_mask(uint8_t note, uint8_t velocity, uint8_t patch_mask, int16_t tune16);
void fm6_note_off(uint8_t note);
void fm6_note_off_mask(uint8_t note, uint8_t patch_mask);
void fm6_pitch_bend(uint16_t value);
void fm6_set_tempo(uint16_t bpm);
void fm6_sequencer_set_running(uint8_t running);
void fm6_sequencer_set_length(uint8_t steps);
void fm6_sequencer_set_lane(uint8_t lane, uint8_t patch);
void fm6_sequencer_set_step(uint8_t lane, uint8_t step, uint8_t enabled, uint8_t note, uint8_t velocity, uint8_t gate);
uint8_t fm6_sequencer_step(void);
uint8_t fm6_sequencer_running(void);
void fm6_arpeggiator_set_mode(uint8_t mode);
uint8_t fm6_arpeggiator_mode(void);
int fm6_performance_state_valid(const fm6_performance_state_t *state);
void fm6_performance_state_get(fm6_performance_state_t *state);
int fm6_performance_state_set(const fm6_performance_state_t *state);
void fm6_fx_set_enabled(uint8_t effect, uint8_t enabled);
void fm6_fx_set_mix(uint8_t effect, uint8_t mix);
void fm6_fx_set_time(uint8_t effect, uint8_t value);
void fm6_fx_set_feedback(uint8_t effect, uint8_t value);
void fm6_fx_set_depth(uint8_t effect, uint8_t value);
uint8_t fm6_fx_enabled(uint8_t effect);
uint8_t fm6_fx_mix(uint8_t effect);
uint8_t fm6_fx_time(uint8_t effect);
uint8_t fm6_fx_feedback(uint8_t effect);
uint8_t fm6_fx_depth(uint8_t effect);
void fm6_fx_set_send(uint8_t patch, uint8_t effect, uint8_t send);
uint8_t fm6_fx_get_send(uint8_t patch, uint8_t effect);
/* Render length must be a multiple of the FM6 core's 32-frame quantum (CTL). */
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
uint32_t fm6_active_voices_patch(uint32_t patch);
