import test from "node:test";
import assert from "node:assert/strict";
import {
  createTwoTrackSequence, normalizeSequence, setTrackLength, setTrackDivision,
  setTrackSwing, setStep, selectSequenceTrack, setGlobalKeyShift, DIVISIONS
} from "../src/sequencer-model.js";

test("sequencer contract is exactly two independent 64-step tracks", () => {
  const sequence = createTwoTrackSequence();
  assert.equal(sequence.tracks.length, 2);
  assert.equal(sequence.tracks[0].steps.length, 64);
  assert.equal(sequence.tracks[1].steps.length, 64);
  assert.notEqual(sequence.tracks[0].steps, sequence.tracks[1].steps);
});

test("track length clamps to 8–64 without resizing the fixed timeline", () => {
  const sequence = createTwoTrackSequence();
  const next = setTrackLength(sequence, 0, 100);
  assert.equal(next.tracks[0].length, 64);
  assert.equal(next.tracks[0].steps.length, 64);
  assert.equal(next.tracks[1].length, 16);
  assert.equal(setTrackLength(sequence, 1, 2).tracks[1].length, 8);
});

test("division options include straight and triplet timing", () => {
  assert.deepEqual(DIVISIONS.map(item => item.label), ["1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T"]);
  assert.equal(setTrackDivision(createTwoTrackSequence(), 0, 6).tracks[0].division, 6);
});

test("swing and global key shift are bounded and independent of per-track settings", () => {
  const sequence = setTrackSwing(createTwoTrackSequence(), 1, 90);
  assert.equal(sequence.tracks[1].swing, 75);
  assert.equal(sequence.tracks[0].swing, 0);
  assert.equal(setGlobalKeyShift(sequence, -99).keyShift, -24);
});

test("step edits preserve velocity, note, gate, tie and slide as independent properties", () => {
  const sequence = setStep(createTwoTrackSequence(), 0, 7, {
    active: true, note: 72, velocity: 91, gate: 110, tie: true, slide: true
  });
  const step = sequence.tracks[0].steps[7];
  assert.deepEqual(
    {active:step.active,note:step.note,velocity:step.velocity,gate:step.gate,tie:step.tie,slide:step.slide},
    {active:true,note:72,velocity:91,gate:110,tie:true,slide:true}
  );
  assert.equal(sequence.tracks[1].steps[7].active, false);
});

test("selected track is a two-value state and normalization rejects other track counts", () => {
  assert.equal(selectSequenceTrack(createTwoTrackSequence(), 1).selectedTrack, 1);
  assert.throws(() => selectSequenceTrack(createTwoTrackSequence(), 2), RangeError);
  assert.throws(() => normalizeSequence({tracks:[{}, {}, {}]}), TypeError);
});

test("step index validation protects the fixed 64-step timeline", () => {
  assert.throws(() => setStep(createTwoTrackSequence(), 0, 64, {active:true}), RangeError);
});
