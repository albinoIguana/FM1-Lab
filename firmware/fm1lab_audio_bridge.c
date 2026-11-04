#include "fm1lab_audio_bridge.h"

#include <string.h>
#include "fm1_block.h"
#include "fm1lab_audio_engine.h"
#include "fm1lab_instrument.h"
#include "fm1lab_runtime.h"

enum { FM1LAB_AUDIO_VOICES_PER_PATCH = 4 };

static fm1lab_audio_engine_t audio_engine;
static fm1lab_instrument_t audio_shadow;
static uint8_t live_voice[FM1LAB_AUDIO_PATCHES][FM1LAB_PATCH_BYTES];
static volatile uint32_t audio_ready;
static volatile uint32_t keyboard_mode;

static int32_t limit_q23(int64_t value)
{
    if (value > 8388607) return 8388607;
    if (value < -8388608) return -8388608;
    return (int32_t)value;
}

int fm1lab_audio_init(void)
{
    if (__atomic_load_n(&audio_ready, __ATOMIC_ACQUIRE)) return 0;

    fm1lab_runtime_t runtime;
    memset(&runtime, 0, sizeof runtime);
    if (fm1lab_runtime_load(&runtime, 0u) != 0) return -1;

    uint8_t voices[FM1LAB_AUDIO_PATCHES][FM1LAB_PATCH_BYTES];
    uint8_t enabled[FM1LAB_AUDIO_PATCHES];
    uint8_t levels[FM1LAB_AUDIO_PATCHES];
    for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p) {
        if (fm1lab_render_patch(&runtime.instrument, p, voices[p]) != 0)
            return -1;
        enabled[p] = runtime.instrument.patches[p].enabled;
        levels[p] = runtime.instrument.patches[p].level;
    }

    if (fm1lab_audio_engine_init(&audio_engine, voices, enabled, levels,
                                 FM1LAB_AUDIO_VOICES_PER_PATCH,
                                 FM1_SAMPLE_RATE) != 0)
        return -1;

    audio_shadow = runtime.instrument;
    memcpy(live_voice, voices, sizeof live_voice);
    __atomic_store_n(&audio_ready, 1u, __ATOMIC_RELEASE);
    return 0;
}

int fm1lab_audio_is_ready(void)
{
    return __atomic_load_n(&audio_ready, __ATOMIC_ACQUIRE) != 0u;
}

int fm1lab_audio_sync_instrument(const fm1lab_instrument_t *instrument)
{
    if (!fm1lab_audio_is_ready() || !instrument ||
        !fm1lab_instrument_valid(instrument)) return -1;

    int result = 0;
    for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p) {
        uint8_t rendered[FM1LAB_PATCH_BYTES];
        if (fm1lab_render_patch(instrument, p, rendered) != 0) return -1;

        if (memcmp(rendered, live_voice[p], FM1LAB_PATCH_BYTES) != 0) {
            int r = fm1lab_audio_engine_set_patch(&audio_engine, p, rendered);
            if (r) result = r;
            else memcpy(live_voice[p], rendered, FM1LAB_PATCH_BYTES);
        }
        if (instrument->patches[p].enabled != audio_shadow.patches[p].enabled) {
            int r = fm1lab_audio_engine_set_enabled(
                &audio_engine, p, instrument->patches[p].enabled);
            if (r) result = r;
            else audio_shadow.patches[p].enabled = instrument->patches[p].enabled;
        }
        if (instrument->patches[p].level != audio_shadow.patches[p].level) {
            int r = fm1lab_audio_engine_set_level(
                &audio_engine, p, instrument->patches[p].level);
            if (r) result = r;
            else audio_shadow.patches[p].level = instrument->patches[p].level;
        }
    }
    memcpy(audio_shadow.name, instrument->name, sizeof audio_shadow.name);
    return result;
}

void fm1lab_audio_set_keyboard_mode(int enabled)
{
    __atomic_store_n(&keyboard_mode, enabled ? 1u : 0u, __ATOMIC_RELEASE);
}

int fm1lab_audio_route_midi(const uint8_t *message, size_t bytes)
{
    if (!message || !bytes || !fm1lab_audio_is_ready() ||
        !__atomic_load_n(&keyboard_mode, __ATOMIC_ACQUIRE)) return 0;
    if (message[0] == 0xffu) {
        (void)fm1lab_audio_engine_all_notes_off(&audio_engine);
        return 0;
    }
    if (message[0] >= 0xf0u) return 0;
    size_t expected = (message[0] & 0xe0u) == 0xc0u ? 2u : 3u;
    if (bytes != expected || message[1] > 127u ||
        (expected == 3u && message[2] > 127u)) return -1;
    return fm1lab_audio_engine_midi(&audio_engine, message, bytes, 0x0fu) == 0 ? 1 : -1;
}

void fm1lab_audio_mix(int32_t *stereo_q23)
{
    if (!fm1lab_audio_is_ready() || !stereo_q23) return;

    int32_t mono_q24[FM1_BLOCK_FRAMES];
    if (fm1lab_audio_engine_render(&audio_engine, mono_q24,
                                   FM1_BLOCK_FRAMES) != 0)
        return;

    for (unsigned frame = 0; frame < FM1_BLOCK_FRAMES; ++frame) {
        int32_t sample = mono_q24[frame] >> 1; /* Q24 FM voice to Q23 device mix */
        stereo_q23[frame * 2u] = limit_q23((int64_t)stereo_q23[frame * 2u] + sample);
        stereo_q23[frame * 2u + 1u] =
            limit_q23((int64_t)stereo_q23[frame * 2u + 1u] + sample);
    }
}
