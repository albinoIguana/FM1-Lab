import test from "node:test";
import assert from "node:assert/strict";
import { FM1_PANEL_ENCODERS, SEQUENCER_BUTTONS, resolveSequencerEncoder } from "../src/sequencer-controls.js";

test("physical encoder names map explicitly to firmware scan indices", () => {
  assert.deepEqual(Object.fromEntries(Object.entries(FM1_PANEL_ENCODERS).map(([name,value])=>[name,value.scanIndex])), {
    SELECT:0, ALGORITHM:1, "KNOB 1":2, "KNOB 2":3, "KNOB 3":4, "KNOB 4":5, PRESETS:6
  });
});

test("two-lane screen maps named knobs to native controls", () => {
  assert.equal(resolveSequencerEncoder("SELECT").action, "select-track");
  assert.equal(resolveSequencerEncoder("KNOB 1").action, "sequence-length");
  assert.equal(resolveSequencerEncoder("KNOB 2").action, "global-swing");
  assert.equal(resolveSequencerEncoder("KNOB 3").action, "tempo");
  assert.equal(resolveSequencerEncoder("KNOB 4").action, "patch-assignment");
});

test("piano roll changes knob roles only while SELECT is held", () => {
  assert.equal(resolveSequencerEncoder("KNOB 1",{pianoRoll:true}).action,"select-note");
  assert.equal(resolveSequencerEncoder("KNOB 1",{pianoRoll:true,selectHeld:true}).action,"note-velocity");
  assert.equal(resolveSequencerEncoder("KNOB 2",{pianoRoll:true,selectHeld:true}).action,"note-pitch");
  assert.equal(resolveSequencerEncoder("KNOB 3",{pianoRoll:true,selectHeld:true}).action,"note-gate");
  assert.equal(resolveSequencerEncoder("KNOB 4",{pianoRoll:true,selectHeld:true}).action,"note-tie-alt");
});

test("SEQ enters the piano roll and SELECT tap remains deliberately unused", () => {
  assert.equal(SEQUENCER_BUTTONS.SEQ,"toggle-piano-roll");
  assert.equal(SEQUENCER_BUTTONS.SELECT_TAP,null);
});

test("unknown or non-sequencer panel controls do not acquire accidental actions", () => {
  assert.equal(resolveSequencerEncoder("ALGORITHM"),null);
  assert.equal(resolveSequencerEncoder("PRESETS",{pianoRoll:true}),null);
  assert.equal(resolveSequencerEncoder("UNKNOWN"),null);
});
