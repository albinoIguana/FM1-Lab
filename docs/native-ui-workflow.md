# Native UI workflow — design specification

This document preserves the agreed physical-screen workflow and visual design notes. It is a **design target**, not a claim that the current fm1-base bring-up app already implements these pages.

## Navigation contract

- HOME is the four-patch Instrument overview.
- On HOME, SELECT chooses the focused patch.
- Pressing the physical **SEL** button opens that patch's performance macro page.
- HOME returns from the macro page to HOME.
- GLO remains reserved for global settings/mixer; it must never be repurposed as a macro shortcut.
- EDIT/SCL opens the deep patch editor; FX, ENV and LFO retain their dedicated pages.
- The current fm1-base platform names the printed SEL key `B_SEL` in `include/plat.h`. The older M-VAVE SDK's internal panel enum calls the same physical key `SCL`; this is a naming difference, not a different button.

## Macro page

Four performance controls, neutral at 50:

- BRIGHTNESS — modulation-index depth on the FM structure.
- BODY — modulator ratio and envelope-time shaping.
- MOTION — LFO rate and modulation depth.
- DIRT — bounded feedback and carrier detune.

The page should show the selected patch name, algorithm, live carrier/modulator roles, each macro's value and a visible neutral midpoint. Four physical knobs map one-to-one to the four macros. Macro edits are non-destructive: the stored base voice is not overwritten.

## Visual language

- Dark, high-contrast instrument UI with a restrained accent color per patch.
- Four HOME tiles in a 2×2 grid, each showing patch name, active/muted state, algorithm and level.
- Use the algorithm's actual topology to distinguish carriers and modulators; do not imply connections that are not in the selected algorithm.
- Use small, purposeful animation for selection and value changes; avoid decorative motion during audio-critical interaction.
- The screen should remain readable at the FM1's 240×280 resolution. Keep labels short and make the selected control/value obvious.

## Implementation boundary

The active device app is built on fm1-base/Felucca-BSP and uses `plat.h` (`B_SEL`, `B_GLO`, `EN_SELECT`, etc.) plus its `cv_*` drawing API. The scripts under `scripts/patch-m-vave-*` patch the separate M-VAVE SDK/simulator tree and are retained as reference only; they are not the active device UI implementation.
