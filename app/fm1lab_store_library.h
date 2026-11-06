#ifndef FM1LAB_STORE_LIBRARY_H
#define FM1LAB_STORE_LIBRARY_H

#include "fm1lab_instrument.h"
#include "fm1lab_insert_state.h"

#define FM1LAB_STORE_BANK_COUNT 4u
#define FM1LAB_STORE_INSTRUMENTS_PER_BANK 8u
#define FM1LAB_STORE_INSTRUMENT_COUNT 32u

/* Uses four 3,840-byte plat_store objects in the app's existing pooled flash area.
 * Control task only. Each Instrument contains four packed DX7 voices. */
int fm1lab_store_library_load(unsigned index, fm1lab_instrument_t *out);
int fm1lab_store_library_save(unsigned index, const fm1lab_instrument_t *instrument);
int fm1lab_store_library_load_full(unsigned index, fm1lab_instrument_t *instrument, fm1lab_insert_state_t *inserts);
int fm1lab_store_library_save_full(unsigned index, const fm1lab_instrument_t *instrument, const fm1lab_insert_state_t *inserts);
int fm1lab_store_library_format_bank(unsigned bank, const fm1lab_instrument_t *factory_instrument);
int fm1lab_store_library_format(const fm1lab_instrument_t *factory_instrument);

#endif
