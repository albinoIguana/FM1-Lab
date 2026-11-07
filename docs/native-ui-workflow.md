# Native UI workflow — design specification

This document preserves the agreed physical-screen workflow. It distinguishes implemented bring-up behavior from the final design target.

## Navigation contract

- HOME is the four-patch Instrument overview.
- The physical SELECT encoder moves the selected patch on HOME.
- Pressing the physical **SEL** button opens the selected patch's performance macro page.
- K1–K4 on HOME control patch slots 1–4 volume respectively; they do not follow the selected patch.
- K1–K4 on the macro page control BRIGHTNESS, BODY, MOTION and EDGE for the selected patch.
- Reconsidering HOME K1–K4 volume assignment is Phase 2; do not remap it in the current pass.
- HOME returns from the macro page to HOME.
- GLO remains reserved for global settings/mixer; it must never be repurposed as a macro shortcut.
- EDIT/SCL opens the deep patch editor; FX, ENV and LFO retain their dedicated pages.
- The current fm1-base platform names the printed SEL key `B_SEL` in `include/plat.h`. The older M-VAVE SDK's internal panel enum calls the same physical key `SCL`; this is a naming difference, not a different button.

## Macro page

Four performance controls, neutral at 50:

- BRIGHTNESS — modulation-index depth on the FM structure.
- BODY — modulator ratio and envelope-time shaping.
- MOTION — LFO rate and modulation depth.
- EDGE — bounded feedback and carrier detune.

The page should show the selected patch name, algorithm, live carrier/modulator roles, each macro's value and a visible neutral midpoint. Four physical knobs map one-to-one to the four macros. Macro edits are non-destructive: the stored base voice is not overwritten. A small page index belongs in the bottom-right corner on every page.

## FX page design

- Preserve the official FM1 routing model and native effect processing.
- Use multiple dedicated effect pages, one effect per page.
- Each effect page uses four physical knobs in the same positions and visual treatment as the macro page.
- Keep labels, values, meters and navigation consistent across effect pages.
- Add a small bottom-right page index (current page / page count).
- UI controls must not be presented as functional until they change the real audio effect parameters and the instrument signal is confirmed to pass through the FX path.

## Visual language

- Physical display: 240×240 pixels. Every screen uses the same rounded 2×2 quadrant composition and compact bottom control/status strip.
- Dark, high-contrast base with four bold patch identities: cyan, magenta, yellow and red. A patch retains its color across HOME, macro and operator views.
- Four HOME tiles in a 2×2 grid, each showing patch name, active/muted state, algorithm and level.
- Use the algorithm's actual topology to distinguish carriers and modulators; do not imply connections that are not in the selected algorithm.
- Use small, purposeful animation for selection and value changes; avoid decorative motion during audio-critical interaction.
- HOME, macro and operator screens reuse the same quadrant geometry. The bottom band shows real physical-control assignments and the page index; do not draw a decorative piano keyboard. Keep labels short and make the selected control/value obvious.

## Implementation boundary

The active device app is built on fm1-base/Felucca-BSP and uses `plat.h` (`B_SEL`, `B_GLO`, `EN_SELECT`, etc.) plus its `cv_*` drawing API. The scripts under `scripts/patch-m-vave-*` patch the separate M-VAVE SDK/simulator tree and are retained as reference only.
