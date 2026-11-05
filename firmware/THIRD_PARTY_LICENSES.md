# Third-party source and licenses

- `base/` — fm1-base, MPL-2.0; its nested `bsp/` submodule is felucca-bsp, MPL-2.0.
- `upstream/felucca/` — Felucca application/engine wrapper and sequencer, GPL-3.0-only.
- Felucca `firmware/src/fm6_core.c` — Apache-2.0; original license header is retained in source.
- Felucca FM6 patch/table generators and generated factory patch data — retain upstream GPL-3.0-only notices.
- Dinghy is used as an audit/reference project only; no Dinghy source is copied into this firmware.

This firmware is distributed under GPL-3.0-only because its FM6 integration is based on GPL-licensed Felucca engine code. Keep all upstream copyright and SPDX notices intact.
