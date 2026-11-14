#ifndef FM1LAB_DX7_PRESET_H
#define FM1LAB_DX7_PRESET_H

#include <stddef.h>
#include <stdint.h>
#include "fm1lab_instrument.h"

enum fm1lab_dx7_preset_result {
    FM1LAB_DX7_PRESET_OK = 0,
    FM1LAB_DX7_PRESET_BAD_ARGUMENT,
    FM1LAB_DX7_PRESET_INDEX_INVALID,
    FM1LAB_DX7_PRESET_DATA_MISSING,
    FM1LAB_DX7_PRESET_DECODE_FAILED,
    FM1LAB_DX7_PRESET_VOICE_INVALID,
    FM1LAB_DX7_PRESET_PATCH_REPLACE_FAILED
};

/* Decode one 156-byte expanded FM1 voice from the bundled Z85 bank. The
 * data-taking form is shared with tests so malformed source can be injected. */
enum fm1lab_dx7_preset_result fm1lab_dx7_decode_z85(
    const char *source, size_t source_bytes, uint32_t voice_index,
    uint32_t voice_count, uint8_t out[FM1LAB_PATCH_BYTES]);
enum fm1lab_dx7_preset_result fm1lab_dx7_decode_voice(
    uint32_t voice_index, uint8_t out[FM1LAB_PATCH_BYTES]);
int fm1lab_dx7_voice_valid(const uint8_t voice[FM1LAB_PATCH_BYTES]);
enum fm1lab_dx7_preset_result fm1lab_dx7_replace_voice(
    fm1lab_instrument_t *instrument, uint8_t patch,
    const uint8_t voice[FM1LAB_PATCH_BYTES]);

/* Apply one PRESETS detent to a focused patch. Slot-owned performance values
 * are preserved by fm1lab_patch_replace_voice(). next_cursor changes on success only. */
enum fm1lab_dx7_preset_result fm1lab_dx7_patch_preset_step(
    fm1lab_instrument_t *instrument, uint8_t patch, uint8_t cursor, int delta,
    uint8_t *next_cursor);

#endif
