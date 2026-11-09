#include "fm1lab_audio_engine.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void make_voice(uint8_t voice[FM1LAB_PATCH_BYTES], unsigned algorithm)
{
    memset(voice, 0, FM1LAB_PATCH_BYTES);
    for (unsigned op = 0; op < 6; ++op) {
        unsigned base = op * 21u;
        for (unsigned i = 0; i < 4; ++i) {
            voice[base + i] = 99;       /* fast operator rates */
            voice[base + 4u + i] = 99;  /* full envelope levels */
        }
        voice[base + 16u] = 90;         /* operator output */
        voice[base + 18u] = 1;          /* coarse ratio */
    }
    voice[134] = (uint8_t)algorithm;
    voice[135] = 2;                     /* feedback */
    voice[136] = 1;
    voice[137] = 35;
    voice[138] = 0;
    voice[139] = 0;
    voice[140] = 0;
    voice[141] = 1;
    voice[142] = 0;
    voice[143] = 0;
    voice[144] = 0;
    voice[145] = 24;                    /* transpose */
}

static int peak(const int32_t *samples, unsigned count)
{
    int value = 0;
    for (unsigned i = 0; i < count; ++i) {
        int x = samples[i] < 0 ? -samples[i] : samples[i];
        if (x > value) value = x;
    }
    return value;
}

int main(void)
{
    uint8_t voices[FM1LAB_AUDIO_PATCHES][FM1LAB_PATCH_BYTES];
    uint8_t enabled[FM1LAB_AUDIO_PATCHES] = {1, 1, 1, 1};
    uint8_t levels[FM1LAB_AUDIO_PATCHES] = {127, 127, 127, 127};
    fm1lab_audio_engine_t engine;
    int32_t output[FM1_BLOCK_FRAMES];

    for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p)
        make_voice(voices[p], p * 7u);

    assert(fm1lab_audio_engine_init(&engine, voices, enabled, levels, 4,
                                    FM1_SAMPLE_RATE) == 0);
    assert(fm1lab_audio_engine_set_pitch(&engine,0u,1,25)==0);
    assert(fm1lab_audio_engine_set_pitch(&engine,1u,-1,25)==0);
    assert(fm1lab_audio_engine_set_pitch(&engine,2u,5,0)==-1);
    assert(fm1lab_audio_engine_set_pitch(&engine,2u,0,101)==-1);
    assert(fm1lab_audio_engine_note_on(&engine, 60, 120) == 0);
    assert(fm1lab_audio_engine_render(&engine, output, FM1_BLOCK_FRAMES) == 0);
    assert(peak(output, FM1_BLOCK_FRAMES) > 0);
    for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p)
        assert(fm1lab_audio_engine_active_voices(&engine, p) == 1);
    assert(engine.octave[0]==1 && engine.fine_cents[0]==25);
    assert(engine.octave[1]==-1 && engine.fine_cents[1]==25);
    /* Changing one patch's tuning releases only that patch's notes. */
    assert(fm1lab_audio_engine_set_pitch(&engine,2u,-1,-50)==0);
    assert(fm1lab_audio_engine_render(&engine,output,FM1_BLOCK_FRAMES)==0);
    assert(engine.octave[2]==-1 && engine.fine_cents[2]==-50);
    assert(fm1lab_audio_engine_active_voices(&engine,2u)==0);
    assert(fm1lab_audio_engine_active_voices(&engine,1u)==1);

    /* The sequencer can address one patch without triggering the other layers. */
    assert(fm1lab_audio_engine_note_on_mask(&engine, 64, 100, 0x02u) == 0);
    assert(fm1lab_audio_engine_render(&engine, output, FM1_BLOCK_FRAMES) == 0);
    assert(fm1lab_audio_engine_active_voices(&engine, 1) == 2);
    assert(fm1lab_audio_engine_active_voices(&engine, 0) == 1);
    assert(fm1lab_audio_engine_active_voices(&engine, 2) == 0);

    /* Disabling a layer is immediate and cannot leave a stuck note behind. */
    assert(fm1lab_audio_engine_set_enabled(&engine, 1, 0) == 0);
    assert(fm1lab_audio_engine_render(&engine, output, FM1_BLOCK_FRAMES) == 0);
    assert(fm1lab_audio_engine_active_voices(&engine, 1) == 0);

    /* Per-patch level zero removes that layer from the mix. */
    assert(fm1lab_audio_engine_set_level(&engine, 2, 0) == 0);
    assert(fm1lab_audio_engine_render(&engine, output, FM1_BLOCK_FRAMES) == 0);
    assert(engine.level[2] == 0);
    assert(fm1lab_audio_engine_active_voices(&engine, 2) == 0);

    /* A patch edit resets only its own voice pool and changes its live voice. */
    make_voice(voices[3], 31);
    assert(fm1lab_audio_engine_set_patch(&engine, 3, voices[3]) == 0);
    assert(fm1lab_audio_engine_render(&engine, output, FM1_BLOCK_FRAMES) == 0);
    assert(fm1lab_audio_engine_active_voices(&engine, 3) == 0);
    assert(engine.voice[3][134] == 31);

    /* General MIDI messages (including controllers and pitch bend) share the same patch pools. */
    const uint8_t midi_note[3] = {0x90u, 67u, 100u};
    const uint8_t midi_bend[3] = {0xe0u, 0u, 96u};
    const uint8_t midi_mod[3] = {0xb0u, 1u, 72u};
    assert(fm1lab_audio_engine_midi(&engine, midi_note, 3, 0x01u) == 0);
    assert(fm1lab_audio_engine_midi(&engine, midi_bend, 3, 0x01u) == 0);
    assert(fm1lab_audio_engine_midi(&engine, midi_mod, 3, 0x01u) == 0);
    assert(fm1lab_audio_engine_render(&engine, output, FM1_BLOCK_FRAMES) == 0);
    assert(fm1lab_audio_engine_active_voices(&engine, 0) == 2);

    /* A full command queue must not lose note-offs silently: overflow schedules
       an audio-thread panic and drops stale queued note-ons. */
    for (unsigned i = 0; i < FM1LAB_AUDIO_QUEUE_CAPACITY - 1u; ++i)
        assert(fm1lab_audio_engine_note_on(&engine, (uint8_t)(48u + i), 90) == 0);
    assert(fm1lab_audio_engine_note_on(&engine, 80, 90) == -2);
    assert(fm1lab_audio_engine_note_off(&engine, 60) == -2);
    assert(fm1lab_audio_engine_render(&engine, output, FM1_BLOCK_FRAMES) == 0);
    for (unsigned p = 0; p < FM1LAB_AUDIO_PATCHES; ++p)
        assert(fm1lab_audio_engine_active_voices(&engine, p) == 0);

    assert(fm1lab_audio_engine_error_count(&engine) == 0);
    fm1lab_audio_engine_destroy(&engine);
    puts("FM1-Lab four-layer FM6 audio engine tests passed.");
    return 0;
}
