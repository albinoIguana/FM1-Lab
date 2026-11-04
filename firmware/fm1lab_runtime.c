#include "fm1lab_runtime.h"
#include "fm1lab_library.h"
int fm1lab_runtime_load(fm1lab_runtime_t *runtime, unsigned index)
{
    if (!runtime || index >= FM1LAB_INSTRUMENT_COUNT) return -1;
    fm1lab_instrument_t next;
    int result = fm1lab_library_load(index, &next);
    if (result) return result;
    runtime->instrument = next; runtime->instrument_index = index;
    runtime->selected_patch = 0; runtime->dirty = 0; runtime->loaded = 1;
    return 0;
}
int fm1lab_runtime_save(fm1lab_runtime_t *runtime)
{
    if (!runtime || !runtime->loaded) return -1;
    int result = fm1lab_library_save(runtime->instrument_index, &runtime->instrument);
    if (!result) runtime->dirty = 0;
    return result;
}
int fm1lab_runtime_select_instrument(fm1lab_runtime_t *runtime, int delta)
{
    if (!runtime || !runtime->loaded) return -1;
    int next = (int)runtime->instrument_index + delta;
    while (next < 0) next += (int)FM1LAB_INSTRUMENT_COUNT;
    next %= (int)FM1LAB_INSTRUMENT_COUNT;
    if ((unsigned)next == runtime->instrument_index) return 0;
    if (runtime->dirty) return -2;
    return fm1lab_runtime_load(runtime, (unsigned)next);
}
int fm1lab_runtime_select_patch(fm1lab_runtime_t *runtime, unsigned patch)
{
    if (!runtime || !runtime->loaded || patch >= FM1LAB_PATCH_COUNT) return -1;
    runtime->selected_patch = (uint8_t)patch; return 0;
}
int fm1lab_runtime_toggle_patch(fm1lab_runtime_t *runtime, unsigned patch)
{
    if (!runtime || !runtime->loaded || patch >= FM1LAB_PATCH_COUNT) return -1;
    runtime->instrument.patches[patch].enabled ^= 1u; runtime->dirty = 1; return 0;
}
int fm1lab_runtime_set_level(fm1lab_runtime_t *runtime, unsigned patch, int delta)
{
    if (!runtime || !runtime->loaded || patch >= FM1LAB_PATCH_COUNT) return -1;
    int value = (int)runtime->instrument.patches[patch].level + delta;
    if (value < 0) value = 0; if (value > 127) value = 127;
    if (runtime->instrument.patches[patch].level != (uint8_t)value) {
        runtime->instrument.patches[patch].level = (uint8_t)value; runtime->dirty = 1;
    }
    return 0;
}
int fm1lab_runtime_set_macro(fm1lab_runtime_t *runtime, unsigned patch,
                             unsigned macro, unsigned value)
{
    if (!runtime || !runtime->loaded || patch >= FM1LAB_PATCH_COUNT ||
        macro >= FM1LAB_MACRO_COUNT || value > 100) return -1;
    if (runtime->instrument.patches[patch].macros[macro] != value) {
        runtime->instrument.patches[patch].macros[macro] = (uint8_t)value; runtime->dirty = 1;
    }
    return 0;
}
int fm1lab_runtime_render(const fm1lab_runtime_t *runtime, unsigned patch,
                          uint8_t voice[FM1LAB_PATCH_BYTES], uint8_t *level)
{
    if (!runtime || !runtime->loaded || patch >= FM1LAB_PATCH_COUNT || !voice || !level) return -1;
    const fm1lab_patch_t *part = &runtime->instrument.patches[patch];
    *level = part->level;
    if (!part->enabled) return 0;
    return fm1lab_render_patch(&runtime->instrument, patch, voice) ? -1 : 1;
}
