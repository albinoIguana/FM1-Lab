#ifndef FM1LAB_PHYSICAL_CONTROLS_H
#define FM1LAB_PHYSICAL_CONTROLS_H

/* Physical FM1 panel labels are the public names. Values are the verified
 * upstream fm1_controls.encoders[] scan indices, not physical numbering. */
enum fm1lab_encoder {
    FM1LAB_ENCODER_SELECT = 0,    /* panel: SELECT */
    FM1LAB_ENCODER_ALGORITHM = 1, /* panel: ALGORITHM */
    FM1LAB_ENCODER_PRESETS = 2,   /* panel: PRESETS */
    FM1LAB_ENCODER_KNOB_1 = 3,    /* panel: KNOB 1 */
    FM1LAB_ENCODER_KNOB_2 = 4,    /* panel: KNOB 2 */
    FM1LAB_ENCODER_KNOB_3 = 5,    /* panel: KNOB 3 */
    FM1LAB_ENCODER_KNOB_4 = 6,    /* panel: KNOB 4 */
    FM1LAB_ENCODER_COUNT = 7
};

/* MASTER is an ADC input, not one of the seven rotary encoders. */
enum { FM1LAB_ADC_MASTER = 4u };

/* SELECT is a directional two-track selector: clockwise selects T2,
 * counter-clockwise selects T1. Use direction, not raw quadrature count:
 * the device encoder can report multiple counts for one physical detent. */
static inline unsigned fm1lab_sequencer_lane_advance(unsigned lane, int delta)
{
    if (delta > 0) return 1u;
    if (delta < 0) return 0u;
    return lane & 1u;
}

/* Global-page controls. The hardware MASTER pot remains the primary volume
 * control; K1 provides a software trim. BLE and Maelstrom use direction-based
 * selectors so multi-count encoder reports still produce one clear action. */
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
