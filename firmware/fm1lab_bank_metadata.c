#include "fm1lab_bank_metadata.h"

#include <string.h>

enum {
    META_MAGIC = 0x324d4c46u, /* "FLM2" little-endian */
    META_VERSION = 2u
};

static void put_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((unsigned)p[0] | ((unsigned)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t crc32(const uint8_t *data, unsigned length)
{
    uint32_t crc = 0xffffffffu;
    for (unsigned i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

void fm1lab_bank_metadata_init(fm1lab_bank_metadata_t *metadata)
{
    if (!metadata) return;
    memset(metadata, 0, sizeof *metadata);
    for (unsigned i = 0; i < FM1LAB_BANK_INSTRUMENTS; ++i) {
        fm1lab_bank_instrument_meta_t *instrument = &metadata->instruments[i];
        static const char prefix[] = "INSTRUMENT ";
        memcpy(instrument->name, prefix, sizeof prefix - 1);
        instrument->name[sizeof prefix - 1] = (char)('1' + i / 10);
        instrument->name[sizeof prefix] = (char)('1' + i % 10);
        for (unsigned p = 0; p < FM1LAB_BANK_PATCHES; ++p) {
            for (unsigned m = 0; m < FM1LAB_BANK_MACROS; ++m)
                instrument->patches[p].macros[m] = 50;
            instrument->patches[p].enabled = 1;
            instrument->patches[p].level = 127;
        }
    }
}

int fm1lab_bank_metadata_valid(const fm1lab_bank_metadata_t *metadata)
{
    if (!metadata) return 0;
    for (unsigned i = 0; i < FM1LAB_BANK_INSTRUMENTS; ++i) {
        const fm1lab_bank_instrument_meta_t *instrument = &metadata->instruments[i];
        for (unsigned n = 0; n < FM1LAB_BANK_NAME_BYTES; ++n) {
            unsigned char c = (unsigned char)instrument->name[n];
            if (c && (c < 32 || c > 126)) return 0;
        }
        for (unsigned p = 0; p < FM1LAB_BANK_PATCHES; ++p) {
            const fm1lab_bank_patch_meta_t *patch = &instrument->patches[p];
            if (patch->enabled > 1 || patch->level > 127 ||
                patch->octave < -4 || patch->octave > 4 ||
                patch->fine_cents < -100 || patch->fine_cents > 100) return 0;
            for (unsigned m = 0; m < FM1LAB_BANK_MACROS; ++m)
                if (patch->macros[m] > 100) return 0;
        }
    }
    return 1;
}

int fm1lab_bank_metadata_encode(const fm1lab_bank_metadata_t *metadata,
                                uint8_t output[FM1LAB_BANK_META_BYTES])
{
    if (!output || !fm1lab_bank_metadata_valid(metadata)) return -1;
    memset(output, 0, FM1LAB_BANK_META_BYTES);
    put_u32(output, META_MAGIC);
    put_u16(output + 4, META_VERSION);
    put_u16(output + 6, FM1LAB_BANK_META_PAYLOAD_BYTES);

    uint8_t *payload = output + FM1LAB_BANK_META_HEADER_BYTES;
    for (unsigned i = 0; i < FM1LAB_BANK_INSTRUMENTS; ++i) {
        const fm1lab_bank_instrument_meta_t *instrument = &metadata->instruments[i];
        uint8_t *record = payload + i * FM1LAB_BANK_META_RECORD_BYTES;
        memcpy(record, instrument->name, FM1LAB_BANK_NAME_BYTES);
        for (unsigned p = 0; p < FM1LAB_BANK_PATCHES; ++p) {
            uint8_t *patch = record + FM1LAB_BANK_NAME_BYTES + p * 8u;
            memcpy(patch, instrument->patches[p].macros, FM1LAB_BANK_MACROS);
            patch[4] = instrument->patches[p].enabled;
            patch[5] = instrument->patches[p].level;
            patch[6] = (uint8_t)instrument->patches[p].octave;
            patch[7] = (uint8_t)instrument->patches[p].fine_cents;
        }
    }
    put_u32(output + 8, crc32(payload, FM1LAB_BANK_META_PAYLOAD_BYTES));
    put_u32(output + 12, 0u);
    return 0;
}

int fm1lab_bank_metadata_decode(fm1lab_bank_metadata_t *metadata,
                                const uint8_t input[FM1LAB_BANK_META_BYTES])
{
    if (!metadata || !input || get_u32(input) != META_MAGIC ||
        get_u32(input + 12) != 0u) return -1;
    uint16_t version = get_u16(input + 4);
    uint16_t payload_bytes = get_u16(input + 6);
    unsigned record_bytes, patch_bytes;
    if (version == 1u && payload_bytes == 384u) {
        record_bytes = 48u; patch_bytes = 6u;
    } else if (version == META_VERSION && payload_bytes == FM1LAB_BANK_META_PAYLOAD_BYTES) {
        record_bytes = FM1LAB_BANK_META_RECORD_BYTES; patch_bytes = 8u;
    } else return -1;
    if (get_u32(input + 8) != crc32(input + FM1LAB_BANK_META_HEADER_BYTES,payload_bytes))
        return -1;

    fm1lab_bank_metadata_t decoded;
    memset(&decoded, 0, sizeof decoded);
    const uint8_t *payload = input + FM1LAB_BANK_META_HEADER_BYTES;
    for (unsigned i = 0; i < FM1LAB_BANK_INSTRUMENTS; ++i) {
        fm1lab_bank_instrument_meta_t *instrument = &decoded.instruments[i];
        const uint8_t *record = payload + i * record_bytes;
        memcpy(instrument->name, record, FM1LAB_BANK_NAME_BYTES);
        for (unsigned p = 0; p < FM1LAB_BANK_PATCHES; ++p) {
            fm1lab_bank_patch_meta_t *patch = &instrument->patches[p];
            const uint8_t *source = record + FM1LAB_BANK_NAME_BYTES + p * patch_bytes;
            memcpy(patch->macros, source, FM1LAB_BANK_MACROS);
            patch->enabled = source[4];
            patch->level = source[5];
            if (patch_bytes == 8u) {
                patch->octave = (int8_t)source[6];
                patch->fine_cents = (int8_t)source[7];
            }
        }
    }
    if (!fm1lab_bank_metadata_valid(&decoded)) return -1;
    *metadata = decoded;
    return 0;
}

_Static_assert(FM1LAB_BANK_META_END <= FM1LAB_BANK_SECTOR_BYTES,
               "Instrument metadata must fit in existing bank sector");
_Static_assert(FM1LAB_BANK_META_PAYLOAD_BYTES ==
               FM1LAB_BANK_INSTRUMENTS * FM1LAB_BANK_META_RECORD_BYTES,
               "Metadata payload layout mismatch");
