/* SPDX-License-Identifier: GPL-3.0-only
 * Single-track live performance state.
 *
 * This is the runtime shape. Versioned two-lane records remain persistence
 * concerns and must be decoded into this type before reaching live systems.
 */
#pragma once
#include <stdint.h>
#include "dsp/fm6.h"

typedef struct {
    uint8_t start;
    uint8_t duration;
    uint8_t pitch;
    uint8_t velocity;
    uint8_t accent;
} fm1lab_live_seq_event_t;

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

/* Decode the current two-lane-shaped compatibility state into the single
 * live lane. Lane 1 is intentionally not copied. Retired compressor fields
 * are absent from the destination type. */
static inline int fm1lab_live_performance_from_compat(
    fm1lab_live_performance_state_t *dst,
    const fm6_performance_state_t *src)
{
    if (!dst || !src || src->seq_event_count[0] > 128u) return -1;
    *dst = (fm1lab_live_performance_state_t){0};
    dst->tempo_bpm = src->tempo_bpm;
    dst->seq_length = src->seq_length;
    dst->instrument = src->lane_instrument[0];
    for (uint32_t step = 0u; step < 64u; ++step) {
        dst->seq_enabled[step] = src->seq_enabled[0][step];
        dst->seq_note[step] = src->seq_note[0][step];
        dst->seq_velocity[step] = src->seq_velocity[0][step];
        dst->seq_gate[step] = src->seq_gate[0][step];
    }
    dst->arp_mode = src->arp_mode;
    dst->arp_division = src->arp_division;
    dst->arp_gate = src->arp_gate;
    dst->arp_octaves = src->arp_octaves;
    dst->arp_length = src->arp_length;
    for (uint32_t step = 0u; step < 16u; ++step) {
        dst->arp_step_gain[step] = src->arp_step_gain[step];
        dst->arp_step_accent[step] = src->arp_step_accent[step];
        dst->arp_step_gate[step] = src->arp_step_gate[step];
    }
    dst->arp_output = src->arp_output;
    dst->arp_random = src->arp_random;
    dst->arp_macro_dest = src->arp_macro_dest;
    dst->arp_depth = src->arp_depth;
    for (uint32_t fx = 0u; fx < 4u; ++fx) {
        dst->fx_enabled[fx] = src->fx_enabled[fx];
        dst->fx_mix[fx] = src->fx_mix[fx];
        dst->fx_time[fx] = src->fx_time[fx];
        dst->fx_feedback[fx] = src->fx_feedback[fx];
        dst->fx_depth[fx] = src->fx_depth[fx];
        for (uint32_t patch = 0u; patch < FM1LAB_PATCH_COUNT; ++patch)
            dst->fx_send[patch][fx] = src->fx_send[patch][fx];
    }
    dst->seq_event_count = src->seq_event_count[0];
    for (uint32_t i = 0u; i < dst->seq_event_count; ++i) {
        const fm6_seq_event_t *from = &src->seq_events[0][i];
        fm1lab_live_seq_event_t *to = &dst->seq_events[i];
        to->start = from->start;
        to->duration = from->duration;
        to->pitch = from->pitch;
        to->velocity = from->velocity;
        to->accent = from->accent;
    }
    dst->chaos_amount = src->chaos_amount;
    dst->chaos_rate = src->chaos_rate;
    dst->chaos_tone = src->chaos_tone;
    dst->chaos_shape = src->chaos_shape;
    return 0;
}

/* Bridge the one-lane runtime model to the legacy-shaped DSP API while the
 * DSP internals are migrated. Lane 1 and retired compressor controls are
 * deliberately initialized to inert compatibility defaults. */
static inline int fm1lab_live_performance_to_compat(
    fm6_performance_state_t *dst,
    const fm1lab_live_performance_state_t *src)
{
    if (!dst || !src || src->instrument >= 32u ||
        src->seq_length < 1u || src->seq_length > 64u ||
        src->seq_event_count > 128u) return -1;
    *dst = (fm6_performance_state_t){0};
    dst->tempo_bpm = src->tempo_bpm;
    dst->seq_length = src->seq_length;
    dst->lane_instrument[0] = src->instrument;
    for (uint32_t step = 0u; step < 64u; ++step) {
        dst->seq_enabled[0][step] = src->seq_enabled[step];
        dst->seq_note[0][step] = src->seq_note[step];
        dst->seq_velocity[0][step] = src->seq_velocity[step];
        dst->seq_gate[0][step] = src->seq_gate[step];
    }
    dst->arp_mode = src->arp_mode;
    dst->arp_division = src->arp_division;
    dst->arp_gate = src->arp_gate;
    dst->arp_octaves = src->arp_octaves;
    dst->arp_length = src->arp_length;
    for (uint32_t step = 0u; step < 16u; ++step) {
        dst->arp_step_gain[step] = src->arp_step_gain[step];
        dst->arp_step_accent[step] = src->arp_step_accent[step];
        dst->arp_step_gate[step] = src->arp_step_gate[step];
    }
    dst->arp_output = src->arp_output;
    dst->arp_random = src->arp_random;
    dst->arp_macro_dest = src->arp_macro_dest;
    dst->arp_depth = src->arp_depth;
    for (uint32_t fx = 0u; fx < 4u; ++fx) {
        dst->fx_enabled[fx] = src->fx_enabled[fx];
        dst->fx_mix[fx] = src->fx_mix[fx];
        dst->fx_time[fx] = src->fx_time[fx];
        dst->fx_feedback[fx] = src->fx_feedback[fx];
        dst->fx_depth[fx] = src->fx_depth[fx];
        for (uint32_t patch = 0u; patch < FM1LAB_PATCH_COUNT; ++patch)
            dst->fx_send[patch][fx] = src->fx_send[patch][fx];
    }
    dst->seq_event_count[0] = src->seq_event_count;
    for (uint32_t i = 0u; i < src->seq_event_count; ++i) {
        const fm1lab_live_seq_event_t *from = &src->seq_events[i];
        fm6_seq_event_t *to = &dst->seq_events[0][i];
        to->start = from->start;
        to->duration = from->duration;
        to->pitch = from->pitch;
        to->velocity = from->velocity;
        to->accent = from->accent;
    }
    dst->chaos_amount = src->chaos_amount;
    dst->chaos_rate = src->chaos_rate;
    dst->chaos_tone = src->chaos_tone;
    dst->chaos_shape = src->chaos_shape;
    dst->compressor_enabled = 0u;
    dst->compressor_threshold = 65u;
    dst->compressor_ratio = 40u;
    dst->compressor_attack = 20u;
    dst->compressor_release = 40u;
    dst->compressor_makeup = 0u;
    return 0;
}
