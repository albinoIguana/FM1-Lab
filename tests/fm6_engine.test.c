#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fm1.h"
#include "fm6.h"

static int32_t output[FM1_BLOCK_WORDS];

static int32_t peak(const int32_t *samples, unsigned count)
{
    int32_t result = 0;
    for (unsigned i = 0; i < count; ++i) {
        int32_t value = samples[i] < 0 ? -samples[i] : samples[i];
        if (value > result) result = value;
    }
    return result;
}

int main(void)
{
    assert(fm6_patch_count() == FM1LAB_PATCH_COUNT);
    fm6_init();
    assert(fm6_patch_enabled(0) == 1u);
    assert(fm6_patch_enabled(1) == 0u);
    assert(fm6_patch_algorithm(0) < 32u);
    assert(fm6_patch_name(0) && fm6_patch_name(0)[0]);

    uint8_t voice[156] = {0};
    const uint8_t *factory = fm6_patch_data(0);
    assert(factory);
    memcpy(voice, factory, 155u);
    assert(fm6_patch_set_voice(1, voice) == 0);
    fm6_patch_set_level(1, 127u);
    fm6_patch_set_enabled(1, 1u);
    assert(fm6_patch_enabled(1) == 1u);

    fm6_note_on_mask(60u, 100u, 0x02u);
    assert(fm6_active_voices() == 1u);
    memset(output, 0, sizeof output);
    fm6_render(output, FM1_BLOCK_FRAMES, 4096u);
    assert(peak(output, FM1_BLOCK_WORDS) > 0);

    uint8_t invalid[156];
    memcpy(invalid, voice, sizeof invalid);
    invalid[0] = 255u;
    assert(fm6_patch_set_voice(1, invalid) == -1);

    fm6_macro_set(1, FM1LAB_MACRO_BRIGHTNESS, 73u);
    assert(fm6_macro_get(1, FM1LAB_MACRO_BRIGHTNESS) == 73u);
    assert(memcmp(fm6_patch_data(1), voice, 155u) == 0);

    fm6_note_on_tuned_mask(64u, 90u, 0x03u, 12);
    assert(fm6_active_voices() == 3u);
    fm6_pitch_bend(16383u);
    memset(output, 0, sizeof output);
    fm6_render(output, FM1_BLOCK_FRAMES, 4096u);
    assert(peak(output, FM1_BLOCK_WORDS) > 0);

    fm6_patch_set_level(1, 0u);
    assert(fm6_active_voices() == 1u);
    fm6_patch_set_enabled(0, 0u);
    assert(fm6_active_voices() == 0u);
    fm6_panic();
    assert(fm6_active_voices() == 0u);

    puts("Felucca FM6 engine: patch loading, macros, layer masks, tuning, bend and render passed");
    return 0;
}
