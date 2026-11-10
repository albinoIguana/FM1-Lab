#include "fm1lab_instrument.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_four_patch_contract(void)
{
    fm1lab_instrument_t instrument;
    uint8_t voice[FM1LAB_PATCH_BYTES] = {0};
    fm1lab_instrument_init(&instrument, "TEST BANK");
    assert(fm1lab_instrument_valid(&instrument));
    assert(instrument.patches[0].enabled == 1);
    assert(instrument.patches[0].level == 31);
    assert(instrument.patches[0].octave == 0 && instrument.patches[0].fine_cents == 0);
    assert(fm1lab_patch_pan(&instrument, 0u) == 64u);
    fm1lab_patch_set_pan(&instrument, 2u, 17u);
    assert(fm1lab_patch_pan(&instrument, 2u) == 17u);
    for (unsigned i = 0; i < FM1LAB_PATCH_COUNT; ++i) {
        assert(instrument.patches[i].macros[0] == 50);
        assert(instrument.patches[i].macros[1] == 50);
        assert(instrument.patches[i].macros[2] == 50);
        assert(instrument.patches[i].macros[3] == 50);
    }
    voice[134] = 0;
    voice[135] = 3;
    voice[105 + 16] = 40;
    instrument.patches[0].level = 127u;
    assert(fm1lab_patch_set_voice(&instrument, 0, voice) == 0);
    assert(instrument.patches[0].level == 31u);
    assert(fm1lab_patch_pan(&instrument, 0u) == 64u);
    assert(fm1lab_patch_set_voice(&instrument, 3, voice) == 0);
    assert(instrument.patches[3].level == 31u);
    assert(fm1lab_instrument_valid(&instrument));
    assert(fm1lab_patch_set_voice(&instrument, 4, voice) == -1);
    voice[134] = 32;
    assert(fm1lab_patch_set_voice(&instrument, 1, voice) == -1);
}

static void test_replace_voice_preserves_slot_state_and_isolates_patch(void)
{
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
    assert(memcmp(instrument.patches[0].voice, before[0].voice, FM1LAB_PATCH_BYTES) == 0);
    assert(memcmp(instrument.patches[1].voice, before[1].voice, FM1LAB_PATCH_BYTES) == 0);
    assert(memcmp(instrument.patches[3].voice, before[3].voice, FM1LAB_PATCH_BYTES) == 0);
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
}

static void test_official_algorithm_carriers(void)
{
    assert(fm1lab_carrier_mask(0) == 0b000101);
    assert(fm1lab_carrier_mask(1) == 0b000101);
    assert(fm1lab_carrier_mask(18) == 0b011001);
    assert(fm1lab_carrier_mask(31) == 0b111111);
    assert(fm1lab_carrier_mask(32) == fm1lab_carrier_mask(0));
    assert(fm1lab_operator_depth(0, 2) == 1);
    assert(fm1lab_operator_depth(0, 0) == 0);
    assert(fm1lab_operator_modulates(0, 6, 5) == 1);
    assert(fm1lab_operator_modulates(0, 5, 4) == 1);
    assert(fm1lab_operator_modulates(0, 4, 3) == 1);
    assert(fm1lab_operator_modulates(0, 2, 1) == 1);
    assert(fm1lab_operator_modulates(0, 6, 4) == 0);
    assert(fm1lab_operator_modulates(0, 0, 1) == -1);
}

static void test_non_destructive_macro_render(void)
{
    fm1lab_instrument_t instrument;
    uint8_t voice[FM1LAB_PATCH_BYTES] = {0};
    uint8_t rendered[FM1LAB_PATCH_BYTES];
    fm1lab_instrument_init(&instrument, "MACRO TEST");
    voice[134] = 0; /* Algorithm 1: OP1 and OP3 are carriers. */
    voice[135] = 2;
    voice[105 + 16] = 40; /* OP1 carrier output. */
    voice[84 + 16] = 40;  /* OP2 modulator output. */
    voice[105 + 6] = 50;  /* OP1 carrier envelope level 3. */
    voice[84 + 6] = 40;   /* OP2 modulator envelope level 3. */
    voice[84 + 7] = 60;   /* OP2 modulator envelope level 4. */
    voice[84 + 6] = 40;   /* OP2 modulator envelope level 3. */
    voice[84 + 7] = 60;   /* OP2 modulator envelope level 4. */
    voice[137] = 40;
    voice[140] = 20;
    voice[155] = 64u; /* reserved pan sentinel defaults to center */
    assert(fm1lab_patch_set_voice(&instrument, 0, voice) == 0);

    instrument.patches[0].macros[FM1LAB_BRIGHTNESS] = 100;
    instrument.patches[0].macros[FM1LAB_BODY] = 100;
    instrument.patches[0].macros[FM1LAB_MOTION] = 100;
    instrument.patches[0].macros[FM1LAB_DIRT] = 100;
    assert(fm1lab_render_patch(&instrument, 0, rendered) == 0);
    assert(rendered[105 + 16] == voice[105 + 16]);
    assert(rendered[84 + 16] > voice[84 + 16]);
    assert(rendered[105 + 6] > voice[105 + 6]);
    assert(rendered[84 + 6] > voice[84 + 6]);
    assert(rendered[84 + 7] < voice[84 + 7]);
    assert(rendered[137] == voice[137]);
    assert(rendered[140] == voice[140]);
    assert(rendered[135] == 6);
    assert(memcmp(instrument.patches[0].voice, voice, sizeof voice) == 0);
    assert(rendered[155] == voice[155]);
}

static void test_validation_rejects_bad_state(void)
{
    fm1lab_instrument_t instrument;
    fm1lab_instrument_init(&instrument, "VALIDATE");
    instrument.patches[2].macros[FM1LAB_DIRT] = 101;
    assert(!fm1lab_instrument_valid(&instrument));
    instrument.patches[2].macros[FM1LAB_DIRT] = 50;
    instrument.patches[2].enabled = 2;
    assert(!fm1lab_instrument_valid(&instrument));
    instrument.patches[2].enabled = 1;
    instrument.patches[2].octave = 5;
    assert(!fm1lab_instrument_valid(&instrument));
    instrument.patches[2].octave = 0;
    instrument.patches[2].fine_cents = 101;
    assert(!fm1lab_instrument_valid(&instrument));
    instrument.patches[2].fine_cents = 0;
    instrument.version++;
    assert(!fm1lab_instrument_valid(&instrument));
}

int main(void)
{
    test_four_patch_contract();
    test_replace_voice_preserves_slot_state_and_isolates_patch();
    test_official_algorithm_carriers();
    test_non_destructive_macro_render();
    test_validation_rejects_bad_state();
    puts("FM1-Lab native Instrument C tests passed.");
    return 0;
}
