#ifndef FM1LAB_SEQUENCER_H
#define FM1LAB_SEQUENCER_H

#include <stdint.h>
#include "fm1lab_instrument.h"

enum { FM1LAB_SEQUENCE_LANES = 2, FM1LAB_SEQUENCE_STEPS = 64 };

typedef struct {
    uint8_t active;
    uint8_t note;
    uint8_t velocity;
    uint8_t gate; /* Native 0..15 encoding; zero means the engine's 50% default. */
    uint8_t tie;
    uint8_t alt;  /* Native alternate-tuning flag; not a slide/glide control. */
} fm1lab_sequence_step_t;

typedef struct {
    uint8_t backend_track; /* Explicit underlying M-VAVE project track, 0..7. */
    uint8_t length;        /* Native 16, 32, 48 or 64-step loop. */
    uint8_t patch_index;   /* One of the Instrument's four FM patches. */
    fm1lab_sequence_step_t steps[FM1LAB_SEQUENCE_STEPS];
} fm1lab_sequence_lane_t;

typedef struct {
    uint16_t tempo; /* Shared native tempo, 30..300 BPM. */
    uint8_t swing;  /* Shared native swing, 0..50. */
    uint8_t selected_lane;
    uint8_t playing;
    fm1lab_sequence_lane_t lanes[FM1LAB_SEQUENCE_LANES];
} fm1lab_sequence_state_t;

/* Bind two distinct NORMAL DX7 tracks. Other NORMAL tracks are temporarily
 * set OFF through the official role API and restored on unbind. Existing
 * PLAY/OFF roles are preserved. Role changes may invalidate project undo. */
int fm1lab_sequencer_bind(unsigned backend_track_a, unsigned backend_track_b);
int fm1lab_sequencer_unbind(void);
int fm1lab_sequencer_read(fm1lab_sequence_state_t *state);
int fm1lab_sequencer_set_tempo(int bpm);
int fm1lab_sequencer_set_swing(unsigned swing);
int fm1lab_sequencer_set_length(unsigned lane, unsigned steps);
int fm1lab_sequencer_set_patch(unsigned lane, const fm1lab_instrument_t *instrument, unsigned patch);
int fm1lab_sequencer_set_step(unsigned lane, unsigned step,
                              const fm1lab_sequence_step_t *value);
int fm1lab_sequencer_select_lane(unsigned lane);
int fm1lab_sequencer_transport(unsigned playing);

#endif
