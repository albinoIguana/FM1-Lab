#include "fm1lab_audio_engine.h"

#include <limits.h>
#include <string.h>
#include "fm1.h"

static inline __attribute__((always_inline)) void audio_barrier(void)
{
    __asm__ volatile("" ::: "memory");
}

enum {
    CMD_NOTE_ON = 1,
    CMD_NOTE_OFF,
    CMD_ALL_NOTES_OFF,
    CMD_SET_PATCH,
    CMD_SET_ENABLED,
    CMD_SET_LEVEL,
    CMD_SET_PITCH,
    CMD_MIDI
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
    uint32_t head = engine->queue_head;
    audio_barrier();
    uint32_t next = (head + 1u) & (FM1LAB_AUDIO_QUEUE_CAPACITY - 1u);
    uint32_t tail = engine->queue_tail;
    if (next == tail) return -2;
    engine->queue[head] = *command;
    audio_barrier();
    engine->queue_head = next;
    return 0;
}

static FM1_AUDIO_CODE int dequeue(fm1lab_audio_engine_t *engine,
                                   fm1lab_audio_command_t *command)
{
    uint32_t tail = engine->queue_tail;
    audio_barrier();
    uint32_t head = engine->queue_head;
    if (tail == head) return 0;
    const fm1lab_audio_command_t *source = &engine->queue[tail];
    command->type = source->type;
    command->patch = source->patch;
    command->note = source->note;
    command->velocity = source->velocity;
    command->patch_mask = source->patch_mask;
    command->enabled = source->enabled;
    command->level = source->level;
    command->octave = source->octave;
    command->fine_cents = source->fine_cents;
    command->midi_bytes = source->midi_bytes;
    for (unsigned i = 0; i < sizeof command->midi; ++i)
        command->midi[i] = source->midi[i];
    for (unsigned i = 0; i < FM1LAB_PATCH_BYTES; ++i)
        command->voice[i] = source->voice[i];
    audio_barrier();
    engine->queue_tail = (tail + 1u) & (FM1LAB_AUDIO_QUEUE_CAPACITY - 1u);
    return 1;
}

static FM1_AUDIO_CODE void panic_all(fm1lab_audio_engine_t *engine)
{
    for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p)
        if (engine->synth[p]) fm1_dx7_panic(engine->synth[p]);
}

static FM1_AUDIO_CODE void apply_command(fm1lab_audio_engine_t *engine,
                          const fm1lab_audio_command_t *command)
{
    uint8_t message[3];
    switch (command->type) {
    case CMD_NOTE_ON:
        message[0] = 0x90u; message[2] = command->velocity;
        for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p) {
            if (!(command->patch_mask & (1u << p)) || !engine->enabled[p] || !engine->level[p]) continue;
            int note=(int)command->note+(int)engine->octave[p]*12;
            if(note<0)note=0;else if(note>127)note=127;
            message[1]=(uint8_t)note;
            if(synth_dx7_midi_tuned(engine->synth[p],engine->voice[p],
                    message,sizeof message,engine->fine_cents[p])!=0)engine->errors++;
        }
        break;
    case CMD_NOTE_OFF:
        message[0] = 0x80u; message[2] = 0u;
        for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p) {
            if (!(command->patch_mask & (1u << p))) continue;
            int note=(int)command->note+(int)engine->octave[p]*12;
            if(note<0)note=0;else if(note>127)note=127;
            message[1]=(uint8_t)note;
            if(synth_dx7_midi(engine->synth[p],engine->voice[p],message,sizeof message)!=0)
                engine->errors++;
        }
        break;
    case CMD_ALL_NOTES_OFF:
        panic_all(engine);
        break;
    case CMD_MIDI: {
        uint8_t status=(uint8_t)(command->midi[0]&0xf0u);
        int note_on=(status==0x90u && command->midi_bytes==3u && command->midi[2]>0u);
        int note_event=(status==0x80u || status==0x90u) && command->midi_bytes==3u;
        for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p) {
            if (!(command->patch_mask & (1u << p)) || !engine->enabled[p] || !engine->level[p]) continue;
            message[0]=command->midi[0];message[1]=command->midi[1];message[2]=command->midi[2];
            if(note_event){
                int note=(int)message[1]+(int)engine->octave[p]*12;
                if(note<0)note=0;else if(note>127)note=127;
                message[1]=(uint8_t)note;
            }
            int result=note_on
                ? synth_dx7_midi_tuned(engine->synth[p],engine->voice[p],message,command->midi_bytes,engine->fine_cents[p])
                : synth_dx7_midi(engine->synth[p],engine->voice[p],note_event?message:command->midi,command->midi_bytes);
            if(result!=0)engine->errors++;
        }
        break;
    }
    case CMD_SET_PATCH: {
        unsigned p = command->patch;
        fm1_dx7_panic(engine->synth[p]);
        if (synth_dx7_lfo_reset(engine->synth[p],
                                (uint8_t *)command->voice) == 0) {
            for (unsigned i = 0; i < FM1LAB_PATCH_BYTES; ++i)
                engine->voice[p][i] = command->voice[i];
        } else {
            engine->errors++;
        }
        break;
    }
    case CMD_SET_ENABLED: {
        unsigned p = command->patch;
        if (!command->enabled) fm1_dx7_panic(engine->synth[p]);
        else if (synth_dx7_lfo_reset(engine->synth[p], engine->voice[p]) != 0) {
            engine->errors++;
            break;
        }
        engine->enabled[p] = command->enabled;
        break;
    }
    case CMD_SET_LEVEL:
        if (!command->level && engine->level[command->patch])
            fm1_dx7_panic(engine->synth[command->patch]);
        engine->level[command->patch] = command->level;
        break;
    case CMD_SET_PITCH: {
        unsigned p=command->patch;
        if(engine->octave[p]!=command->octave || engine->fine_cents[p]!=command->fine_cents)
            fm1_dx7_panic(engine->synth[p]);
        engine->octave[p]=command->octave;
        engine->fine_cents[p]=command->fine_cents;
        break;
    }
    default:
        engine->errors++;
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
int fm1lab_audio_engine_set_pitch(fm1lab_audio_engine_t *engine,
                                  unsigned patch, int octave, int fine_cents)
{
    if (!engine || !engine->ready || patch >= FM1LAB_AUDIO_PATCHES ||
        octave < -4 || octave > 4 || fine_cents < -100 || fine_cents > 100)
        return -1;
    fm1lab_audio_command_t command = {0};
    command.type = CMD_SET_PITCH;
    command.patch = (uint8_t)patch;
    command.octave = (int8_t)octave;
    command.fine_cents = (int8_t)fine_cents;
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
    if (result) {
        engine->panic_pending = 1u;
        audio_barrier();
    }
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
    if (result) {
        engine->panic_pending = 1u;
        audio_barrier();
    }
    return result;
}

int fm1lab_audio_engine_midi(fm1lab_audio_engine_t *engine,
                             const uint8_t *message, size_t bytes,
                             uint8_t patch_mask)
{
    if (!engine || !engine->ready || !message || !bytes || bytes > 3u ||
        message[0] < 0x80u || message[0] >= 0xf0u ||
        !patch_mask || (patch_mask & 0xf0u)) return -1;
    size_t expected = (message[0] & 0xe0u) == 0xc0u ? 2u : 3u;
    if (bytes != expected || message[1] > 127u ||
        (expected == 3u && message[2] > 127u)) return -1;
    fm1lab_audio_command_t command = {0};
    command.type = CMD_MIDI;
    command.patch_mask = patch_mask;
    command.midi_bytes = (uint8_t)bytes;
    memcpy(command.midi, message, bytes);
    int result = enqueue(engine, &command);
    if (result && ((message[0] & 0xf0u) == 0x80u ||
        ((message[0] & 0xf0u) == 0x90u && bytes == 3u && message[2] == 0u))) {
        engine->panic_pending = 1u;
        audio_barrier();
    }
    return result;
}

FM1_AUDIO_CODE int fm1lab_audio_engine_render(fm1lab_audio_engine_t *engine,
                               int32_t *output, size_t samples)
{
    if (!engine || !engine->ready || !output ||
        (samples != SYNTH_DX7_BLOCK_SAMPLES && samples != FM1_BLOCK_FRAMES))
        return -1;

    if (engine->panic_pending) {
        engine->panic_pending = 0u;
        audio_barrier();
        engine->queue_tail = engine->queue_head;
        panic_all(engine);
    }

    fm1lab_audio_command_t command;
    while (dequeue(engine, &command)) apply_command(engine, &command);

    unsigned active = 0;
    for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p)
        active += engine->enabled[p] && engine->level[p] != 0u;

    for (size_t i = 0; i < samples; ++i) output[i] = 0;
    if (!active) return 0;

    int32_t patch_output[FM1_BLOCK_FRAMES > SYNTH_DX7_BLOCK_SAMPLES
                             ? FM1_BLOCK_FRAMES : SYNTH_DX7_BLOCK_SAMPLES];
    for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p) {
        if (!engine->enabled[p] || !engine->level[p]) continue;
        for (size_t i = 0; i < samples; ++i) patch_output[i] = 0;
        if (synth_dx7_render(engine->synth[p], patch_output, samples) != 0) {
            engine->errors++;
            continue;
        }
        int32_t gain_q15 = (int32_t)((uint32_t)engine->level[p] * 32768u /
                                       (127u * active));
        for (size_t i = 0; i < samples; ++i) {
            int32_t high = patch_output[i] >> 15;
            int32_t low = patch_output[i] & 0x7fff;
            int32_t contribution = high * gain_q15 +
                ((low * gain_q15) >> 15);
            if (contribution > 0 && output[i] > INT32_MAX - contribution)
                output[i] = INT32_MAX;
            else if (contribution < 0 && output[i] < INT32_MIN - contribution)
                output[i] = INT32_MIN;
            else
                output[i] += contribution;
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
    if (!engine) return 0u;
    audio_barrier();
    return engine->errors;
}
