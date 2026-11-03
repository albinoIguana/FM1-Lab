import test from "node:test";
import assert from "node:assert/strict";
import { createFactoryInstruments, createDefaultPatch, normalizeInstrument, migrateLegacyPatch } from "../src/instrument.js";

test("factory creates four Instruments with four independent six-operator patches each",()=>{
  const instruments=createFactoryInstruments();
  assert.equal(instruments.length,4);
  for(const instrument of instruments){
    assert.equal(instrument.patches.length,4);
    for(const patch of instrument.patches)assert.equal(patch.operators.length,6);
  }
  assert.notEqual(instruments[0].patches[0],instruments[0].patches[1]);
  assert.notEqual(instruments[0].patches[0].operators[0],instruments[0].patches[1].operators[0]);
});

test("normalization preserves four patch data and rejects an incomplete Instrument",()=>{
  const source=createFactoryInstruments()[0];
  source.patches[2].operators[3].ratio=7.5;
  const normalized=normalizeInstrument(source);
  assert.equal(normalized.patches[2].operators[3].ratio,7.5);
  assert.throws(()=>normalizeInstrument({...source,patches:source.patches.slice(0,3)}),/exactly four/);
});

test("legacy single patch migrates into an Instrument without losing the original patch",()=>{
  const legacy=createDefaultPatch("Old Patch",1);
  legacy.operators[0].ratio=9;
  const migrated=migrateLegacyPatch(legacy);
  assert.equal(migrated.patches.length,4);
  assert.equal(migrated.patches[0].name,"Old Patch");
  assert.equal(migrated.patches[0].operators[0].ratio,9);
  assert.equal(migrated.patches[1].operators.length,6);
});
