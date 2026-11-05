# FM1 physical control map

| Printed panel label | Upstream input | FM1-Lab use |
|---|---:|---|
| MASTER | ADC channel 4 | Master output level |
| SELECT | `encoders[0]` | Selects one of four patches on HOME |
| ALGORITHM | `encoders[1]` | Algorithm editing / BLE device selection where enabled |
| KNOB 1 | `encoders[2]` | HOME: patch 1 volume; macro page: BRIGHTNESS |
| KNOB 2 | `encoders[3]` | HOME: patch 2 volume; macro page: BODY |
| KNOB 3 | `encoders[4]` | HOME: patch 3 volume; macro page: MOTION |
| KNOB 4 | `encoders[5]` | HOME: patch 4 volume; macro page: EDGE |
| PRESETS | `encoders[6]` | Instrument/preset navigation (library not yet implemented) |

The physical **SEL button** (`B_SEL`) is separate from the SELECT encoder: on HOME it opens the selected patch's macro page. HOME returns to the four-patch overview. The HOME K1–K4 volume assignment is current Phase 1 behavior and is deferred for reconsideration in Phase 2.

The seven encoder indices are confirmed by the upstream scanner and screen handlers. MASTER is read separately through ADC channel 4, not part of the seven-element encoder array.
