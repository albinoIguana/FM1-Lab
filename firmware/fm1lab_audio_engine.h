#ifndef FM1LAB_AUDIO_ENGINE_H
#define FM1LAB_AUDIO_ENGINE_H

#include <stddef.h>
#include <stdint.h>
#include "fm1lab_instrument.h"
#include "synth_dx7.h"

#define FM1LAB_AUDIO_PATCHES FM1LAB_PATCH_COUNT
#define FM1LAB_AUDIO_QUEUE_CAPACITY 32u

typedef struct {
    uint8_t type;
    uint8_t patch;
    uint8_t note;
    uint8_t velocity;
    uint8_t patch_mask;
    uint8_t enabled;
    uint8_t level;
    uint8_t midi_bytes;
    uint8_t midi[3];
    uint8_t voice[FM1LAB_PATCH_BYTES];
} fm1lab_audio_command_t;

/*
 * Four independent six-operator DX7 instances. Control-side calls enqueue
 * commands; only fm1lab_audio_engine_render() touches live synth state.
 * Render output is signed Q24. Initialize/destroy only while audio is stopped.
 */
typedef struct {
    synth_dx7_t *synth[FM1LAB_AUDIO_PATCHES];
    uint8_t voice[FM1LAB_AUDIO_PATCHES][FM1LAB_PATCH_BYTES];
    uint8_t enabled[FM1LAB_AUDIO_PATCHES];
    uint8_t level[FM1LAB_AUDIO_PATCHES];
    volatile uint32_t queue_head;
    volatile uint32_t queue_tail;
    volatile uint32_t panic_pending;
    volatile uint32_t errors;
    fm1lab_audio_command_t queue[FM1LAB_AUDIO_QUEUE_CAPACITY];
    uint8_t max_voices;
    uint8_t ready;
} fm1lab_audio_engine_t;

int fm1lab_audio_engine_init(
    fm1lab_audio_engine_t *engine,
    const uint8_t voices[FM1LAB_AUDIO_PATCHES][FM1LAB_PATCH_BYTES],
    const uint8_t enabled[FM1LAB_AUDIO_PATCHES],
    const uint8_t levels[FM1LAB_AUDIO_PATCHES],
    uint8_t max_voices,
    uint32_t sample_rate);
void fm1lab_audio_engine_destroy(fm1lab_audio_engine_t *engine);

int fm1lab_audio_engine_set_patch(fm1lab_audio_engine_t *engine,
                                  unsigned patch,
                                  const uint8_t voice[FM1LAB_PATCH_BYTES]);
int fm1lab_audio_engine_set_enabled(fm1lab_audio_engine_t *engine,
                                    unsigned patch, int enabled);
int fm1lab_audio_engine_set_level(fm1lab_audio_engine_t *engine,
                                  unsigned patch, unsigned level);

int fm1lab_audio_engine_note_on(fm1lab_audio_engine_t *engine,
                                uint8_t note, uint8_t velocity);
int fm1lab_audio_engine_note_off(fm1lab_audio_engine_t *engine,
                                 uint8_t note);
int fm1lab_audio_engine_note_on_mask(fm1lab_audio_engine_t *engine,
                                     uint8_t note, uint8_t velocity,
                                     uint8_t patch_mask);
int fm1lab_audio_engine_note_off_mask(fm1lab_audio_engine_t *engine,
                                      uint8_t note, uint8_t patch_mask);
int fm1lab_audio_engine_all_notes_off(fm1lab_audio_engine_t *engine);
int fm1lab_audio_engine_midi(fm1lab_audio_engine_t *engine,
                             const uint8_t *message, size_t bytes,
                             uint8_t patch_mask);

/* Clears and renders 64 or FM1_BLOCK_FRAMES samples into signed Q24 output. */
int fm1lab_audio_engine_render(fm1lab_audio_engine_t *engine,
                               int32_t *output, size_t samples);

/* Audio-state diagnostics; call from the render owner or while audio is stopped. */
unsigned fm1lab_audio_engine_active_voices(const fm1lab_audio_engine_t *engine,
                                           unsigned patch);
uint32_t fm1lab_audio_engine_error_count(const fm1lab_audio_engine_t *engine);

#endif
