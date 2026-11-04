#include "fm1lab_audio_engine.h"

#include <limits.h>
#include <string.h>
#include "fm1_block.h"

enum {
    CMD_NOTE_ON = 1,
    CMD_NOTE_OFF,
    CMD_ALL_NOTES_OFF,
    CMD_SET_PATCH,
    CMD_SET_ENABLED,
    CMD_SET_LEVEL
};

static int voice_header_valid(const uint8_t *voice)
{
    if (!voice) return 0;
    for (unsigned i = 0; i < FM1LAB_PATCH_BYTES; ++i)
        if (voice[i] > 127u) return 0;
    return voice[134] <= 31u && voice[135] <= 7u;
}

static int enqueue(fm1lab_audio_engine_t *engine,
                   const fm1lab_audio_command_t *command)
{
    uint32_t head = __atomic_load_n(&engine->queue_head, __ATOMIC_RELAXED);
    uint32_t next = (head + 1u) & (FM1LAB_AUDIO_QUEUE_CAPACITY - 1u);
    uint32_t tail = __atomic_load_n(&engine->queue_tail, __ATOMIC_ACQUIRE);
    if (next == tail) return -2;
    engine->queue[head] = *command;
    __atomic_store_n(&engine->queue_head, next, __ATOMIC_RELEASE);
    return 0;
}

static int dequeue(fm1lab_audio_engine_t *engine,
                   fm1lab_audio_command_t *command)
{
    uint32_t tail = __atomic_load_n(&engine->queue_tail, __ATOMIC_RELAXED);
    uint32_t head = __atomic_load_n(&engine->queue_head, __ATOMIC_ACQUIRE);
    if (tail == head) return 0;
    *command = engine->queue[tail];
    __atomic_store_n(&engine->queue_tail,
                     (tail + 1u) & (FM1LAB_AUDIO_QUEUE_CAPACITY - 1u),
                     __ATOMIC_RELEASE);
    return 1;
}

static void panic_all(fm1lab_audio_engine_t *engine)
{
    for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p)
        if (engine->synth[p]) fm1_dx7_panic(engine->synth[p]);
}

static void apply_command(fm1lab_audio_engine_t *engine,
                          const fm1lab_audio_command_t *command)
{
    uint8_t message[3];
    switch (command->type) {
    case CMD_NOTE_ON:
        message[0] = 0x90u; message[1] = command->note; message[2] = command->velocity;
        for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p)
            if ((command->patch_mask & (1u << p)) && engine->enabled[p] &&
                synth_dx7_midi_tuned(engine->synth[p], engine->voice[p],
                                     message, sizeof message, 0) != 0)
                __atomic_add_fetch(&engine->errors, 1u, __ATOMIC_RELAXED);
        break;
    case CMD_NOTE_OFF:
        message[0] = 0x80u; message[1] = command->note; message[2] = 0u;
        for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p)
            if (command->patch_mask & (1u << p))
                if (synth_dx7_midi_tuned(engine->synth[p], engine->voice[p],
                                         message, sizeof message, 0) != 0)
                    __atomic_add_fetch(&engine->errors, 1u, __ATOMIC_RELAXED);
        break;
    case CMD_ALL_NOTES_OFF:
        for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p)
            synth_dx7_all_notes_off(engine->synth[p]);
        break;
    case CMD_SET_PATCH: {
        unsigned p = command->patch;
        fm1_dx7_panic(engine->synth[p]);
        if (synth_dx7_lfo_reset(engine->synth[p],
                                (uint8_t *)command->voice) == 0) {
            memcpy(engine->voice[p], command->voice, FM1LAB_PATCH_BYTES);
        } else {
            __atomic_add_fetch(&engine->errors, 1u, __ATOMIC_RELAXED);
        }
        break;
    }
    case CMD_SET_ENABLED: {
        unsigned p = command->patch;
        if (!command->enabled) fm1_dx7_panic(engine->synth[p]);
        else if (synth_dx7_lfo_reset(engine->synth[p], engine->voice[p]) != 0) {
            __atomic_add_fetch(&engine->errors, 1u, __ATOMIC_RELAXED);
            break;
        }
        engine->enabled[p] = command->enabled;
        break;
    }
    case CMD_SET_LEVEL:
        engine->level[command->patch] = command->level;
        break;
    default:
        __atomic_add_fetch(&engine->errors, 1u, __ATOMIC_RELAXED);
        break;
    }
}

int fm1lab_audio_engine_init(
    fm1lab_audio_engine_t *engine,
    const uint8_t voices[FM1LAB_AUDIO_PATCHES][FM1LAB_PATCH_BYTES],
    const uint8_t enabled[FM1LAB_AUDIO_PATCHES],
    const uint8_t levels[FM1LAB_AUDIO_PATCHES],
    uint8_t max_voices, uint32_t sample_rate)
{
    if (!engine || !voices || !enabled || !levels || !max_voices ||
        max_voices > 32u || !sample_rate)
        return -1;
    memset(engine, 0, sizeof *engine);
    for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p) {
        if (!voice_header_valid(voices[p]) || enabled[p] > 1u || levels[p] > 127u)
            goto fail;
        memcpy(engine->voice[p], voices[p], FM1LAB_PATCH_BYTES);
        engine->enabled[p] = enabled[p];
        engine->level[p] = levels[p];
        engine->synth[p] = synth_dx7_create(max_voices, sample_rate,
                                             FM1_BLOCK_FRAMES);
        if (!engine->synth[p] ||
            synth_dx7_lfo_reset(engine->synth[p], engine->voice[p]) != 0)
            goto fail;
    }
    engine->max_voices = max_voices;
    engine->ready = 1u;
    return 0;

fail:
    for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p) {
        if (engine->synth[p]) synth_dx7_destroy(engine->synth[p]);
    }
    memset(engine, 0, sizeof *engine);
    return -1;
}

void fm1lab_audio_engine_destroy(fm1lab_audio_engine_t *engine)
{
    if (!engine) return;
    for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p)
        if (engine->synth[p]) synth_dx7_destroy(engine->synth[p]);
    memset(engine, 0, sizeof *engine);
}

int fm1lab_audio_engine_set_patch(fm1lab_audio_engine_t *engine,
                                  unsigned patch,
                                  const uint8_t voice[FM1LAB_PATCH_BYTES])
{
    if (!engine || !engine->ready || patch >= FM1LAB_AUDIO_PATCHES ||
        !voice_header_valid(voice)) return -1;
    fm1lab_audio_command_t command = {0};
    command.type = CMD_SET_PATCH;
    command.patch = (uint8_t)patch;
    memcpy(command.voice, voice, FM1LAB_PATCH_BYTES);
    return enqueue(engine, &command);
}

int fm1lab_audio_engine_set_enabled(fm1lab_audio_engine_t *engine,
                                    unsigned patch, int enabled)
{
    if (!engine || !engine->ready || patch >= FM1LAB_AUDIO_PATCHES ||
        (enabled != 0 && enabled != 1)) return -1;
    fm1lab_audio_command_t command = {0};
    command.type = CMD_SET_ENABLED;
    command.patch = (uint8_t)patch;
    command.enabled = (uint8_t)enabled;
    return enqueue(engine, &command);
}

int fm1lab_audio_engine_set_level(fm1lab_audio_engine_t *engine,
                                  unsigned patch, unsigned level)
{
    if (!engine || !engine->ready || patch >= FM1LAB_AUDIO_PATCHES || level > 127u)
        return -1;
    fm1lab_audio_command_t command = {0};
    command.type = CMD_SET_LEVEL;
    command.patch = (uint8_t)patch;
    command.level = (uint8_t)level;
    return enqueue(engine, &command);
}

int fm1lab_audio_engine_note_on_mask(fm1lab_audio_engine_t *engine,
                                     uint8_t note, uint8_t velocity,
                                     uint8_t patch_mask)
{
    if (!engine || !engine->ready || note > 127u || velocity > 127u ||
        !patch_mask || (patch_mask & 0xf0u)) return -1;
    if (!velocity) return fm1lab_audio_engine_note_off_mask(engine, note, patch_mask);
    fm1lab_audio_command_t command = {0};
    command.type = CMD_NOTE_ON;
    command.note = note;
    command.velocity = velocity;
    command.patch_mask = patch_mask;
    return enqueue(engine, &command);
}

int fm1lab_audio_engine_note_off_mask(fm1lab_audio_engine_t *engine,
                                      uint8_t note, uint8_t patch_mask)
{
    if (!engine || !engine->ready || note > 127u ||
        !patch_mask || (patch_mask & 0xf0u)) return -1;
    fm1lab_audio_command_t command = {0};
    command.type = CMD_NOTE_OFF;
    command.note = note;
    command.patch_mask = patch_mask;
    int result = enqueue(engine, &command);
    if (result) __atomic_store_n(&engine->panic_pending, 1u, __ATOMIC_RELEASE);
    return result;
}

int fm1lab_audio_engine_note_on(fm1lab_audio_engine_t *engine,
                                uint8_t note, uint8_t velocity)
{
    return fm1lab_audio_engine_note_on_mask(engine, note, velocity, 0x0fu);
}

int fm1lab_audio_engine_note_off(fm1lab_audio_engine_t *engine, uint8_t note)
{
    return fm1lab_audio_engine_note_off_mask(engine, note, 0x0fu);
}

int fm1lab_audio_engine_all_notes_off(fm1lab_audio_engine_t *engine)
{
    if (!engine || !engine->ready) return -1;
    fm1lab_audio_command_t command = {0};
    command.type = CMD_ALL_NOTES_OFF;
    int result = enqueue(engine, &command);
    if (result) __atomic_store_n(&engine->panic_pending, 1u, __ATOMIC_RELEASE);
    return result;
}

int fm1lab_audio_engine_render(fm1lab_audio_engine_t *engine,
                               int32_t *output, size_t samples)
{
    if (!engine || !engine->ready || !output ||
        (samples != SYNTH_DX7_BLOCK_SAMPLES && samples != FM1_BLOCK_FRAMES))
        return -1;

    if (__atomic_exchange_n(&engine->panic_pending, 0u, __ATOMIC_ACQ_REL)) {
        uint32_t head = __atomic_load_n(&engine->queue_head, __ATOMIC_ACQUIRE);
        __atomic_store_n(&engine->queue_tail, head, __ATOMIC_RELEASE);
        panic_all(engine);
    }

    fm1lab_audio_command_t command;
    while (dequeue(engine, &command)) apply_command(engine, &command);

    unsigned active = 0;
    for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p)
        active += engine->enabled[p] && engine->level[p] != 0u;

    memset(output, 0, samples * sizeof *output);
    if (!active) return 0;

    int32_t patch_output[FM1_BLOCK_FRAMES > SYNTH_DX7_BLOCK_SAMPLES
                             ? FM1_BLOCK_FRAMES : SYNTH_DX7_BLOCK_SAMPLES];
    for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p) {
        if (!engine->enabled[p] || !engine->level[p]) continue;
        memset(patch_output, 0, samples * sizeof *patch_output);
        if (synth_dx7_render(engine->synth[p], patch_output, samples) != 0) {
            __atomic_add_fetch(&engine->errors, 1u, __ATOMIC_RELAXED);
            continue;
        }
        for (size_t i = 0; i < samples; ++i) {
            int64_t contribution = (int64_t)patch_output[i] * engine->level[p];
            contribution /= (int64_t)(127u * active);
            int64_t mixed = (int64_t)output[i] + contribution;
            if (mixed > INT32_MAX) mixed = INT32_MAX;
            if (mixed < INT32_MIN) mixed = INT32_MIN;
            output[i] = (int32_t)mixed;
        }
    }
    return 0;
}

unsigned fm1lab_audio_engine_active_voices(const fm1lab_audio_engine_t *engine,
                                           unsigned patch)
{
    if (!engine || !engine->ready || patch >= FM1LAB_AUDIO_PATCHES) return 0;
    return synth_dx7_active(engine->synth[patch]);
}

uint32_t fm1lab_audio_engine_error_count(const fm1lab_audio_engine_t *engine)
{
    return engine ? __atomic_load_n(&engine->errors, __ATOMIC_RELAXED) : 0u;
}
