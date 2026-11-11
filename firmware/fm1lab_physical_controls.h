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

/* SELECT chooses sequencer lane A/B. PRESETS changes the selected lane's
 * Instrument on the first Sequencer page; negative SELECT turns wrap too. */
static inline unsigned fm1lab_sequencer_lane_advance(unsigned lane, int delta)
{
    int next = ((int)(lane & 1u) + delta) % 2;
    if (next < 0) next += 2;
    return (unsigned)next;
}

#endif
