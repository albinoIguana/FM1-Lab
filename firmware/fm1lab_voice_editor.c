#include "fm1lab_voice_editor.h"

#include <ctype.h>
#include <string.h>

enum { OPERATOR_STRIDE = 21, GLOBAL_ALGORITHM = 134, GLOBAL_FEEDBACK = 135,
       GLOBAL_LFO_SPEED = 137, GLOBAL_LFO_DELAY = 138,
       GLOBAL_LFO_PITCH_DEPTH = 139, GLOBAL_LFO_AMP_DEPTH = 140,
       VOICE_NAME_OFFSET = 145, VOICE_NAME_BYTES = 10 };

static int operator_field_offset(fm1lab_operator_field_t field, unsigned *offset,
                                 unsigned *maximum)
{
    if (!offset || !maximum) return -1;
    if (field >= FM1LAB_OP_RATE_1 && field <= FM1LAB_OP_RATE_4) {
        *offset = (unsigned)field;
        *maximum = 99;
    } else if (field >= FM1LAB_OP_LEVEL_1 && field <= FM1LAB_OP_LEVEL_4) {
        *offset = 4U + (unsigned)(field - FM1LAB_OP_LEVEL_1);
        *maximum = 99;
    } else {
        switch (field) {
        case FM1LAB_OP_OUTPUT:  *offset = 16; *maximum = 99; break;
        case FM1LAB_OP_MODE:    *offset = 17; *maximum = 1; break;
        case FM1LAB_OP_COARSE:  *offset = 18; *maximum = 31; break;
        case FM1LAB_OP_FINE:    *offset = 19; *maximum = 99; break;
        case FM1LAB_OP_DETUNE:  *offset = 20; *maximum = 14; break;
        default: return -1;
        }
    }
    return 0;
}

static int global_field_offset(fm1lab_global_field_t field, unsigned *offset,
                               unsigned *maximum)
{
    if (!offset || !maximum) return -1;
    switch (field) {
    case FM1LAB_GLOBAL_ALGORITHM:       *offset = GLOBAL_ALGORITHM; *maximum = 32; break;
    case FM1LAB_GLOBAL_FEEDBACK:        *offset = GLOBAL_FEEDBACK; *maximum = 7; break;
    case FM1LAB_GLOBAL_LFO_SPEED:       *offset = GLOBAL_LFO_SPEED; *maximum = 99; break;
    case FM1LAB_GLOBAL_LFO_DELAY:       *offset = GLOBAL_LFO_DELAY; *maximum = 99; break;
    case FM1LAB_GLOBAL_LFO_PITCH_DEPTH: *offset = GLOBAL_LFO_PITCH_DEPTH; *maximum = 99; break;
    case FM1LAB_GLOBAL_LFO_AMP_DEPTH:   *offset = GLOBAL_LFO_AMP_DEPTH; *maximum = 99; break;
    default: return -1;
    }
    return 0;
}

int fm1lab_voice_operator_get(const uint8_t voice[FM1LAB_PATCH_BYTES],
                              unsigned operator_number, fm1lab_operator_field_t field,
                              uint8_t *value)
{
    unsigned offset, maximum;
    if (!voice || !value || operator_number < 1 || operator_number > 6 ||
        operator_field_offset(field, &offset, &maximum)) return -1;
    (void)maximum;
    *value = voice[(6U - operator_number) * OPERATOR_STRIDE + offset];
    return 0;
}

int fm1lab_voice_operator_set(uint8_t voice[FM1LAB_PATCH_BYTES],
                              unsigned operator_number, fm1lab_operator_field_t field,
                              unsigned value)
{
    unsigned offset, maximum;
    if (!voice || operator_number < 1 || operator_number > 6 ||
        operator_field_offset(field, &offset, &maximum) || value > maximum) return -1;
    voice[(6U - operator_number) * OPERATOR_STRIDE + offset] = (uint8_t)value;
    return 0;
}

int fm1lab_voice_global_get(const uint8_t voice[FM1LAB_PATCH_BYTES],
                            fm1lab_global_field_t field, uint8_t *value)
{
    unsigned offset, maximum;
    if (!voice || !value || global_field_offset(field, &offset, &maximum)) return -1;
    *value = voice[offset];
    if (field == FM1LAB_GLOBAL_ALGORITHM) *value = (uint8_t)(*value + 1U);
    (void)maximum;
    return 0;
}

int fm1lab_voice_global_set(uint8_t voice[FM1LAB_PATCH_BYTES],
                            fm1lab_global_field_t field, unsigned value)
{
    unsigned offset, maximum;
    if (!voice || global_field_offset(field, &offset, &maximum) || value > maximum ||
        (field == FM1LAB_GLOBAL_ALGORITHM && value == 0)) return -1;
    voice[offset] = (uint8_t)(field == FM1LAB_GLOBAL_ALGORITHM ? value - 1U : value);
    return 0;
}

int fm1lab_voice_operator_is_carrier(const uint8_t voice[FM1LAB_PATCH_BYTES],
                                     unsigned operator_number)
{
    if (!voice || operator_number < 1 || operator_number > 6) return -1;
    uint8_t mask = fm1lab_carrier_mask(voice[GLOBAL_ALGORITHM] & 31U);
    return (mask & (uint8_t)(1U << (operator_number - 1U))) != 0;
}

unsigned fm1lab_voice_operator_depth(const uint8_t voice[FM1LAB_PATCH_BYTES],
                                     unsigned operator_number)
{
    if (!voice || operator_number < 1 || operator_number > 6) return 0;
    return fm1lab_operator_depth(voice[GLOBAL_ALGORITHM] & 31U, operator_number);
}

int fm1lab_voice_operator_modulates(const uint8_t voice[FM1LAB_PATCH_BYTES],
                                    unsigned source_operator, unsigned target_operator)
{
    if (!voice) return -1;
    return fm1lab_operator_modulates(voice[GLOBAL_ALGORITHM] & 31U,
                                     source_operator, target_operator);
}

int fm1lab_voice_name_get(const uint8_t voice[FM1LAB_PATCH_BYTES], char name[11])
{
    if (!voice || !name) return -1;
    memcpy(name, voice + VOICE_NAME_OFFSET, VOICE_NAME_BYTES);
    name[VOICE_NAME_BYTES] = '\0';
    for (int i = VOICE_NAME_BYTES - 1; i >= 0 && name[i] == ' '; --i) name[i] = '\0';
    return 0;
}

int fm1lab_voice_name_set(uint8_t voice[FM1LAB_PATCH_BYTES], const char *name)
{
    if (!voice || !name) return -1;
    for (unsigned i = 0; i < VOICE_NAME_BYTES && name[i]; ++i) {
        unsigned char ch = (unsigned char)name[i];
        if (ch < 32 || ch > 126) return -1;
    }
    memset(voice + VOICE_NAME_OFFSET, ' ', VOICE_NAME_BYTES);
    for (unsigned i = 0; i < VOICE_NAME_BYTES && name[i]; ++i)
        voice[VOICE_NAME_OFFSET + i] = (uint8_t)toupper((unsigned char)name[i]);
    return 0;
}
