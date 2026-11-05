#include "fm1lab_audio_bridge.h"

#include <limits.h>
#include <string.h>
#include "fm1.h"
#include "dsp/fm6.h"
#include "fm1lab_instrument.h"
#include "fm1lab_runtime.h"

enum {
    CMD_PATCH_CONFIG = 1,
    CMD_NOTE_ON,
    CMD_NOTE_OFF,
    CMD_ALL_NOTES_OFF,
    CMD_PITCH_BEND,
    FM1LAB_AUDIO_QUEUE_CAPACITY = 32
};

typedef struct {
    uint8_t type;
    uint8_t patch;
    uint8_t note;
    uint8_t velocity;
    uint8_t patch_mask;
    uint8_t enabled;
    uint8_t level;
    uint8_t macros[FM1LAB_MACRO_COUNT];
    uint16_t bend;
    uint8_t voice[FM1LAB_PATCH_BYTES];
} fm1lab_audio_command_t;

static fm1lab_audio_command_t command_queue[FM1LAB_AUDIO_QUEUE_CAPACITY];
static fm1lab_instrument_t audio_shadow;
static volatile uint32_t queue_head, queue_tail;
static volatile uint32_t panic_pending, audio_ready, keyboard_mode;
static int32_t fm6_output[FM1_BLOCK_WORDS] __attribute__((aligned(32)));

static inline __attribute__((always_inline)) void audio_barrier(void)
{
    __asm__ volatile("" ::: "memory");
}

static inline __attribute__((always_inline)) int32_t add_sat32(int32_t a, int32_t b)
{
    if (b > 0 && a > INT32_MAX - b) return INT32_MAX;
    if (b < 0 && a < INT32_MIN - b) return INT32_MIN;
    return a + b;
}

static int enqueue(const fm1lab_audio_command_t *command)
{
    uint32_t head = queue_head;
    audio_barrier();
    uint32_t next = (head + 1u) & (FM1LAB_AUDIO_QUEUE_CAPACITY - 1u);
    uint32_t tail = queue_tail;
    if (next == tail) return -2;
    command_queue[head] = *command;
    audio_barrier();
    queue_head = next;
    return 0;
}

static FM1_AUDIO_CODE int dequeue(fm1lab_audio_command_t *command)
{
    uint32_t tail = queue_tail;
    audio_barrier();
    uint32_t head = queue_head;
    if (tail == head) return 0;
    *command = command_queue[tail];
    audio_barrier();
    queue_tail = (tail + 1u) & (FM1LAB_AUDIO_QUEUE_CAPACITY - 1u);
    return 1;
}

static FM1_AUDIO_CODE void apply_patch_config(const fm1lab_audio_command_t *command)
{
    unsigned patch = command->patch;
    if (patch >= FM1LAB_PATCH_COUNT) return;
    if (fm6_patch_set_voice(patch, command->voice)) return;
    for (unsigned macro = 0; macro < FM1LAB_MACRO_COUNT; ++macro)
        fm6_macro_set(patch, macro, command->macros[macro]);
    fm6_patch_set_enabled(patch, command->enabled);
    fm6_patch_set_level(patch, command->level);
}

int fm1lab_audio_init(void)
{
    if (fm1lab_audio_is_ready()) return 0;

    fm1lab_runtime_t runtime;
    memset(&runtime, 0, sizeof runtime);
    if (fm1lab_runtime_load(&runtime, 0u) != 0) return -1;

    fm6_init();
    for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch) {
        const fm1lab_patch_t *part = &runtime.instrument.patches[patch];
        if (fm6_patch_set_voice(patch, part->voice) != 0) return -1;
        for (unsigned macro = 0; macro < FM1LAB_MACRO_COUNT; ++macro)
            fm6_macro_set(patch, macro, part->macros[macro]);
        fm6_patch_set_enabled(patch, part->enabled);
        fm6_patch_set_level(patch, part->level);
    }

    audio_shadow = runtime.instrument;
    audio_barrier();
    audio_ready = 1u;
    return 0;
}

FM1_AUDIO_CODE int fm1lab_audio_is_ready(void)
{
    uint32_t ready = audio_ready;
    audio_barrier();
    return ready != 0u;
}

int fm1lab_audio_sync_instrument(const fm1lab_instrument_t *instrument)
{
    if (!fm1lab_audio_is_ready() || !instrument ||
        !fm1lab_instrument_valid(instrument)) return -1;

    int result = 0;
    for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch) {
        const fm1lab_patch_t *next = &instrument->patches[patch];
        fm1lab_patch_t *previous = &audio_shadow.patches[patch];
        if (memcmp(previous, next, sizeof *next) == 0) continue;

        fm1lab_audio_command_t command;
        memset(&command, 0, sizeof command);
        command.type = CMD_PATCH_CONFIG;
        command.patch = (uint8_t)patch;
        command.enabled = next->enabled;
        command.level = next->level;
        memcpy(command.macros, next->macros, sizeof command.macros);
        memcpy(command.voice, next->voice, sizeof command.voice);
        if (enqueue(&command) != 0) {
            result = -2;
            continue;
        }
        *previous = *next;
    }
    memcpy(audio_shadow.name, instrument->name, sizeof audio_shadow.name);
    return result;
}

void fm1lab_audio_set_keyboard_mode(int enabled)
{
    audio_barrier();
    keyboard_mode = enabled ? 1u : 0u;
}

static int queue_note(uint8_t type, uint8_t note, uint8_t velocity)
{
    fm1lab_audio_command_t command;
    memset(&command, 0, sizeof command);
    command.type = type;
    command.note = note;
    command.velocity = velocity;
    command.patch_mask = (uint8_t)((1u << FM1LAB_PATCH_COUNT) - 1u);
    int result = enqueue(&command);
    if (result && type == CMD_NOTE_OFF) {
        audio_barrier();
        panic_pending = 1u;
    }
    return result;
}

int fm1lab_audio_route_midi(const uint8_t *message, size_t bytes)
{
    if (!message || !bytes || !fm1lab_audio_is_ready()) return 0;
    uint32_t mode = keyboard_mode;
    audio_barrier();
    if (!mode) return 0;

    uint8_t status = message[0] & 0xf0u;
    if (status == 0x80u || status == 0x90u) {
        if (bytes != 3u || message[1] > 127u || message[2] > 127u) return -1;
        if (status == 0x80u || message[2] == 0u)
            return queue_note(CMD_NOTE_OFF, message[1], 0u) == 0 ? 1 : -1;
        return queue_note(CMD_NOTE_ON, message[1], message[2]) == 0 ? 1 : -1;
    }
    if (status == 0xe0u) {
        if (bytes != 3u || message[1] > 127u || message[2] > 127u) return -1;
        fm1lab_audio_command_t command;
        memset(&command, 0, sizeof command);
        command.type = CMD_PITCH_BEND;
        command.bend = (uint16_t)((uint16_t)message[1] | ((uint16_t)message[2] << 7));
        return enqueue(&command) == 0 ? 1 : -1;
    }
    if (status == 0xb0u && bytes == 3u && (message[1] == 120u || message[1] == 123u)) {
        fm1lab_audio_command_t command;
        memset(&command, 0, sizeof command);
        command.type = CMD_ALL_NOTES_OFF;
        return enqueue(&command) == 0 ? 1 : -1;
    }
    if (message[0] == 0xffu) {
        fm1lab_audio_command_t command;
        memset(&command, 0, sizeof command);
        command.type = CMD_ALL_NOTES_OFF;
        (void)enqueue(&command);
    }
    return 0;
}

/* Called only from the audio owner (DMA callback or a sequencer engine hook). */
FM1_AUDIO_CODE void fm1lab_audio_sequence_note(unsigned patch, unsigned note,
                                                unsigned velocity, int cents)
{
    if (!fm1lab_audio_is_ready() || patch >= FM1LAB_PATCH_COUNT ||
        note > 127u || velocity > 127u) return;
    if (velocity) {
        int tune16 = cents * 16 / 100;
        if (tune16 > 32) tune16 = 32;
        if (tune16 < -32) tune16 = -32;
        fm6_note_on_tuned_mask((uint8_t)note, (uint8_t)velocity,
                               (uint8_t)(1u << patch), (int16_t)tune16);
    } else {
        fm6_note_off_mask((uint8_t)note, (uint8_t)(1u << patch));
    }
}

/* The FM6 render is mixed after the stock engine render and before APP master
 * gain/USB/BT mixing. The FM6 core and its lookup tables are staged in the
 * SDK's volatile-RAM code/data sections for flash-safe audio execution. */
FM1_AUDIO_CODE void fm1lab_audio_mix(int32_t *stereo_q23)
{
    if (!fm1lab_audio_is_ready() || !stereo_q23) return;

    uint32_t panic = panic_pending;
    if (panic) {
        panic_pending = 0u;
        audio_barrier();
        fm6_panic();
    }

    fm1lab_audio_command_t command;
    while (dequeue(&command)) {
        if (panic && (command.type == CMD_NOTE_ON || command.type == CMD_NOTE_OFF))
            continue;
        switch (command.type) {
        case CMD_PATCH_CONFIG:
            apply_patch_config(&command);
            break;
        case CMD_NOTE_ON:
            fm6_note_on_mask(command.note, command.velocity, command.patch_mask);
            break;
        case CMD_NOTE_OFF:
            fm6_note_off_mask(command.note, command.patch_mask);
            break;
        case CMD_ALL_NOTES_OFF:
            fm6_panic();
            break;
        case CMD_PITCH_BEND:
            fm6_pitch_bend(command.bend);
            break;
        default:
            break;
        }
    }

    if (!fm6_active_voices()) return;
    fm6_render(fm6_output, FM1_BLOCK_FRAMES, 4096u);
    for (unsigned frame = 0; frame < FM1_BLOCK_FRAMES; ++frame) {
        stereo_q23[frame * 2u] = add_sat32(stereo_q23[frame * 2u], fm6_output[frame * 2u]);
        stereo_q23[frame * 2u + 1u] =
            add_sat32(stereo_q23[frame * 2u + 1u], fm6_output[frame * 2u + 1u]);
    }
}
