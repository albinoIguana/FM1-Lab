#include "../app/fm1lab_instrument.h"
#include "../app/dsp/fm1lab_insert_state.h"
#include "../app/fm1lab_pending_watch.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_track_activation_preserves_shared_master_chain(void)
{
    fm1lab_insert_state_t active, lane;
    fm1lab_insert_state_init(&active);
    fm1lab_insert_state_init(&lane);
    fm1lab_insert_state_set(&active, FM6_INSERT_AMP_ENABLED, 1u);
    fm1lab_insert_state_set(&active, FM6_INSERT_DRIVE, 73u);
    fm1lab_insert_state_set(&active, FM6_INSERT_EQ_MID_GAIN, 19u);
    fm1lab_insert_state_set(&lane, FM6_INSERT_AMP_ENABLED, 0u);
    fm1lab_insert_state_set(&lane, FM6_INSERT_DRIVE, 12u);
    fm1lab_insert_state_set(&lane, FM6_INSERT_EQ_MID_GAIN, 4u);
    lane.instrument_macros[0]=17u;
    lane.instrument_macros[1]=28u;
    lane.patch_octave[2]=-2;
    lane.patch_fine_cents[2]=35;

    fm1lab_insert_state_activate_instrument(&active,&lane);
    /* Shared live chain stays exactly as heard before the track handoff. */
    assert(fm1lab_insert_state_get(&active,FM6_INSERT_AMP_ENABLED)==1u);
    assert(fm1lab_insert_state_get(&active,FM6_INSERT_DRIVE)==73u);
    assert(fm1lab_insert_state_get(&active,FM6_INSERT_EQ_MID_GAIN)==19u);
    /* Instrument-owned controls follow the newly focused lane. */
    assert(active.instrument_macros[0]==17u && active.instrument_macros[1]==28u);
    assert(active.patch_octave[2]==-2 && active.patch_fine_cents[2]==35);
}

static void test_pending_watch_requires_stalled_audio_heartbeat(void)
{
    fm1lab_pending_watch_t watch={0};
    assert(!fm1lab_pending_watch_update(&watch,1u,100u,10u));
    assert(!fm1lab_pending_watch_update(&watch,1u,599u,10u));
    assert(fm1lab_pending_watch_update(&watch,1u,600u,10u));
    assert(!fm1lab_pending_watch_update(&watch,1u,650u,11u));
    assert(!fm1lab_pending_watch_update(&watch,1u,1149u,11u));
    assert(fm1lab_pending_watch_update(&watch,1u,1150u,11u));
    assert(!fm1lab_pending_watch_update(&watch,0u,1200u,11u));
    assert(!fm1lab_pending_watch_update(&watch,1u,1800u,11u));
}

static void test_blank_library_slot_detection(void)
{
    fm1lab_instrument_t blank;
    uint8_t voice[FM1LAB_PATCH_BYTES] = {0};
    fm1lab_instrument_init(&blank, "INSTR 01");
    assert(fm1lab_instrument_is_blank_library_slot(&blank));

    /* One audible carrier means this record is no longer an empty slot. */
    voice[134] = 0u;
    voice[105u + 16u] = 40u;
    assert(fm1lab_patch_set_voice(&blank, 2u, voice) == 0);
    assert(!fm1lab_instrument_is_blank_library_slot(&blank));

    fm1lab_instrument_init(&blank, "MY SOUND");
    assert(!fm1lab_instrument_is_blank_library_slot(&blank));
}

int main(void)
{
    test_track_activation_preserves_shared_master_chain();
    test_pending_watch_requires_stalled_audio_heartbeat();
    test_blank_library_slot_detection();
    fm1lab_instrument_t instrument;
    uint8_t voice[FM1LAB_PATCH_BYTES];
    uint8_t expected[FM1LAB_PATCH_BYTES];
    fm1lab_patch_t before[FM1LAB_PATCH_COUNT];

    fm1lab_instrument_init(&instrument, "REPLACE TEST");
    for (unsigned p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        memset(voice, (int)(p + 1u), sizeof voice);
        voice[134] = (uint8_t)p;
        voice[135] = (uint8_t)p;
        voice[155] = 64u;
        assert(fm1lab_patch_set_voice(&instrument, p, voice) == 0);
        instrument.patches[p].level = (uint8_t)(30u + p);
    }

    instrument.patches[2].level = 73u;
    instrument.patches[2].enabled = 0u;
    instrument.patches[2].octave = -2;
    instrument.patches[2].fine_cents = -25;
    instrument.patches[2].macros[0] = 11u;
    instrument.patches[2].macros[1] = 22u;
    instrument.patches[2].macros[2] = 33u;
    instrument.patches[2].macros[3] = 44u;
    fm1lab_patch_set_pan(&instrument, 2u, 91u);
    for (unsigned p = 0; p < FM1LAB_PATCH_COUNT; ++p)
        before[p] = instrument.patches[p];

    memset(voice, 17, sizeof voice);
    voice[134] = 18u;
    voice[135] = 5u;
    voice[155] = 12u;
    memcpy(expected, voice, sizeof expected);
    expected[155] = 91u;

    assert(fm1lab_patch_replace_voice(&instrument, 2u, voice) == 0);
    for (unsigned p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        if (p != 2u)
            assert(memcmp(&instrument.patches[p], &before[p], sizeof(fm1lab_patch_t)) == 0);
    }
    assert(memcmp(instrument.patches[2].voice, expected, FM1LAB_PATCH_BYTES) == 0);
    assert(instrument.patches[2].level == 73u);
    assert(instrument.patches[2].enabled == 0u);
    assert(instrument.patches[2].octave == -2);
    assert(instrument.patches[2].fine_cents == -25);
    assert(instrument.patches[2].macros[0] == 11u);
    assert(instrument.patches[2].macros[1] == 22u);
    assert(instrument.patches[2].macros[2] == 33u);
    assert(instrument.patches[2].macros[3] == 44u);
    assert(fm1lab_patch_pan(&instrument, 2u) == 91u);
    assert(fm1lab_instrument_valid(&instrument));
    assert(fm1lab_patch_replace_voice(&instrument, 4u, voice) == -1);
    return 0;
}
