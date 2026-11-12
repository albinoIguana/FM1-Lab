/* SPDX-License-Identifier: GPL-3.0-only
 * Regression tests for the FM1 Open-style aside-buffer DMA commit backport.
 */
#include <stdint.h>
#include <stdio.h>

#define HALF_WORDS 256u
#define FM1_AUDIO_HALF 1u
static int32_t abuf[2u * HALF_WORDS];
static uint32_t sim_icfg, sim_free, sim_pending, sim_reads, sim_reads_irq_on, sim_move_at;

static uint32_t fm1_icfg(void) { return sim_icfg; }
static void fm1_irq_off(void) { sim_icfg &= ~0x200u; }
static void fm1_irq_on(void) { sim_icfg |= 0x200u; }
static uint32_t fm1_audio_free_half(void)
{
    sim_reads++;
    sim_reads_irq_on += (sim_icfg & 0x200u) != 0u;
    if (sim_move_at && sim_reads == sim_move_at) {
        sim_free ^= 1u;
        sim_pending |= FM1_AUDIO_HALF;
    }
    return sim_free;
}
static uint8_t fm1_audio_pending(void) { return (uint8_t)sim_pending; }

#include "audio_commit.c"

static unsigned failures;
static void want(const char *label, int ok)
{
    printf("  %s %s\n", ok ? "ok  " : "FAIL", label);
    failures += !ok;
}
static void setup(uint32_t free_half, uint32_t pending, uint32_t move_at, uint32_t icfg)
{
    for (uint32_t i = 0; i < 2u * HALF_WORDS; ++i)
        abuf[i] = -1;
    for (uint32_t i = 0; i < HALF_WORDS; ++i)
        aside[i] = (int32_t)(i * 3u + 1u);
    sim_free = free_half;
    sim_pending = pending;
    sim_move_at = move_at;
    sim_icfg = icfg;
    sim_reads = sim_reads_irq_on = 0u;
}
static int half_is(uint32_t half, int from_aside)
{
    for (uint32_t i = 0; i < HALF_WORDS; ++i)
        if (abuf[half * HALF_WORDS + i] != (from_aside ? aside[i] : -1))
            return 0;
    return 1;
}

int main(void)
{
    uint32_t r;
    setup(1u, 0u, 0u, 0x300u);
    r = audio_commit(1u, 1000u, 5804u);
    want("in time: copied whole into the free half", r == 1u && half_is(1u, 1));
    want("DMA-playing half untouched", half_is(0u, 0));
    want("DMA state read with interrupts off", sim_reads > 0u && sim_reads_irq_on == 0u);
    want("previous interrupt state restored", (sim_icfg & 0x200u) != 0u && (sim_icfg & 0x100u) != 0u);

    setup(0u, 0u, 0u, 0x300u);
    r = audio_commit(1u, 1000u, 5804u);
    want("DMA already moved: no copy", r == 0u && half_is(0u, 0) && half_is(1u, 0));

    setup(1u, FM1_AUDIO_HALF, 0u, 0x300u);
    r = audio_commit(1u, 1000u, 5804u);
    want("new DMA half pending: no copy", r == 0u && half_is(0u, 0) && half_is(1u, 0));

    setup(1u, 0u, 0u, 0x300u);
    r = audio_commit(1u, 5079u, 5804u); /* exactly 7/8 of 5804 us */
    want("at the 7/8 guard: no copy", r == 0u && half_is(0u, 0) && half_is(1u, 0));

    setup(1u, 0u, 0u, 0x300u);
    r = audio_commit(1u, 6000u, 5804u);
    want("past the DMA half budget: no copy", r == 0u && half_is(0u, 0) && half_is(1u, 0));

    setup(1u, 0u, 0u, 0x300u);
    r = audio_commit(1u, 1000u, 0u);
    want("zero budget: no copy", r == 0u && half_is(1u, 0));

    setup(1u, 0u, 2u, 0x300u);
    r = audio_commit(1u, 1000u, 5804u);
    want("DMA moving during copy is reported late", r == 0u);
    want("the other half remains untouched", half_is(0u, 0));

    setup(0u, 0u, 0u, 0x100u);
    r = audio_commit(0u, 1000u, 5804u);
    want("inside audio IRQ: copy with global interrupts already off",
         r == 1u && half_is(0u, 1) && half_is(1u, 0));
    want("inside audio IRQ: interrupts remain off", (sim_icfg & 0x200u) == 0u);

    printf(failures ? "audio deadline commit: %u FAILED\n" : "audio deadline commit: all passed\n",
           failures);
    return failures != 0u;
}
