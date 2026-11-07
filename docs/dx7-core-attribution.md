# DX7 engine API and source attribution

The actual FM1 device firmware reuses the fixed-point six-operator DX7 synth already linked in the pinned M-VAVE SDK engine archive. FM1-Lab stages only the matching public API header from the pinned M-VAVE repository:

- https://github.com/m-vave/fm1_open/tree/4c7496daac04a95ff10d1afdcc8f9ead2dbabc41/web/synth

The device build does not compile a second copy of the DX7 DSP source. This is deliberate: the SDK archive already exports the required `synth_dx7_*` entry points, so staging the same source again only duplicates code and lookup tables.

The host C test compiles the pinned upstream source files to exercise the same API without relying on the proprietary SDK archive:

- Apache-2.0: dx7core.c, dx7note.c, dx7env.c, dx7lfo.c, dx7pitchenv.c.
- GPL-3.0-only WITH GPL-3.0-linking-exception: dx7synth.c, dx7math.c, fixed.c, adsr.c.

Source files retain their upstream SPDX headers. Consult the pinned upstream repository's THIRD_PARTY.md and license files for complete notices and revision details.
