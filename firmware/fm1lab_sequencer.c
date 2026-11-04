#include "fm1lab_sequencer.h"
#include "instrument.h"
#include <string.h>

/* The alternate-tuning flag is part of the native step encoding. */
static unsigned bound_tracks[FM1LAB_SEQUENCE_LANES] = {255U, 255U};
static unsigned selected_lane;

static int lane_track(unsigned lane, unsigned *track)
{
    if (lane >= FM1LAB_SEQUENCE_LANES || bound_tracks[lane] >= GROOVE_TRACKS)
        return -1;
    if (groove_track_role(bound_tracks[lane]) != GROOVE_TRACK_NORMAL ||
        groove_source_type(bound_tracks[lane]) != GROOVE_DX7)
        return -2;
    *track = bound_tracks[lane];
    return 0;
}

int fm1lab_sequencer_bind(unsigned a, unsigned b)
{
    if (a >= GROOVE_TRACKS || b >= GROOVE_TRACKS || a == b)
        return -1;
    if (groove_track_role(a) != GROOVE_TRACK_NORMAL ||
        groove_track_role(b) != GROOVE_TRACK_NORMAL ||
        groove_source_type(a) != GROOVE_DX7 || groove_source_type(b) != GROOVE_DX7)
        return -2;
    bound_tracks[0] = a;
    bound_tracks[1] = b;
    selected_lane = 0;
    return 0;
}

int fm1lab_sequencer_read(fm1lab_sequence_state_t *state)
{
    if (!state) return -1;
    unsigned track;
    memset(state, 0, sizeof *state);
    state->tempo = (uint16_t)groove_tempo_saved();
    state->swing = (uint8_t)groove_sequence_setting(0);
    state->selected_lane = (uint8_t)selected_lane;
    for (unsigned lane = 0; lane < FM1LAB_SEQUENCE_LANES; ++lane) {
        int result = lane_track(lane, &track);
        if (result) return result;
        fm1lab_sequence_lane_t *out = &state->lanes[lane];
        struct groove_step native[FM1LAB_SEQUENCE_STEPS];
        out->backend_track = (uint8_t)track;
        out->length = (uint8_t)groove_sequence_length(track);
        groove_sequence_read(track, 0, native, FM1LAB_SEQUENCE_STEPS);
        for (unsigned step = 0; step < FM1LAB_SEQUENCE_STEPS; ++step) {
            fm1lab_sequence_step_t *dst = &out->steps[step];
            dst->active = native[step].velocity != 0;
            dst->note = native[step].note;
            dst->velocity = native[step].velocity;
            dst->gate = (uint8_t)groove_step_setting(track, step, 1);
            dst->tie = (uint8_t)groove_step_setting(track, step, 4);
            dst->alt = (uint8_t)((native[step].drums & GROOVE_STEP_ALT) != 0);
        }
    }
    return 0;
}

int fm1lab_sequencer_set_tempo(int bpm)
{
    if (bound_tracks[0] >= GROOVE_TRACKS || bound_tracks[1] >= GROOVE_TRACKS) return -1;
    if (bpm < 30 || bpm > 300) return -1;
    int delta = bpm - (int)groove_tempo_saved();
    groove_tempo_edit(delta);
    return groove_tempo_saved() == (unsigned)bpm ? 0 : -2;
}

int fm1lab_sequencer_set_swing(unsigned swing)
{
    if (bound_tracks[0] >= GROOVE_TRACKS || bound_tracks[1] >= GROOVE_TRACKS) return -1;
    if (swing > 50) return -1;
    int delta = (int)swing - (int)groove_sequence_setting(0);
    groove_sequence_setting_edit(0, delta);
    return groove_sequence_setting(0) == swing ? 0 : -2;
}

int fm1lab_sequencer_set_length(unsigned lane, unsigned steps)
{
    unsigned track;
    int result = lane_track(lane, &track);
    if (result) return result;
    if (steps != 16 && steps != 32 && steps != 48 && steps != 64) return -1;
    groove_sequence_length_set(track, steps);
    return groove_sequence_length(track) == steps ? 0 : -2;
}

int fm1lab_sequencer_set_step(unsigned lane, unsigned step,
                              const fm1lab_sequence_step_t *value)
{
    unsigned track;
    int result = lane_track(lane, &track);
    if (result) return result;
    if (!value || step >= FM1LAB_SEQUENCE_STEPS || value->note > 127 ||
        value->active > 1 || value->velocity > 127 || value->gate > 15 || value->tie > 1 || value->alt > 1)
        return -1;

    unsigned velocity = value->active ? (value->velocity ? value->velocity : 1U) : 0U;
    groove_sequence_edit(track, step, value->note, velocity);

    int gate_delta = (int)value->gate - (int)groove_step_setting(track, step, 1);
    if (gate_delta && groove_step_setting_edit(track, step, 1, gate_delta))
        return -2;
    int tie_delta = (int)value->tie - (int)groove_step_setting(track, step, 4);
    if (tie_delta && groove_step_setting_edit(track, step, 4, tie_delta))
        return -2;

    struct groove_step native;
    groove_sequence_read(track, step, &native, 1);
    unsigned alt = (native.drums & GROOVE_STEP_ALT) != 0;
    if (alt != value->alt) {
        native.drums = (uint16_t)((native.drums & ~GROOVE_STEP_ALT) |
                                  (value->alt ? GROOVE_STEP_ALT : 0U));
        groove_sequence_replace(track, step, &native, 1);
    }
    return 0;
}

int fm1lab_sequencer_select_lane(unsigned lane)
{
    unsigned track;
    int result = lane_track(lane, &track);
    (void)track;
    if (result) return result;
    selected_lane = lane;
    return 0;
}
