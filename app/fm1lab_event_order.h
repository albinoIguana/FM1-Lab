#ifndef FM1LAB_EVENT_ORDER_H
#define FM1LAB_EVENT_ORDER_H
#include <stdint.h>
enum fm1lab_event_type {
    FM1LAB_EVENT_NOTE_ON=1, FM1LAB_EVENT_NOTE_OFF, FM1LAB_EVENT_PANIC,
    FM1LAB_EVENT_MACRO_SET, FM1LAB_EVENT_PATCH_LEVEL, FM1LAB_EVENT_INSTRUMENT_LOAD,
    FM1LAB_EVENT_PATCH_VOICE, FM1LAB_EVENT_MASTER_INSERT, FM1LAB_EVENT_MASTER_COMP,
    FM1LAB_EVENT_PERFORMANCE_APPLY, FM1LAB_EVENT_SEQ_RUN, FM1LAB_EVENT_PATCH_PITCH
};
static inline uint8_t fm1lab_event_is_heavy(uint8_t type) {
    return (uint8_t)(type==FM1LAB_EVENT_INSTRUMENT_LOAD ||
        type==FM1LAB_EVENT_PERFORMANCE_APPLY || type==FM1LAB_EVENT_PATCH_VOICE);
}
/* A heavy event may be applied once per audio callback. The caller must leave
 * a deferred event at the queue head so every later event retains FIFO order. */
static inline uint8_t fm1lab_event_should_defer(uint8_t type,uint8_t heavy_done) {
    return (uint8_t)(heavy_done && fm1lab_event_is_heavy(type));
}
#endif
