import test from "node:test";
import assert from "node:assert/strict";
import { applyMacros, DEFAULT_MACROS, operatorByteOffset, DX7_GLOBAL } from "../src/dx7-macros.js";

test("macro engine works on the official 156-byte M-VAVE DX7 patch format", () => {
  const base = new Uint8Array(156);
  base[operatorByteOffset(1, 16)] = 40;
  base[operatorByteOffset(2, 16)] = 40;
  base[operatorByteOffset(1, 6)] = 50;
  base[DX7_GLOBAL.feedback] = 2;
  const result = applyMacros(base, { ...DEFAULT_MACROS, brightness: 100, body: 100 }, 0b000001);
  assert.equal(result.length, 156);
  assert.ok(result[operatorByteOffset(1, 16)] > 40);
  assert.equal(result[operatorByteOffset(2, 16)], 40);
  assert.ok(result[operatorByteOffset(1, 6)] > 50);
  assert.equal(base[operatorByteOffset(1, 16)], 40, "base patch remains unchanged");
});

test("edge raises feedback and modulator index without changing carrier output", () => {
  const base = new Uint8Array(156);
  base[operatorByteOffset(1, 16)] = 55;
  base[operatorByteOffset(2, 16)] = 55;
  base[DX7_GLOBAL.feedback] = 3;
  const result = applyMacros(base, { ...DEFAULT_MACROS, edge: 100 }, 0b000001);
  assert.equal(result[operatorByteOffset(1, 16)], 55);
  assert.ok(result[operatorByteOffset(2, 16)] > 55);
  assert.equal(result[DX7_GLOBAL.feedback], 7);
});

test("motion changes LFO rate and depth, while centered macros preserve source values", () => {
  const base = new Uint8Array(156);
  base[DX7_GLOBAL.lfoSpeed] = 40;
  base[DX7_GLOBAL.lfoAmpDepth] = 20;
  assert.deepEqual(applyMacros(base), base);
  const moved = applyMacros(base, { ...DEFAULT_MACROS, motion: 100 });
  assert.ok(moved[DX7_GLOBAL.lfoSpeed] > 40);
  assert.ok(moved[DX7_GLOBAL.lfoAmpDepth] > 20);
});

test("operator byte mapping follows M-VAVE's OP6-first storage order", () => {
  assert.equal(operatorByteOffset(1, 16), 121);
  assert.equal(operatorByteOffset(6, 16), 16);
  assert.throws(() => operatorByteOffset(7, 0), RangeError);
});
