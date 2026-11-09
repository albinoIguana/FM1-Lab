#include "fm1lab_library.h"
#include "fm1lab_bank_metadata.h"
#include "fm_patch.h"
#include "instrument.h"

#include <string.h>

/* Added to the upstream library by patch-m-vave-instrument-storage.mjs. */
extern int groove_fm_library_metadata_read(unsigned bank, uint8_t metadata[FM1LAB_BANK_META_BYTES]);
extern int groove_fm_library_instrument_write(unsigned bank, const uint8_t raw[4096],
                                               const uint8_t metadata[FM1LAB_BANK_META_BYTES]);

static unsigned bank_for(unsigned index) { return index / FM1LAB_INSTRUMENTS_PER_BANK; }
static unsigned slot_for(unsigned index) { return index % FM1LAB_INSTRUMENTS_PER_BANK; }

static void copy_name(char *dst, const char *src, unsigned bytes)
{
    memcpy(dst, src, bytes);
    dst[bytes - 1] = 0;
}

int fm1lab_library_load(unsigned index, fm1lab_instrument_t *out)
{
    if (!out || index >= FM1LAB_INSTRUMENT_COUNT) return GROOVE_PROJECT_INVALID;

    uint8_t raw[4096];
    uint8_t encoded[FM1LAB_BANK_META_BYTES];
    fm1lab_bank_metadata_t metadata;
    unsigned bank = bank_for(index), slot = slot_for(index);

    int result = groove_fm_library_read(bank, raw);
    if (result) return result;
    result = groove_fm_library_metadata_read(bank, encoded);
    if (result) return result;
    if (fm1lab_bank_metadata_decode(&metadata, encoded)) return GROOVE_PROJECT_INVALID;

    fm1lab_instrument_init(out, NULL);
    copy_name(out->name, metadata.instruments[slot].name, FM1LAB_INSTRUMENT_NAME_BYTES);
    for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch) {
        unsigned voice_index = slot * FM1LAB_PATCH_COUNT + patch;
        uint8_t expanded[FM1LAB_PATCH_BYTES];
        groove_fm_unpack(expanded, raw + voice_index * 128u);
        if (fm1lab_patch_set_voice(out, patch, expanded)) return GROOVE_PROJECT_INVALID;
        memcpy(out->patches[patch].macros,
               metadata.instruments[slot].patches[patch].macros, FM1LAB_MACRO_COUNT);
        out->patches[patch].enabled = metadata.instruments[slot].patches[patch].enabled;
        out->patches[patch].level = metadata.instruments[slot].patches[patch].level;
        out->patches[patch].octave = metadata.instruments[slot].patches[patch].octave;
        out->patches[patch].fine_cents = metadata.instruments[slot].patches[patch].fine_cents;
    }
    return fm1lab_instrument_valid(out) ? 0 : GROOVE_PROJECT_INVALID;
}

int fm1lab_library_save(unsigned index, const fm1lab_instrument_t *instrument)
{
    if (index >= FM1LAB_INSTRUMENT_COUNT || !fm1lab_instrument_valid(instrument))
        return GROOVE_PROJECT_INVALID;

    uint8_t raw[4096];
    uint8_t encoded[FM1LAB_BANK_META_BYTES];
    fm1lab_bank_metadata_t metadata;
    unsigned bank = bank_for(index), slot = slot_for(index);

    int result = groove_fm_library_read(bank, raw);
    if (result) return result;
    result = groove_fm_library_metadata_read(bank, encoded);
    if (result) return result;
    if (fm1lab_bank_metadata_decode(&metadata, encoded)) return GROOVE_PROJECT_INVALID;

    memcpy(metadata.instruments[slot].name, instrument->name, FM1LAB_BANK_NAME_BYTES);
    for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch) {
        const fm1lab_patch_t *source = &instrument->patches[patch];
        unsigned voice_index = slot * FM1LAB_PATCH_COUNT + patch;
        groove_fm_pack(raw + voice_index * 128u, source->voice);
        memcpy(metadata.instruments[slot].patches[patch].macros,
               source->macros, FM1LAB_MACRO_COUNT);
        metadata.instruments[slot].patches[patch].enabled = source->enabled;
        metadata.instruments[slot].patches[patch].level = source->level;
        metadata.instruments[slot].patches[patch].octave = source->octave;
        metadata.instruments[slot].patches[patch].fine_cents = source->fine_cents;
    }

    if (fm1lab_bank_metadata_encode(&metadata, encoded)) return GROOVE_PROJECT_INVALID;
    return groove_fm_library_instrument_write(bank, raw, encoded);
}
