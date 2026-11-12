#ifndef FM1LAB_INPUT_ROUTE_H
#define FM1LAB_INPUT_ROUTE_H

#include <stdint.h>

/* These values are kept in lockstep with app.c's PAGE_* enum. */
enum {
    FM1LAB_INPUT_PAGE_HOME = 0u,
    FM1LAB_INPUT_PAGE_SEQ = 7u,
    FM1LAB_INPUT_PAGE_SEQ_OPTIONS = 8u,
    FM1LAB_INPUT_PAGE_COUNT = 23u
};

enum fm1lab_input_action {
    FM1LAB_INPUT_NONE = 0u,
    FM1LAB_INPUT_PATCH_FOCUS_STEP,
    FM1LAB_INPUT_PATCH_PRESET_STEP,
    FM1LAB_INPUT_INSTRUMENT_STEP,
    FM1LAB_INPUT_TRACK_TOGGLE,
    FM1LAB_INPUT_TRACK_INSTRUMENT_STEP
};

/* Return all matching claims so tests can detect overlapping routing rules.
 * Production routing accepts only a single claim; ambiguity fails closed. */
static inline uint32_t fm1lab_input_route_claims(uint8_t page,
        uint8_t encoder, uint8_t patch_focus)
{
    uint32_t claims = 0u;
    if (page == FM1LAB_INPUT_PAGE_HOME) {
        if (encoder == 0u)
            claims |= (uint32_t)1u << FM1LAB_INPUT_PATCH_FOCUS_STEP;
        if (encoder == 2u)
            claims |= (uint32_t)1u << (patch_focus
                ? FM1LAB_INPUT_PATCH_PRESET_STEP
                : FM1LAB_INPUT_INSTRUMENT_STEP);
    } else if (page == FM1LAB_INPUT_PAGE_SEQ ||
               page == FM1LAB_INPUT_PAGE_SEQ_OPTIONS) {
        if (encoder == 0u)
            claims |= (uint32_t)1u << FM1LAB_INPUT_TRACK_TOGGLE;
        if (encoder == 2u)
            claims |= (uint32_t)1u << FM1LAB_INPUT_TRACK_INSTRUMENT_STEP;
    }
    return claims;
}

static inline enum fm1lab_input_action fm1lab_input_route(
        uint8_t page, uint8_t encoder, uint8_t patch_focus)
{
    uint32_t claims = fm1lab_input_route_claims(page, encoder, patch_focus);
    if (!claims || (claims & (claims - 1u)))
        return FM1LAB_INPUT_NONE;
    for (uint32_t action = FM1LAB_INPUT_PATCH_FOCUS_STEP;
         action <= FM1LAB_INPUT_TRACK_INSTRUMENT_STEP; ++action)
        if (claims == ((uint32_t)1u << action))
            return (enum fm1lab_input_action)action;
    return FM1LAB_INPUT_NONE;
}

static inline uint8_t fm1lab_input_track_next(uint8_t lane, int delta)
{
    return delta ? (uint8_t)((lane ^ 1u) & 1u) : (uint8_t)(lane & 1u);
}

static inline uint8_t fm1lab_input_focus_after_page_change(
        uint8_t current_page, uint8_t next_page, uint8_t patch_focus)
{
    return (current_page == FM1LAB_INPUT_PAGE_HOME &&
            next_page == FM1LAB_INPUT_PAGE_HOME) ? patch_focus : 0u;
}

#endif
