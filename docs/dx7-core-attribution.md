# DX7 core source attribution

The FM1-Lab native Instrument engine stages source files from the M-VAVE open FM1 repository, pinned to commit 4c7496daac04a95ff10d1afdcc8f9ead2dbabc41:

- https://github.com/m-vave/fm1_open/tree/4c7496daac04a95ff10d1afdcc8f9ead2dbabc41/web/synth

The staged implementation includes the following source files and their upstream license notices:

- Apache-2.0: dx7core.c, dx7note.c, dx7env.c, dx7lfo.c, dx7pitchenv.c.
- GPL-3.0-only WITH GPL-3.0-linking-exception: dx7synth.c, dx7math.c, fixed.c, adsr.c.

The copied headers and fixed-point support header retain their original SPDX and copyright notices. The build-time staging script prepends only the FM1-Lab symbol-namespace include; it does not remove or rewrite upstream license notices. Consult the pinned upstream repository's THIRD_PARTY.md and license files for complete notices and upstream revision details.
