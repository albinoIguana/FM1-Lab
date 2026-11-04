import test from "node:test";
import assert from "node:assert/strict";
import { createTwoTrackSequence, setStep } from "../src/sequencer-model.js";
import { renderSequencer } from "../src/ui/sequencer-view.js";

test("SEQ view always displays two fixed-width 64-step lanes", () => {
  const html = renderSequencer(createTwoTrackSequence(), 0, false, false, ["LEAD", "BASS", "PAD", "FX"]);
  assert.equal((html.match(/class="seq-step/g) || []).length, 128);
  assert.equal((html.match(/class="seq-lane /g) || []).length, 2);
  assert.match(html, /64-step timeline/);
  assert.match(html, /LEAD/);
  assert.match(html, /BASS/);
  assert.match(html, /data-seq-patch="0"/);
  assert.match(html, /data-seq-patch="1"/);
});

test("SEQ view shows active steps and disables positions beyond each track length", () => {
  let sequence = createTwoTrackSequence();
  sequence.tracks[0].length = 8;
  sequence = setStep(sequence, 0, 3, {active:true});
  const html = renderSequencer(sequence, 3);
  assert.match(html, /class="seq-step active chosen"/);
  assert.match(html, /class="seq-step beyond-length"/);
});

test("advanced note edit exposes pitch, velocity, gate, tie and slide", () => {
  const html = renderSequencer(createTwoTrackSequence(), 0, true, true);
  assert.match(html, /data-seq-note/);
  assert.match(html, /data-seq-piano-step/);
  assert.match(html, /data-seq-velocity/);
  assert.match(html, /data-seq-gate/);
  assert.match(html, /data-seq-tie/);
  assert.match(html, /data-seq-slide/);
});

test("SEQ view rejects any model other than exactly two tracks", () => {
  assert.throws(() => renderSequencer({tracks:[{}]}), TypeError);
});
