#include "fm1lab_synth_engines.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define VOICES 8u
#define FRAMES 256u
#define BLOCKS 12000u

static fm1lab_synth_voice_t voices[FM1LAB_SYNTH_ENGINE_COUNT][VOICES];
static int16_t output[VOICES][FRAMES];
static volatile uint64_t checksum_sink;

int main(void)
{
    for (uint32_t engine = 0; engine < FM1LAB_SYNTH_ENGINE_COUNT; ++engine) {
        for (uint32_t voice = 0; voice < VOICES; ++voice) {
            fm1lab_synth_init(&voices[engine][voice], (fm1lab_synth_engine_t)engine);
            fm1lab_synth_note_on(&voices[engine][voice], (uint8_t)(48u + voice * 3u), 100u);
        }
        clock_t start = clock();
        for (uint32_t block = 0; block < BLOCKS; ++block) {
            for (uint32_t voice = 0; voice < VOICES; ++voice) {
                fm1lab_synth_render(&voices[engine][voice], output[voice], FRAMES);
                checksum_sink += (uint16_t)output[voice][block % FRAMES];
            }
        }
        clock_t elapsed = clock() - start;
        double samples = (double)BLOCKS * VOICES * FRAMES;
        double seconds = (double)elapsed / (double)CLOCKS_PER_SEC;
        printf("engine=%u voices=%u frames=%u blocks=%u ns/sample=%.2f Msamples/s=%.2f checksum=%llu\n",
               engine, VOICES, FRAMES, BLOCKS,
               seconds > 0.0 ? seconds * 1.0e9 / samples : 0.0,
               seconds > 0.0 ? samples / seconds / 1.0e6 : 0.0,
               (unsigned long long)checksum_sink);
    }
    return 0;
}
