#include <assert.h>
#include <stdint.h>
#include "../app/fm1lab_input_route.h"

static unsigned bit_count(uint32_t value)
{
    unsigned count = 0u;
    while (value) { count += value & 1u; value >>= 1u; }
    return count;
}

int main(void)
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
                /* This fails if two routing rules claim the same physical report. */
                assert(bit_count(claims) <= 1u);
                assert(fm1lab_input_route(page, encoder, focus) == expected);
            }
        }
    }

    /* The track control is a toggle: every nonzero report switches once,
     * irrespective of the quadrature delta magnitude. */
    assert(fm1lab_input_track_next(0u, 1) == 1u);
    assert(fm1lab_input_track_next(1u, -1) == 0u);
    assert(fm1lab_input_track_next(0u, 7) == 1u);
    assert(fm1lab_input_track_next(1u, -9) == 0u);
    assert(fm1lab_input_track_next(0u, 0) == 0u);

    /* Golden routing cases from the pre-regression HOME/SEQ behavior. */
    assert(fm1lab_input_route(FM1LAB_INPUT_PAGE_HOME, 0u, 0u) ==
           FM1LAB_INPUT_PATCH_FOCUS_STEP);
    assert(fm1lab_input_route(FM1LAB_INPUT_PAGE_HOME, 2u, 0u) ==
           FM1LAB_INPUT_INSTRUMENT_STEP);
    assert(fm1lab_input_route(FM1LAB_INPUT_PAGE_HOME, 2u, 1u) ==
           FM1LAB_INPUT_PATCH_PRESET_STEP);
    assert(fm1lab_input_route(FM1LAB_INPUT_PAGE_SEQ, 0u, 0u) ==
           FM1LAB_INPUT_TRACK_TOGGLE);
    assert(fm1lab_input_route(FM1LAB_INPUT_PAGE_SEQ, 2u, 0u) ==
           FM1LAB_INPUT_TRACK_INSTRUMENT_STEP);
    assert(fm1lab_input_route(FM1LAB_INPUT_PAGE_SEQ_OPTIONS, 0u, 1u) ==
           FM1LAB_INPUT_TRACK_TOGGLE);
    assert(fm1lab_input_route(FM1LAB_INPUT_PAGE_SEQ_OPTIONS, 2u, 1u) ==
           FM1LAB_INPUT_TRACK_INSTRUMENT_STEP);
    return 0;
}
