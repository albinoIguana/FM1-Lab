/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#include "dsp/fm6.h"

#define FM1LAB_STORE_CHUNK_HEADER_BYTES 12u
#define FM1LAB_STORE_CHUNK_MAX_RECORDS 6u

int fm1lab_store_chunk_encode(uint8_t first, uint8_t count,
                              const fm1lab_instrument_record_t *records,
                              uint8_t *output, uint32_t capacity, uint32_t *written);
int fm1lab_store_chunk_decode(uint8_t expected_first, uint8_t expected_count,
                              const uint8_t *input, uint32_t length,
                              fm1lab_instrument_record_t *records);
