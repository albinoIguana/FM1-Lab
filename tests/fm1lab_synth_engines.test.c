#include "fm1lab_synth_engines.h"
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
    uint32_t hashes[FM1LAB_SYNTH_ENGINE_COUNT];
    for (uint32_t engine = 0; engine < FM1LAB_SYNTH_ENGINE_COUNT; ++engine) {
        fm1lab_synth_voice_t voice;
        fm1lab_synth_init(&voice, (fm1lab_synth_engine_t)engine);
        memset(buffers[engine], 0x55, sizeof buffers[engine]);
        assert(fm1lab_synth_render(&voice, buffers[engine], FRAMES) == 0);
        for (uint32_t i = 0; i < FRAMES; ++i) assert(buffers[engine][i] == 0);
        fm1lab_synth_note_on(&voice, 60u, 127u);
        assert(fm1lab_synth_render(&voice, buffers[engine], FRAMES) == 0);
        int nonzero = 0;
        for (uint32_t i = 0; i < FRAMES; ++i) {
            if (buffers[engine][i]) nonzero = 1;
            assert(buffers[engine][i] <= 32760 && buffers[engine][i] >= -32760);
        }
        assert(nonzero);
        hashes[engine] = checksum(buffers[engine], FRAMES);
        fm1lab_synth_note_off(&voice);
        assert(fm1lab_synth_render(&voice, buffers[engine], 256u) == 0);
        assert(voice.active == 0u);
        assert(fm1lab_synth_render(&voice, buffers[engine], 64u) == 0);
        for (uint32_t i = 0; i < 64u; ++i) assert(buffers[engine][i] == 0);
    }
    assert(hashes[0] != hashes[1]);
    assert(hashes[0] != hashes[2]);
    assert(hashes[1] != hashes[2]);
    assert(fm1lab_synth_voice_bytes() <= 36u);
    puts("FM1-Lab mixed-synthesis prototypes: PASS");
    return 0;
}
