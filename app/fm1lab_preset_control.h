#ifndef FM1LAB_PRESET_CONTROL_H
#define FM1LAB_PRESET_CONTROL_H

#include <stdint.h>

enum {
    FM1LAB_PRESET_TARGET_INSTRUMENT = 0u,
    FM1LAB_PRESET_TARGET_PATCH = 1u
};

/* PRESETS always selects a complete four-patch Instrument. SELECT owns
 * individual patch focus; changing focus never changes PRESETS semantics. */
static inline uint8_t fm1lab_preset_target(uint8_t patch_focus, uint8_t all_patches_active)
{
    (void)patch_focus;
    (void)all_patches_active;
    return FM1LAB_PRESET_TARGET_INSTRUMENT;
}

/* PRESETS is active on Home, or on the main Sequencer page when a
 * track is highlighted. Options, piano roll, and all other pages are inert. */
static inline uint8_t fm1lab_preset_encoder_enabled(uint8_t page,
        uint8_t home_page, uint8_t sequencer_page, uint8_t track_highlighted)
{
    return (uint8_t)(page == home_page ||
        (page == sequencer_page && track_highlighted));
}

/* Wrap a signed encoder delta across the curated individual-voice bank. */
static inline uint8_t fm1lab_preset_cursor_advance(uint8_t cursor, int delta,
                                                   uint32_t voice_count)
{
    int32_t next;
    if (!voice_count) return 0u;
    next = ((int32_t)cursor + delta) % (int32_t)voice_count;
    if (next < 0) next += (int32_t)voice_count;
    return (uint8_t)next;
}

#endif
