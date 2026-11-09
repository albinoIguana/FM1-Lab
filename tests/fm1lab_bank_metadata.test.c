#include "fm1lab_bank_metadata.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t test_crc32(const uint8_t *data,unsigned length)
{
    uint32_t crc=0xffffffffu;
    for(unsigned i=0;i<length;i++){
        crc^=data[i];
        for(unsigned bit=0;bit<8;bit++)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));
    }
    return ~crc;
}

static void test_legacy_v1_decode(void)
{
    fm1lab_bank_metadata_t decoded;
    uint8_t bytes[FM1LAB_BANK_META_BYTES]={0};
    bytes[0]=0x46u;bytes[1]=0x4cu;bytes[2]=0x4du;bytes[3]=0x32u;
    bytes[4]=1u;bytes[6]=0x80u;bytes[7]=0x01u;
    uint8_t *record=bytes+FM1LAB_BANK_META_HEADER_BYTES;
    memcpy(record,"OLD BANK",8u);
    uint8_t *patch=record+FM1LAB_BANK_NAME_BYTES;
    patch[0]=64u;patch[1]=50u;patch[2]=42u;patch[3]=55u;patch[4]=1u;patch[5]=87u;
    uint32_t crc=test_crc32(record,384u);
    bytes[8]=(uint8_t)crc;bytes[9]=(uint8_t)(crc>>8);bytes[10]=(uint8_t)(crc>>16);bytes[11]=(uint8_t)(crc>>24);
    assert(fm1lab_bank_metadata_decode(&decoded,bytes)==0);
    assert(decoded.instruments[0].patches[0].macros[0]==64u);
    assert(decoded.instruments[0].patches[0].level==87u);
    assert(decoded.instruments[0].patches[0].octave==0);
    assert(decoded.instruments[0].patches[0].fine_cents==0);
}

static void test_sector_layout(void)
{
    assert(FM1LAB_BANK_VOICE_AREA_BYTES == 3604u);
    assert(FM1LAB_BANK_META_OFFSET == 3604u);
    assert(FM1LAB_BANK_META_END == 4068u);
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
    source.instruments[7].patches[3].octave = -2;
    source.instruments[7].patches[3].fine_cents = 37;

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
    metadata.instruments[0].patches[0].macros[0] = 50;
    metadata.instruments[0].patches[0].octave = -5;
    assert(fm1lab_bank_metadata_encode(&metadata, bytes) == -1);
}

int main(void)
{
    test_sector_layout();
    test_legacy_v1_decode();
    test_round_trip();
    test_corruption_and_invalid_values();
    puts("FM1-Lab bank metadata tests passed.");
    return 0;
}
