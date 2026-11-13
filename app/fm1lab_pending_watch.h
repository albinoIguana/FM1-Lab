#ifndef FM1LAB_PENDING_WATCH_H
#define FM1LAB_PENDING_WATCH_H
#include <stdint.h>
typedef struct {
    uint32_t since_ms;
    uint32_t heartbeat;
    uint8_t active;
    uint8_t reported;
} fm1lab_pending_watch_t;
/* Report only when a pending apply has seen no audio callback for 500 ms.
 * A new heartbeat restarts the window; this helper never clears pending. */
static inline uint8_t fm1lab_pending_watch_update(fm1lab_pending_watch_t *watch,
        uint8_t pending,uint32_t now_ms,uint32_t heartbeat) {
    if(!watch)return 0u;
    if(!pending) {
        watch->active=0u;
        watch->reported=0u;
        watch->since_ms=now_ms;
        watch->heartbeat=heartbeat;
        return 0u;
    }
    if(!watch->active) {
        watch->active=1u;
        watch->reported=0u;
        watch->since_ms=now_ms;
        watch->heartbeat=heartbeat;
        return 0u;
    }
    if(watch->heartbeat!=heartbeat) {
        watch->heartbeat=heartbeat;
        watch->since_ms=now_ms;
        watch->reported=0u;
        return 0u;
    }
    if(!watch->reported && (uint32_t)(now_ms-watch->since_ms)>=500u) {
        watch->reported=1u;
        return 1u;
    }
    return 0u;
}
#endif
