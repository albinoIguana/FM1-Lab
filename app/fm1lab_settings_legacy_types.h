/* SPDX-License-Identifier: GPL-3.0-only
 * Versioned FM1-Lab performance records retained for settings migration.
 *
 * These structures describe historical serialized layouts. They are not live
 * DSP state and intentionally live outside the synthesis API.
 */
#pragma once
#include "dsp/fm6.h"

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

