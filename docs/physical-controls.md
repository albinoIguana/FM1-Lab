# FM1 physical control map

| Printed panel label | Upstream input | FM1-Lab use |
|---|---:|---|
| MASTER | ADC channel 4 | Master output level |
| SELECT | `encoders[0]` | Selects one of four patches on HOME; cycles FX pages in the FX section |
| ALGORITHM | `encoders[1]` | Algorithm editing / BLE device selection; selects EQ band on the EQ page |
| PRESETS | `encoders[2]` | Instrument/preset navigation; selects Track 1/2 on the SEQ and SEQ OPTIONS pages |
| KNOB 1 | `encoders[3]` | HOME: patch 1 volume; macro page: BRIGHTNESS; FX parameter 1 |
| KNOB 2 | `encoders[4]` | HOME: patch 2 volume; macro page: BODY; FX parameter 2 |
| KNOB 3 | `encoders[5]` | HOME: patch 3 volume; macro page: MOTION; FX parameter 3 |
| KNOB 4 | `encoders[6]` | HOME: patch 4 volume; macro page: EDGE; FX parameter 4 |

The physical **SEL button** (`B_SEL`) is separate from the SELECT encoder: on HOME it opens the selected patch's macro page. HOME returns to the four-patch overview. The HOME K1–K4 volume assignment is current Phase 1 behavior and is deferred for reconsideration in Phase 2.

The seven encoder indices match the pinned `base/include/plat.h` `EN_*` enum: SELECT=0, ALGO=1, PRESET=2, K1=3, K2=4, K3=5, K4=6. MASTER is read separately through ADC channel 4, not part of the seven-element encoder array. In the FX section, SELECT and FX both advance through the rack; ALGORITHM selects the EQ band or compressor makeup, and K1–K4 adjust the page's four parameters.
