#ifndef FM1LAB_AUDIO_BRIDGE_H
#define FM1LAB_AUDIO_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
#include "fm1lab_instrument.h"
int fm1lab_audio_init(void);
int fm1lab_audio_is_ready(void);
int fm1lab_audio_sync_instrument(const fm1lab_instrument_t *instrument);
int fm1lab_audio_midi(const uint8_t *message, size_t bytes);
void fm1lab_audio_mix(int32_t *stereo_q23);
#endif
