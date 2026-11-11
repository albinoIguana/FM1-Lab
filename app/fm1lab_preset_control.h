#ifndef FM1LAB_PRESET_CONTROL_H
#define FM1LAB_PRESET_CONTROL_H

#include <stdint.h>

enum {
    FM1LAB_PRESET_TARGET_INSTRUMENT = 0u,
    FM1LAB_PRESET_TARGET_PATCH = 1u
};

/* Patch focus is explicit UI state: the PRESETS encoder must not infer it
 * from the current page, because focus persists while editing that patch. */
static inline uint8_t fm1lab_preset_target(uint8_t patch_focus, uint8_t all_patches_active)
{
    /* A fully layered Home view is the Instrument overview, even if a prior
     * K1-K4 level edit left transient patch focus set. */
    return patch_focus && !all_patches_active
        ? FM1LAB_PRESET_TARGET_PATCH : FM1LAB_PRESET_TARGET_INSTRUMENT;
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
