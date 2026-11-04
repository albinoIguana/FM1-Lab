# FM1-Lab UI and module architecture

## Design objective

Deep synthesis should remain approachable. The interface must reveal complexity progressively while the underlying model remains capable of full six-operator editing. The user should learn one coherent instrument, not navigate a collection of unrelated screens.

## Architectural boundaries

| Layer | Owns | Must not own |
|---|---|---|
| Instrument model | Four-patch contract, validation, names, patch state | DOM, engine transport, persistence |
| DX7 voice codec | 156-byte parameter encoding/decoding | UI layout, audio transport |
| FM topology | Algorithm routing, carrier/modulator roles, modulation depth | Patch storage, screen state |
| Macro renderer | Non-destructive musical transforms | DOM, persistence, engine selection |
| Instrument session | Coordinates model + topology + engine; serializes track writes | Screen markup, visual styling |
| M-VAVE engine adapter | Simulator/device command transport, note events, audio lifecycle | Instrument naming, UI navigation |
| UI components | Render and bind focused controls | DX7 byte offsets, engine internals |
| Application controller | Navigation, selection, dirty state, orchestration | Synthesis algorithms or low-level encoding |

InstrumentSession is the first enforced boundary between the data model and the engine. The UI must use it instead of writing raw voices directly to the engine.

## Screen model

### HOME / Instrument performance

One Instrument at a time, always exactly four patch slots. Show the Instrument name and four patch tiles. Each tile communicates patch identity, enabled state, level and selection. The physical PRESETS control changes Instrument; the four named patch encoders address the four patch slots. HOME is a performance surface, not a file browser.

### SEQ / Two-track composition

The main SEQ view displays exactly two user-facing lanes on fixed 64-step timelines. Each lane can be 16, 32, 48 or 64 steps, matching the native engine's length granularity. Tempo is shared (30–300 BPM) and swing is shared (0–50). The engine uses its native fixed step grid; the UI does not advertise unsupported per-track divisions or independent swing. SELECT chooses the active lane; the piano roll displays 64 steps against 24 chromatic note rows. Per-step editing exposes pitch, velocity, native gate amount, tie and alternate-tuning flag. The underlying engine still has eight project tracks: the two-lane adapter must bind two explicit normal tracks and leave the other six untouched. It must not silently repurpose or erase project tracks. The physical input adapter is keyed by printed panel names and stores verified scan indices explicitly: SELECT=0, ALGORITHM=1, KNOB 1–4=2–5, PRESETS=6. Sequencer state is its own domain module and communicates with the engine through an adapter; it does not own voice bytes or duplicate patch data.

### PATCH / Focused sound shaping

A patch opens into a focused editor. The four musical macros are the first layer. Algorithm, feedback, output level and a compact topology view provide context without requiring six operators to be visible immediately.

### OPERATOR / Deep sound design

The six-operator editor is a deeper layer of the same patch, not a separate page with a separate data model. It exposes algorithm topology, carrier/modulator role, ratio or fixed frequency, output, envelope rates and levels, and feedback. Selecting a different algorithm updates topology and role indicators from the engine's authoritative routing data.

### FX / Performance effects

Effects should be a reusable module with a small set of immediate performance controls and a deeper parameter view only where needed. It must not be embedded as special-case controls in every patch page.

## Reusable UI primitives

- Patch tile: patch name, number, active/muted state, level and selected state.
- Macro control: name, value, musical purpose, neutral marker and algorithm-aware explanation.
- Operator selector: physical OP1–OP6 labels, role indicator and selection state.
- Parameter control: label, value, valid range, units and optional contextual help.
- Topology diagram: six named operators and their current modulation/carrier relationships.
- Section header and navigation control: consistent location and behavior across screens.

## Interaction rules

1. One source of truth: all screens edit the same Instrument model.
2. Progressive disclosure: performance controls first, synthesis detail on demand.
3. Physical naming wins: labels printed on the FM1 panel are the user-facing names; firmware scan indices remain an explicit adapter mapping.
4. Algorithm-aware context: role and topology are data, never hard-coded screen assumptions.
5. Safe editing: macro rendering is derived from base voice data; raw edits update the base voice and are undoable/dirty-tracked.
6. No hidden destructive behavior: switching Instruments with unsaved edits requires a clear save/discard decision.
7. Reusable components: screen modules compose controls; controls do not know which screen contains them.
8. Responsive but consistent: phone preview may stack panels, while hardware layout should be designed against the physical display's actual pixel dimensions and input affordances.

## Implementation sequence

1. Establish and test the InstrumentSession service boundary.
2. Extract pure HOME and patch-editor view modules from the current controller.
3. Extract reusable macro, parameter, operator-selector and topology components.
4. Introduce a single navigation/state controller and dirty-state policy.
5. Map physical named controls to actions through a dedicated input adapter.
6. Implement the hardware HOME and editor screens only after the component contracts are stable.

## Non-goals

- Do not build a separate screen for every operator parameter.
- Do not duplicate the Instrument model between browser and device.
- Do not design hardware pages around the browser simulator's viewport.
- Do not infer physical control labels from C array indices.