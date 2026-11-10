#include "fm6.h"
#include "fm1lab_instrument.h"
#include "fm1lab_dx7_data.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { BLOCK_FRAMES = 256u, BLOCK_COUNT = 172u, SAMPLE_WORDS = BLOCK_FRAMES * 2u };

typedef struct {
    char name[11];
    uint64_t peak;
    uint64_t mean_abs;
    uint64_t pre_limiter_peak;
    uint64_t output_peak;
    uint32_t limiter_events;
} result_t;

static const char z85_alphabet[] =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-:+=^!/*?&<>()[]{}@%$#";

static int decode_voice(unsigned index, uint8_t out[FM1LAB_PATCH_BYTES])
{
    if (index >= FM1LAB_DX7_VOICE_COUNT || !out) return -1;
    const char *source = fm1lab_dx7_voice_z85 + index * FM1LAB_DX7_Z85_BYTES;
    for (unsigned group = 0; group < 39u; ++group) {
        uint64_t value = 0u;
        for (unsigned digit = 0; digit < 5u; ++digit) {
            const char *at = strchr(z85_alphabet, source[group * 5u + digit]);
            if (!at) return -1;
            value = value * 85u + (uint64_t)(at - z85_alphabet);
        }
        if (value > UINT32_MAX) return -1;
        out[group * 4u] = (uint8_t)(value >> 24);
        out[group * 4u + 1u] = (uint8_t)(value >> 16);
        out[group * 4u + 2u] = (uint8_t)(value >> 8);
        out[group * 4u + 3u] = (uint8_t)value;
    }
    return 0;
}

static uint64_t abs_sample(int32_t sample)
{
    return sample < 0 ? (uint64_t)(-(int64_t)sample) : (uint64_t)sample;
}

int main(void)
{
    result_t results[FM1LAB_DX7_VOICE_COUNT];
    int32_t output[SAMPLE_WORDS];
    uint64_t min_peak = UINT64_MAX, max_peak = 0u;
    uint64_t min_mean = UINT64_MAX, max_mean = 0u;
    unsigned quiet = 0u, silent = 0u, limited = 0u;

    for (unsigned index = 0; index < FM1LAB_DX7_VOICE_COUNT; ++index) {
        uint8_t voice[FM1LAB_PATCH_BYTES];
        uint64_t peak = 0u, sum_abs = 0u, pre_limiter_peak = 0u;
        char name[11];

        if (decode_voice(index, voice) != 0) {
            fprintf(stderr, "DX7 voice %u: Z85 decode failed\n", index);
            return 1;
        }
        for (unsigned byte = 0; byte < FM1LAB_PATCH_BYTES; ++byte) {
            if (voice[byte] > 127u) {
                fprintf(stderr, "DX7 voice %u: non-7-bit byte %u=%u\n",
                        index, byte, voice[byte]);
                return 1;
            }
        }
        memcpy(name, voice + 145u, 10u);
        name[10] = '\0';

        fm6_init();
        for (unsigned patch = 1u; patch < FM6_ENGINE_PATCH_COUNT; ++patch)
            fm6_patch_set_enabled(patch, 0u);
        if (fm6_patch_set_voice(0u, voice) != 0) {
            fprintf(stderr, "DX7 voice %u (%s): FM6 rejected voice\n", index, name);
            return 1;
        }
        fm6_patch_set_enabled(0u, 1u);
        fm6_patch_set_level(0u, 31u);
        fm6_render(output, BLOCK_FRAMES, 4096u); /* settle the master-gain ramp */
        fm6_note_on_mask(60u, 110u, 0x01u);

        for (unsigned block = 0; block < BLOCK_COUNT; ++block) {
            fm6_render(output, BLOCK_FRAMES, 4096u);
            if (fm6_test_patch_peak(0u) > pre_limiter_peak)
                pre_limiter_peak = fm6_test_patch_peak(0u);
            for (unsigned sample = 0; sample < SAMPLE_WORDS; ++sample) {
                uint64_t magnitude = abs_sample(output[sample]);
                if (magnitude > peak) peak = magnitude;
                sum_abs += magnitude;
            }
        }

        results[index].peak = peak;
        results[index].mean_abs = sum_abs / ((uint64_t)BLOCK_COUNT * SAMPLE_WORDS);
        results[index].pre_limiter_peak = pre_limiter_peak;
        results[index].output_peak = fm6_test_output_peak();
        results[index].limiter_events = fm6_test_limiter_events();
        memcpy(results[index].name, name, sizeof name);
        if (peak < min_peak) min_peak = peak;
        if (peak > max_peak) max_peak = peak;
        if (results[index].mean_abs < min_mean) min_mean = results[index].mean_abs;
        if (results[index].mean_abs > max_mean) max_mean = results[index].mean_abs;
        if (peak < 100000u) ++quiet;
        if (peak < 1000u) ++silent;
        if (results[index].limiter_events) ++limited;
    }

    printf("DX7 library audio audit (one patch, level 31, note 60, 1 s):\n");
    printf("  peak Q23 min/max: %" PRIu64 " / %" PRIu64 "\n", min_peak, max_peak);
    printf("  mean-absolute Q23 min/max: %" PRIu64 " / %" PRIu64 "\n", min_mean, max_mean);
    printf("  low-peak voices (<100000): %u/%u; silent (<1000): %u/%u\n",
           quiet, FM1LAB_DX7_VOICE_COUNT, silent, FM1LAB_DX7_VOICE_COUNT);
    for (unsigned shown = 0; shown < 12u; ++shown) {
        unsigned best = FM1LAB_DX7_VOICE_COUNT;
        for (unsigned i = 0; i < FM1LAB_DX7_VOICE_COUNT; ++i) {
            if (results[i].peak == UINT64_MAX) continue;
            if (best == FM1LAB_DX7_VOICE_COUNT ||
                results[i].peak < results[best].peak) best = i;
        }
        if (best == FM1LAB_DX7_VOICE_COUNT) break;
        printf("  #%03u %-10.10s peak=%" PRIu64 " mean=%" PRIu64 "\n",
               best, results[best].name, results[best].peak, results[best].mean_abs);
        results[best].peak = UINT64_MAX;
    }
    for (unsigned shown = 0; shown < 12u; ++shown) {
        unsigned best = FM1LAB_DX7_VOICE_COUNT;
        for (unsigned i = 0; i < FM1LAB_DX7_VOICE_COUNT; ++i) {
            if (results[i].peak == UINT64_MAX) continue;
            if (best == FM1LAB_DX7_VOICE_COUNT ||
                results[i].peak > results[best].peak) best = i;
        }
        if (best == FM1LAB_DX7_VOICE_COUNT) break;
        printf("  loud #%03u %-10.10s pre=%" PRIu64 " out=%" PRIu64 " lim=%" PRIu32 "\\n",
               best, results[best].name, results[best].pre_limiter_peak,
               results[best].output_peak, results[best].limiter_events);
        results[best].peak = UINT64_MAX;
    }
    return 0;
}
