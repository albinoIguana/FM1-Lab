#include "fm1lab_physical_controls.h"
#include <assert.h>

int main(void)
{
    /* Lock the printed panel-to-source mapping so future UI work cannot
     * silently swap physical controls by assuming array order. */
    assert(FM1LAB_ENCODER_SELECT == 0);
    assert(FM1LAB_ENCODER_ALGORITHM == 1);
    assert(FM1LAB_ENCODER_PRESETS == 2);
    assert(FM1LAB_ENCODER_KNOB_1 == 3);
    assert(FM1LAB_ENCODER_KNOB_2 == 4);
    assert(FM1LAB_ENCODER_KNOB_3 == 5);
    assert(FM1LAB_ENCODER_KNOB_4 == 6);
    assert(FM1LAB_ENCODER_COUNT == 7);
    assert(FM1LAB_ADC_MASTER == 4u);

    /* SELECT is directional and robust to quadrature counts larger than one. */
    assert(fm1lab_sequencer_lane_advance(0u, 1) == 1u);
    assert(fm1lab_sequencer_lane_advance(1u, 1) == 1u);
    assert(fm1lab_sequencer_lane_advance(0u, 2) == 1u);
    assert(fm1lab_sequencer_lane_advance(1u, 4) == 1u);
    assert(fm1lab_sequencer_lane_advance(0u, -1) == 0u);
    assert(fm1lab_sequencer_lane_advance(1u, -1) == 0u);
    assert(fm1lab_sequencer_lane_advance(1u, -2) == 0u);
    assert(fm1lab_sequencer_lane_advance(0u, 0) == 0u);
    return 0;
}

    /* Global page: K1 trim, K2/K3 are existing swing/tempo, K4 BLE direction. */
    assert(fm1lab_master_trim_advance(100u, 1) == 100u);
    assert(fm1lab_master_trim_advance(50u, -7) == 43u);
    assert(fm1lab_master_trim_advance(3u, -8) == 0u);
    assert(fm1lab_master_gain_scale(4096u, 100u) == 4096u);
    assert(fm1lab_master_gain_scale(4096u, 50u) == 2048u);
    assert(fm1lab_master_gain_scale(4096u, 0u) == 0u);
    assert(fm1lab_chaos_rate_advance(0u, 1) == 10u);
    assert(fm1lab_chaos_rate_advance(90u, 2) == 100u);
    assert(fm1lab_chaos_rate_advance(10u, -2) == 0u);
    assert(fm1lab_ble_enabled_from_turn(0u, 2) == 1u);
    assert(fm1lab_ble_enabled_from_turn(1u, -2) == 0u);
    assert(fm1lab_ble_enabled_from_turn(1u, 0) == 1u);
