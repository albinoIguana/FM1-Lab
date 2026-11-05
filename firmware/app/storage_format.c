/* SPDX-License-Identifier: GPL-3.0-only
 * Versioned, CRC-protected Instrument chunk serialization.
 */
#include <stdint.h>
#include "storage_format.h"

#define STORE_MAGIC 0x31494D46u
#define STORE_VERSION 1u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t first;
    uint8_t count;
    uint32_t crc;
} store_header_t;

typedef char store_header_size_check[(sizeof(store_header_t) == FM1LAB_STORE_CHUNK_HEADER_BYTES) ? 1 : -1];

static void copy_bytes(void *dst, const void *src, uint32_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--)
        *d++ = *s++;
}

static uint32_t crc32(const uint8_t *data, uint32_t n)
{
    uint32_t crc = 0xFFFFFFFFu, i, bit;
    for (i = 0; i < n; i++) {
        crc ^= data[i];
        for (bit = 0; bit < 8u; bit++)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return ~crc;
}

int fm1lab_store_chunk_encode(uint8_t first, uint8_t count,
                              const fm1lab_instrument_record_t *records,
                              uint8_t *output, uint32_t capacity, uint32_t *written)
{
    store_header_t h;
    uint32_t payload, length;
    if (!records || !output || !written || !count || count > FM1LAB_STORE_CHUNK_MAX_RECORDS
        || (uint32_t)first + count > FM1LAB_INSTRUMENT_COUNT)
        return 0;
    payload = (uint32_t)count * sizeof(fm1lab_instrument_record_t);
    length = FM1LAB_STORE_CHUNK_HEADER_BYTES + payload;
    if (capacity < length)
        return 0;
    copy_bytes(output + FM1LAB_STORE_CHUNK_HEADER_BYTES, records, payload);
    h.magic = STORE_MAGIC;
    h.version = STORE_VERSION;
    h.first = first;
    h.count = count;
    h.crc = crc32(output + FM1LAB_STORE_CHUNK_HEADER_BYTES, payload);
    copy_bytes(output, &h, sizeof(h));
    *written = length;
    return 1;
}

int fm1lab_store_chunk_decode(uint8_t expected_first, uint8_t expected_count,
                              const uint8_t *input, uint32_t length,
                              fm1lab_instrument_record_t *records)
{
    store_header_t h;
    uint32_t payload;
    if (!input || !records || !expected_count || expected_count > FM1LAB_STORE_CHUNK_MAX_RECORDS
        || (uint32_t)expected_first + expected_count > FM1LAB_INSTRUMENT_COUNT
        || length < FM1LAB_STORE_CHUNK_HEADER_BYTES)
        return 0;
    copy_bytes(&h, input, sizeof(h));
    payload = (uint32_t)expected_count * sizeof(fm1lab_instrument_record_t);
    if (h.magic != STORE_MAGIC || h.version != STORE_VERSION
        || h.first != expected_first || h.count != expected_count
        || length != FM1LAB_STORE_CHUNK_HEADER_BYTES + payload
        || h.crc != crc32(input + FM1LAB_STORE_CHUNK_HEADER_BYTES, payload))
        return 0;
    copy_bytes(records, input + FM1LAB_STORE_CHUNK_HEADER_BYTES, payload);
    return 1;
}
