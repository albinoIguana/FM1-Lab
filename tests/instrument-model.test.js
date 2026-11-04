import test from "node:test";
import assert from "node:assert/strict";
import { createPatch, createInstrument, normalizeInstrument, renderPatch, patchAlgorithm, setPatchByte } from "../src/instrument-model.js";

test("an Instrument is exactly four independent M-VAVE DX7 patches", () => {
  const patches = Array.from({length:4}, (_,i) => createPatch("PATCH "+(i+1)));
  const instrument = createInstrument("TEST INSTRUMENT", patches);
  assert.equal(instrument.patches.length, 4);
  assert.ok(instrument.patches.every(patch => patch.baseData.length === 156));
  assert.notEqual(instrument.patches[0], instrument.patches[1]);
  assert.throws(() => createInstrument("BROKEN", patches.slice(0,3)), /exactly four/);
});

test("patch normalization preserves voice bytes and macro positions", () => {
  const bytes = new Uint8Array(156); bytes[134] = 17; bytes[121] = 88;
  const patch = createPatch("LEAD", bytes);
  patch.macros.brightness = 72;
  const instrument = normalizeInstrument(createInstrument("BANK", [patch, ...Array.from({length:3},()=>createPatch())]));
  assert.equal(instrument.patches[0].baseData[121], 88);
  assert.equal(instrument.patches[0].macros.brightness, 72);
  assert.equal(patchAlgorithm(instrument.patches[0]), 17);
});

test("rendering applies macros to a derived voice without overwriting saved patch bytes", () => {
  const bytes = new Uint8Array(156);
  bytes[121] = 40;
  const patch = createPatch("BRIGHT", bytes);
  patch.macros.brightness = 100;
  const rendered = renderPatch(patch, 1);
  assert.ok(rendered[121] > 40);
  assert.equal(patch.baseData[121], 40);
});

test("editing one patch byte returns an independent patch value", () => {
  const patch = createPatch("EDIT");
  const changed = setPatchByte(patch, 134, 31);
  assert.equal(patch.baseData[134], 0);
  assert.equal(patchAlgorithm(changed), 31);
});
