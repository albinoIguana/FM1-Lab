#include "fm1lab_dx7_preset.h"
#include "fm1lab_dx7_data.h"
#include "fm1lab_preset_control.h"

static int z85_digit(char c)
{
    static const char alphabet[] =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-:+=^!/*?&<>()[]{}@%$#";
    for (unsigned i = 0; i < 85u; ++i)
        if (alphabet[i] == c) return (int)i;
    return -1;
}

enum fm1lab_dx7_preset_result fm1lab_dx7_decode_z85(
    const char *source, size_t source_bytes, uint32_t voice_index,
    uint32_t voice_count, uint8_t out[FM1LAB_PATCH_BYTES])
{
    if (!source || !out || !voice_count)
        return FM1LAB_DX7_PRESET_BAD_ARGUMENT;
    if (voice_index >= voice_count) return FM1LAB_DX7_PRESET_INDEX_INVALID;
    if (source_bytes < (size_t)voice_count * FM1LAB_DX7_Z85_BYTES)
        return FM1LAB_DX7_PRESET_DATA_MISSING;

    const char *voice = source + (size_t)voice_index * FM1LAB_DX7_Z85_BYTES;
    for (uint32_t group = 0; group < 39u; ++group) {
        uint64_t value = 0u;
        for (uint32_t digit = 0; digit < 5u; ++digit) {
            int decoded = z85_digit(voice[group * 5u + digit]);
            if (decoded < 0) return FM1LAB_DX7_PRESET_DECODE_FAILED;
            value = value * 85u + (uint32_t)decoded;
        }
        if (value > 0xffffffffu) return FM1LAB_DX7_PRESET_DECODE_FAILED;
        out[group * 4u] = (uint8_t)(value >> 24);
        out[group * 4u + 1u] = (uint8_t)(value >> 16);
        out[group * 4u + 2u] = (uint8_t)(value >> 8);
        out[group * 4u + 3u] = (uint8_t)value;
    }
    return FM1LAB_DX7_PRESET_OK;
}

enum fm1lab_dx7_preset_result fm1lab_dx7_decode_voice(
    uint32_t voice_index, uint8_t out[FM1LAB_PATCH_BYTES])
{
    return fm1lab_dx7_decode_z85(fm1lab_dx7_voice_z85,
        sizeof fm1lab_dx7_voice_z85 - 1u, voice_index,
        FM1LAB_DX7_VOICE_COUNT, out);
}

int fm1lab_dx7_voice_valid(const uint8_t voice[FM1LAB_PATCH_BYTES])
{
    if (!voice) return 0;
    for (unsigned i = 0; i < FM1LAB_PATCH_BYTES - 1u; ++i)
        if (voice[i] > 127u) return 0;
    return voice[134] <= 31u && voice[135] <= 7u;
}

enum fm1lab_dx7_preset_result fm1lab_dx7_replace_voice(
    fm1lab_instrument_t *instrument, uint8_t patch,
    const uint8_t voice[FM1LAB_PATCH_BYTES])
{
    if (patch >= FM1LAB_PATCH_COUNT)
        return FM1LAB_DX7_PRESET_INDEX_INVALID;
    if (!fm1lab_dx7_voice_valid(voice))
        return FM1LAB_DX7_PRESET_VOICE_INVALID;
    if (fm1lab_patch_replace_voice(instrument, patch, voice) != 0)
        return FM1LAB_DX7_PRESET_PATCH_REPLACE_FAILED;
    return FM1LAB_DX7_PRESET_OK;
}

enum fm1lab_dx7_preset_result fm1lab_dx7_patch_preset_step(
    fm1lab_instrument_t *instrument, uint8_t patch, uint8_t cursor, int delta,
    uint8_t *next_cursor)
{
    uint8_t voice[FM1LAB_PATCH_BYTES];
    uint8_t next;
    enum fm1lab_dx7_preset_result result;

    if (!instrument || !next_cursor || !delta)
        return FM1LAB_DX7_PRESET_BAD_ARGUMENT;
    if (patch >= FM1LAB_PATCH_COUNT)
        return FM1LAB_DX7_PRESET_INDEX_INVALID;

    next = fm1lab_preset_cursor_step(cursor, delta, FM1LAB_DX7_VOICE_COUNT);
    result = fm1lab_dx7_decode_voice(next, voice);
    if (result != FM1LAB_DX7_PRESET_OK) return result;
    result = fm1lab_dx7_replace_voice(instrument, patch, voice);
    if (result != FM1LAB_DX7_PRESET_OK) return result;

    *next_cursor = next;
    return FM1LAB_DX7_PRESET_OK;
}
