#ifndef FM1LAB_AUDIO_BRIDGE_H
#define FM1LAB_AUDIO_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#include "fm1lab_instrument.h"
int fm1lab_audio_init(void);
int fm1lab_audio_is_ready(void);
int fm1lab_audio_sync_instrument(const fm1lab_instrument_t *instrument);
void fm1lab_audio_set_keyboard_mode(int enabled);
/* 1 = consumed by the Instrument, 0 = leave stock MIDI routing alone, -1 = queue full. */
int fm1lab_audio_route_midi(const uint8_t *message, size_t bytes);
void fm1lab_audio_mix(int32_t *stereo_q23);
void fm1lab_audio_sequence_note(unsigned patch, unsigned note, unsigned velocity, int cents);
#endif
