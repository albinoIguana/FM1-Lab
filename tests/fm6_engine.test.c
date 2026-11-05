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
    assert(fm6_active_voices() == 1u);
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
    assert(fm6_active_voices() == 8u);
    fm6_patch_set_enabled(0, 0u);
    assert(fm6_active_voices() == 0u);
    fm6_panic();
    assert(fm6_active_voices() == 0u);

    /* Full four-layer stress: all four independent pools reach their
     * documented per-patch limit simultaneously (32 live FM voices). */
    fm6_init();
    memcpy(voice, fm6_patch_data(0), 155u);
    for (unsigned patch = 1; patch < FM1LAB_PATCH_COUNT; ++patch) {
        assert(fm6_patch_set_voice(patch, voice) == 0);
        fm6_patch_set_level(patch, 127u);
        fm6_patch_set_enabled(patch, 1u);
    }
    for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch) {
        for (unsigned n = 0; n < FM1LAB_VOICE_LIMIT; ++n)
            fm6_note_on_mask((uint8_t)(40u + patch * 12u + n), 100u,
                             (uint8_t)(1u << patch));
        assert(fm6_active_voices_patch(patch) == FM1LAB_VOICE_LIMIT);
    }
    assert(fm6_active_voices() == FM1LAB_PATCH_COUNT * FM1LAB_VOICE_LIMIT);

    /* Overfilling one layer may steal only inside that layer's own pool. */
    fm6_note_on_mask(100u, 100u, 0x04u);
    fm6_note_on_mask(101u, 100u, 0x04u);
    assert(fm6_active_voices() == FM1LAB_PATCH_COUNT * FM1LAB_VOICE_LIMIT);
    for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch)
        assert(fm6_active_voices_patch(patch) == FM1LAB_VOICE_LIMIT);

    memset(output, 0, sizeof output);
    fm6_render(output, TEST_FRAMES, 4096u);
    assert(peak(output, TEST_WORDS) > 0);
    fm6_panic();
    assert(fm6_active_voices() == 0u);

    /* Sample-clocked two-lane sequencer drives the same FM6 note allocator. */
    fm6_init();
    fm6_set_tempo(120u);
    fm6_sequencer_set_lane(0u,0u);
    fm6_sequencer_set_lane(1u,0u);
    fm6_sequencer_set_step(0u,0u,1u,60u,110u,75u);
    fm6_sequencer_set_step(1u,0u,1u,67u,100u,75u);
    fm6_sequencer_set_running(1u);
    int32_t seq_peak=0;
    /* At 44.1 kHz / 120 BPM, a 16th note is 5512.5 samples.
     * The phase accumulator must not advance early or drift at 48 kHz. */
    for(unsigned block=0;block<43u;block++) {
        memset(output,0,sizeof output);
        fm6_render(output,128u,4096u);
        int32_t p=peak(output,TEST_WORDS); if(p>seq_peak) seq_peak=p;
    }
    assert(fm6_sequencer_step()==0u);
    memset(output,0,sizeof output); fm6_render(output,8u,4096u);
    assert(fm6_sequencer_step()==0u); /* 5512 samples: just before the tick */
    memset(output,0,sizeof output); fm6_render(output,1u,4096u);
    assert(fm6_sequencer_step()==1u); /* sample 5513: first tick */
    assert(fm6_sequencer_running()==1u);
    assert(fm6_active_voices_patch(0u)>=2u); /* both independent lanes reached the same FM6 patch */
    assert(seq_peak>0);
    fm6_sequencer_set_running(0u);
    assert(fm6_sequencer_running()==0u);

    /* Arpeggiator captures held key input, then emits timed FM6 note events. */
    fm6_init();
    fm6_arpeggiator_set_mode(1u);
    fm6_note_on(60u,100u); fm6_note_on(64u,100u); fm6_note_on(67u,100u);
    assert(fm6_active_voices()==0u);
    int32_t arp_peak=0;
    for(unsigned block=0;block<48u;block++) {
        memset(output,0,sizeof output); fm6_render(output,128u,4096u);
        int32_t p=peak(output,TEST_WORDS); if(p>arp_peak) arp_peak=p;
    }
    assert(arp_peak>0); /* prove the generated arp note reaches the FM6 output */
    fm6_note_off(60u); fm6_note_off(64u); fm6_note_off(67u);
    fm6_arpeggiator_set_mode(0u);

    /* Every effect changes the active FM6 output, not just a UI parameter. */
    int32_t dry[64u*128u*2u], wet[64u*128u*2u];
    fm6_init(); fm6_note_on(60u,110u);
    for(unsigned block=0;block<64u;block++)
        fm6_render(dry+block*TEST_WORDS,128u,4096u);
    for(unsigned effect=0;effect<3u;effect++) {
        fm6_init(); fm6_fx_set_enabled((uint8_t)effect,1u);
        fm6_fx_set_mix((uint8_t)effect,100u);
        fm6_note_on(60u,110u);
        for(unsigned block=0;block<64u;block++)
            fm6_render(wet+block*TEST_WORDS,128u,4096u);
        int changed=0,stereo_spread=0;
        for(unsigned i=0;i<64u*TEST_WORDS;i++) if(wet[i]!=dry[i]) { changed=1; break; }
        for(unsigned frame=0;frame<64u*128u;frame++)
            if(wet[frame*2u]!=wet[frame*2u+1u]) { stereo_spread=1; break; }
        assert(changed);
        if(effect==0u) assert(stereo_spread);
    }


    /* Per-patch send controls isolate an instrument from the shared FX bus. */
    fm6_init(); fm6_fx_set_enabled(0u,1u); fm6_fx_set_mix(0u,100u);
    fm6_fx_set_send(0u,0u,0u);
    assert(fm6_fx_get_send(0u,0u)==0u);
    fm6_note_on(60u,110u);
    for(unsigned block=0;block<64u;block++)
        fm6_render(wet+block*TEST_WORDS,128u,4096u);
    fm6_init();
    fm6_note_on(60u,110u);
    for(unsigned block=0;block<64u;block++)
        fm6_render(dry+block*TEST_WORDS,128u,4096u);
    for(unsigned i=0;i<64u*TEST_WORDS;i++) assert(wet[i]==dry[i]);

    puts("Felucca FM6 engine: voice pools, dual-lane sequencing, arp scheduling, FX buses/sends and render passed");
    return 0;
}