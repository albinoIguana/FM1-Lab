import test from "node:test";
import assert from "node:assert/strict";
import { applyMacros, DEFAULT_MACROS, operatorByteOffset, DX7_GLOBAL } from "../src/dx7-macros.js";

test("centered macros preserve all 156 source bytes", () => {
  const base = Uint8Array.from({ length: 156 }, (_, i) => i % 100);
  assert.deepEqual(applyMacros(base), base);
});

test("brightness shapes modulators, not direct carrier output", () => {
  const base = new Uint8Array(156);
  base[operatorByteOffset(1, 16)] = 55;
  base[operatorByteOffset(2, 16)] = 40;
  const result = applyMacros(base, { ...DEFAULT_MACROS, brightness: 100 }, 0b000001);
  assert.equal(result[operatorByteOffset(1, 16)], 55);
  assert.ok(result[operatorByteOffset(2, 16)] > 40);
  assert.equal(base[operatorByteOffset(2, 16)], 40);
});

test("body changes carrier sustain while preserving modulator envelope", () => {
  const base = new Uint8Array(156);
  base[operatorByteOffset(1, 6)] = 50;
  base[operatorByteOffset(2, 6)] = 50;
  const result = applyMacros(base, { ...DEFAULT_MACROS, body: 100 }, 0b000001);
  assert.ok(result[operatorByteOffset(1, 6)] > 50);
  assert.equal(result[operatorByteOffset(2, 6)], 50);
});

test("motion reshapes modulator EG contour rather than adding LFO wobble", () => {
  const base = new Uint8Array(156);
  base[operatorByteOffset(2, 6)] = 40;
  base[operatorByteOffset(2, 7)] = 60;
  base[DX7_GLOBAL.lfoSpeed] = 30;
  base[DX7_GLOBAL.lfoAmpDepth] = 20;
  const result = applyMacros(base, { ...DEFAULT_MACROS, motion: 100 }, 0b000001);
  assert.ok(result[operatorByteOffset(2, 6)] > 40);
  assert.ok(result[operatorByteOffset(2, 7)] < 60);
  assert.equal(result[DX7_GLOBAL.lfoSpeed], 30);
  assert.equal(result[DX7_GLOBAL.lfoAmpDepth], 20);
});

test("dirt raises bounded feedback without changing carrier or modulator levels", () => {
  const base = new Uint8Array(156);
  base[DX7_GLOBAL.feedback] = 3;
  base[operatorByteOffset(1, 16)] = 55;
  base[operatorByteOffset(2, 16)] = 45;
  const result = applyMacros(base, { ...DEFAULT_MACROS, dirt: 100 }, 0b000001);
  assert.equal(result[DX7_GLOBAL.feedback], 7);
  assert.equal(result[operatorByteOffset(1, 16)], 55);
  assert.equal(result[operatorByteOffset(2, 16)], 45);
});

test("operator byte mapping follows M-VAVE's OP6-first storage order", () => {
  assert.equal(operatorByteOffset(1, 16), 121);
  assert.equal(operatorByteOffset(6, 16), 16);
  assert.throws(() => operatorByteOffset(7, 0), RangeError);
});
