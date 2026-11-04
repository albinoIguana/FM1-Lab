# FM1 physical control map

The printed labels on the FM1 panel are the names used in FM1-Lab code and UI specifications. The numeric values below are implementation indices in upstream `struct fm1_controls.encoders[]`, not physical labels.

| Printed panel label | Upstream input | Confirmed existing use |
|---|---:|---|
| MASTER | ADC channel 4 | Master potentiometer |
| SELECT | `encoders[0]` | Track/item selection |
| ALGORITHM | `encoders[1]` | DX7 algorithm editing |
| KNOB 1 | `encoders[2]` | First of four parameter knobs |
| KNOB 2 | `encoders[3]` | Second of four parameter knobs |
| KNOB 3 | `encoders[4]` | Third of four parameter knobs |
| KNOB 4 | `encoders[5]` | Fourth of four parameter knobs |
| PRESETS | `encoders[6]` | Preset/project navigation |

The seven encoder indices are confirmed by the upstream scanner and screen handlers. The MASTER potentiometer is read separately through ADC channel 4; it is not encoder 7 or part of the seven-element encoder array.

Source basis: pinned upstream M-VAVE FM1 source, `app/board/fm1_input.h`, `app/board/panel.c`, `app/board/device.c`, and `app/screen.c`. The C constants live in `firmware/fm1lab_physical_controls.h`; tests lock the mapping.
