#include "fm1lab_dx7_preset.h"
#include "fm1lab_dx7_data.h"
#include "fm1lab_input_dispatch.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static int dispatch_patch_preset(void *context, uint8_t patch, int delta)
{
    static uint8_t cursor[FM1LAB_PATCH_COUNT] = {255u,255u,255u,255u};
    uint8_t next;
    enum fm1lab_dx7_preset_result r = fm1lab_dx7_patch_preset_step(
        (fm1lab_instrument_t *)context, patch, cursor[patch], delta, &next);
    if (r != FM1LAB_DX7_PRESET_OK) return -1;
    cursor[patch] = next;
    return 0;
}

int main(void)
{
    uint8_t voice[FM1LAB_PATCH_BYTES];
    for (uint32_t i = 0; i < FM1LAB_DX7_VOICE_COUNT; ++i) {
        assert(fm1lab_dx7_decode_voice(i, voice) == FM1LAB_DX7_PRESET_OK);
        assert(fm1lab_dx7_voice_valid(voice));
    }

    char bad[FM1LAB_DX7_Z85_BYTES + 1u];
    memcpy(bad, fm1lab_dx7_voice_z85, FM1LAB_DX7_Z85_BYTES);
    bad[37] = '\\';
    bad[FM1LAB_DX7_Z85_BYTES] = '\0';
    assert(fm1lab_dx7_decode_z85(bad, FM1LAB_DX7_Z85_BYTES,
        0u, 1u, voice) == FM1LAB_DX7_PRESET_DECODE_FAILED);
    assert(fm1lab_dx7_decode_z85(fm1lab_dx7_voice_z85,
        sizeof fm1lab_dx7_voice_z85 - 1u, FM1LAB_DX7_VOICE_COUNT,
        FM1LAB_DX7_VOICE_COUNT, voice) == FM1LAB_DX7_PRESET_INDEX_INVALID);
    assert(fm1lab_dx7_decode_z85(fm1lab_dx7_voice_z85, 20u, 0u, 1u, voice)
        == FM1LAB_DX7_PRESET_DATA_MISSING);
    memset(voice, 0, sizeof voice);
    voice[0] = 128u;
    fm1lab_instrument_t invalid_voice_instrument;
    fm1lab_instrument_init(&invalid_voice_instrument, "INVALID VOICE");
    assert(fm1lab_dx7_replace_voice(&invalid_voice_instrument, 0u, voice)
        == FM1LAB_DX7_PRESET_VOICE_INVALID);

    uint8_t expected[FM1LAB_PATCH_BYTES];
    assert(fm1lab_dx7_decode_voice(0u, expected) == FM1LAB_DX7_PRESET_OK);

    fm1lab_instrument_t instrument;
    fm1lab_instrument_init(&instrument, "PRESET TEST");
    fm1lab_patch_t before = instrument.patches[2];
    uint8_t next = 255u;
    assert(fm1lab_dx7_patch_preset_step(&instrument, 2u, 255u, 1, &next)
        == FM1LAB_DX7_PRESET_OK);
    assert(next == 0u);
    assert(memcmp(instrument.patches[2].voice, expected, FM1LAB_PATCH_BYTES - 1u) == 0);
    assert(instrument.patches[2].level == before.level);
    assert(instrument.patches[2].enabled == before.enabled);
    assert(instrument.patches[2].octave == before.octave);
    assert(instrument.patches[2].fine_cents == before.fine_cents);
    assert(instrument.patches[2].voice[155] == before.voice[155]);
    assert(memcmp(instrument.patches[2].macros, before.macros, FM1LAB_MACRO_COUNT) == 0);
    assert(fm1lab_instrument_valid(&instrument));

    /* Exercise the same physical PRESETS route and dispatcher as app.c,
     * ending in the production bundled-data decode + patch replacement. */
    fm1lab_input_state_t state = {0};
    fm1lab_input_ops_t ops = {0};
    state.patch_focus = 1u;
    state.patch = 1u;
    ops.patch_preset_step = dispatch_patch_preset;
    enum fm1lab_input_action action = fm1lab_input_route(
        FM1LAB_INPUT_PAGE_HOME, FM1LAB_ENCODER_PRESETS, 1u);
    assert(action == FM1LAB_INPUT_PATCH_PRESET_STEP);
    assert(fm1lab_input_apply(&state, action, 1, 32u,
        FM1LAB_PATCH_COUNT, &ops, &instrument) == FM1LAB_INPUT_RESULT_APPLIED);

    uint8_t prior = next;
    assert(fm1lab_dx7_patch_preset_step(&instrument, FM1LAB_PATCH_COUNT,
        next, 1, &next) == FM1LAB_DX7_PRESET_INDEX_INVALID);
    assert(fm1lab_dx7_replace_voice(&instrument, FM1LAB_PATCH_COUNT, expected)
        == FM1LAB_DX7_PRESET_INDEX_INVALID);
    assert(next == prior);
    assert(fm1lab_dx7_patch_preset_step(&instrument, 0u, next, 0, &next)
        == FM1LAB_DX7_PRESET_BAD_ARGUMENT);
    instrument.magic = 0u;
    assert(fm1lab_dx7_patch_preset_step(&instrument, 0u, prior, 1, &next)
        == FM1LAB_DX7_PRESET_PATCH_REPLACE_FAILED);
    return 0;
}
