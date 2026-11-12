#!/usr/bin/env python3
"""Backport FM1 Open's 7/8 DMA-half render deadline guard to fm1-base."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "base"


def replace_once(path, old, new):
    source = path.read_text()
    if new in source:
        return
    if old not in source:
        raise SystemExit(f"Expected upstream patch site not found: {path.relative_to(ROOT)}")
    path.write_text(source.replace(old, new, 1))


audio = BASE / "device/audio_commit.c"
replace_once(audio,
    "static uint32_t audio_commit(uint32_t half)",
    "static uint32_t audio_commit(uint32_t half, uint32_t render_us, uint32_t budget_us)")
replace_once(audio,
    "    uint32_t ie = fm1_icfg() & 0x200u, ok = 0, i;   /* (icfg bit 9: the global enable) */",
    """    uint32_t ie, ok = 0, i;
    /* Match fm1_open: reserve the final 1/8 of the DMA half for a safe copy. */
    if (!budget_us || render_us >= budget_us - (budget_us >> 3))
        return 0u;
    ie = fm1_icfg() & 0x200u;                       /* (icfg bit 9: the global enable) */""")

main = BASE / "device/main_fm1.c"
replace_once(main,
    "static void render_block(int32_t *o)                /* ALNK0: one half, measured */",
    "static uint32_t render_block(int32_t *o)             /* ALNK0: one half, measured */")
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
    """#if FM1B_AUDIO_ASIDE && FM1B_AUDIO_AHEAD
        uint32_t ok;
        static uint8_t behind;""",
    """#if FM1B_AUDIO_ASIDE && FM1B_AUDIO_AHEAD
        uint32_t ok;
        static uint32_t aside_render_us;
        static uint8_t behind;""")
replace_once(main,
    """        ok = audio_commit(half);                    /* the block rendered last time */
        render_block(aside);""",
    """        ok = audio_commit(half, aside_render_us, HALF_FRAMES * 1000000u / FS);
        aside_render_us = render_block(aside);""")
replace_once(main,
    """#elif FM1B_AUDIO_ASIDE
        uint32_t ok;
        fm1_audio_ack_half();
        render_block(aside);
        ok = audio_commit(half);""",
    """#elif FM1B_AUDIO_ASIDE
        uint32_t ok, render_us;
        fm1_audio_ack_half();
        render_us = render_block(aside);
        ok = audio_commit(half, render_us, HALF_FRAMES * 1000000u / FS);""")

test = BASE / "tests/audio_commit_test.c"
source = test.read_text()
source = source.replace("audio_commit(1u)", "audio_commit(1u, 1000u, 5804u)")
source = source.replace("audio_commit(0u)", "audio_commit(0u, 1000u, 5804u)")
if "audio_commit(1u, 5079u, 5804u)" not in source:
    anchor = """    setup(1u, FM1_AUDIO_HALF, 0u, 0x300u);
    r = audio_commit(1u, 1000u, 5804u);
    want("a new half pending: late, nothing written", r == 0u && half_is(0u, 0) && half_is(1u, 0));
"""
    addition = anchor + """
    setup(1u, 0u, 0u, 0x300u);
    r = audio_commit(1u, 5079u, 5804u);             /* exactly 7/8 of 5804 us */
    want("at the 7/8 deadline guard: late, nothing written",
         r == 0u && half_is(0u, 0) && half_is(1u, 0));
    setup(1u, 0u, 0u, 0x300u);
    r = audio_commit(1u, 6000u, 5804u);
    want("past the DMA half budget: late, nothing written",
         r == 0u && half_is(0u, 0) && half_is(1u, 0));
"""
    if anchor not in source:
        raise SystemExit("Expected audio_commit_test insertion point not found")
    test.write_text(source.replace(anchor, addition, 1))

print("Applied fm1-base 7/8 DMA-half render deadline guard.")
