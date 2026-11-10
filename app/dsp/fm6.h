/* SPDX-License-Identifier: GPL-3.0-only
 * FM1-Lab: four-patch Instrument API. FM6 core provenance is retained in fm6.c. */
#pragma once
#include <stdint.h>
#include "fm1lab_insert_state.h"

#define FM6_SAMPLE_RATE_HZ 44100u /* fm1-base device/main_fm1.c: FS */
#define FM1LAB_PATCH_COUNT 4u
#define FM6_ENGINE_PATCH_COUNT 8u /* two independent four-patch sequencer instruments */
#define FM1LAB_MACRO_COUNT 4u
#define FM1LAB_VOICE_LIMIT 8u /* voices per patch; eight independent pools */
enum { FM6_FX_CHORUS=0u, FM6_FX_DELAY, FM6_FX_REVERB, FM6_FX_PHASER, FM6_FX_COUNT };

/* Event positions and durations are measured in 32nd-note ticks:
 * two ticks per displayed sixteenth step, up to 128 ticks per 64-step loop. */
typedef struct {
    uint8_t start;
    uint8_t duration;
    uint8_t pitch;
    uint8_t velocity;
    uint8_t accent;
} fm6_seq_event_t;

typedef struct {
    uint32_t tick;
    uint8_t lane, note, velocity, on;
} fm6_arp_capture_event_t;

/* Persistent musical/effect controls. Transport playback state and delay buffers
 * are intentionally not serialized; a restored pattern always boots stopped. */
typedef struct {
    uint16_t tempo_bpm;
    uint8_t seq_length;
    uint8_t lane_instrument[2]; /* library Instrument slot (0..31) per sequencer lane */
    uint8_t seq_enabled[2][64];
    uint8_t seq_note[2][64];
    uint8_t seq_velocity[2][64];
    uint8_t seq_gate[2][64]; /* low 7 bits: gate %, high bit: tie from previous step */
    uint8_t arp_mode, arp_division, arp_gate, arp_octaves, arp_length;
    uint8_t arp_step_gain[16], arp_step_accent[16], arp_step_gate[16];
    uint8_t arp_output, arp_random, arp_macro_dest, arp_depth;
    uint8_t fx_enabled[4];
    uint8_t fx_mix[4];
    uint8_t fx_time[4];
    uint8_t fx_feedback[4];
    uint8_t fx_send[FM1LAB_PATCH_COUNT][4]; /* layer-role sends shared by both track banks */
    uint8_t fx_depth[4];
    uint8_t compressor_enabled, compressor_threshold, compressor_ratio;
    uint8_t compressor_attack, compressor_release, compressor_makeup;
} fm6_performance_state_v9_t;

typedef struct {
    uint16_t tempo_bpm;
    uint8_t seq_length;
    uint8_t lane_instrument[2]; /* library Instrument slot (0..31) per sequencer lane */
    uint8_t seq_enabled[2][64];
    uint8_t seq_note[2][64];
    uint8_t seq_velocity[2][64];
    uint8_t seq_gate[2][64]; /* low 7 bits: gate %, high bit: tie from previous step */
    uint8_t arp_mode, arp_division, arp_gate, arp_octaves, arp_length;
    uint8_t arp_step_gain[16], arp_step_accent[16], arp_step_gate[16];
    uint8_t arp_output, arp_random, arp_macro_dest, arp_depth;
    uint8_t fx_enabled[4];
    uint8_t fx_mix[4];
    uint8_t fx_time[4];
    uint8_t fx_feedback[4];
    uint8_t fx_send[FM1LAB_PATCH_COUNT][4]; /* layer-role sends shared by both track banks */
    uint8_t fx_depth[4];
    uint8_t compressor_enabled, compressor_threshold, compressor_ratio;
    uint8_t compressor_attack, compressor_release, compressor_makeup;
    uint8_t seq_event_count[2];
    fm6_seq_event_t seq_events[2][128];
} fm6_performance_state_v10_t;

typedef struct {
    uint16_t tempo_bpm;
    uint8_t seq_length;
    uint8_t lane_instrument[2]; /* library Instrument slot (0..31) per sequencer lane */
    uint8_t seq_enabled[2][64];
    uint8_t seq_note[2][64];
    uint8_t seq_velocity[2][64];
    uint8_t seq_gate[2][64]; /* low 7 bits: gate %, high bit: tie from previous step */
    uint8_t arp_mode, arp_division, arp_gate, arp_octaves, arp_length;
    uint8_t arp_step_gain[16], arp_step_accent[16], arp_step_gate[16];
    uint8_t arp_output, arp_random, arp_macro_dest, arp_depth;
    uint8_t fx_enabled[4];
    uint8_t fx_mix[4];
    uint8_t fx_time[4];
    uint8_t fx_feedback[4];
    uint8_t fx_send[FM1LAB_PATCH_COUNT][4]; /* layer-role sends shared by both track banks */
    uint8_t fx_depth[4];
    uint8_t compressor_enabled, compressor_threshold, compressor_ratio;
    uint8_t compressor_attack, compressor_release, compressor_makeup;
    uint8_t seq_event_count[2];
    fm6_seq_event_t seq_events[2][128];
    uint8_t chaos_amount, chaos_rate, chaos_tone, chaos_shape;
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
/* Sequencer events use unique nonzero tokens (>3), so releasing one repeated
 * pitch cannot release another overlapping event on the same lane. */
void fm6_sequencer_note_on(uint8_t note, uint8_t velocity, uint8_t patch_mask, uint16_t event_token);
void fm6_sequencer_note_off(uint8_t note, uint8_t patch_mask, uint16_t event_token);
void fm6_note_on_tuned_mask(uint8_t note, uint8_t velocity, uint8_t patch_mask, int16_t tune16);
void fm6_note_off(uint8_t note);
void fm6_note_off_mask(uint8_t note, uint8_t patch_mask);
void fm6_pitch_bend(uint16_t value);
void fm6_set_tempo(uint16_t bpm);
void fm6_set_swing(uint8_t percent);
uint8_t fm6_get_swing(void);
void fm6_sequencer_set_running(uint8_t running);
void fm6_arpeggiator_reset_steps(void);
void fm6_sequencer_clear_lane(uint8_t lane);
void fm6_sequencer_set_length(uint8_t steps);
void fm6_sequencer_set_lane(uint8_t lane, uint8_t patch_base); /* 0 for T1 bank, 4 for T2 bank */
void fm6_sequencer_set_step(uint8_t lane, uint8_t step, uint8_t enabled, uint8_t note, uint8_t velocity, uint8_t gate);
int fm6_sequencer_set_events(uint8_t lane, const fm6_seq_event_t *events, uint16_t count);
uint8_t fm6_sequencer_step(void);
uint8_t fm6_sequencer_playing_step(void);
uint8_t fm6_sequencer_tick_position(void);
uint8_t fm6_sequencer_running(void);
uint8_t fm6_arp_capture_pop(fm6_arp_capture_event_t *event);
uint8_t fm6_arpeggiator_playing_step(void);
uint8_t fm6_arpeggiator_active(void);
void fm6_arpeggiator_set_mode(uint8_t mode);
uint8_t fm6_arpeggiator_mode(void);
void fm6_arpeggiator_set_latch(uint8_t enabled);
#ifdef FM6_TEST
uint8_t fm6_test_arpeggiator_amp_level(void);
void fm6_test_set_cpu2_mode(uint8_t mode); /* 0 serial, 1 split, 2 simulated partial-job fallback */
#endif
int fm6_performance_state_valid(const fm6_performance_state_t *state);
void fm6_performance_state_get(fm6_performance_state_t *state);
int fm6_performance_state_set(const fm6_performance_state_t *state);
int fm6_performance_state_update(const fm6_performance_state_t *state);
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
/* master_gain_q12 is fm1-base APP_GAIN: Q12 amplitude, 0..4096. */
void fm6_render(int32_t *stereo, uint32_t frames, uint32_t master_gain_q12);
#ifdef FM6_TEST
/* Test-only pre-clip Q24 probes; never used by the device UI/audio path. */
uint64_t fm6_test_patch_peak(uint32_t patch);
uint64_t fm6_test_dry_peak(void);
uint64_t fm6_test_fx_send_peak(uint32_t effect, uint32_t channel);
uint32_t fm6_test_fx_saturations(void);
uint32_t fm6_test_limiter_events(void);
uint64_t fm6_test_output_peak(void);
int32_t fm6_test_macro_mod_level(uint32_t patch);
uint8_t fm6_test_macro_feedback(uint32_t patch);
int32_t fm6_test_macro_detune(uint32_t patch, uint32_t operator_index);
uint8_t fm6_test_effective_byte(uint32_t patch, uint32_t byte_index);
uint32_t fm6_test_sequencer_active_events(uint8_t lane);
#endif

uint32_t fm6_patch_count(void);
const char *fm6_patch_name(uint32_t patch);
uint8_t fm6_patch_enabled(uint32_t patch);
void fm6_patch_set_enabled(uint32_t patch, uint8_t enabled);
int fm6_patch_set_voice(uint32_t patch, const uint8_t *voice);
void fm6_patch_set_level(uint32_t patch, uint8_t level);
uint8_t fm6_patch_level(uint32_t patch);
void fm6_patch_set_pan(uint32_t patch, uint8_t pan);
/* Whole-patch tuning is applied after the DX7 note lookup, independently of operator ratios. */
int fm6_patch_set_pitch(uint32_t patch, int octave, int fine_cents);
uint8_t fm6_patch_pan(uint32_t patch);
void fm6_master_insert_set(uint32_t field, uint8_t value);
void fm6_master_insert_reset_state(void);
/* MAELSTROM is the instrument-wide destructive post-mix effect. */
void fm6_chaos_set(uint8_t amount,uint8_t rate,uint8_t tone,uint8_t shape);
void fm6_chaos_get(uint8_t *amount,uint8_t *rate,uint8_t *tone,uint8_t *shape);
uint8_t fm6_master_insert_get(uint32_t field);
void fm6_master_comp_set(uint32_t field, uint8_t value);
uint8_t fm6_master_comp_get(uint32_t field);
uint8_t fm6_patch_algorithm(uint32_t patch);
uint8_t fm6_operator_is_carrier(uint32_t patch, uint32_t operator_1_to_6);
const uint8_t *fm6_patch_data(uint32_t patch);
void fm6_patch_set_byte(uint32_t patch, uint32_t byte_index, uint8_t value);
void fm6_macro_set(uint32_t patch, uint32_t macro, uint8_t value);
uint8_t fm6_macro_get(uint32_t patch, uint32_t macro);
uint32_t fm6_active_voices(void);
uint32_t fm6_active_voices_patch(uint32_t patch);
