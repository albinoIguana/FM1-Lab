#include "fm1lab_fm4.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FRAMES 512u
static int16_t output[4][FRAMES];

static uint32_t hash(const int16_t *p)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < FRAMES; ++i) h = (h ^ (uint16_t)p[i]) * 16777619u;
    return h;
}

int main(void)
{
    uint32_t hashes[4];
    for (uint32_t algorithm = 0; algorithm < 4u; ++algorithm) {
        fm1lab_fm4_voice_t voice, twin;
        fm1lab_fm4_init(&voice);
        fm1lab_fm4_init(&twin);
        assert(fm1lab_fm4_render(&voice, output[algorithm], FRAMES) == 0);
        for (uint32_t i = 0; i < FRAMES; ++i) assert(output[algorithm][i] == 0);
        fm1lab_fm4_note_on(&voice, 60u, 127u, (uint8_t)algorithm);
        fm1lab_fm4_note_on(&twin, 60u, 127u, (uint8_t)algorithm);
        assert(fm1lab_fm4_render(&voice, output[algorithm], FRAMES) == 0);
        int nonzero = 0;
        for (uint32_t i = 0; i < FRAMES; ++i) {
            if (output[algorithm][i]) nonzero = 1;
            assert(output[algorithm][i] <= 32760 && output[algorithm][i] >= -32760);
        }
        assert(nonzero);
        hashes[algorithm] = hash(output[algorithm]);
        int16_t twin_output[FRAMES];
        assert(fm1lab_fm4_render(&twin, twin_output, FRAMES) == 0);
        assert(memcmp(output[algorithm], twin_output, sizeof twin_output) == 0);
        fm1lab_fm4_note_off(&voice);
        assert(fm1lab_fm4_render(&voice, output[algorithm], 256u) == 0);
        assert(voice.active == 0u);
    }
    for (uint32_t i = 0; i < 4u; ++i) {
        for (uint32_t j = i + 1u; j < 4u; ++j) assert(hashes[i] != hashes[j]);
    }
    assert(fm1lab_fm4_voice_bytes() == 44u);
    puts("FM1-Lab four-operator FM prototype: PASS");
    return 0;
}
