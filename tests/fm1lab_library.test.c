#include "fm1lab_library.h"
#include "fm1lab_bank_metadata.h"
#include "fm_patch.h"
#include "instrument.h"

#include <assert.h>
#include <string.h>

static uint8_t bank_raw[4096];
static uint8_t bank_meta[FM1LAB_BANK_META_BYTES];
static unsigned write_count;

int groove_fm_library_read(unsigned bank, uint8_t raw[4096])
{
    assert(bank < 4);
    memcpy(raw, bank_raw, sizeof bank_raw);
    return 0;
}
int groove_fm_library_metadata_read(unsigned bank, uint8_t metadata[FM1LAB_BANK_META_BYTES])
{
    assert(bank < 4);
    memcpy(metadata, bank_meta, sizeof bank_meta);
    return 0;
}
int groove_fm_library_instrument_write(unsigned bank, const uint8_t raw[4096],
                                       const uint8_t metadata[FM1LAB_BANK_META_BYTES])
{
    assert(bank < 4);
    memcpy(bank_raw, raw, sizeof bank_raw);
    memcpy(bank_meta, metadata, sizeof bank_meta);
    ++write_count;
    return 0;
}
void groove_fm_unpack(uint8_t patch[156], const uint8_t vmem[128])
{
    memset(patch, 0, 156);
    memcpy(patch, vmem, 128);
    patch[134] = vmem[110] & 31u;
}
void groove_fm_pack(uint8_t vmem[128], const uint8_t patch[156])
{
    memcpy(vmem, patch, 128);
    vmem[110] = patch[134];
}

static void test_instrument_bank_mapping(void)
{
    fm1lab_bank_metadata_t metadata;
    fm1lab_bank_metadata_init(&metadata);
    assert(fm1lab_bank_metadata_encode(&metadata, bank_meta) == 0);

    /* Instrument 10 = bank 1, slot 2, voices 8..11. */
    for (unsigned p = 0; p < 4; ++p) {
        memset(bank_raw + (8u + p) * 128u, (int)(10u + p), 128);
        bank_raw[(8u + p) * 128u + 110] = 3;
    }
    fm1lab_instrument_t source, loaded;
    fm1lab_instrument_init(&source, "QUAD LAYER");
    for (unsigned p = 0; p < 4; ++p) {
        memset(source.patches[p].voice, (int)(40u + p), FM1LAB_PATCH_BYTES);
        source.patches[p].voice[134] = 3;
        source.patches[p].voice[135] = 3;
        source.patches[p].macros[FM1LAB_BRIGHTNESS] = (uint8_t)(60u + p);
        source.patches[p].level = (uint8_t)(90u + p);
    }

    assert(fm1lab_library_save(10, &source) == 0);
    assert(write_count == 1);
    assert(bank_raw[8u * 128u] == 40);
    assert(bank_raw[11u * 128u] == 43);
    assert(bank_raw[7u * 128u] == 0);  /* neighboring Instrument preserved */
    assert(fm1lab_library_load(10, &loaded) == 0);
    assert(loaded.patches[0].voice[0] == 40);
    assert(loaded.patches[3].voice[0] == 43);
    assert(loaded.patches[2].macros[FM1LAB_BRIGHTNESS] == 62);
    assert(loaded.patches[3].level == 93);
    assert(memcmp(loaded.name, "QUAD LAYER", 10) == 0);
}

int main(void)
{
    test_instrument_bank_mapping();
    return 0;
}
