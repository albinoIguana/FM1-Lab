/* Host model for patches/fm1_flash_wait.h.
 * Exercises normal/late completion, permanent BUSY, reset threshold and timer wrap.
 */
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>

#define RAMINL static inline
static uint32_t now_ticks, ready_ticks, poll_ticks = 24u;
static int stuck;
static jmp_buf reset_env;
#define T4_CNT now_ticks
static uint8_t fl_rdsr(uint8_t command)
{
    uint8_t value = (stuck || (int32_t)(ready_ticks - now_ticks) > 0) ? 1u : 0u;
    (void)command;
    now_ticks += poll_ticks;
    return value;
}
static void fl_reset_busy(void) { longjmp(reset_env, 1); }
#include "../patches/fm1_flash_wait.h"

static unsigned failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); failures++; } } while (0)

static int run(uint32_t start, uint32_t busy_us, int never,
               uint32_t normal_us, uint32_t extended_us,
               int *reset, int *xip_safe, uint32_t *took)
{
    uint32_t issued;
    now_ticks = start;
    stuck = never;
    issued = now_ticks;
    now_ticks += 24u * 20u; /* SPI command + cache invalidation */
    ready_ticks = issued + 24u * busy_us;
    *reset = 0;
    if (setjmp(reset_env)) {
        *reset = 1;
        *xip_safe = 1; /* reset path cannot reach fl_exit()/XIP restore */
        return -99;
    }
    {
        int rc = fl_wait_issued(issued, normal_us, extended_us, took);
        *xip_safe = !(stuck || (int32_t)(ready_ticks - now_ticks) > 0);
        return rc;
    }
}

static void test_bounds(uint32_t normal, uint32_t extended, uint32_t start)
{
    int reset, safe, rc;
    uint32_t took;
    rc = run(start, 100u, 0, normal, extended, &reset, &safe, &took);
    CHECK(!reset && rc == 0 && safe && took >= 100u && took <= 102u);
    rc = run(start, normal - 2u, 0, normal, extended, &reset, &safe, &took);
    CHECK(!reset && rc == 0 && safe && took <= normal);
    rc = run(start, normal + 2u, 0, normal, extended, &reset, &safe, &took);
    CHECK(!reset && rc == FL_ERR_LATE_READY && safe && took > normal);
    rc = run(start, extended - 3u, 0, normal, extended, &reset, &safe, &took);
    CHECK(!reset && rc == FL_ERR_LATE_READY && safe && took < extended);
    rc = run(start, 0u, 1, normal, extended, &reset, &safe, &took);
    CHECK(reset && safe);
    rc = run(start, extended + 1000u, 0, normal, extended, &reset, &safe, &took);
    CHECK(reset && safe);
}

int main(void)
{
    test_bounds(500000u, 2000000u, 1000u); /* 4 KiB erase */
    test_bounds(10000u, 50000u, 1000u);    /* 256-byte page program */
    test_bounds(500000u, 2000000u, 0xFFFFFFFFu - 24u * 300000u);
    test_bounds(10000u, 50000u, 0xFFFFFFFFu - 24u * 5000u);
    CHECK(FL_ERR_LATE_READY == -4);
    if (failures) {
        printf("flash_wait: %u failure(s)\n", failures);
        return 1;
    }
    puts("flash_wait: all boundary and timer-wrap checks passed");
    return 0;
}
