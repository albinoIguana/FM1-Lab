#include "fm1lab_audio_bridge.h"

#include <limits.h>
#include <string.h>
#include "fm1.h"
#include "fm1lab_audio_engine.h"
#include "fm1lab_instrument.h"
#include "fm1lab_runtime.h"

enum { FM1LAB_AUDIO_VOICES_PER_PATCH = 4 };

static fm1lab_audio_engine_t audio_engine;
static fm1lab_instrument_t audio_shadow;
static uint8_t live_voice[FM1LAB_AUDIO_PATCHES][FM1LAB_PATCH_BYTES];
static volatile uint32_t audio_ready;
static volatile uint32_t keyboard_mode;

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

int fm1lab_audio_init(void)
{
    if (fm1lab_audio_is_ready()) return 0;

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
    for(unsigned p=0;p<FM1LAB_AUDIO_PATCHES;p++)
        if(fm1lab_audio_engine_set_pitch(&audio_engine,p,
            runtime.instrument.patches[p].octave,
            runtime.instrument.patches[p].fine_cents)!=0){
            fm1lab_audio_engine_destroy(&audio_engine);
            return -1;
        }

    audio_shadow = runtime.instrument;
    memcpy(live_voice, voices, sizeof live_voice);
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
        if (instrument->patches[p].octave != audio_shadow.patches[p].octave ||
            instrument->patches[p].fine_cents != audio_shadow.patches[p].fine_cents) {
            int r = fm1lab_audio_engine_set_pitch(&audio_engine,p,
                instrument->patches[p].octave,instrument->patches[p].fine_cents);
            if (r) result = r;
            else {
                audio_shadow.patches[p].octave = instrument->patches[p].octave;
                audio_shadow.patches[p].fine_cents = instrument->patches[p].fine_cents;
            }
        }
    }
    memcpy(audio_shadow.name, instrument->name, sizeof audio_shadow.name);
    return result;
}

void fm1lab_audio_set_keyboard_mode(int enabled)
{
    audio_barrier();
    keyboard_mode = enabled ? 1u : 0u;
}

int fm1lab_audio_route_midi(const uint8_t *message, size_t bytes)
{
    if (!message || !bytes || !fm1lab_audio_is_ready()) return 0;
    uint32_t mode = keyboard_mode;
    audio_barrier();
    if (!mode) return 0;
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

/* DMA callback contract: stereo is interleaved Q23. This runs after the
 * stock synth render and before fm1_output_mix applies master gain, USB/BT
 * inputs and final 24-bit clipping. Keep this call chain allocation/Flash-free. */
FM1_AUDIO_CODE void fm1lab_audio_mix(int32_t *stereo_q23)
{
    if (!fm1lab_audio_is_ready() || !stereo_q23) return;

    int32_t mono_q24[FM1_BLOCK_FRAMES];
    if (fm1lab_audio_engine_render(&audio_engine, mono_q24,
                                   FM1_BLOCK_FRAMES) != 0)
        return;

    for (unsigned frame = 0; frame < FM1_BLOCK_FRAMES; ++frame) {
        int32_t sample = mono_q24[frame] >> 1;
        stereo_q23[frame * 2u] = add_sat32(stereo_q23[frame * 2u], sample);
        stereo_q23[frame * 2u + 1u] =
            add_sat32(stereo_q23[frame * 2u + 1u], sample);
    }
}
