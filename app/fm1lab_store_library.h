#ifndef FM1LAB_STORE_LIBRARY_H
#define FM1LAB_STORE_LIBRARY_H

#include "fm1lab_instrument.h"
#include "dsp/fm1lab_insert_state.h"

#define FM1LAB_STORE_BANK_COUNT 4u
#define FM1LAB_STORE_INSTRUMENTS_PER_BANK 8u
#define FM1LAB_STORE_INSTRUMENT_COUNT 32u
#define FM1LAB_STORE_PATCHES_PER_BANK 32u
#define FM1LAB_STORE_NAME_BYTES FM1LAB_INSTRUMENT_NAME_BYTES
#define FM1LAB_STORE_PATCH_BANK_BYTES 3752u
#define FM1LAB_STORE_METADATA_BYTES 1742u

/* Last load failure, exposed for native diagnostics and the compact device
 * status overlay. Zero means no failure / success. */
enum fm1lab_store_load_stage {
    FM1LAB_STORE_LOAD_OK = 0,
    FM1LAB_STORE_LOAD_BAD_ARGUMENT,
    FM1LAB_STORE_LOAD_META_SIZE,
    FM1LAB_STORE_LOAD_META_READ,
    FM1LAB_STORE_LOAD_META_HEADER,
    FM1LAB_STORE_LOAD_META_CRC,
    FM1LAB_STORE_LOAD_PATCH_IDS,
    FM1LAB_STORE_LOAD_INSERTS,
    FM1LAB_STORE_LOAD_BANK_SIZE,
    FM1LAB_STORE_LOAD_BANK_READ,
    FM1LAB_STORE_LOAD_BANK_DATA,
    FM1LAB_STORE_LOAD_INSTRUMENT
};
enum fm1lab_store_load_stage fm1lab_store_library_last_load_stage(void);

/* Four 32-patch banks (IDs 0..127) and one Instrument directory. Each Instrument
 * owns four stable patch IDs: instrument_index*4 + slot. Factory-preset selection
 * copies into those IDs; switching Instruments never resets their saved patch data. */
int fm1lab_store_library_load(unsigned index, fm1lab_instrument_t *out);
int fm1lab_store_library_save(unsigned index, const fm1lab_instrument_t *instrument);
int fm1lab_store_library_load_full(unsigned index, fm1lab_instrument_t *instrument, fm1lab_insert_state_t *inserts);
int fm1lab_store_library_save_full(unsigned index, const fm1lab_instrument_t *instrument, const fm1lab_insert_state_t *inserts);
int fm1lab_store_library_format_bank(unsigned bank, const fm1lab_instrument_t *factory_instrument);
int fm1lab_store_library_format(const fm1lab_instrument_t *factory_instrument);

#endif
