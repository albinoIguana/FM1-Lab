/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#include "fm6.h"

void seq_init(void);
void seq_select_instrument(uint32_t slot);
void seq_instrument_load(uint32_t slot, const fm1lab_instrument_record_t *record);
void seq_instrument_export_current(fm1lab_instrument_record_t *record);
void seq_process(uint32_t frames);
void seq_start(void);
void seq_stop(void);
void seq_toggle(void);
uint8_t seq_running(void);
uint8_t seq_current_step(void);
uint8_t seq_step_note(uint32_t lane, uint32_t step);
uint8_t seq_step_velocity(uint32_t lane, uint32_t step);
void seq_set_step(uint32_t lane, uint32_t step, uint8_t note, uint8_t velocity);
void seq_clear_step(uint32_t lane, uint32_t step);
uint8_t seq_tempo(void);
uint8_t seq_instrument_dirty(uint32_t slot);
void seq_instrument_mark_saved(uint32_t slot);
void seq_set_tempo(uint8_t bpm);
