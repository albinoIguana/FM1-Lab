import test from "node:test";
import assert from "node:assert/strict";
import {
  createTwoTrackSequence, normalizeSequence, setTempo, setGlobalSwing, setTrackPatch,
  setTrackLength, setStep, selectSequenceTrack, SEQUENCE_LENGTHS
} from "../src/sequencer-model.js";

test("sequencer contract is exactly two independent 64-step tracks", () => {
  const sequence = createTwoTrackSequence();
  assert.equal(sequence.tracks.length, 2);
  assert.equal(sequence.tracks[0].steps.length, 64);
  assert.equal(sequence.tracks[1].steps.length, 64);
  assert.notEqual(sequence.tracks[0].steps, sequence.tracks[1].steps);
});

test("tempo, global swing and patch assignment match native bounds", () => {
  const sequence = createTwoTrackSequence();
  assert.equal(setTempo(sequence, 400).tempo, 300);
  assert.equal(setTempo(sequence, 2).tempo, 30);
  assert.equal(setGlobalSwing(sequence, 80).swing, 50);
  assert.equal(setTrackPatch(sequence, 1, 3).tracks[1].patchIndex, 3);
  assert.equal(sequence.tracks[0].patchIndex, 0);
});

test("track lengths use native 16-step increments without resizing the timeline", () => {
  const sequence = createTwoTrackSequence();
  assert.deepEqual(SEQUENCE_LENGTHS, [16, 32, 48, 64]);
  assert.equal(setTrackLength(sequence, 0, 63).tracks[0].length, 64);
  assert.equal(setTrackLength(sequence, 0, 40).tracks[0].length, 32);
  assert.equal(setTrackLength(sequence, 1, 8).tracks[1].length, 16);
  assert.equal(sequence.tracks[1].length, 16);
});

test("step values use the native note, velocity, gate, tie and alternate-tuning fields", () => {
  const sequence = setStep(createTwoTrackSequence(), 0, 7, {
    active: true, note: 72, velocity: 91, gate: 15, tie: true, alt: true
  });
  const step = sequence.tracks[0].steps[7];
  assert.deepEqual(
    {active:step.active,note:step.note,velocity:step.velocity,gate:step.gate,tie:step.tie,alt:step.alt},
    {active:true,note:72,velocity:91,gate:15,tie:true,alt:true}
  );
  assert.equal(sequence.tracks[1].steps[7].active, false);
  assert.equal(setStep(sequence, 0, 7, {gate:99}).tracks[0].steps[7].gate, 15);
});

test("selected track is a two-value state and normalization rejects other track counts", () => {
  assert.equal(selectSequenceTrack(createTwoTrackSequence(), 1).selectedTrack, 1);
  assert.throws(() => selectSequenceTrack(createTwoTrackSequence(), 2), RangeError);
  assert.throws(() => normalizeSequence({tracks:[{}, {}, {}]}), TypeError);
});

test("step index validation protects the fixed 64-step timeline", () => {
  assert.throws(() => setStep(createTwoTrackSequence(), 0, 64, {active:true}), RangeError);
});
