#include <assert.h>
#include <stdint.h>
#include "../app/fm1lab_preset_control.h"

int main(void)
{
    /* Home starts in whole-Instrument mode; selecting a patch changes target. */
    assert(fm1lab_preset_target(0u) == FM1LAB_PRESET_TARGET_INSTRUMENT);
    assert(fm1lab_preset_target(1u) == FM1LAB_PRESET_TARGET_PATCH);

    /* PRESETS is enabled only on Home, regardless of patch focus. */
    assert(fm1lab_preset_encoder_enabled(0u, 0u));
    assert(!fm1lab_preset_encoder_enabled(1u, 0u)); /* Macro */
    assert(!fm1lab_preset_encoder_enabled(2u, 0u)); /* Global */
    assert(!fm1lab_preset_encoder_enabled(7u, 0u)); /* Sequencer */
    assert(!fm1lab_preset_encoder_enabled(20u, 0u)); /* Maelstrom */
    assert(!fm1lab_preset_encoder_enabled(22u, 0u)); /* Save */

    /* The cursor spans all 128 curated individual voices and wraps both ways. */
    assert(fm1lab_preset_cursor_advance(0u, -1, 128u) == 127u);
    assert(fm1lab_preset_cursor_advance(127u, 1, 128u) == 0u);
    assert(fm1lab_preset_cursor_advance(12u, 3, 128u) == 15u);
    assert(fm1lab_preset_cursor_advance(12u, -3, 128u) == 9u);
    assert(fm1lab_preset_cursor_advance(12u, 0, 128u) == 12u);
    assert(fm1lab_preset_cursor_advance(12u, 99, 0u) == 0u);
    return 0;
}
