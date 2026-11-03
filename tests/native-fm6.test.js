import test from "node:test";
import assert from "node:assert/strict";
import { createFactoryInstruments, normalizeInstrument } from "../src/instrument.js";
import { decodeNativePatch, nativePatchName, topologyEdges, writeNativeField } from "../src/native-fm6.js";

function blankVoice() {
  const bytes = new Uint8Array(155);
  bytes.fill(99, 0, 8);
  bytes.fill(32, 145, 155);
  return bytes;
}

test("two FM1-Lab Instruments each contain four Felucca factory voices", () => {
  const instruments = createFactoryInstruments();
  assert.equal(instruments.length, 2);
  assert.deepEqual(instruments[0].factorySlots, [0, 1, 2, 3]);
  assert.deepEqual(instruments[1].factorySlots, [4, 5, 6, 7]);
  for (const instrument of instruments) assert.equal(instrument.patches.length, 4);
  const saved = normalizeInstrument(instruments[1]);
  assert.deepEqual(saved.factorySlots, [4, 5, 6, 7]);
});

test("decode native voice reads algorithm, feedback, operator order, envelope and voice name", () => {
  const bytes = blankVoice();
  bytes[134] = 11;
  bytes[135] = 5;
  bytes.set(new TextEncoder().encode("NIGHTBELL "), 145);
  const op1 = 5 * 21;
  bytes[op1 + 16] = 50;
  bytes[op1 + 17] = 0;
  bytes[op1 + 18] = 2;
  bytes[op1 + 19] = 50;
  bytes[op1] = 75;
  bytes[op1 + 1] = 60;
  bytes[op1 + 6] = 42;
  bytes[op1 + 3] = 33;
  const seed = createFactoryInstruments()[0].patches[0];
  seed.feedbackOperator = 3;
  const patch = decodeNativePatch(seed, bytes);
  assert.equal(nativePatchName(bytes), "NIGHTBELL");
  assert.equal(patch.name, "NIGHTBELL");
  assert.equal(patch.algorithm, 12);
  assert.equal(patch.feedback, 5);
  assert.equal(patch.operators[0].ratio, 3);
  assert.equal(patch.operators[0].level, 51);
  assert.equal(patch.operators[0].attack, 75);
  assert.equal(patch.operators[0].decay, 60);
  assert.equal(patch.operators[0].sustain, 42);
  assert.equal(patch.operators[0].release, 33);
  assert.equal(patch.operators[2].feedback, 71);
  assert.equal(patch.operators[0].feedback, 0);
});

test("operator controls encode to Felucca's native 155-byte layout", () => {
  const bytes = blankVoice();
  const patch = createFactoryInstruments()[0].patches[0];
  patch.algorithm = 32;
  patch.feedback = 6;
  patch.name = "Chrome Dust";
  patch.operators[0] = { ...patch.operators[0], ratio: 2.5, level: 50, attack: 72, decay: 41, sustain: 63, release: 19 };
  let out = writeNativeField(bytes, patch, "algorithm");
  out = writeNativeField(out, patch, "feedback");
  out = writeNativeField(out, patch, "name");
  for (const field of ["ratio", "level", "attack", "decay", "sustain", "release"]) {
    out = writeNativeField(out, patch, field, 1);
  }
  const op1 = 5 * 21;
  assert.equal(out[134], 31);
  assert.equal(out[135], 6);
  assert.equal(out[145], "C".charCodeAt(0));
  assert.equal(out[op1 + 17], 0);
  assert.equal(out[op1 + 18], 2);
  assert.equal(out[op1 + 19], 50);
  assert.equal(out[op1 + 16], 50);
  assert.equal(out[op1], 72);
  assert.equal(out[op1 + 1], 41);
  assert.equal(out[op1 + 6], 63);
  assert.equal(out[op1 + 3], 19);
});

test("native algorithm routing masks become source-to-destination edges", () => {
  const edges = topologyEdges({ modulators: [0b000010, 0, 0b000001, 0, 0, 0] });
  assert.deepEqual(edges, [[2, 1], [1, 3]]);
});
