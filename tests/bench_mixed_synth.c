#define _POSIX_C_SOURCE 200809L
#include "fm1lab_synth_engines.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define VOICES 8u
#define FRAMES 256u
#define BLOCKS 12000u
#define TRIALS 5u

static fm1lab_synth_voice_t voices[FM1LAB_SYNTH_ENGINE_COUNT][VOICES];
static int16_t output[VOICES][FRAMES];
static volatile uint64_t checksum_sink;

static uint64_t monotonic_ns(void)
{
    struct timespec t;
    (void)clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
}

static void sort_u64(uint64_t *values, uint32_t count)
{
    for (uint32_t i = 1; i < count; ++i) {
        uint64_t value = values[i];
        uint32_t j = i;
        while (j && values[j - 1u] > value) {
            values[j] = values[j - 1u];
            --j;
        }
        values[j] = value;
    }
}

int main(void)
{
    for (uint32_t engine = 0; engine < FM1LAB_SYNTH_ENGINE_COUNT; ++engine) {
        uint64_t elapsed[TRIALS];
        for (uint32_t trial = 0; trial < TRIALS; ++trial) {
            for (uint32_t voice = 0; voice < VOICES; ++voice) {
                fm1lab_synth_init(&voices[engine][voice], (fm1lab_synth_engine_t)engine);
                fm1lab_synth_note_on(&voices[engine][voice], (uint8_t)(48u + voice * 3u), 100u);
            }
            uint64_t start = monotonic_ns();
            for (uint32_t block = 0; block < BLOCKS; ++block) {
                for (uint32_t voice = 0; voice < VOICES; ++voice) {
                    (void)fm1lab_synth_render(&voices[engine][voice], output[voice], FRAMES);
                    checksum_sink += (uint16_t)output[voice][block % FRAMES];
                }
            }
            elapsed[trial] = monotonic_ns() - start;
        }
        sort_u64(elapsed, TRIALS);
        double samples = (double)BLOCKS * VOICES * FRAMES;
        double ns_per_sample = (double)elapsed[TRIALS / 2u] / samples;
        printf("engine=%u voices=%u frames=%u blocks/trial=%u trials=%u median_ns/sample=%.2f Msamples/s=%.2f checksum=%llu\n",
               engine, VOICES, FRAMES, BLOCKS, TRIALS, ns_per_sample,
               1000.0 / ns_per_sample, (unsigned long long)checksum_sink);
    }
    return 0;
}
