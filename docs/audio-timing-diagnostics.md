# Audio timing diagnostics

These values describe one completed audio callback unless the name says
"peak" or "count". They are diagnostic counters, not polyphony guarantees.
The platform's `plat_ticks24()` name is historical: its documented free-running
counter is 32-bit and wraps every 2^32 ticks. Unsigned subtraction of two reads
is the intended elapsed-time calculation; no 24-bit mask is appropriate.
On the FM1, press **GLO** from HOME for GLOBAL, press GLO again for the
read-only AUDIO DIAG page, and press GLO once more to return HOME. The render-time peak window resets on physical control presses that change or
perform something, but not on page-navigation buttons (HOME, GLO, ENV, LFO,
EDIT, SEL, SEQ, ARP, FX, SAVE). The touch piano keys are scanned separately
and do not reset it. Opening AUDIO DIAG therefore preserves the test window.

| Readout | Meaning |
|---|---|
| `renderus` | Wall time for the full application callback: event drain, MIDI input, master-gain scaling, FM6 synthesis and effects. DMA buffer commit happens after this callback and is not included. |
| `renderbudget` | Sample-derived callback duration. For 256 frames at 44.1 kHz this is about 5,805 us. |
| `rendershedat` | 85% of the callback budget. For 256/44.1 kHz this is about 4,934 us. |
| `renderdma` | 7/8 callback budget, the DMA commit guard. For 256/44.1 kHz this is about 5,079 us. |
| `renderlate` | Count of callbacks whose render time exceeded the full sample budget. It is not the DMA late-commit counter. |
| `core0us` | CPU0 busy portion of the callback: callback wall time minus the sum of CPU0's waits for CPU2. It includes CPU0-side non-voice work. |
| `cpu0voice` | Sum of CPU0 voice-rendering time across all FM6 quanta in this callback, including CPU0 fallback rerenders. |
| `cpu2job` | Longest individual CPU2 voice job in any quantum of this callback. |
| `cpu2jobsum` | Sum of CPU2 voice-job durations across all quanta in this callback. |
| `cpu2wait` | Longest individual CPU0 wait for CPU2 in any quantum. |
| `cpu2waitsum` | Sum of CPU0 wait durations across all quanta. |
| `renderquanta` | Total FM6 processing quanta in this callback. |
| `cpu2quanta` | Quanta in which more than one voice was active and a split was eligible. |
| `cpu2dispatch` / `cpu2fallback` | Successful CPU2 dispatches and quanta rendered/finalized through the CPU0 fallback path. |
| `uius` / `midius` / `gainus` | Callback time spent draining queued UI events, processing MIDI input, and applying master gain. |
| `voicepeak` | Maximum simultaneous active FM voices observed in any quantum of this callback. |
| `prelimpeak` | Maximum absolute stereo sample immediately before the final output limiter, in Q23 units. The device page displays it as percent of full scale. |
| `limitergr` | Maximum gain reduction applied by the final stereo-linked limiter during this callback, displayed as a percentage. This is limiter action, not compressor gain reduction. |
| `renderpeakall` | All-time maximum callback render duration since boot. |
| `renderpeak` | Maximum callback render duration since the most recent physical button press. |
| `shedvoice0` ... `shedvoice15` | Active voice count at each of the 16 most recent overload-shed events; 0 is newest. |

## Reading the numbers

Do not add `cpu2job` or `cpu2wait` to anything: they are per-quantum
maxima. Use `cpu0voice`, `cpu2jobsum`, and `cpu2waitsum` for callback
accounting. CPU0 utilization is `core0us / renderus`; CPU2 utilization over
the callback interval is `cpu2jobsum / renderus`. The cores overlap, so
their percentages are not meant to add to 100%.

Compare `renderus` with both limits, not only `renderbudget`. The 85%
shedding threshold occurs before the 7/8 DMA commit guard. The DMA commit
itself is outside `renderus`.

The pre-limiter peak and limiter gain-reduction readouts show whether the final
output stage is being driven into limiting. They do not replace a full internal
headroom audit of every FX bus. The event-drain and MIDI sub-timings help explain an unexpectedly expensive
callback at low voice counts. They are included in `renderus`, not in the
FM6 voice-only sums.

## Polyphony measurements

Collect repeated steady-state rows with columns:

```csv
regime,voices,renderus
single,1,....
single,2,....
split,2,....
split,4,....
```

Keep sample rate, frame count, patches, FX, note velocity, and workload
consistent within a fit. Collect multiple observations per voice count and
fit each regime separately. The model is:

```
renderus = fixed_callback_cost + per_voice_cost * voices
```

Run `python3 tools/fit-polyphony.py measurements.csv`. Its default limit is
4,934 us, the 85% shedding point for a 256-frame/44.1 kHz callback. Host
measurements validate counter aggregation only; they do not establish FM1
hardware timing or maximum polyphony.
