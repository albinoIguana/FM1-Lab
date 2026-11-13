#!/usr/bin/env python3
"""Backport FM1 Open's safe aside-buffer DMA commit to the pinned fm1-base."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "base"


def replace_once(path: Path, old: str, new: str) -> None:
    source = path.read_text()
    if new in source:
        return
    if old not in source:
        raise SystemExit(f"Expected upstream patch site not found: {path.relative_to(ROOT)}")
    path.write_text(source.replace(old, new, 1))


# The pinned fm1-base predates audio_aside/audio_commit.c. Add the audited
# aside renderer here rather than relying on an unsupported TOML key.
audio = BASE / "device/audio_commit.inc"
audio_source = """/* FM1 Open-style aside buffer and deadline-safe DMA commit.
 * Included by main_fm1.c after HALF_WORDS and abuf are declared. */
static int32_t aside[HALF_WORDS] __attribute__((aligned(4)));

static uint32_t audio_commit(uint32_t half, uint32_t render_us, uint32_t budget_us)
{
    volatile int32_t *dst = &abuf[half * HALF_WORDS];
    uint32_t ie, ok = 0u, i;

    /* Match fm1_open: reserve the final 1/8 of the DMA half for the copy. */
    if (!budget_us || render_us >= budget_us - (budget_us >> 3))
        return 0u;

    ie = fm1_icfg() & 0x200u; /* global interrupt enable */
    fm1_irq_off();
    if (fm1_audio_free_half() == half && !(fm1_audio_pending() & FM1_AUDIO_HALF)) {
        for (i = 0; i < HALF_WORDS; ++i)
            dst[i] = aside[i];
        ok = fm1_audio_free_half() == half && !(fm1_audio_pending() & FM1_AUDIO_HALF);
    }
    if (ie)
        fm1_irq_on();
    return ok;
}
"""
if not audio.exists():
    audio.write_text(audio_source)
elif audio.read_text() != audio_source:
    raise SystemExit("Unexpected base/device/audio_commit.inc already exists; inspect before replacing it")

main = BASE / "device/main_fm1.c"
replace_once(main,
    "static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));",
    """static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));
#include "audio_commit.inc" /* separate render buffer and guarded DMA-half commit */""")
replace_once(main,
    "static void render_block(int32_t *o)                /* ALNK0: one half, measured */",
    "static uint32_t render_block(int32_t *o)             /* ALNK0: one half, measured */")
replace_once(main,
    """#ifdef APP_OVERLOAD
    if (us > budget * 9u / 10u)
        app_overload();""",
    """#ifdef APP_OVERLOAD
    if (us > budget * 85u / 100u)
        app_overload();""")
replace_once(main,
    """    fm1b_dbg.last_us = us;
    if (us > fm1b_dbg.max_us)
        fm1b_dbg.max_us = us;
    fm1b_dbg.in_audio = 0;
}""",
    """    fm1b_dbg.last_us = us;
    if (us > fm1b_dbg.max_us)
        fm1b_dbg.max_us = us;
    fm1b_dbg.in_audio = 0;
    return us;
}""")
replace_once(main,
    """        uint32_t half = fm1_audio_free_half();
        render_block(&abuf[half * HALF_WORDS]);
        fm1_audio_ack_half();
        if (fm1_audio_free_half() != half) {
            fm1b_dbg.late++;                         /* the DMA moved on while we rendered */
            audio_xruns++;
        }""",
    """        uint32_t half = fm1_audio_free_half(), render_us, ok;
        fm1_audio_ack_half();
        render_us = render_block(aside);
        ok = audio_commit(half, render_us, HALF_FRAMES * 1000000u / FS);
        if (fm1_audio_pending() & FM1_AUDIO_HALF)
            fm1_audio_ack_half();
        if (!ok) {
            fm1b_dbg.late++;                         /* late render or unsafe DMA commit */
            audio_xruns++;
        }""")

print("Applied FM1 Open-style aside buffer and 7/8 DMA deadline guard.")
