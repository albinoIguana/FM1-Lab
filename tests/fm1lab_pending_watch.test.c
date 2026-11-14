#include "../app/fm1lab_pending_watch.h"
#include <assert.h>
#include <stdint.h>

static void test_pending_stall_threshold(void)
{
    fm1lab_pending_watch_t watch={0};
    assert(!fm1lab_pending_watch_update(&watch,1u,100u,7u));
    assert(watch.active && !watch.reported);
    assert(!fm1lab_pending_watch_update(&watch,1u,599u,7u));
    assert(fm1lab_pending_watch_update(&watch,1u,600u,7u));
    assert(watch.reported);
    assert(!fm1lab_pending_watch_update(&watch,1u,900u,7u));
}

static void test_heartbeat_restarts_stall_window(void)
{
    fm1lab_pending_watch_t watch={0};
    assert(!fm1lab_pending_watch_update(&watch,1u,1000u,10u));
    assert(!fm1lab_pending_watch_update(&watch,1u,1499u,10u));
    assert(!fm1lab_pending_watch_update(&watch,1u,1500u,11u));
    assert(!fm1lab_pending_watch_update(&watch,1u,1999u,11u));
    assert(fm1lab_pending_watch_update(&watch,1u,2000u,11u));
}

static void test_pending_clear_and_rearm(void)
{
    fm1lab_pending_watch_t watch={0};
    assert(!fm1lab_pending_watch_update(&watch,1u,50u,1u));
    assert(fm1lab_pending_watch_update(&watch,1u,550u,1u));
    assert(!fm1lab_pending_watch_update(&watch,0u,551u,1u));
    assert(!watch.active && !watch.reported);
    assert(!fm1lab_pending_watch_update(&watch,1u,700u,1u));
    assert(!fm1lab_pending_watch_update(&watch,1u,1199u,1u));
    assert(fm1lab_pending_watch_update(&watch,1u,1200u,1u));
}

static void test_clock_wrap_is_unsigned_safe(void)
{
    fm1lab_pending_watch_t watch={0};
    assert(!fm1lab_pending_watch_update(&watch,1u,UINT32_MAX-200u,3u));
    assert(!fm1lab_pending_watch_update(&watch,1u,UINT32_MAX,3u));
    assert(fm1lab_pending_watch_update(&watch,1u,299u,3u));
}

int main(void)
{
    test_pending_stall_threshold();
    test_heartbeat_restarts_stall_window();
    test_pending_clear_and_rearm();
    test_clock_wrap_is_unsigned_safe();
    return 0;
}
