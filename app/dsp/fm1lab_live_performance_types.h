/* SPDX-License-Identifier: GPL-3.0-only
 * Shared live-performance data types. No persistence compatibility types.
 */
#pragma once
#include <stdint.h>
#include "fm1lab_dsp_config.h"

typedef struct {
    uint8_t start;
    uint8_t duration;
    uint8_t pitch;
    uint8_t velocity;
    uint8_t accent;
} fm6_seq_event_t;

typedef fm6_seq_event_t fm1lab_live_seq_event_t;

typedef struct {
    uint16_t tempo_bpm;
    uint8_t seq_length;
    uint8_t instrument;
    uint8_t seq_enabled[64];
    uint8_t seq_note[64];
    uint8_t seq_velocity[64];
    uint8_t seq_gate[64];
    uint8_t arp_mode, arp_division, arp_gate, arp_octaves, arp_length;
    uint8_t arp_step_gain[16], arp_step_accent[16], arp_step_gate[16];
    uint8_t arp_output, arp_random, arp_macro_dest, arp_depth;
    uint8_t fx_enabled[4], fx_mix[4], fx_time[4], fx_feedback[4];
    uint8_t fx_send[FM1LAB_PATCH_COUNT][4], fx_depth[4];
    uint8_t seq_event_count;
    fm1lab_live_seq_event_t seq_events[128];
    uint8_t chaos_amount, chaos_rate, chaos_tone, chaos_shape;
} fm1lab_live_performance_state_t;
