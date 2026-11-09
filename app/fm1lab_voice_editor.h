#ifndef FM1LAB_VOICE_EDITOR_H
#define FM1LAB_VOICE_EDITOR_H

#include <stdint.h>
#include "fm1lab_instrument.h"

typedef enum {
    FM1LAB_OP_RATE_1,
    FM1LAB_OP_RATE_2,
    FM1LAB_OP_RATE_3,
    FM1LAB_OP_RATE_4,
    FM1LAB_OP_LEVEL_1,
    FM1LAB_OP_LEVEL_2,
    FM1LAB_OP_LEVEL_3,
    FM1LAB_OP_LEVEL_4,
    FM1LAB_OP_OUTPUT,
    FM1LAB_OP_MODE,
    FM1LAB_OP_COARSE,
    FM1LAB_OP_FINE,
    FM1LAB_OP_DETUNE
} fm1lab_operator_field_t;

typedef enum {
    FM1LAB_GLOBAL_ALGORITHM,
    FM1LAB_GLOBAL_FEEDBACK,
    FM1LAB_GLOBAL_LFO_SPEED,
    FM1LAB_GLOBAL_LFO_DELAY,
    FM1LAB_GLOBAL_LFO_PITCH_DEPTH,
    FM1LAB_GLOBAL_LFO_AMP_DEPTH,
    FM1LAB_GLOBAL_LFO_WAVE,
    FM1LAB_GLOBAL_LFO_SYNC,
    FM1LAB_GLOBAL_OSC_SYNC,
    FM1LAB_GLOBAL_PITCH_SENS
} fm1lab_global_field_t;

/* Expanded M-VAVE DX7 voice accessors. Operator numbers are the panel labels
 * OP1..OP6; the underlying packed voice stores OP6 first and OP1 last. */
int fm1lab_voice_operator_get(const uint8_t voice[FM1LAB_PATCH_BYTES],
                              unsigned operator_number, fm1lab_operator_field_t field,
                              uint8_t *value);
int fm1lab_voice_operator_set(uint8_t voice[FM1LAB_PATCH_BYTES],
                              unsigned operator_number, fm1lab_operator_field_t field,
                              unsigned value);
int fm1lab_voice_global_get(const uint8_t voice[FM1LAB_PATCH_BYTES],
                            fm1lab_global_field_t field, uint8_t *value);
int fm1lab_voice_global_set(uint8_t voice[FM1LAB_PATCH_BYTES],
                            fm1lab_global_field_t field, unsigned value);
/* Patch-level ADSR: shape all carrier EGs while preserving modulator envelopes.
 * Values are 0..99 in DX7 rate/level units: attack, decay, sustain, release. */
int fm1lab_voice_adsr_get(const uint8_t voice[FM1LAB_PATCH_BYTES], uint8_t adsr[4]);
int fm1lab_voice_adsr_set(uint8_t voice[FM1LAB_PATCH_BYTES], const uint8_t adsr[4]);
/* True when at least one algorithm carrier can produce audible output. */
int fm1lab_voice_has_audible_carrier(const uint8_t voice[FM1LAB_PATCH_BYTES]);
int fm1lab_voice_operator_is_carrier(const uint8_t voice[FM1LAB_PATCH_BYTES],
                                     unsigned operator_number);
unsigned fm1lab_voice_operator_depth(const uint8_t voice[FM1LAB_PATCH_BYTES],
                                     unsigned operator_number);
int fm1lab_voice_operator_modulates(const uint8_t voice[FM1LAB_PATCH_BYTES],
                                    unsigned source_operator, unsigned target_operator);
int fm1lab_voice_name_get(const uint8_t voice[FM1LAB_PATCH_BYTES], char name[11]);
int fm1lab_voice_name_set(uint8_t voice[FM1LAB_PATCH_BYTES], const char *name);

#endif
