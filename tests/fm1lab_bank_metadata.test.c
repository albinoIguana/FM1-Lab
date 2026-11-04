#include "fm1lab_bank_metadata.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_sector_layout(void)
{
    assert(FM1LAB_BANK_VOICE_AREA_BYTES == 3604u);
    assert(FM1LAB_BANK_META_OFFSET == 3604u);
    assert(FM1LAB_BANK_META_END == 4004u);
    assert(FM1LAB_BANK_META_END <= FM1LAB_BANK_SECTOR_BYTES);
    assert(FM1LAB_BANK_INSTRUMENTS * FM1LAB_BANK_PATCHES == FM1LAB_BANK_VOICE_COUNT);
}

static void test_round_trip(void)
{
    fm1lab_bank_metadata_t source, decoded;
    uint8_t bytes[FM1LAB_BANK_META_BYTES];
    fm1lab_bank_metadata_init(&source);
    assert(fm1lab_bank_metadata_valid(&source));

    memcpy(source.instruments[7].name, "NIGHT DRIVE             ", FM1LAB_BANK_NAME_BYTES);
    source.instruments[7].patches[3].macros[2] = 83;
    source.instruments[7].patches[3].enabled = 0;
    source.instruments[7].patches[3].level = 91;

    assert(fm1lab_bank_metadata_encode(&source, bytes) == 0);
    memset(&decoded, 0xa5, sizeof decoded);
    assert(fm1lab_bank_metadata_decode(&decoded, bytes) == 0);
    assert(memcmp(&source, &decoded, sizeof source) == 0);
}

static void test_corruption_and_invalid_values(void)
{
    fm1lab_bank_metadata_t metadata, decoded;
    uint8_t bytes[FM1LAB_BANK_META_BYTES];
    fm1lab_bank_metadata_init(&metadata);
    assert(fm1lab_bank_metadata_encode(&metadata, bytes) == 0);

    bytes[FM1LAB_BANK_META_HEADER_BYTES + 30] ^= 0x40;
    assert(fm1lab_bank_metadata_decode(&decoded, bytes) == -1);

    metadata.instruments[0].patches[0].macros[0] = 101;
    assert(fm1lab_bank_metadata_encode(&metadata, bytes) == -1);
}

int main(void)
{
    test_sector_layout();
    test_round_trip();
    test_corruption_and_invalid_values();
    puts("FM1-Lab bank metadata tests passed.");
    return 0;
}
