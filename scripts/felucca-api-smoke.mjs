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

const flash = ex.fh_nor();
const sourcePtr = flash + 0x80000;
const readbackPtr = flash + 0x81000;
const size = ex.fh_fm6_patch_size();
const source = new Uint8Array(ex.memory.buffer, sourcePtr, size);
const readback = new Uint8Array(ex.memory.buffer, readbackPtr, size);

assert.equal(ex.fh_fm6_patch_read(0, readbackPtr, size), size, "Could not read track 1 patch");
const before = readback.slice();
assert.equal(ex.fh_fm6_patch_read(1, readbackPtr, size), size, "Could not read track 2 patch");
const untouchedTrack = readback.slice();

const candidate = before.slice();
candidate[134] = (candidate[134] + 1) & 31; // native FM6 algorithm byte
candidate.set(new TextEncoder().encode("FM1LAB0001"), 145); // ten-byte native voice name
source.set(candidate);
assert.equal(ex.fh_fm6_patch_write(0, sourcePtr, size), 1, "Native FM6 patch write was rejected");
assert.equal(ex.fh_fm6_patch_read(0, readbackPtr, size), size, "Could not read back the written patch");
assert.deepEqual(readback, candidate, "FM6 patch did not survive a native write/read round trip");
assert.notEqual(ex.fh_fm6_carrier_mask(candidate[134] + 1), 0, "The selected algorithm has no carrier operators");

assert.equal(ex.fh_fm6_patch_read(1, readbackPtr, size), size, "Could not re-read track 2 patch");
assert.deepEqual(readback, untouchedTrack, "Writing one patch unexpectedly changed another track");
assert.equal(ex.fh_sample_rate(), 44100, "Unexpected Felucca sample rate");

console.log("FM1-Lab native bridge OK: 4 tracks, 155-byte FM6 patches, 32 native routing graphs, isolated patch round trip.");
