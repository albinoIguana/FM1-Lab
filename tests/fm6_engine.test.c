#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fm6.h"
enum { TEST_FRAMES = 128, TEST_WORDS = TEST_FRAMES * 2 };
static int32_t output[TEST_WORDS];
static int32_t peak(const int32_t *samples, unsigned count) {
    int32_t result = 0;
    for (unsigned i = 0; i < count; ++i) {
        int32_t value = samples[i] < 0 ? -samples[i] : samples[i];
        if (value > result) result = value;
    }
    return result;
}
int main(void) {
    assert(fm6_patch_count() == FM1LAB_PATCH_COUNT);
    fm6_init();
    assert(fm6_patch_enabled(0) == 1u && fm6_patch_enabled(1) == 0u);
    assert(fm6_patch_algorithm(0) < 32u && fm6_patch_name(0)[0]);
    uint8_t voice[156] = {0};
    memcpy(voice, fm6_patch_data(0), 155u);
    assert(fm6_patch_set_voice(1, voice) == 0);
    fm6_patch_set_level(1, 127u);
    fm6_patch_set_enabled(1, 1u);
    fm6_note_on_mask(60u, 100u, 0x02u);
    assert(fm6_active_voices() == 8u);
    memset(output, 0, sizeof output);
    fm6_render(output, TEST_FRAMES, 4096u);
    assert(peak(output, TEST_WORDS) > 0);
    uint8_t invalid[156];
    memcpy(invalid, voice, sizeof invalid);
    invalid[0] = 255u;
    assert(fm6_patch_set_voice(1, invalid) == -1);
    fm6_macro_set(1, FM1LAB_MACRO_BRIGHTNESS, 73u);
    assert(fm6_macro_get(1, FM1LAB_MACRO_BRIGHTNESS) == 73u);
    assert(memcmp(fm6_patch_data(1), voice, 155u) == 0);
    /* Fill one patch pool while preserving the other patch's live voice. */
    fm6_note_on_mask(48u, 80u, 0x02u);
    for (unsigned n = 0; n < FM1LAB_VOICE_LIMIT; ++n)
        fm6_note_on_mask((uint8_t)(36u + n), 90u, 0x01u);
    assert(fm6_active_voices() == 10u);
    fm6_note_on_mask(72u, 100u, 0x01u); /* steals only within patch 0 */
    assert(fm6_active_voices() == 10u);
    fm6_note_on_tuned_mask(64u, 90u, 0x03u, 12);
    assert(fm6_active_voices() == 11u);
    fm6_pitch_bend(16383u);
    memset(output, 0, sizeof output);
    fm6_render(output, TEST_FRAMES, 4096u);
    assert(peak(output, TEST_WORDS) > 0);
    fm6_patch_set_level(1, 0u);
    assert(fm6_active_voices() == 1u);
    fm6_patch_set_enabled(0, 0u);
    assert(fm6_active_voices() == 0u);
    fm6_panic();
    assert(fm6_active_voices() == 0u);
    puts("Felucca FM6 engine: independent patch voice pools, patch loading, macros, layer masks, tuning, bend and render passed");
    return 0;
}