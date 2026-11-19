/* SPDX-License-Identifier: GPL-3.0-only
 * Versioned FM1-Lab performance records retained for settings migration.
 *
 * These structures describe historical serialized layouts. They are not live
 * DSP state and intentionally live outside the synthesis API.
 */
#pragma once
#include "dsp/fm6.h"

typedef struct {
    uint16_t tempo_bpm;
    uint8_t seq_length;
    uint8_t lane_instrument[2];
    uint8_t seq_enabled[2][64];
    uint8_t seq_note[2][64];
    uint8_t seq_velocity[2][64];
    uint8_t seq_gate[2][64];
    uint8_t arp_mode, arp_division, arp_gate, arp_octaves, arp_length;
    uint8_t arp_step_gain[16], arp_step_accent[16], arp_step_gate[16];
    uint8_t arp_output, arp_random, arp_macro_dest, arp_depth;
    uint8_t fx_enabled[4], fx_mix[4], fx_time[4], fx_feedback[4];
    uint8_t fx_send[FM1LAB_PATCH_COUNT][4];
    uint8_t fx_depth[4];
    uint8_t compressor_enabled, compressor_threshold, compressor_ratio;
    uint8_t compressor_attack, compressor_release, compressor_makeup;
    uint8_t seq_event_count[2];
    fm6_seq_event_t seq_events[2][128];
    uint8_t chaos_amount, chaos_rate, chaos_tone, chaos_shape;
} fm6_performance_state_compat_t;



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


/* Persistence-only validator for the historical two-lane settings payload.
 * This deliberately does not enter the live DSP API. */
static inline int fm6_compat_performance_state_valid(const fm6_performance_state_compat_t *s)
{
    if (!s || s->tempo_bpm < 30u || s->tempo_bpm > 300u ||
        s->seq_length < 1u || s->seq_length > 64u || s->arp_mode > 4u ||
        s->arp_division > 5u || s->arp_gate < 10u || s->arp_gate > 100u ||
        s->arp_octaves < 1u || s->arp_octaves > 4u ||
        (s->arp_length != 8u && s->arp_length != 16u) || s->arp_output > 2u ||
        s->arp_random > 100u || s->arp_macro_dest > 3u || s->arp_depth > 100u)
        return 0;
    for (uint32_t step=0u; step<16u; ++step)
        if (s->arp_step_gain[step]>127u || s->arp_step_accent[step]>100u ||
            s->arp_step_gate[step]>1u) return 0;
    for (uint32_t lane=0u; lane<2u; ++lane) {
        if (s->lane_instrument[lane]>=32u || s->seq_event_count[lane]>128u) return 0;
        for (uint32_t step=0u; step<64u; ++step)
            if (s->seq_enabled[lane][step]>1u || s->seq_note[lane][step]>127u ||
                s->seq_velocity[lane][step]>127u || (s->seq_gate[lane][step]&0x7fu)>100u) return 0;
        for (uint32_t i=0u; i<s->seq_event_count[lane]; ++i) {
            const fm6_seq_event_t *e=&s->seq_events[lane][i];
            if (e->start >= (uint32_t)s->seq_length*2u || !e->duration ||
                e->duration > (uint32_t)s->seq_length*2u-e->start ||
                e->pitch>127u || !e->velocity || e->velocity>127u || e->accent>100u) return 0;
        }
    }
    for (uint32_t fx=0u; fx<4u; ++fx) {
        if (s->fx_enabled[fx]>1u || s->fx_mix[fx]>100u || s->fx_time[fx]>100u ||
            s->fx_depth[fx]>100u || s->fx_feedback[fx]>(fx==0u?70u:90u)) return 0;
        for (uint32_t patch=0u; patch<FM1LAB_PATCH_COUNT; ++patch)
            if (s->fx_send[patch][fx]>100u) return 0;
    }
    if (s->compressor_enabled>1u || s->compressor_threshold>100u ||
        s->compressor_ratio>100u || s->compressor_attack>100u ||
        s->compressor_release>100u || s->compressor_makeup>100u ||
        s->chaos_amount>100u || s->chaos_rate>100u ||
        s->chaos_tone>100u || s->chaos_shape>5u) return 0;
    return 1;
}
