#include <assert.h>
#include <stdint.h>
#include "../app/fm1lab_preset_control.h"

int main(void)
{
    assert(fm1lab_preset_target(0u, 0u) == FM1LAB_PRESET_TARGET_INSTRUMENT);
    assert(fm1lab_preset_target(0u, 1u) == FM1LAB_PRESET_TARGET_INSTRUMENT);
    assert(fm1lab_preset_target(1u, 0u) == FM1LAB_PRESET_TARGET_PATCH);
    assert(fm1lab_preset_target(1u, 1u) == FM1LAB_PRESET_TARGET_PATCH);

    assert(fm1lab_preset_encoder_enabled(0u, 0u, 7u, 0u));
    assert(fm1lab_preset_encoder_enabled(7u, 0u, 7u, 1u));
    assert(!fm1lab_preset_encoder_enabled(7u, 0u, 7u, 0u));
    assert(!fm1lab_preset_encoder_enabled(8u, 0u, 7u, 1u));
    assert(!fm1lab_preset_encoder_enabled(9u, 0u, 7u, 1u));
    assert(!fm1lab_preset_encoder_enabled(1u, 0u, 7u, 1u));
    assert(!fm1lab_preset_encoder_enabled(2u, 0u, 7u, 1u));
    assert(!fm1lab_preset_encoder_enabled(20u, 0u, 7u, 1u));
    assert(!fm1lab_preset_encoder_enabled(22u, 0u, 7u, 1u));

    assert(fm1lab_preset_cursor_advance(0u, -1, 128u) == 127u);
    assert(fm1lab_preset_cursor_advance(127u, 1, 128u) == 0u);
    assert(fm1lab_preset_cursor_advance(12u, 3, 128u) == 15u);
    assert(fm1lab_preset_cursor_advance(12u, -3, 128u) == 9u);
    assert(fm1lab_preset_cursor_advance(12u, 0, 128u) == 12u);
    assert(fm1lab_preset_cursor_advance(12u, 99, 0u) == 0u);
    return 0;
}
