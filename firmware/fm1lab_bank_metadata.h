#ifndef FM1LAB_BANK_METADATA_H
#define FM1LAB_BANK_METADATA_H

#include <stdint.h>

/* M-VAVE v1 bank: 20-byte header + 32 packed 112-byte DX7 voices. */
#define FM1LAB_BANK_VOICE_BYTES 112u
#define FM1LAB_BANK_VOICE_COUNT 32u
#define FM1LAB_BANK_VOICE_AREA_BYTES 3604u
#define FM1LAB_BANK_SECTOR_BYTES 4096u
#define FM1LAB_BANK_INSTRUMENTS 8u
#define FM1LAB_BANK_PATCHES 4u
#define FM1LAB_BANK_NAME_BYTES 24u
#define FM1LAB_BANK_MACROS 4u
#define FM1LAB_BANK_META_RECORD_BYTES 56u
#define FM1LAB_BANK_META_PAYLOAD_BYTES 448u
#define FM1LAB_BANK_META_HEADER_BYTES 16u
#define FM1LAB_BANK_META_BYTES 464u
#define FM1LAB_BANK_META_OFFSET FM1LAB_BANK_VOICE_AREA_BYTES
#define FM1LAB_BANK_META_END (FM1LAB_BANK_META_OFFSET + FM1LAB_BANK_META_BYTES)

typedef struct {
    uint8_t macros[FM1LAB_BANK_MACROS]; /* 0..100, neutral at 50 */
    uint8_t enabled;                    /* 0 = muted, 1 = active */
    uint8_t level;                      /* 0..127 */
    int8_t octave;                       /* -4..+4 */
    int8_t fine_cents;                   /* -100..+100 */
} fm1lab_bank_patch_meta_t;

typedef struct {
    char name[FM1LAB_BANK_NAME_BYTES]; /* printable ASCII, space padded */
    fm1lab_bank_patch_meta_t patches[FM1LAB_BANK_PATCHES];
} fm1lab_bank_instrument_meta_t;

typedef struct {
    fm1lab_bank_instrument_meta_t instruments[FM1LAB_BANK_INSTRUMENTS];
} fm1lab_bank_metadata_t;

/* Initialize metadata for an empty or legacy v1 bank. */
void fm1lab_bank_metadata_init(fm1lab_bank_metadata_t *metadata);

/* Encode/decode the fixed 400-byte v2 metadata block, including CRC. */
int fm1lab_bank_metadata_encode(const fm1lab_bank_metadata_t *metadata,
                                uint8_t output[FM1LAB_BANK_META_BYTES]);
int fm1lab_bank_metadata_decode(fm1lab_bank_metadata_t *metadata,
                                const uint8_t input[FM1LAB_BANK_META_BYTES]);
int fm1lab_bank_metadata_valid(const fm1lab_bank_metadata_t *metadata);

#endif
