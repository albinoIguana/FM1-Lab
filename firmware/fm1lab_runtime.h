#ifndef FM1LAB_RUNTIME_H
#define FM1LAB_RUNTIME_H
#include "fm1lab_instrument.h"
typedef struct {
    fm1lab_instrument_t instrument;
    unsigned instrument_index;
    uint8_t selected_patch, dirty, loaded;
} fm1lab_runtime_t;
/* Control-task APIs: never call load/save from the audio interrupt. */
int fm1lab_runtime_load(fm1lab_runtime_t *runtime, unsigned instrument_index);
int fm1lab_runtime_save(fm1lab_runtime_t *runtime);
int fm1lab_runtime_select_instrument(fm1lab_runtime_t *runtime, int delta);
int fm1lab_runtime_select_patch(fm1lab_runtime_t *runtime, unsigned patch);
int fm1lab_runtime_toggle_patch(fm1lab_runtime_t *runtime, unsigned patch);
int fm1lab_runtime_set_level(fm1lab_runtime_t *runtime, unsigned patch, int delta);
int fm1lab_runtime_set_macro(fm1lab_runtime_t *runtime, unsigned patch, unsigned macro, unsigned value);
/* Audio-side read: 1 rendered, 0 muted, -1 invalid. No flash I/O. */
int fm1lab_runtime_render(const fm1lab_runtime_t *runtime, unsigned patch,
                          uint8_t voice[FM1LAB_PATCH_BYTES], uint8_t *level);
#endif
