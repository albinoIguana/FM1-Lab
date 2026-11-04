#ifndef FM1LAB_LIBRARY_H
#define FM1LAB_LIBRARY_H

#include "fm1lab_instrument.h"

#define FM1LAB_LIBRARY_BANKS 4u
#define FM1LAB_INSTRUMENTS_PER_BANK 8u
#define FM1LAB_INSTRUMENT_COUNT (FM1LAB_LIBRARY_BANKS * FM1LAB_INSTRUMENTS_PER_BANK)

/* Control-task APIs. A four-patch Instrument occupies four consecutive DX7
 * programs in one bank; the eight Instruments in a bank cover its 32 voices. */
int fm1lab_library_load(unsigned instrument_index, fm1lab_instrument_t *instrument);
int fm1lab_library_save(unsigned instrument_index, const fm1lab_instrument_t *instrument);

#endif
