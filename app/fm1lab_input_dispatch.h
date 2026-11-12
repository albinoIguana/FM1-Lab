#ifndef FM1LAB_INPUT_DISPATCH_H
#define FM1LAB_INPUT_DISPATCH_H

#include <stdint.h>
#include "fm1lab_input_route.h"

enum fm1lab_input_result {
    FM1LAB_INPUT_RESULT_NONE = 0u,
    FM1LAB_INPUT_RESULT_APPLIED,
    FM1LAB_INPUT_RESULT_LOAD_BUSY,
    FM1LAB_INPUT_RESULT_LOAD_FAILED,
    FM1LAB_INPUT_RESULT_SAVE_FAILED_BLOCKED,
    FM1LAB_INPUT_RESULT_SAVE_FAILED_CHANGED,
    FM1LAB_INPUT_RESULT_PATCH_FAILED
};

enum { FM1LAB_INPUT_LOAD_ENGINE_BANK = 1, FM1LAB_INPUT_LOAD_UI_ONLY = 2 };

typedef struct {
    uint8_t patch_focus;
    uint8_t patch;
    uint8_t lane;
    uint8_t instrument;
    uint8_t track_instrument[2];
    uint8_t track_patch[2];
    uint8_t instrument_apply_pending;
    uint8_t instrument_dirty;
} fm1lab_input_state_t;

typedef struct {
    int (*save_current)(void *context);
    int (*load_instrument)(void *context, uint8_t lane, uint8_t instrument, int mode);
    int (*patch_preset_step)(void *context, uint8_t patch, int delta);
    void (*publish)(void *context);
} fm1lab_input_ops_t;

/* Shared action executor: app.c and host tests call this exact implementation.
 * load_instrument returns 0 on success, 1 when the audio apply is busy, else -1. */
static inline enum fm1lab_input_result fm1lab_input_apply(
        fm1lab_input_state_t *state, enum fm1lab_input_action action, int delta,
        uint8_t instrument_count, uint8_t patch_count,
        const fm1lab_input_ops_t *ops, void *context)
{
    int save_failed = 0;
    int load_result;
    uint8_t next;
    if (!state || !ops || !delta)
        return FM1LAB_INPUT_RESULT_NONE;

    switch (action) {
    case FM1LAB_INPUT_PATCH_FOCUS_STEP: {
        int value;
        if (!patch_count) return FM1LAB_INPUT_RESULT_NONE;
        value = ((int)state->patch + delta) % (int)patch_count;
        if (value < 0) value += patch_count;
        state->patch = (uint8_t)value;
        state->patch_focus = 1u;
        state->track_patch[state->lane & 1u] = state->patch;
        if (ops->publish) ops->publish(context);
        return FM1LAB_INPUT_RESULT_APPLIED;
    }
    case FM1LAB_INPUT_PATCH_PRESET_STEP:
        if (state->instrument_apply_pending)
            return FM1LAB_INPUT_RESULT_LOAD_BUSY;
        if (!ops->patch_preset_step ||
            ops->patch_preset_step(context, state->patch, delta) != 0)
            return FM1LAB_INPUT_RESULT_PATCH_FAILED;
        return FM1LAB_INPUT_RESULT_APPLIED;

    case FM1LAB_INPUT_INSTRUMENT_STEP:
    case FM1LAB_INPUT_TRACK_INSTRUMENT_STEP:
        if (state->instrument_apply_pending)
            return FM1LAB_INPUT_RESULT_LOAD_BUSY;
        if (!instrument_count || !ops->load_instrument)
            return FM1LAB_INPUT_RESULT_LOAD_FAILED;
        if (state->instrument_dirty && ops->save_current) {
            save_failed = ops->save_current(context) != 0;
            if (!save_failed) state->instrument_dirty = 0u;
        }
        {
            int value = ((int)state->track_instrument[state->lane & 1u] + delta) %
                (int)instrument_count;
            if (value < 0) value += instrument_count;
            next = (uint8_t)value;
        }
        if (next == state->instrument)
            return save_failed ? FM1LAB_INPUT_RESULT_SAVE_FAILED_CHANGED
                               : FM1LAB_INPUT_RESULT_NONE;
        load_result = ops->load_instrument(context, state->lane, next,
                                            FM1LAB_INPUT_LOAD_ENGINE_BANK);
        if (load_result == 1) return FM1LAB_INPUT_RESULT_LOAD_BUSY;
        if (load_result != 0) return FM1LAB_INPUT_RESULT_LOAD_FAILED;
        state->instrument = next;
        state->track_instrument[state->lane & 1u] = next;
        state->patch_focus = 0u;
        state->instrument_apply_pending = 1u;
        if (ops->publish) ops->publish(context);
        return save_failed ? FM1LAB_INPUT_RESULT_SAVE_FAILED_CHANGED
                           : FM1LAB_INPUT_RESULT_APPLIED;

    case FM1LAB_INPUT_TRACK_TOGGLE:
        if (state->instrument_apply_pending)
            return FM1LAB_INPUT_RESULT_LOAD_BUSY;
        if (!ops->load_instrument)
            return FM1LAB_INPUT_RESULT_LOAD_FAILED;
        if (state->instrument_dirty && ops->save_current) {
            if (ops->save_current(context) != 0)
                return FM1LAB_INPUT_RESULT_SAVE_FAILED_BLOCKED;
            state->instrument_dirty = 0u;
        }
        next = fm1lab_input_track_next(state->lane, delta);
        if (next == state->lane)
            return FM1LAB_INPUT_RESULT_NONE;
        load_result = ops->load_instrument(context, next,
            state->track_instrument[next], FM1LAB_INPUT_LOAD_UI_ONLY);
        if (load_result == 1) return FM1LAB_INPUT_RESULT_LOAD_BUSY;
        if (load_result != 0) return FM1LAB_INPUT_RESULT_LOAD_FAILED;
        state->lane = next;
        state->instrument = state->track_instrument[next];
        state->patch = state->track_patch[next];
        state->patch_focus = 0u;
        if (ops->publish) ops->publish(context);
        return FM1LAB_INPUT_RESULT_APPLIED;

    case FM1LAB_INPUT_NONE:
    default:
        return FM1LAB_INPUT_RESULT_NONE;
    }
}

#endif
