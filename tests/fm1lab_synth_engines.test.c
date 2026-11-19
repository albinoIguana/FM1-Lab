#include "fm1lab_synth_engines.h"
#include "fm1lab_instrument_engine_map.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FRAMES 512u
static int16_t buffers[FM1LAB_SYNTH_ENGINE_COUNT][FRAMES];

static uint32_t checksum(const int16_t *p, uint32_t n)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < n; ++i) h = (h ^ (uint16_t)p[i]) * 16777619u;
    return h;
}

int main(void)
{
    static const fm1lab_patch_engine_t expected[FM1LAB_INSTRUMENT_SLOT_COUNT] = {
        FM1LAB_PATCH_ENGINE_FM6,
        FM1LAB_PATCH_ENGINE_WAVETABLE,
        FM1LAB_PATCH_ENGINE_PSG,
        FM1LAB_PATCH_ENGINE_SUBTRACTIVE
    };
    for (uint8_t slot = 0; slot < FM1LAB_INSTRUMENT_SLOT_COUNT; ++slot) {
        assert(fm1lab_patch_engine_for_context(slot) == expected[slot]);
    }
    assert(fm1lab_patch_engine_for_context(4u) == FM1LAB_PATCH_ENGINE_INVALID);
    assert(fm1lab_patch_engine_for_context(8u) == FM1LAB_PATCH_ENGINE_INVALID);
    uint32_t hashes[FM1LAB_SYNTH_ENGINE_COUNT];
    for (uint32_t engine = 0; engine < FM1LAB_SYNTH_ENGINE_COUNT; ++engine) {
        fm1lab_synth_voice_t voice, twin;
        fm1lab_synth_init(&voice, (fm1lab_synth_engine_t)engine);
        fm1lab_synth_init(&twin, (fm1lab_synth_engine_t)engine);
        memset(buffers[engine], 0x55, sizeof buffers[engine]);
        assert(fm1lab_synth_render(&voice, buffers[engine], FRAMES) == 0);
        for (uint32_t i = 0; i < FRAMES; ++i) assert(buffers[engine][i] == 0);
        fm1lab_synth_note_on(&voice, 60u, 127u);
        fm1lab_synth_note_on(&twin, 60u, 127u);
        assert(fm1lab_synth_render(&voice, buffers[engine], FRAMES) == 0);
        int nonzero = 0;
        for (uint32_t i = 0; i < FRAMES; ++i) {
            if (buffers[engine][i]) nonzero = 1;
            assert(buffers[engine][i] <= 32760 && buffers[engine][i] >= -32760);
        }
        assert(nonzero);
        hashes[engine] = checksum(buffers[engine], FRAMES);
        int16_t twin_buffer[FRAMES];
        assert(fm1lab_synth_render(&twin, twin_buffer, FRAMES) == 0);
        assert(memcmp(buffers[engine], twin_buffer, sizeof twin_buffer) == 0);

        fm1lab_synth_note_off(&voice);
        while(voice.active) assert(fm1lab_synth_render(&voice,buffers[engine],FRAMES)==0);
        assert(voice.envelope_q15==0u);
        assert(fm1lab_synth_render(&voice, buffers[engine], 64u) == 0);
        for (uint32_t i = 0; i < 64u; ++i) assert(buffers[engine][i] == 0);
    }
    assert(hashes[0] != hashes[1]);
    assert(hashes[0] != hashes[2]);
    assert(hashes[1] != hashes[2]);

    fm1lab_synth_voice_t pitch;
    fm1lab_synth_init(&pitch, FM1LAB_SYNTH_WAVETABLE);
    fm1lab_synth_note_on(&pitch, 60u, 127u);
    uint32_t c4 = pitch.phase_inc;
    fm1lab_synth_note_on(&pitch, 72u, 127u);
    assert(pitch.phase_inc >= c4 * 2u - 2u && pitch.phase_inc <= c4 * 2u + 2u);

    /* Engine-native patch edits must change the actual rendered waveform. */
    for (uint32_t engine=0; engine<FM1LAB_SYNTH_ENGINE_COUNT; ++engine) {
        fm1lab_synth_voice_t a,b;
        fm1lab_synth_patch_t patch;
        int16_t first[FRAMES], second[FRAMES];
        fm1lab_synth_init(&a,(fm1lab_synth_engine_t)engine);
        fm1lab_synth_init(&b,(fm1lab_synth_engine_t)engine);
        fm1lab_synth_note_on(&a,48u,120u);
        fm1lab_synth_note_on(&b,48u,120u);
        assert(fm1lab_synth_render(&a,first,FRAMES)==0);
        fm1lab_synth_patch_default((fm1lab_synth_engine_t)engine,&patch);
        if(engine==FM1LAB_SYNTH_WAVETABLE) {
            patch.waveform=2u;
            patch.secondary_waveform=3u;
            patch.osc_mix=75u;
        } else if(engine==FM1LAB_SYNTH_PSG) { patch.noise_mix=90u; patch.osc_mix=65u; patch.detune=90u; }
        else { patch.cutoff=95u; patch.detune=80u; patch.noise_mix=85u; }
        assert(fm1lab_synth_set_patch(&b,&patch)==0);
        /* Apply the new recipe at the next note boundary. */
        fm1lab_synth_note_on(&b,48u,120u);
        assert(fm1lab_synth_render(&b,second,FRAMES)==0);
        assert(checksum(first,FRAMES)!=checksum(second,FRAMES));
        patch.cutoff=101u;
        assert(fm1lab_synth_set_patch(&b,&patch)==-1);
    }

    fm1lab_synth_voice_t psg_a,psg_b; fm1lab_synth_patch_t psg_patch;
    int16_t psg_first[FRAMES],psg_second[FRAMES];
    fm1lab_synth_init(&psg_a,FM1LAB_SYNTH_PSG);fm1lab_synth_init(&psg_b,FM1LAB_SYNTH_PSG);
    fm1lab_synth_patch_default(FM1LAB_SYNTH_PSG,&psg_patch);
    psg_patch.noise_mix=70u;psg_patch.detune=0u;
    assert(fm1lab_synth_set_patch(&psg_a,&psg_patch)==0);
    psg_patch.detune=100u;assert(fm1lab_synth_set_patch(&psg_b,&psg_patch)==0);
    fm1lab_synth_note_on(&psg_a,48u,127u);fm1lab_synth_note_on(&psg_b,48u,127u);
    assert(fm1lab_synth_render(&psg_a,psg_first,FRAMES)==0);
    assert(fm1lab_synth_render(&psg_b,psg_second,FRAMES)==0);
    assert(checksum(psg_first,FRAMES)!=checksum(psg_second,FRAMES));

    fm1lab_synth_voice_t sub_a,sub_b; fm1lab_synth_patch_t sub_patch;
    int16_t sub_first[FRAMES],sub_second[FRAMES];
    fm1lab_synth_init(&sub_a,FM1LAB_SYNTH_SUBTRACTIVE);fm1lab_synth_init(&sub_b,FM1LAB_SYNTH_SUBTRACTIVE);
    fm1lab_synth_patch_default(FM1LAB_SYNTH_SUBTRACTIVE,&sub_patch);
    sub_patch.waveform=2u;sub_patch.secondary_waveform=3u;sub_patch.cutoff=15u;sub_patch.noise_mix=0u;
    assert(fm1lab_synth_set_patch(&sub_a,&sub_patch)==0);
    sub_patch.noise_mix=100u;assert(fm1lab_synth_set_patch(&sub_b,&sub_patch)==0);
    fm1lab_synth_note_on(&sub_a,48u,127u);fm1lab_synth_note_on(&sub_b,48u,127u);
    assert(fm1lab_synth_render(&sub_a,sub_first,FRAMES)==0);
    assert(fm1lab_synth_render(&sub_b,sub_second,FRAMES)==0);
    assert(checksum(sub_first,FRAMES)!=checksum(sub_second,FRAMES));

    fm1lab_synth_voice_t adsr; fm1lab_synth_patch_t adsr_patch; int16_t adsr_buf[FRAMES];
    fm1lab_synth_init(&adsr,FM1LAB_SYNTH_SUBTRACTIVE);
    fm1lab_synth_patch_default(FM1LAB_SYNTH_SUBTRACTIVE,&adsr_patch);
    adsr_patch.attack=0u;adsr_patch.sustain=40u;adsr_patch.release=0u;
    assert(fm1lab_synth_set_patch(&adsr,&adsr_patch)==0);
    fm1lab_synth_note_on(&adsr,60u,127u);
    for(uint32_t i=0;i<10u;i++)assert(fm1lab_synth_render(&adsr,adsr_buf,FRAMES)==0);
    assert(adsr.envelope_stage==2u && adsr.envelope_q15>=13000u && adsr.envelope_q15<=13200u);
    fm1lab_synth_note_off(&adsr);
    while(adsr.active)assert(fm1lab_synth_render(&adsr,adsr_buf,FRAMES)==0);
    assert(adsr.envelope_q15==0u);

    /* Wavetable morph must use both selected tables and the morph amount. */
    fm1lab_synth_voice_t morph_a, morph_b;
    fm1lab_synth_patch_t morph_patch;
    int16_t morph_first[FRAMES], morph_second[FRAMES];
    fm1lab_synth_init(&morph_a, FM1LAB_SYNTH_WAVETABLE);
    fm1lab_synth_init(&morph_b, FM1LAB_SYNTH_WAVETABLE);
    fm1lab_synth_patch_default(FM1LAB_SYNTH_WAVETABLE, &morph_patch);
    morph_patch.waveform=0u; morph_patch.secondary_waveform=2u; morph_patch.osc_mix=0u;
    assert(fm1lab_synth_set_patch(&morph_a,&morph_patch)==0);
    morph_patch.osc_mix=100u;
    assert(fm1lab_synth_set_patch(&morph_b,&morph_patch)==0);
    fm1lab_synth_note_on(&morph_a,48u,120u);
    fm1lab_synth_note_on(&morph_b,48u,120u);
    assert(fm1lab_synth_render(&morph_a,morph_first,FRAMES)==0);
    assert(fm1lab_synth_render(&morph_b,morph_second,FRAMES)==0);
    assert(checksum(morph_first,FRAMES)!=checksum(morph_second,FRAMES));

    /* Continuous pitch updates must not reset either oscillator phase. */
    for (uint32_t engine = 0; engine < FM1LAB_SYNTH_ENGINE_COUNT; ++engine) {
        fm1lab_synth_init(&pitch, (fm1lab_synth_engine_t)engine);
        fm1lab_synth_note_on(&pitch, 60u, 127u);
        assert(fm1lab_synth_render(&pitch, buffers[engine], 37u) == 0);
        uint32_t phase_a = pitch.phase, phase_b = pitch.phase_b;
        uint32_t old_inc = pitch.phase_inc;
        assert(fm1lab_synth_set_pitch(&pitch, 72 * 256) == 0);
        assert(pitch.phase == phase_a && pitch.phase_b == phase_b);
        assert(pitch.phase_inc >= old_inc * 2u - 2u && pitch.phase_inc <= old_inc * 2u + 2u);
        assert(pitch.phase_inc_b >= pitch.phase_inc);
        uint32_t bent_inc = pitch.phase_inc;
        assert(fm1lab_synth_set_pitch(&pitch, 72 * 256 + 128) == 0);
        assert(pitch.phase_inc > bent_inc);
    }

    fm1lab_synth_init(&pitch, FM1LAB_SYNTH_PSG);
    fm1lab_synth_note_on(&pitch, 60u, 0u);
    assert(pitch.active == 0u);
    fm1lab_synth_init(&pitch, (fm1lab_synth_engine_t)255u);
    assert(fm1lab_synth_render(&pitch, buffers[0], 1u) == -1);
    assert(fm1lab_synth_voice_bytes() == sizeof(fm1lab_synth_voice_t));
    puts("FM1-Lab mixed-synthesis prototypes: PASS");
    return 0;
}
