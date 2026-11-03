import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";

const wasmPath = process.argv[2];
if (!wasmPath) throw new Error("Usage: node scripts/felucca-api-smoke.mjs path/to/felucca.wasm");

const bytes = await readFile(wasmPath);
const { instance } = await WebAssembly.instantiate(bytes, {});
const ex = instance.exports;
assert.equal(typeof ex._initialize, "function", "WASM initialization export is missing");
ex._initialize();
ex.fh_nor_erase();
ex.fh_boot();

assert.equal(ex.fh_fm6_api_version(), 1, "Unexpected FM1-Lab bridge API version");
assert.equal(ex.fh_fm6_track_count(), 4, "Felucca should expose its four synth parts");
assert.equal(ex.fh_fm6_patch_size(), 155, "FM6 voice size must match Felucca's native format");
assert.equal(ex.fh_fm6_factory_count(), 8, "Felucca should expose its eight native FM6 factory voices");
assert.equal(ex.fh_fm6_carrier_mask(0), 0, "Algorithm numbers are one-based");
assert.equal(ex.fh_fm6_carrier_mask(33), 0, "Algorithm 33 must be rejected");

assert.equal(ex.fh_fm6_modulator_mask(0, 1), 0, "Algorithm numbers are one-based");
assert.equal(ex.fh_fm6_modulator_mask(1, 0), 0, "Operator numbers are one-based");
assert.equal(ex.fh_fm6_feedback_operator(33), 0, "Algorithm 33 must be rejected");
for (let algorithm = 1; algorithm <= 32; algorithm++) {
  const carriers = ex.fh_fm6_carrier_mask(algorithm);
  assert.ok(carriers > 0 && carriers < 64, `Algorithm ${algorithm} must expose a valid carrier mask`);
  const feedback = ex.fh_fm6_feedback_operator(algorithm);
  assert.ok(feedback >= 1 && feedback <= 6, `Algorithm ${algorithm} must expose its feedback operator`);
  for (let operator = 1; operator <= 6; operator++) {
    const modulators = ex.fh_fm6_modulator_mask(algorithm, operator);
    assert.ok(modulators >= 0 && modulators < 64, `Algorithm ${algorithm}, OP ${operator} must expose its routing mask`);
  }
}

const scratchPtr = ex.fh_fm6_patch_buffer();
const size = ex.fh_fm6_patch_size();
const scratch = new Uint8Array(ex.memory.buffer, scratchPtr, size);

// Load native factory voices across the four Felucca parts, then test isolated edits.
for (let track = 0; track < 4; track++) {
  assert.equal(ex.fh_fm6_factory_load(track, track), 1, "Could not load factory voice " + (track + 1));
}
assert.equal(ex.fh_fm6_factory_load(4, 0), 0, "Track 5 must be rejected");
assert.equal(ex.fh_fm6_factory_load(0, 8), 0, "Factory voice 9 must be rejected");

assert.equal(ex.fh_fm6_patch_read(0, scratchPtr, size), size, "Could not read track 1 patch");
const before = scratch.slice();
assert.equal(ex.fh_fm6_patch_read(1, scratchPtr, size), size, "Could not read track 2 patch");
const untouchedTrack = scratch.slice();

const candidate = before.slice();
candidate[134] = (candidate[134] + 1) & 31;
candidate.set(new TextEncoder().encode("FM1LAB0001"), 145);
scratch.set(candidate);
assert.equal(ex.fh_fm6_patch_write(0, scratchPtr, size), 1, "Native FM6 patch write was rejected");
assert.equal(ex.fh_fm6_patch_read(0, scratchPtr, size), size, "Could not read back the written patch");
assert.deepEqual(scratch, candidate, "FM6 patch did not survive a native write/read round trip");
assert.notEqual(ex.fh_fm6_carrier_mask(candidate[134] + 1), 0, "The selected algorithm has no carrier operators");

assert.equal(ex.fh_fm6_patch_read(1, scratchPtr, size), size, "Could not re-read track 2 patch");
assert.deepEqual(scratch, untouchedTrack, "Writing one patch unexpectedly changed another track");
assert.equal(ex.fh_sample_rate(), 44100, "Unexpected Felucca sample rate");

console.log("FM1-Lab native bridge OK: 4 tracks, 155-byte FM6 patches, 32 native routing graphs, isolated patch round trip.");
