/* SPDX-License-Identifier: GPL-3.0-only
 * Two-lane, sixteen-step note sequencer. It shares the Instrument's FM6
 * voice allocator; each lane has a source ID so its note-offs cannot cut
 * keyboard/MIDI notes or the other lane.
 */
#include <stdint.h>
#include "seq.h"

#define LANES FM1LAB_SEQ_LANES
#define STEPS FM1LAB_SEQ_STEPS
#define SAMPLE_RATE 44100u
#define EMPTY_NOTE 0xFFu
#define DEFAULT_TEMPO 120u
#define GATE_NUMERATOR 3u
#define GATE_DENOMINATOR 4u

typedef struct {
    uint8_t note[LANES][STEPS];
    uint8_t velocity[LANES][STEPS];
    uint8_t tempo;
} seq_pattern_t;

static seq_pattern_t patterns[FM1LAB_INSTRUMENT_COUNT];
static uint8_t pattern_dirty[FM1LAB_INSTRUMENT_COUNT];
static uint32_t current_instrument;
static uint32_t sample_clock;
static uint32_t next_step_sample;
static uint32_t step_samples;
static uint32_t gate_samples;
static volatile uint8_t current_step;
static volatile uint8_t running;
static uint8_t active_note[LANES];

static uint8_t clamp_tempo(uint8_t bpm)
{
    return bpm < 60u ? 60u : bpm > 240u ? 240u : bpm;
}

static void update_timing(void)
{
    uint32_t bpm = clamp_tempo(patterns[current_instrument].tempo);
    patterns[current_instrument].tempo = (uint8_t)bpm;
    step_samples = (SAMPLE_RATE * 15u) / bpm; /* sixteenth note */
    if (!step_samples)
        step_samples = 1u;
    gate_samples = (step_samples * GATE_NUMERATOR) / GATE_DENOMINATOR;
    if (!gate_samples)
        gate_samples = 1u;
}

void seq_init(void)
{
    uint32_t p, lane, step;
    for (p = 0; p < FM1LAB_INSTRUMENT_COUNT; p++) {
        patterns[p].tempo = DEFAULT_TEMPO;
        for (lane = 0; lane < LANES; lane++)
            for (step = 0; step < STEPS; step++) {
                patterns[p].note[lane][step] = EMPTY_NOTE;
                patterns[p].velocity[lane][step] = 0;
            }
    }
    current_instrument = 0;
    for (p = 0; p < FM1LAB_INSTRUMENT_COUNT; p++)
        pattern_dirty[p] = 0;
    sample_clock = 0;
    current_step = STEPS - 1u;
    running = 0;
    active_note[0] = active_note[1] = EMPTY_NOTE;
    update_timing();
}

void seq_instrument_load(uint32_t slot, const fm1lab_instrument_record_t *record)
{
    uint32_t lane, step;
    if (slot >= FM1LAB_INSTRUMENT_COUNT || !record)
        return;
    patterns[slot].tempo = record->seq_tempo ? clamp_tempo(record->seq_tempo) : DEFAULT_TEMPO;
    for (lane = 0; lane < LANES; lane++)
        for (step = 0; step < STEPS; step++) {
            uint8_t note = record->seq_note[lane][step];
            patterns[slot].note[lane][step] = (note <= 127u || note == EMPTY_NOTE) ? note : EMPTY_NOTE;
            patterns[slot].velocity[lane][step] = record->seq_velocity[lane][step] > 127u
                ? 127u : record->seq_velocity[lane][step];
        }
    pattern_dirty[slot] = 0;
    if (slot == current_instrument)
        update_timing();
}

void seq_instrument_export_current(fm1lab_instrument_record_t *record)
{
    uint32_t lane, step;
    if (!record)
        return;
    record->seq_tempo = patterns[current_instrument].tempo;
    for (lane = 0; lane < LANES; lane++)
        for (step = 0; step < STEPS; step++) {
            record->seq_note[lane][step] = patterns[current_instrument].note[lane][step];
            record->seq_velocity[lane][step] = patterns[current_instrument].velocity[lane][step];
        }
}

void seq_stop(void)
{
    uint32_t lane;
    for (lane = 0; lane < LANES; lane++) {
        if (active_note[lane] != EMPTY_NOTE)
            fm6_note_off_source(active_note[lane], (uint8_t)(lane + 1u));
        active_note[lane] = EMPTY_NOTE;
    }
    running = 0;
}

void seq_select_instrument(uint32_t slot)
{
    if (slot >= FM1LAB_INSTRUMENT_COUNT)
        return;
    seq_stop();
    current_instrument = slot;
    current_step = STEPS - 1u;
    next_step_sample = sample_clock;
    update_timing();
}

void seq_start(void)
{
    seq_stop();
    current_step = STEPS - 1u;
    next_step_sample = sample_clock;
    running = 1;
}

void seq_toggle(void)
{
    if (running)
        seq_stop();
    else
        seq_start();
}

void seq_process(uint32_t frames)
{
    uint32_t i, lane;
    if (!running) {
        sample_clock += frames;
        return;
    }
    for (i = 0; i < frames; i++) {
        if (sample_clock >= next_step_sample) {
            current_step = (uint8_t)((current_step + 1u) & (STEPS - 1u));
            for (lane = 0; lane < LANES; lane++) {
                if (active_note[lane] != EMPTY_NOTE)
                    fm6_note_off_source(active_note[lane], (uint8_t)(lane + 1u));
                active_note[lane] = EMPTY_NOTE;
                if (patterns[current_instrument].note[lane][current_step] != EMPTY_NOTE) {
                    uint8_t note = patterns[current_instrument].note[lane][current_step];
                    uint8_t velocity = patterns[current_instrument].velocity[lane][current_step];
                    fm6_note_on_source(note, velocity ? velocity : 100u, (uint8_t)(lane + 1u));
                    active_note[lane] = note;
                }
            }
            next_step_sample += step_samples;
        }
        for (lane = 0; lane < LANES; lane++) {
            if (active_note[lane] != EMPTY_NOTE && sample_clock >= next_step_sample - step_samples + gate_samples) {
                fm6_note_off_source(active_note[lane], (uint8_t)(lane + 1u));
                active_note[lane] = EMPTY_NOTE;
            }
        }
        sample_clock++;
    }
}

uint8_t seq_running(void) { return running; }
uint8_t seq_current_step(void) { return current_step; }
uint8_t seq_step_note(uint32_t lane, uint32_t step)
{
    return lane < LANES && step < STEPS ? patterns[current_instrument].note[lane][step] : EMPTY_NOTE;
}
uint8_t seq_step_velocity(uint32_t lane, uint32_t step)
{
    return lane < LANES && step < STEPS ? patterns[current_instrument].velocity[lane][step] : 0u;
}
void seq_set_step(uint32_t lane, uint32_t step, uint8_t note, uint8_t velocity)
{
    if (lane >= LANES || step >= STEPS)
        return;
    patterns[current_instrument].note[lane][step] = note <= 127u ? note : EMPTY_NOTE;
    patterns[current_instrument].velocity[lane][step] = velocity > 127u ? 127u : velocity;
    pattern_dirty[current_instrument] = 1;
}
void seq_clear_step(uint32_t lane, uint32_t step)
{
    if (lane >= LANES || step >= STEPS)
        return;
    patterns[current_instrument].note[lane][step] = EMPTY_NOTE;
    patterns[current_instrument].velocity[lane][step] = 0;
    pattern_dirty[current_instrument] = 1;
}
uint8_t seq_tempo(void) { return patterns[current_instrument].tempo; }
void seq_set_tempo(uint8_t bpm)
{
    bpm = clamp_tempo(bpm);
    if (patterns[current_instrument].tempo != bpm)
        pattern_dirty[current_instrument] = 1;
    patterns[current_instrument].tempo = bpm;
    update_timing();
    if (running)
        next_step_sample = sample_clock + step_samples;
}

uint8_t seq_instrument_dirty(uint32_t slot)
{
    return slot < FM1LAB_INSTRUMENT_COUNT ? pattern_dirty[slot] : 0u;
}
void seq_instrument_mark_saved(uint32_t slot)
{
    if (slot < FM1LAB_INSTRUMENT_COUNT)
        pattern_dirty[slot] = 0;
}
