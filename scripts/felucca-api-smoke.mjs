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

// Verify all eight Felucca factory slots, then build the two four-part FM1-Lab banks.
for (let preset = 0; preset < 8; preset++) {
  assert.equal(ex.fh_fm6_factory_load(0, preset), 1, "Could not load factory voice " + (preset + 1));
  assert.equal(ex.fh_fm6_patch_read(0, scratchPtr, size), size, "Could not read factory voice " + (preset + 1));
}
for (let track = 0; track < 4; track++) {
  assert.equal(ex.fh_fm6_factory_load(track, track), 1, "Could not load factory voice " + (track + 1));
}
assert.equal(ex.fh_fm6_factory_load(4, 0), 0, "Track 5 must be rejected");
assert.equal(ex.fh_fm6_factory_load(0, 8), 0, "Factory voice 9 must be rejected");

// Render a real MIDI note through Felucca's native FM6 engine and require non-silent output.
assert.equal(ex.fh_midi(0x90, 60, 100), 1, "Felucca rejected MIDI note-on");
let audioPeak = 0;
for (let block = 0; block < 12; block++) {
  ex.fh_render(1024);
  const samples = new Float32Array(ex.memory.buffer, ex.fh_out_l(), 1024);
  for (const sample of samples) audioPeak = Math.max(audioPeak, Math.abs(sample));
}
assert.ok(audioPeak > 0.00001, "Felucca's FM6 renderer produced silent audio");
assert.equal(ex.fh_midi(0x80, 60, 0), 1, "Felucca rejected MIDI note-off");
for (let block = 0; block < 4; block++) ex.fh_render(1024);

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

console.log("FM1-Lab native bridge OK: 4 tracks, 155-byte FM6 patches, 32 native routing graphs, live audio render, isolated patch round trip.");
