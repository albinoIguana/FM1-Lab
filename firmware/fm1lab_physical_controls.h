#ifndef FM1LAB_PHYSICAL_CONTROLS_H
#define FM1LAB_PHYSICAL_CONTROLS_H

#include <stdint.h>

/* Physical FM1 panel labels are the public names. Values are the verified
 * upstream fm1_controls.encoders[] scan indices, not physical numbering. */
enum fm1lab_encoder {
    FM1LAB_ENCODER_SELECT = 0,
    FM1LAB_ENCODER_ALGORITHM = 1,
    FM1LAB_ENCODER_PRESETS = 2,
    FM1LAB_ENCODER_KNOB_1 = 3,
    FM1LAB_ENCODER_KNOB_2 = 4,
    FM1LAB_ENCODER_KNOB_3 = 5,
    FM1LAB_ENCODER_KNOB_4 = 6,
    FM1LAB_ENCODER_COUNT = 7
};

enum { FM1LAB_ADC_MASTER = 4u };

/* The SEQUENCER's SELECT encoder selects its two tracks. Treat any nonzero
 * report as one detent: FM1 hardware can report multiple quadrature counts
 * for a single physical detent. */
static inline unsigned fm1lab_sequencer_lane_advance(unsigned lane, int delta)
{
    return delta ? ((lane ^ 1u) & 1u) : (lane & 1u);
}

/* Full input-dispatch decision shared by input() and host tests. The encoder
 * argument is the actual index passed to plat_enc(k), not a simulated label. */
static inline unsigned fm1lab_sequencer_track_select_dispatch(unsigned page,
        unsigned sequencer_page, unsigned options_page, unsigned encoder,
        int delta, unsigned lane, unsigned *next_lane)
{
    if (encoder != FM1LAB_ENCODER_SELECT || !delta || !next_lane ||
        (page != sequencer_page && page != options_page))
        return 0u;
    *next_lane=fm1lab_sequencer_lane_advance(lane,delta);
    return 1u;
}

static inline unsigned fm1lab_master_trim_advance(unsigned current, int delta)
{
    int next=(int)current+delta;
    if(next<0)next=0;
    if(next>100)next=100;
    return (unsigned)next;
}
static inline uint32_t fm1lab_master_gain_scale(uint32_t master_q12, unsigned trim_pct)
{
    if(trim_pct>100u)trim_pct=100u;
    if(master_q12>4096u)master_q12=4096u;
    return (master_q12*trim_pct)/100u;
}
static inline unsigned fm1lab_chaos_rate_advance(unsigned current, int delta)
{
    int next=(int)current+delta*10;
    if(next<0)next=0;
    if(next>100)next=100;
    return (unsigned)next;
}
static inline unsigned fm1lab_ble_enabled_from_turn(unsigned current, int delta)
{
    if(delta>0)return 1u;
    if(delta<0)return 0u;
    return current?1u:0u;
}

#endif
