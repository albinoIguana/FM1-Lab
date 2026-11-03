import test from "node:test";
import assert from "node:assert/strict";
import { createFactoryInstruments, normalizeInstrument } from "../src/instrument.js";
import { decodeNativePatch, nativeFixedFrequency, nativePatchName, topologyEdges, writeNativeField } from "../src/native-fm6.js";

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
  bytes[op1 + 2] = 45;
  bytes[op1 + 3] = 33;
  bytes[op1 + 4] = 99;
  bytes[op1 + 5] = 78;
  bytes[op1 + 6] = 42;
  bytes[op1 + 7] = 0;
  const seed = createFactoryInstruments()[0].patches[0];
  seed.feedbackOperator = 3;
  const patch = decodeNativePatch(seed, bytes);
  assert.equal(nativePatchName(bytes), "NIGHTBELL");
  assert.equal(patch.name, "NIGHTBELL");
  assert.equal(patch.algorithm, 12);
  assert.equal(patch.feedback, 5);
  assert.equal(patch.operators[0].ratio, 3);
  assert.equal(patch.operators[0].level, 51);
  assert.deepEqual(patch.operators[0].rates, [75, 60, 45, 33]);
  assert.deepEqual(patch.operators[0].levels, [99, 78, 42, 0]);
  assert.equal(patch.operators[2].feedback, 5);
  assert.equal(patch.operators[0].feedback, 0);
});

test("operator controls encode to Felucca's native 155-byte layout", () => {
  const bytes = blankVoice();
  const patch = createFactoryInstruments()[0].patches[0];
  patch.algorithm = 32;
  patch.feedback = 6;
  patch.name = "Chrome Dust";
  patch.operators[0] = { ...patch.operators[0], ratio: 2.5, level: 50, rates: [72, 41, 63, 19], levels: [99, 80, 63, 0] };
  let out = writeNativeField(bytes, patch, "algorithm");
  out = writeNativeField(out, patch, "feedback");
  out = writeNativeField(out, patch, "name");
  for (const field of ["ratio", "level", "rate1", "level1", "rate2", "level2", "rate3", "level3", "rate4", "level4"]) {
    out = writeNativeField(out, patch, field, 1);
  }
  const op1 = 5 * 21;
  assert.equal(out[134], 31);
  assert.equal(out[135], 6);
  assert.equal(out[145], "C".charCodeAt(0));
  assert.equal(out[op1 + 17], 0);
  assert.equal(out[op1 + 18], 2);
  assert.equal(out[op1 + 19], 25);
  assert.equal(out[op1 + 16], 50);
  assert.deepEqual(Array.from(out.slice(op1, op1 + 4)), [72, 41, 63, 19]);
  assert.deepEqual(Array.from(out.slice(op1 + 4, op1 + 8)), [99, 80, 63, 0]);
});

test("fixed-frequency operators retain their native base and fine tuning", () => {
  const bytes = blankVoice();
  const patch = createFactoryInstruments()[0].patches[0];
  patch.operators[0] = { ...patch.operators[0], nativeMode: 1, nativeCoarse: 2, nativeFine: 35 };
  let out = writeNativeField(bytes, patch, "mode", 1);
  out = writeNativeField(out, patch, "coarse", 1);
  out = writeNativeField(out, patch, "fine", 1);
  const op1 = 5 * 21;
  assert.equal(out[op1 + 17], 1);
  assert.equal(out[op1 + 18], 2);
  assert.equal(out[op1 + 19], 35);
  const decoded = decodeNativePatch(patch, out);
  assert.equal(decoded.operators[0].nativeMode, 1);
  assert.equal(decoded.operators[0].nativeCoarse, 2);
  assert.equal(decoded.operators[0].nativeFine, 35);
  assert.equal(decoded.operators[0].fixedHz, nativeFixedFrequency(2, 35));
  assert.equal(nativeFixedFrequency(0, 0), 1);
});

test("native algorithm routing masks become source-to-destination edges", () => {
  const edges = topologyEdges({ modulators: [0b000010, 0, 0b000001, 0, 0, 0] });
  assert.deepEqual(edges, [[2, 1], [1, 3]]);
});
