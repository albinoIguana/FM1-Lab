#include "fm1lab_instrument.h"

#include <stddef.h>
#include <string.h>

enum {
    OPERATOR_COUNT = 6,
    OPERATOR_STRIDE = 21,
    GLOBAL_FEEDBACK = 135,
    GLOBAL_LFO_SPEED = 137,
    GLOBAL_LFO_AMP_DEPTH = 140,
    VOICE_ALGORITHM = 134
};

/* Carrier masks are derived from the official M-VAVE dx7core.c algorithm
 * routing table. Bit zero denotes OP1; bit five denotes OP6. */
static const uint8_t carrier_masks[32] = {
    5, 5, 9, 9, 21, 21, 5, 5,
    5, 9, 9, 5, 5, 5, 5, 1,
    1, 1, 25, 11, 27, 29, 27, 31,
    31, 11, 11, 37, 23, 39, 31, 63
};

static uint8_t clamp_u8(int value, unsigned maximum)
{
    if (value < 0) return 0;
    if ((unsigned)value > maximum) return (uint8_t)maximum;
    return (uint8_t)value;
}

static int scaled_delta(int delta, unsigned percent)
{
    int value = delta * (int)percent;
    return value >= 0 ? (value + 50) / 100 : (value - 50) / 100;
}

static void copy_name(char *destination, unsigned capacity, const char *source)
{
    unsigned i = 0;
    if (source) {
        while (i + 1 < capacity && source[i]) {
            unsigned char c = (unsigned char)source[i];
            destination[i] = c >= 32 && c <= 126 ? (char)c : ' ';
            ++i;
        }
    }
    while (i < capacity) destination[i++] = ' ';
}

void fm1lab_instrument_init(fm1lab_instrument_t *instrument, const char *name)
{
    if (!instrument) return;
    memset(instrument, 0, sizeof *instrument);
    instrument->magic = FM1LAB_INSTRUMENT_MAGIC;
    instrument->version = FM1LAB_INSTRUMENT_VERSION;
    copy_name(instrument->name, sizeof instrument->name, name);
    for (unsigned p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        for (unsigned m = 0; m < FM1LAB_MACRO_COUNT; ++m)
            instrument->patches[p].macros[m] = 50;
        instrument->patches[p].enabled = 1;
        instrument->patches[p].level = 127;
    }
}

int fm1lab_patch_set_voice(fm1lab_instrument_t *instrument, unsigned patch,
                           const uint8_t voice[FM1LAB_PATCH_BYTES])
{
    if (!instrument || !voice || patch >= FM1LAB_PATCH_COUNT ||
        instrument->magic != FM1LAB_INSTRUMENT_MAGIC ||
        instrument->version != FM1LAB_INSTRUMENT_VERSION)
        return -1;
    for (unsigned i = 0; i < FM1LAB_PATCH_BYTES; ++i)
        if (voice[i] > 127) return -1;
    if (voice[VOICE_ALGORITHM] > 31 || voice[GLOBAL_FEEDBACK] > 7) return -1;
    memcpy(instrument->patches[patch].voice, voice, FM1LAB_PATCH_BYTES);
    for (unsigned m = 0; m < FM1LAB_MACRO_COUNT; ++m)
        instrument->patches[patch].macros[m] = 50;
    instrument->patches[patch].enabled = 1;
    instrument->patches[patch].level = 127;
    return 0;
}

int fm1lab_instrument_valid(const fm1lab_instrument_t *instrument)
{
    if (!instrument || instrument->magic != FM1LAB_INSTRUMENT_MAGIC ||
        instrument->version != FM1LAB_INSTRUMENT_VERSION)
        return 0;
    for (unsigned i = 0; i < FM1LAB_INSTRUMENT_NAME_BYTES; ++i) {
        unsigned char c = (unsigned char)instrument->name[i];
        if (c && (c < 32 || c > 126)) return 0;
    }
    for (unsigned p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        const fm1lab_patch_t *patch = &instrument->patches[p];
        if (patch->enabled > 1 || patch->level > 127 ||
            patch->voice[VOICE_ALGORITHM] > 31 ||
            patch->voice[GLOBAL_FEEDBACK] > 7)
            return 0;
        for (unsigned i = 0; i < FM1LAB_PATCH_BYTES; ++i)
            if (patch->voice[i] > 127) return 0;
        for (unsigned m = 0; m < FM1LAB_MACRO_COUNT; ++m)
            if (patch->macros[m] > 100) return 0;
    }
    return 1;
}

uint8_t fm1lab_carrier_mask(unsigned algorithm)
{
    return carrier_masks[algorithm & 31u];
}

int fm1lab_render_patch(const fm1lab_instrument_t *instrument, unsigned patch_index,
                        uint8_t output[FM1LAB_PATCH_BYTES])
{
    if (!fm1lab_instrument_valid(instrument) || !output ||
        patch_index >= FM1LAB_PATCH_COUNT)
        return -1;

    const fm1lab_patch_t *patch = &instrument->patches[patch_index];
    const uint8_t *base = patch->voice;
    memcpy(output, base, FM1LAB_PATCH_BYTES);

    const int brightness = (int)patch->macros[FM1LAB_BRIGHTNESS] - 50;
    const int body = (int)patch->macros[FM1LAB_BODY] - 50;
    const int motion = (int)patch->macros[FM1LAB_MOTION] - 50;
    const int edge = (int)patch->macros[FM1LAB_EDGE] - 50;
    const uint8_t carriers = fm1lab_carrier_mask(base[VOICE_ALGORITHM]);

    for (unsigned op = 1; op <= OPERATOR_COUNT; ++op) {
        const unsigned offset = (OPERATOR_COUNT - op) * OPERATOR_STRIDE;
        const unsigned carrier = !!(carriers & (1u << (op - 1u)));
        if (carrier) {
            output[offset + 16] = clamp_u8((int)base[offset + 16] +
                scaled_delta(brightness, 55), 99);
            output[offset + 6] = clamp_u8((int)base[offset + 6] +
                scaled_delta(body, 45), 99);
        } else {
            output[offset + 16] = clamp_u8((int)base[offset + 16] +
                scaled_delta(edge, 45), 99);
        }
    }

    output[GLOBAL_LFO_SPEED] = clamp_u8((int)base[GLOBAL_LFO_SPEED] +
        scaled_delta(motion, 55), 99);
    output[GLOBAL_LFO_AMP_DEPTH] = clamp_u8((int)base[GLOBAL_LFO_AMP_DEPTH] +
        scaled_delta(motion, 45), 99);
    output[GLOBAL_FEEDBACK] = clamp_u8((int)base[GLOBAL_FEEDBACK] +
        scaled_delta(edge, 8), 7);
    return 0;
}
