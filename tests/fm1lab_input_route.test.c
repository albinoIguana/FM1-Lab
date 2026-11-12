#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../app/fm1lab_input_dispatch.h"
#include "../app/fm1lab_instrument.h"
enum { TEST_INSTRUMENT_COUNT = 32u };

typedef struct {
    uint8_t fail_save;
    uint8_t load_busy;
    uint8_t active_lane;
    uint8_t engine_target;
    uint8_t bank_instrument[2];
    uint8_t bank_patch[2][FM1LAB_PATCH_COUNT];
    uint8_t patch_preset[FM1LAB_PATCH_COUNT];
    unsigned save_calls;
    unsigned load_calls;
    unsigned patch_calls;
    unsigned publish_calls;
} fake_engine_t;

static unsigned bit_count(uint32_t value)
{
    unsigned count = 0u;
    while (value) { count += value & 1u; value >>= 1u; }
    return count;
}

static int fake_save(void *opaque)
{
    fake_engine_t *fake = (fake_engine_t *)opaque;
    fake->save_calls++;
    return fake->fail_save ? -1 : 0;
}

static int fake_load(void *opaque, uint8_t lane, uint8_t instrument, int mode)
{
    fake_engine_t *fake = (fake_engine_t *)opaque;
    fake->load_calls++;
    if (fake->load_busy) return 1;
    if (lane > 1u || instrument >= TEST_INSTRUMENT_COUNT) return -1;
    fake->active_lane = lane;
    fake->engine_target = (uint8_t)(lane * FM1LAB_PATCH_COUNT);
    if (mode == FM1LAB_INPUT_LOAD_ENGINE_BANK) {
        fake->bank_instrument[lane] = instrument;
        for (unsigned p = 0u; p < FM1LAB_PATCH_COUNT; ++p)
            fake->bank_patch[lane][p] = (uint8_t)(instrument * 4u + p);
    }
    return 0;
}

static int fake_patch_preset(void *opaque, uint8_t patch, int delta)
{
    fake_engine_t *fake = (fake_engine_t *)opaque;
    fake->patch_calls++;
    fake->patch_preset[patch] = (uint8_t)(fake->patch_preset[patch] + delta);
    return 0;
}

static void fake_publish(void *opaque)
{
    ((fake_engine_t *)opaque)->publish_calls++;
}

static const fm1lab_input_ops_t fake_ops = {
    fake_save, fake_load, fake_patch_preset, fake_publish
};

static void test_complete_route_matrix(void)
{
    for (uint8_t page = 0u; page < FM1LAB_INPUT_PAGE_COUNT; ++page) {
        for (uint8_t encoder = 0u; encoder < 7u; ++encoder) {
            for (uint8_t focus = 0u; focus < 2u; ++focus) {
                enum fm1lab_input_action expected = FM1LAB_INPUT_NONE;
                if (page == FM1LAB_INPUT_PAGE_HOME) {
                    if (encoder == 0u) expected = FM1LAB_INPUT_PATCH_FOCUS_STEP;
                    if (encoder == 2u) expected = focus
                        ? FM1LAB_INPUT_PATCH_PRESET_STEP
                        : FM1LAB_INPUT_INSTRUMENT_STEP;
                } else if (page == FM1LAB_INPUT_PAGE_SEQ ||
                           page == FM1LAB_INPUT_PAGE_SEQ_OPTIONS) {
                    if (encoder == 0u) expected = FM1LAB_INPUT_TRACK_TOGGLE;
                    if (encoder == 2u) expected = FM1LAB_INPUT_TRACK_INSTRUMENT_STEP;
                }
                uint32_t claims = fm1lab_input_route_claims(page, encoder, focus);
                assert(bit_count(claims) <= 1u); /* no two actions may claim one report */
                assert(fm1lab_input_route(page, encoder, focus) == expected);
            }
        }
    }

    /* Patch focus is a HOME-only state and is cleared on every page exit. */
    assert(fm1lab_input_focus_after_page_change(0u, 0u, 1u) == 1u);
    assert(fm1lab_input_focus_after_page_change(0u, 7u, 1u) == 0u);
    assert(fm1lab_input_focus_after_page_change(7u, 0u, 1u) == 0u);
}

static void test_home_patch_and_instrument_actions(void)
{
    fm1lab_input_state_t state = {0u, 0u, 0u, 3u, {3u, 7u}, {0u, 2u}, 0u, 0u};
    fake_engine_t fake;
    memset(&fake, 0, sizeof fake);
    fake.bank_instrument[0] = 3u;
    fake.bank_instrument[1] = 7u;

    enum fm1lab_input_action action =
        fm1lab_input_route(FM1LAB_INPUT_PAGE_HOME, 0u, state.patch_focus);
    assert(action == FM1LAB_INPUT_PATCH_FOCUS_STEP);
    assert(fm1lab_input_apply(&state, action, 1, FM1LAB_STORE_INSTRUMENT_COUNT,
        FM1LAB_PATCH_COUNT, &fake_ops, &fake) == FM1LAB_INPUT_RESULT_APPLIED);
    assert(state.patch == 1u && state.patch_focus == 1u);
    assert(state.track_patch[0] == 1u);
    assert(fake.publish_calls == 1u);

    action = fm1lab_input_route(FM1LAB_INPUT_PAGE_HOME, 2u, state.patch_focus);
    assert(action == FM1LAB_INPUT_PATCH_PRESET_STEP);
    assert(fm1lab_input_apply(&state, action, 2, FM1LAB_STORE_INSTRUMENT_COUNT,
        FM1LAB_PATCH_COUNT, &fake_ops, &fake) == FM1LAB_INPUT_RESULT_APPLIED);
    assert(fake.patch_calls == 1u && fake.patch_preset[1] == 2u);
    assert(state.instrument == 3u && state.track_instrument[0] == 3u);

    state.patch_focus = 0u;
    action = fm1lab_input_route(FM1LAB_INPUT_PAGE_HOME, 2u, state.patch_focus);
    assert(action == FM1LAB_INPUT_INSTRUMENT_STEP);
    assert(fm1lab_input_apply(&state, action, 1, FM1LAB_STORE_INSTRUMENT_COUNT,
        FM1LAB_PATCH_COUNT, &fake_ops, &fake) == FM1LAB_INPUT_RESULT_APPLIED);
    assert(state.instrument == 4u && state.track_instrument[0] == 4u);
    assert(fake.bank_instrument[0] == 4u);
    assert(fake.bank_instrument[1] == 7u); /* HOME change only replaces T1's bank. */
}

static void test_track_round_trip_and_isolation(void)
{
    fm1lab_input_state_t state = {0u, 1u, 0u, 3u, {3u, 7u}, {1u, 2u}, 0u, 0u};
    fake_engine_t fake;
    uint8_t t1_before[FM1LAB_PATCH_COUNT];
    memset(&fake, 0, sizeof fake);
    fake.bank_instrument[0] = 3u;
    fake.bank_instrument[1] = 7u;
    for (unsigned p = 0u; p < FM1LAB_PATCH_COUNT; ++p) {
        fake.bank_patch[0][p] = (uint8_t)(10u + p);
        fake.bank_patch[1][p] = (uint8_t)(20u + p);
    }
    memcpy(t1_before, fake.bank_patch[0], sizeof t1_before);

    /* T1 -> T2; a multi-count report still toggles exactly once. */
    assert(fm1lab_input_apply(&state,
        fm1lab_input_route(FM1LAB_INPUT_PAGE_SEQ, 0u, 1u), 5,
        FM1LAB_STORE_INSTRUMENT_COUNT, FM1LAB_PATCH_COUNT, &fake_ops, &fake)
        == FM1LAB_INPUT_RESULT_APPLIED);
    assert(state.lane == 1u && state.instrument == 7u && state.patch == 2u);
    assert(fake.engine_target == FM1LAB_PATCH_COUNT);
    assert(memcmp(fake.bank_patch[0], t1_before, sizeof t1_before) == 0);

    /* PRESETS on SEQ OPTIONS changes only T2's Instrument/bank. */
    assert(fm1lab_input_apply(&state,
        fm1lab_input_route(FM1LAB_INPUT_PAGE_SEQ_OPTIONS, 2u, 0u), 1,
        FM1LAB_STORE_INSTRUMENT_COUNT, FM1LAB_PATCH_COUNT, &fake_ops, &fake)
        == FM1LAB_INPUT_RESULT_APPLIED);
    assert(state.track_instrument[1] == 8u && state.instrument == 8u);
    assert(fake.bank_instrument[1] == 8u);
    assert(memcmp(fake.bank_patch[0], t1_before, sizeof t1_before) == 0);
    state.instrument_apply_pending = 0u; /* model completion of the queued bank apply */

    /* T2 -> T1 restores T1's own Instrument and patch selection. */
    assert(fm1lab_input_apply(&state,
        fm1lab_input_route(FM1LAB_INPUT_PAGE_SEQ, 0u, 0u), -1,
        FM1LAB_STORE_INSTRUMENT_COUNT, FM1LAB_PATCH_COUNT, &fake_ops, &fake)
        == FM1LAB_INPUT_RESULT_APPLIED);
    assert(state.lane == 0u && state.instrument == 3u && state.patch == 1u);
    assert(fake.engine_target == 0u);
    assert(memcmp(fake.bank_patch[0], t1_before, sizeof t1_before) == 0);
    assert(fake.bank_instrument[0] == 3u && fake.bank_instrument[1] == 8u);
}

static void test_busy_and_save_failure_statuses(void)
{
    fm1lab_input_state_t state = {0u, 0u, 0u, 3u, {3u, 7u}, {0u, 1u}, 1u, 1u};
    fake_engine_t fake;
    memset(&fake, 0, sizeof fake);
    assert(fm1lab_input_apply(&state, FM1LAB_INPUT_INSTRUMENT_STEP, 1,
        FM1LAB_STORE_INSTRUMENT_COUNT, FM1LAB_PATCH_COUNT, &fake_ops, &fake)
        == FM1LAB_INPUT_RESULT_LOAD_BUSY);
    assert(fake.load_calls == 0u && fake.save_calls == 0u);

    state.instrument_apply_pending = 0u;
    fake.fail_save = 1u;
    assert(fm1lab_input_apply(&state, FM1LAB_INPUT_INSTRUMENT_STEP, 1,
        FM1LAB_STORE_INSTRUMENT_COUNT, FM1LAB_PATCH_COUNT, &fake_ops, &fake)
        == FM1LAB_INPUT_RESULT_SAVE_FAILED_CHANGED);
    assert(fake.save_calls == 1u && fake.load_calls == 1u);
    assert(state.instrument == 4u); /* failed save does not swallow PRESETS */
    state.instrument_apply_pending = 0u; /* queued apply completed */

    state.lane = 0u;
    state.instrument = 4u;
    state.track_instrument[0] = 4u;
    state.instrument_dirty = 1u;
    fake.fail_save = 1u;
    assert(fm1lab_input_apply(&state, FM1LAB_INPUT_TRACK_TOGGLE, 1,
        FM1LAB_STORE_INSTRUMENT_COUNT, FM1LAB_PATCH_COUNT, &fake_ops, &fake)
        == FM1LAB_INPUT_RESULT_SAVE_FAILED_BLOCKED);
    assert(state.lane == 0u);
}

int main(void)
{
    test_complete_route_matrix();
    test_home_patch_and_instrument_actions();
    test_track_round_trip_and_isolation();
    test_busy_and_save_failure_statuses();
    return 0;
}
