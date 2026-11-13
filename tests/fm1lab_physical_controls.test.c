#include <assert.h>
#include "fm1lab_physical_controls.h"

int main(void)
{
    assert(FM1LAB_ENCODER_SELECT == 0);
    assert(FM1LAB_ENCODER_ALGORITHM == 1);
    assert(FM1LAB_ENCODER_PRESETS == 2);
    assert(FM1LAB_ENCODER_KNOB_1 == 3);
    assert(FM1LAB_ENCODER_KNOB_2 == 4);
    assert(FM1LAB_ENCODER_KNOB_3 == 5);
    assert(FM1LAB_ENCODER_KNOB_4 == 6);

    /* The physical SEQUENCER track selector is SELECT (scan index 0).
     * Both directions switch tracks; PRESETS (scan index 2) must not. */
    unsigned next_lane=99u;
    assert(fm1lab_sequencer_track_select_dispatch(7u,7u,8u,
        FM1LAB_ENCODER_SELECT,1,0u,&next_lane) && next_lane==1u);
    assert(fm1lab_sequencer_track_select_dispatch(7u,7u,8u,
        FM1LAB_ENCODER_SELECT,-1,0u,&next_lane) && next_lane==1u);
    assert(fm1lab_sequencer_track_select_dispatch(8u,7u,8u,
        FM1LAB_ENCODER_SELECT,-1,1u,&next_lane) && next_lane==0u);
    assert(fm1lab_sequencer_track_select_dispatch(7u,7u,8u,
        FM1LAB_ENCODER_SELECT,4,1u,&next_lane) && next_lane==0u);
    assert(!fm1lab_sequencer_track_select_dispatch(7u,7u,8u,
        FM1LAB_ENCODER_PRESETS,1,0u,&next_lane));
    assert(!fm1lab_sequencer_track_select_dispatch(0u,7u,8u,
        FM1LAB_ENCODER_PRESETS,1,0u,&next_lane));
    assert(!fm1lab_sequencer_track_select_dispatch(7u,7u,8u,
        FM1LAB_ENCODER_PRESETS,0,0u,&next_lane));
    assert(!fm1lab_sequencer_track_select_dispatch(7u,7u,8u,
        FM1LAB_ENCODER_SELECT,1,0u,0));

    assert(fm1lab_master_trim_advance(100u, 1) == 100u);
    assert(fm1lab_master_trim_advance(100u, -1) == 99u);
    assert(fm1lab_master_gain_scale(4096u, 50u) == 2048u);
    assert(fm1lab_chaos_rate_advance(20u, 1) == 30u);
    assert(fm1lab_ble_enabled_from_turn(1u, -2) == 0u);
    assert(fm1lab_ble_enabled_from_turn(0u, 3) == 1u);
    return 0;
}
