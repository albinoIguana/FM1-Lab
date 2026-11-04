#ifndef FM1LAB_INSTRUMENT_H
#define FM1LAB_INSTRUMENT_H

#include <stdint.h>

#define FM1LAB_INSTRUMENT_MAGIC 0x31494c46u
#define FM1LAB_INSTRUMENT_VERSION 1u
#define FM1LAB_PATCH_COUNT 4u
#define FM1LAB_PATCH_BYTES 156u
#define FM1LAB_PATCH_NAME_BYTES 10u
#define FM1LAB_INSTRUMENT_NAME_BYTES 24u
#define FM1LAB_MACRO_COUNT 4u

enum fm1lab_macro {
    FM1LAB_BRIGHTNESS,
    FM1LAB_BODY,
    FM1LAB_MOTION,
    FM1LAB_DIRT,
    FM1LAB_EDGE = FM1LAB_DIRT /* compatibility: serialized macro slot 3 */
};

typedef struct {
    uint8_t voice[FM1LAB_PATCH_BYTES];
    uint8_t macros[FM1LAB_MACRO_COUNT]; /* 0..100; 50 is neutral */
    uint8_t enabled;                    /* 0 = muted, 1 = active */
    uint8_t level;                      /* 0..127; mixer gain, not DX7 voice data */
} fm1lab_patch_t;

typedef struct {
    uint32_t magic;
    uint8_t version;
    char name[FM1LAB_INSTRUMENT_NAME_BYTES];
    fm1lab_patch_t patches[FM1LAB_PATCH_COUNT];
} fm1lab_instrument_t;

/* Initialize a valid, neutral four-patch Instrument. */
void fm1lab_instrument_init(fm1lab_instrument_t *instrument, const char *name);

/* Copy one expanded M-VAVE DX7 voice into a patch and reset its performance state. */
int fm1lab_patch_set_voice(fm1lab_instrument_t *instrument, unsigned patch,
                           const uint8_t voice[FM1LAB_PATCH_BYTES]);

/* Validate the complete serialized value before loading or saving it. */
int fm1lab_instrument_valid(const fm1lab_instrument_t *instrument);

/* Carrier mask bits are OP1..OP6 in bits 0..5, from M-VAVE's DX7 algorithm routing. */
uint8_t fm1lab_carrier_mask(unsigned algorithm);

/* Modulation stages from this operator to a carrier; operator is OP1..OP6. */
uint8_t fm1lab_operator_depth(unsigned algorithm, unsigned operator_number);

/* Returns 1 when source OP modulates target OP, 0 when not, -1 for invalid OPs. */
int fm1lab_operator_modulates(unsigned algorithm, unsigned source_operator,
                              unsigned target_operator);

/* Render a non-destructive macro-adjusted voice. Returns 0 or -1 for invalid input. */
int fm1lab_render_patch(const fm1lab_instrument_t *instrument, unsigned patch,
                        uint8_t output[FM1LAB_PATCH_BYTES]);

#endif
