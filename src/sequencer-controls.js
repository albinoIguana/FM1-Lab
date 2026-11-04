/**
 * Physical FM1 panel labels are the public API. Firmware scan indices are
 * implementation details and are kept beside each printed name explicitly.
 */
export const FM1_PANEL_ENCODERS = Object.freeze({
  SELECT: Object.freeze({ scanIndex: 0 }),
  ALGORITHM: Object.freeze({ scanIndex: 1 }),
  "KNOB 1": Object.freeze({ scanIndex: 2 }),
  "KNOB 2": Object.freeze({ scanIndex: 3 }),
  "KNOB 3": Object.freeze({ scanIndex: 4 }),
  "KNOB 4": Object.freeze({ scanIndex: 5 }),
  PRESETS: Object.freeze({ scanIndex: 6 })
});

const LANE_ACTIONS = Object.freeze({
  SELECT: "select-track",
  "KNOB 1": "sequence-length",
  "KNOB 2": "global-swing",
  "KNOB 3": "tempo",
  "KNOB 4": "patch-assignment"
});

const PIANO_ACTIONS = Object.freeze({
  SELECT: "select-track",
  "KNOB 1": "select-note"
});

const HELD_SELECT_ACTIONS = Object.freeze({
  "KNOB 1": "note-velocity",
  "KNOB 2": "note-pitch",
  "KNOB 3": "note-gate",
  "KNOB 4": "note-tie-alt"
});

export function resolveSequencerEncoder(physicalName, { pianoRoll = false, selectHeld = false } = {}) {
  const physical = FM1_PANEL_ENCODERS[physicalName];
  if (!physical) return null;
  let action = LANE_ACTIONS[physicalName] || null;
  if (pianoRoll) action = selectHeld
    ? (HELD_SELECT_ACTIONS[physicalName] || PIANO_ACTIONS[physicalName] || null)
    : (PIANO_ACTIONS[physicalName] || null);
  return action ? { physicalName, scanIndex: physical.scanIndex, action } : null;
}

export const SEQUENCER_BUTTONS = Object.freeze({
  SEQ: "toggle-piano-roll",
  SELECT_TAP: null
});
