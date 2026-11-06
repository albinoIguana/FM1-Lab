#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fm6.h"
enum { TEST_FRAMES = 128, TEST_WORDS = TEST_FRAMES * 2 };
static int32_t output[TEST_WORDS];
static int32_t chunk_a[4096u * 2u], chunk_b[4096u * 2u];
static int32_t peak(const int32_t *samples, unsigned count) {
    int32_t result = 0;
    for (unsigned i = 0; i < count; ++i) {
        int32_t value = samples[i] < 0 ? -samples[i] : samples[i];
        if (value > result) result = value;
    }
    return result;
}
static void render_phaser_test(uint8_t rate, uint8_t depth, int32_t *destination)
{
    fm6_init();
    assert(fm6_fx_mix(FM6_FX_PHASER)==35u);
    assert(fm6_fx_time(FM6_FX_PHASER)==35u);
    assert(fm6_fx_feedback(FM6_FX_PHASER)==20u);
    assert(fm6_fx_depth(FM6_FX_PHASER)==65u);
    for (unsigned patch=1u; patch<FM1LAB_PATCH_COUNT; ++patch)
        fm6_patch_set_enabled((uint8_t)patch,0u);
    fm6_fx_set_enabled(FM6_FX_PHASER,1u);
    fm6_fx_set_mix(FM6_FX_PHASER,100u);
    fm6_fx_set_time(FM6_FX_PHASER,rate);
    fm6_fx_set_depth(FM6_FX_PHASER,depth);
    fm6_fx_set_feedback(FM6_FX_PHASER,90u);
    fm6_fx_set_send(0u,FM6_FX_PHASER,100u);
    fm6_note_on_mask(60u,110u,0x01u);
    for (unsigned block=0u; block<64u; ++block)
        fm6_render(destination+block*TEST_WORDS,TEST_FRAMES,4096u);
}

static int32_t render_master_peak(uint32_t master_gain_q12)
{
    static int32_t warmup[256u * 2u];
    fm6_init();
    fm6_patch_set_level(0u, 31u); /* calibrated factory level remains in the linear region */
    fm6_note_on_mask(60u, 110u, 0x01u);
    fm6_render(warmup, 256u, master_gain_q12); /* settle the master ramp */
    memset(output, 0, sizeof output);
    fm6_render(output, TEST_FRAMES, master_gain_q12);
    return peak(output, TEST_WORDS);
}

static uint64_t render_pan_send_peak(uint8_t pan, unsigned effect, unsigned channel)
{
    static int32_t warmup[2048u * 2u];
    fm6_init();
    for (unsigned patch=1u; patch<FM1LAB_PATCH_COUNT; ++patch)
        fm6_patch_set_enabled((uint8_t)patch,0u);
    fm6_patch_set_pan(0u,pan);
    fm6_fx_set_send(0u,(uint8_t)effect,100u);
    fm6_render(warmup,2048u,4096u);
    fm6_note_on_mask(60u,110u,0x01u);
    fm6_render(output,TEST_FRAMES,4096u);
    return fm6_test_fx_send_peak(effect,channel);
}

static int32_t render_pan_channel_peak(uint8_t pan, unsigned channel)
{
    static int32_t warmup[2048u * 2u];
    fm6_init();
    for (unsigned patch=1u; patch<FM1LAB_PATCH_COUNT; ++patch)
        fm6_patch_set_enabled((uint8_t)patch,0u);
    fm6_patch_set_pan(0u,pan);
    fm6_render(warmup,2048u,4096u); /* settle master and pan smoothing before measurement */
    fm6_note_on_mask(60u,110u,0x01u);
    memset(output,0,sizeof output);
    fm6_render(output,TEST_FRAMES,4096u);
    int32_t result=0;
    for(unsigned frame=0;frame<TEST_FRAMES;frame++) {
        int32_t value=output[frame*2u+channel];
        if(value<0) value=-value;
        if(value>result) result=value;
    }
    return result;
}

static uint64_t render_stack_patch_peak(uint8_t patch_mask, uint64_t *dry_peak)
{
    uint8_t voice[156] = {0};
    fm6_init();
    memcpy(voice, fm6_patch_data(0u), 155u);
    for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch) {
        if (patch && fm6_patch_set_voice(patch, voice) != 0)
            return 0u;
        fm6_patch_set_level(patch, 31u);
        fm6_patch_set_enabled(patch, (patch_mask & (1u << patch)) != 0u);
    }
    static int32_t master_warmup[512u * 2u];
    fm6_render(master_warmup, 512u, 4096u); /* settle the physical master ramp before measurement */
    fm6_note_on_mask(60u, 110u, patch_mask);
    memset(output, 0, sizeof output);
    fm6_render(output, TEST_FRAMES, 4096u);
    *dry_peak = fm6_test_dry_peak();
    return fm6_test_patch_peak(0u);
}


static void render_insert_variant(uint8_t amp, uint8_t eq, int32_t *destination)
{
    fm6_init();
    for(unsigned patch=1u;patch<FM1LAB_PATCH_COUNT;patch++)fm6_patch_set_enabled(patch,0u);
    fm6_patch_set_level(0u,31u);
    if(amp){
        fm6_patch_insert_set(0u,FM6_INSERT_AMP_ENABLED,1u);
        fm6_patch_insert_set(0u,FM6_INSERT_DRIVE,100u);
        fm6_patch_insert_set(0u,FM6_INSERT_TONE,82u);
        fm6_patch_insert_set(0u,FM6_INSERT_OUTPUT,50u);
    }
    if(eq){
        fm6_patch_insert_set(0u,FM6_INSERT_EQ_ENABLED,1u);
        fm6_patch_insert_set(0u,FM6_INSERT_EQ_MID_FREQUENCY,54u);
        fm6_patch_insert_set(0u,FM6_INSERT_EQ_MID_GAIN,24u);
        fm6_patch_insert_set(0u,FM6_INSERT_EQ_MID_Q,18u);
    }
    fm6_note_on_mask(60u,110u,0x01u);
    for(unsigned block=0u;block<32u;block++)
        fm6_render(destination+block*TEST_WORDS,TEST_FRAMES,4096u);
}
static int32_t render_compressor_peak(uint8_t enabled)
{
    static int32_t warmup[2048u*2u];
    fm6_init();
    for(unsigned patch=0u;patch<FM1LAB_PATCH_COUNT;patch++)fm6_patch_set_level(patch,127u);
    fm6_master_comp_set(FM6_COMP_ENABLED,enabled);
    fm6_master_comp_set(FM6_COMP_THRESHOLD,0u);
    fm6_master_comp_set(FM6_COMP_RATIO,100u);
    fm6_master_comp_set(FM6_COMP_ATTACK,0u);
    fm6_master_comp_set(FM6_COMP_RELEASE,20u);
    fm6_master_comp_set(FM6_COMP_MAKEUP,0u);
    fm6_note_on_mask(60u,110u,0x0fu);
    fm6_render(warmup,2048u,4096u);
    memset(output,0,sizeof output);
    fm6_render(output,TEST_FRAMES,4096u);
    return peak(output,TEST_WORDS);
}

int main(void) {
    assert(fm6_patch_count() == FM1LAB_PATCH_COUNT);
    fm6_init();
    assert(fm6_patch_enabled(0) == 1u && fm6_patch_enabled(1) == 1u &&
           fm6_patch_enabled(2) == 1u && fm6_patch_enabled(3) == 1u);
    /* New factory patches start at <=25% linear amplitude. */
    for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch)
        assert(fm6_patch_level(patch) == 31u);

    /* A patch's gain must not fall as additional identical patches are
     * enabled. Four coherent patches should sum, not be normalized back
     * to the single-patch level. */
    uint64_t single_dry_peak = 0u, four_dry_peak = 0u;
    uint64_t single_patch_peak = render_stack_patch_peak(0x01u, &single_dry_peak);
    uint64_t four_patch_peak = render_stack_patch_peak(0x0Fu, &four_dry_peak);
    assert(single_patch_peak > 0u);
    assert(four_patch_peak == single_patch_peak);
    /* Four identical patch buses add linearly before the output clipper.
     * This avoids mistaking the old final soft clip for mixer headroom. */
    assert(four_dry_peak == single_dry_peak * 4u);
    printf("FM6 gain reference: one-patch=%llu four-patch=%llu\n",
           (unsigned long long)single_dry_peak, (unsigned long long)four_dry_peak);
    assert(four_dry_peak > (uint64_t)(1u << 24) * 98u / 100u);
    assert(four_dry_peak < (uint64_t)(1u << 24) * 102u / 100u);
    printf("FM6 calibrated reference: one-patch=%llu four-patch=%llu output=%llu\n",
           (unsigned long long)single_dry_peak, (unsigned long long)four_dry_peak,
           (unsigned long long)fm6_test_output_peak());
    assert(fm6_test_output_peak() > 8388607u * 24u / 100u);
    assert(fm6_test_output_peak() < 8388607u * 26u / 100u);

    /* At the reference level, all four parallel FX buses must retain their
     * headroom over sustained feedback without any internal Q23 saturation. */
    fm6_init();
    uint8_t reference_voice[156] = {0};
    memcpy(reference_voice, fm6_patch_data(0u), 155u);
    for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch) {
        if (patch) assert(fm6_patch_set_voice(patch, reference_voice) == 0);
        fm6_patch_set_level((uint8_t)patch, 31u);
        fm6_patch_set_enabled((uint8_t)patch, 1u);
        for (unsigned effect = 0; effect < FM6_FX_COUNT; ++effect) {
            fm6_fx_set_enabled((uint8_t)effect, 1u);
            fm6_fx_set_mix((uint8_t)effect, 100u);
            fm6_fx_set_feedback((uint8_t)effect, (uint8_t)(effect == 0u ? 70u : 90u));
            fm6_fx_set_send((uint8_t)patch, (uint8_t)effect, 100u);
        }
    }
    fm6_note_on_mask(60u, 110u, 0x0Fu);
    for (unsigned block = 0; block < 256u; ++block)
        fm6_render(output, TEST_FRAMES, 4096u);
    assert(fm6_test_fx_saturations() == 0u);
    /* Per-patch pan is independent and leaves the centered reference unchanged. */
    assert(render_pan_channel_peak(0u,0u)>0);
    assert(render_pan_channel_peak(0u,1u)==0);
    assert(render_pan_channel_peak(127u,0u)==0);
    assert(render_pan_channel_peak(127u,1u)>0);
    int32_t center_l=render_pan_channel_peak(64u,0u);
    int32_t center_r=render_pan_channel_peak(64u,1u);
    assert(center_l>0 && center_r>0);
    assert(center_l*100>center_r*99 && center_l*100<center_r*101);
    /* Every shared FX bus receives the patch after its smoothed pan stage. */
    for(unsigned effect=0u;effect<FM6_FX_COUNT;effect++) {
        assert(render_pan_send_peak(0u,effect,0u)>0u);
        assert(render_pan_send_peak(0u,effect,1u)==0u);
        assert(render_pan_send_peak(127u,effect,0u)==0u);
        assert(render_pan_send_peak(127u,effect,1u)>0u);
        assert(render_pan_send_peak(64u,effect,0u)>0u);
        assert(render_pan_send_peak(64u,effect,1u)>0u);
    }
    fm6_init();
    fm6_patch_set_pan(0u,255u);
    assert(fm6_patch_pan(0u)==127u);

    /* The platform passes APP_GAIN as Q12 (0..4096). At a settled
     * half-scale master setting, output must be approximately half of unity. */
    int32_t master_unity_peak = render_master_peak(4096u);
    int32_t master_half_peak = render_master_peak(2048u);
    assert(master_unity_peak > 0);
    assert(master_half_peak > 0);
    assert(master_half_peak * 10 > master_unity_peak * 4);
    assert(master_half_peak * 10 < master_unity_peak * 6);

    /* A zero-level patch must contribute exact silence to dry and every
     * enabled parallel send, not merely suppress new note allocation. */
    fm6_init();
    for (unsigned effect = 0; effect < FM6_FX_COUNT; ++effect) {
        fm6_fx_set_enabled((uint8_t)effect, 1u);
        fm6_fx_set_mix((uint8_t)effect, 100u);
        fm6_fx_set_send(0u, (uint8_t)effect, 100u);
    }
    fm6_patch_set_level(0u, 0u);
    fm6_note_on_mask(60u, 110u, 0x01u);
    memset(output, 0, sizeof output);
    fm6_render(output, TEST_FRAMES, 4096u);
    assert(peak(output, TEST_WORDS) == 0);
    fm6_note_on(60u, 100u);
    /* Patch 0 remains muted; only the other three factory layers start. */
    assert(fm6_active_voices() == FM1LAB_PATCH_COUNT - 1u);
    fm6_panic();
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
    /* Restore patch 0 after the earlier exact-mute test; panic clears voices,
     * not the persistent patch level. Fill its pool beside patch 1's live voice. */
    fm6_patch_set_level(0u, 127u);
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
    /* Zero volume is a true layer-off state: no existing or new voices. */
    fm6_note_on_mask(73u, 100u, 0x02u);
    assert(fm6_active_voices() == 8u);
    fm6_patch_set_level(1, 127u);
    fm6_note_on_mask(73u, 100u, 0x02u);
    assert(fm6_active_voices() == 9u);
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
    memset(output,0,sizeof output); fm6_render(output,128u,4096u);
    seq_peak=peak(output,TEST_WORDS);
    assert(seq_peak>0);
    fm6_sequencer_set_running(0u);
    assert(fm6_sequencer_running()==0u);
    /* A rapid restart resets the shared transport phase and does not strand
     * the previous lane notes in a held state. */
    fm6_sequencer_set_running(1u);
    for(unsigned block=0;block<43u;block++) {
        memset(output,0,sizeof output); fm6_render(output,128u,4096u);
    }
    assert(fm6_sequencer_step()==0u);
    memset(output,0,sizeof output); fm6_render(output,8u,4096u);
    assert(fm6_sequencer_step()==0u);
    memset(output,0,sizeof output); fm6_render(output,1u,4096u);
    assert(fm6_sequencer_step()==1u);

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
    for(unsigned effect=0;effect<FM6_FX_COUNT;effect++) {
        fm6_init(); fm6_fx_set_enabled((uint8_t)effect,1u);
        fm6_fx_set_mix((uint8_t)effect,100u);
        if(effect==FM6_FX_PHASER) { fm6_fx_set_send(0u,(uint8_t)effect,100u); fm6_fx_set_depth((uint8_t)effect,90u); fm6_fx_set_time((uint8_t)effect,45u); }
        fm6_note_on(60u,110u);
        for(unsigned block=0;block<64u;block++)
            fm6_render(wet+block*TEST_WORDS,128u,4096u);
        int changed=0,stereo_spread=0;
        for(unsigned i=0;i<64u*TEST_WORDS;i++) if(wet[i]!=dry[i]) { changed=1; break; }
        for(unsigned frame=0;frame<64u*128u;frame++)
            if(wet[frame*2u]!=wet[frame*2u+1u]) { stereo_spread=1; break; }
        assert(changed);
        if(effect==0u || effect==FM6_FX_PHASER) assert(stereo_spread);
    }


    /* Per-patch send controls isolate each instrument from every shared FX bus. */
    for(unsigned effect=0;effect<FM6_FX_COUNT;effect++) {
        fm6_init();
        /* Isolate patch 0 so the other default-on factory layers cannot
         * contribute their own sends to this single-layer routing check. */
        for (unsigned patch = 1u; patch < FM1LAB_PATCH_COUNT; ++patch)
            fm6_patch_set_enabled((uint8_t)patch, 0u);
        fm6_fx_set_enabled((uint8_t)effect,1u);
        fm6_fx_set_mix((uint8_t)effect,100u);
        fm6_fx_set_send(0u,(uint8_t)effect,0u);
        assert(fm6_fx_get_send(0u,(uint8_t)effect)==0u);
        fm6_note_on(60u,110u);
        for(unsigned block=0;block<64u;block++)
            fm6_render(wet+block*TEST_WORDS,128u,4096u);
        fm6_init();
        /* Match the wet render's one-layer fixture; only the send differs. */
        for (unsigned patch = 1u; patch < FM1LAB_PATCH_COUNT; ++patch)
            fm6_patch_set_enabled((uint8_t)patch, 0u);
        fm6_note_on(60u,110u);
        for(unsigned block=0;block<64u;block++)
            fm6_render(dry+block*TEST_WORDS,128u,4096u);
        for(unsigned i=0;i<64u*TEST_WORDS;i++) assert(wet[i]==dry[i]);
    }



    /* Phaser rate and depth must independently alter the filter sweep. */
    render_phaser_test(0u,100u,dry);
    render_phaser_test(100u,100u,wet);
    assert(memcmp(dry,wet,sizeof dry)!=0);
    render_phaser_test(45u,0u,dry);
    render_phaser_test(45u,100u,wet);
    assert(memcmp(dry,wet,sizeof dry)!=0);
    assert(fm6_fx_depth(FM6_FX_PHASER)==100u);
    fm6_fx_set_depth(FM6_FX_PHASER,255u);
    assert(fm6_fx_depth(FM6_FX_PHASER)==100u);

    /* Combined worst-case functional stress: all 32 patch voices occupied,
     * both sequencer lanes advancing, 16 held arp notes, and every FX bus
     * enabled at maximum wet/send. Render one second of 44.1 kHz audio in
     * 128-frame host-test chunks (the device app callback is 256 frames). This checks integration and bounds, not
     * the target's real-time CPU deadline. */
    fm6_init();
    memcpy(voice, fm6_patch_data(0), 155u);
    for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch) {
        if (patch) assert(fm6_patch_set_voice(patch, voice) == 0);
        fm6_patch_set_enabled(patch, 1u);
        fm6_patch_set_level(patch, 127u);
        for (unsigned n = 0; n < FM1LAB_VOICE_LIMIT; ++n)
            fm6_note_on_mask((uint8_t)(36u + patch * 12u + n), 105u,
                             (uint8_t)(1u << patch));
        assert(fm6_active_voices_patch(patch) == FM1LAB_VOICE_LIMIT);
    }
    for (unsigned effect = 0; effect < FM6_FX_COUNT; ++effect) {
        fm6_fx_set_enabled((uint8_t)effect, 1u);
        fm6_fx_set_mix((uint8_t)effect, 100u);
        fm6_fx_set_time((uint8_t)effect, 100u);
        fm6_fx_set_feedback((uint8_t)effect, (uint8_t)(effect == 0u ? 70u : 90u));
        for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch)
            fm6_fx_set_send((uint8_t)patch, (uint8_t)effect, 100u);
    }
    fm6_set_tempo(300u);
    fm6_sequencer_set_length(64u);
    fm6_sequencer_set_lane(0u, 0u);
    fm6_sequencer_set_lane(1u, 1u);
    for (unsigned lane = 0; lane < 2u; ++lane)
        for (unsigned step = 0; step < 64u; ++step)
            fm6_sequencer_set_step((uint8_t)lane, (uint8_t)step, 1u,
                (uint8_t)(48u + lane * 12u + step % 12u), 110u, 90u);
    fm6_arpeggiator_set_mode(1u);
    for (unsigned n = 0; n < 16u; ++n)
        fm6_note_on((uint8_t)(48u + n), (uint8_t)(80u + n * 3u));
    fm6_sequencer_set_running(1u);

    uint32_t stress_peak = 0u;
    unsigned stereo_frames = 0u;
    unsigned rendered_frames = 0u;
    for (unsigned block = 0; block < 44100u / TEST_FRAMES; ++block) {
        memset(output, 0, sizeof output);
        fm6_render(output, TEST_FRAMES, 4096u);
        for (unsigned sample = 0; sample < TEST_WORDS; ++sample) {
            int64_t value = output[sample];
            uint32_t magnitude = (uint32_t)(value < 0 ? -value : value);
            assert(magnitude <= 8388607u);
            if (magnitude > stress_peak) stress_peak = magnitude;
        }
        for (unsigned frame = 0; frame < TEST_FRAMES; ++frame)
            if (output[frame * 2u] != output[frame * 2u + 1u]) ++stereo_frames;
        rendered_frames += TEST_FRAMES;
        assert(fm6_active_voices() <= FM1LAB_PATCH_COUNT * FM1LAB_VOICE_LIMIT);
        for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch)
            assert(fm6_active_voices_patch(patch) <= FM1LAB_VOICE_LIMIT);
    }
    assert(rendered_frames >= 44000u);
    assert(stress_peak > 0u);
    assert(fm6_test_output_peak() <= 7470000u);
    assert(fm6_test_limiter_events() > 0u);
    assert(stereo_frames > 0u); /* chorus is actually contributing stereo output */
    assert(fm6_sequencer_step() > 0u);
    fm6_panic();
    assert(fm6_active_voices() == 0u);
    fm6_sequencer_set_running(0u);
    fm6_arpeggiator_set_mode(0u);

    /* Platform overload callback must shed one globally oldest voice,
     * fade it rather than hard-cutting it, and preserve the other patch pools. */
    fm6_init();
    memcpy(voice, fm6_patch_data(0), 155u);
    for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch) {
        if (patch) assert(fm6_patch_set_voice(patch, voice) == 0);
        fm6_patch_set_enabled(patch, 1u);
        fm6_note_on_mask((uint8_t)(60u + patch), 100u,
                         (uint8_t)(1u << patch));
    }
    memset(output, 0, sizeof output);
    fm6_render(output, TEST_FRAMES, 4096u);
    assert(fm6_active_voices() == 4u);
    fm6_overload();
    assert(fm6_overload_shed_count() == 1u);
    memset(output, 0, sizeof output);
    fm6_render(output, TEST_FRAMES, 4096u);
    assert(fm6_active_voices() == 3u);
    for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch)
        assert(fm6_active_voices_patch(patch) <= 1u);
    fm6_overload();
    assert(fm6_overload_shed_count() == 2u);
    fm6_panic();
    assert(fm6_active_voices() == 0u);

    /* Render chunking must not change the audio stream. This compares
     * ordinary device-sized blocks against irregular sub-block calls with
     * the same notes and all three FX buses active. */
    fm6_init();
    memcpy(voice, fm6_patch_data(0), 155u);
    for (unsigned patch = 1; patch < FM1LAB_PATCH_COUNT; ++patch) {
        assert(fm6_patch_set_voice(patch, voice) == 0);
        fm6_patch_set_enabled(patch, 1u);
        fm6_patch_set_level(patch, 127u);
    }
    for (unsigned fx = 0; fx < FM6_FX_COUNT; ++fx) {
        fm6_fx_set_enabled((uint8_t)fx, 1u);
        fm6_fx_set_mix((uint8_t)fx, (uint8_t)(35u + fx * 20u));
        fm6_fx_set_time((uint8_t)fx, (uint8_t)(45u + fx * 10u));
        fm6_fx_set_feedback((uint8_t)fx, (uint8_t)(30u + fx * 15u));
        for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch)
            fm6_fx_set_send((uint8_t)patch, (uint8_t)fx, 80u);
    }
    fm6_note_on_mask(60u, 105u, 0x0Fu);
    for (unsigned offset = 0; offset < 4096u; offset += TEST_FRAMES)
        fm6_render(chunk_a + offset * 2u, TEST_FRAMES, 4096u);

    fm6_init();
    memcpy(voice, fm6_patch_data(0), 155u);
    for (unsigned patch = 1; patch < FM1LAB_PATCH_COUNT; ++patch) {
        assert(fm6_patch_set_voice(patch, voice) == 0);
        fm6_patch_set_enabled(patch, 1u);
        fm6_patch_set_level(patch, 127u);
    }
    for (unsigned fx = 0; fx < FM6_FX_COUNT; ++fx) {
        fm6_fx_set_enabled((uint8_t)fx, 1u);
        fm6_fx_set_mix((uint8_t)fx, (uint8_t)(35u + fx * 20u));
        fm6_fx_set_time((uint8_t)fx, (uint8_t)(45u + fx * 10u));
        fm6_fx_set_feedback((uint8_t)fx, (uint8_t)(30u + fx * 15u));
        for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch)
            fm6_fx_set_send((uint8_t)patch, (uint8_t)fx, 80u);
    }
    fm6_note_on_mask(60u, 105u, 0x0Fu);
    {
        /* The FM6 core advances in 32-frame CTL quanta; the device app
         * supplies 256-frame calls. Vary only legal multiples. */
        static const unsigned chunks[] = {256u, 512u, 128u, 384u};
        unsigned offset = 0u, chunk = 0u;
        while (offset < 4096u) {
            unsigned count = chunks[chunk++ % (sizeof chunks / sizeof chunks[0])];
            if (count > 4096u - offset) count = 4096u - offset;
            fm6_render(chunk_b + offset * 2u, count, 4096u);
            offset += count;
        }
    }
    for (unsigned sample = 0; sample < 4096u * 2u; ++sample) {
        if (chunk_a[sample] != chunk_b[sample]) {
            fprintf(stderr, "render chunk mismatch at sample %u: %ld vs %ld\n",
                    sample, (long)chunk_a[sample], (long)chunk_b[sample]);
            assert(chunk_a[sample] == chunk_b[sample]);
        }
    }
    fm6_panic();

    /* Performance state snapshot/restore covers every sequencer step, lane
     * assignment, tempo, arp mode, effect control and per-patch send. */
    fm6_performance_state_t saved, restored;
    memset(&saved, 0, sizeof saved);
    memset(&restored, 0, sizeof restored);
    saved.tempo_bpm = 137u;
    saved.seq_length = 48u;
    saved.lane_patch[0] = 2u;
    saved.lane_patch[1] = 3u;
    for (unsigned lane = 0; lane < 2u; ++lane)
        for (unsigned step = 0; step < 64u; ++step) {
            saved.seq_enabled[lane][step] = (uint8_t)((step % 3u) == lane);
            saved.seq_note[lane][step] = (uint8_t)(36u + lane * 12u + step % 48u);
            saved.seq_velocity[lane][step] = (uint8_t)(70u + step % 58u);
            saved.seq_gate[lane][step] = (uint8_t)(10u + step % 91u);
        }
    saved.arp_mode = 3u;
    saved.compressor_enabled=1u; saved.compressor_threshold=42u;
    saved.compressor_ratio=67u; saved.compressor_attack=15u;
    saved.compressor_release=75u; saved.compressor_makeup=10u;
    for (unsigned fx = 0; fx < 3u; ++fx) {
        saved.fx_enabled[fx] = 1u;
        saved.fx_mix[fx] = (uint8_t)(30u + fx * 20u);
        saved.fx_time[fx] = (uint8_t)(40u + fx * 15u);
        saved.fx_feedback[fx] = (uint8_t)(fx == 0u ? 60u : 70u + fx * 5u);
        saved.fx_depth[fx] = (uint8_t)(35u + fx * 15u);
        for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch)
            saved.fx_send[patch][fx] = (uint8_t)(patch * 20u + fx * 5u);
    }
    assert(fm6_performance_state_valid(&saved));
    fm6_init();
    assert(fm6_performance_state_set(&saved) == 0);
    assert(fm6_sequencer_running() == 0u);
    fm6_performance_state_get(&restored);
    assert(memcmp(&saved, &restored, sizeof saved) == 0);
    restored.fx_depth[FM6_FX_PHASER] = 101u;
    assert(!fm6_performance_state_valid(&restored));
    restored.fx_depth[FM6_FX_PHASER] = saved.fx_depth[FM6_FX_PHASER];
    restored.compressor_makeup=101u;
    assert(!fm6_performance_state_valid(&restored));
    restored.compressor_makeup=saved.compressor_makeup;
    restored.tempo_bpm = 29u;
    assert(!fm6_performance_state_valid(&restored));
    assert(fm6_performance_state_set(&restored) == -1);
    fm6_performance_state_get(&restored);
    assert(memcmp(&saved, &restored, sizeof saved) == 0);
    fm6_sequencer_set_running(1u);
    assert(fm6_sequencer_running() == 1u);
    fm6_sequencer_set_running(0u);

    /* Patch inserts are independent, bypassable, and operate before pan/sends. */
    render_insert_variant(0u,0u,chunk_a);
    render_insert_variant(1u,0u,chunk_b);
    assert(memcmp(chunk_a,chunk_b,sizeof chunk_a)!=0);
    render_insert_variant(0u,1u,chunk_b);
    assert(memcmp(chunk_a,chunk_b,sizeof chunk_a)!=0);
    fm6_init();
    fm6_patch_insert_set(2u,FM6_INSERT_DRIVE,255u);
    fm6_patch_insert_set(2u,FM6_INSERT_EQ_MID_GAIN,255u);
    assert(fm6_patch_insert_get(2u,FM6_INSERT_DRIVE)==100u);
    assert(fm6_patch_insert_get(2u,FM6_INSERT_EQ_MID_GAIN)==24u);
    assert(fm6_patch_insert_get(2u,FM6_INSERT_EQ_MID_FREQUENCY)==50u);

    /* Linked master compression reduces sustained four-layer peaks and is
     * fully bypassable; its state is included in performance persistence. */
    int32_t compressor_bypass_peak=render_compressor_peak(0u);
    int32_t compressor_active_peak=render_compressor_peak(1u);
    assert(compressor_bypass_peak>0);
    assert(compressor_active_peak>0);
    assert(compressor_active_peak<compressor_bypass_peak);

    puts("Felucca FM6 engine: voice pools, sequencing, stereo FX, patch Amp/Drive, parametric EQ and master compressor passed");
    return 0;
}