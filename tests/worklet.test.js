import test from "node:test";
import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import vm from "node:vm";

test("FM1-Lab AudioWorklet boots Felucca, transfers native patches, routes MIDI and renders stereo audio", async () => {
  const source = await readFile(new URL("../src/felucca-worklet.js", import.meta.url), "utf8");
  const memory = { buffer: new ArrayBuffer(16384) };
  const patchPointer = 4096, leftPointer = 8192, rightPointer = 12288, patchSize = 155;
  const events = [];
  const exports = {
    memory,
    _initialize() { events.push("initialize"); },
    fh_nor_erase() { events.push("erase"); },
    fh_master(value) { events.push(["master", value]); },
    fh_boot() { events.push("boot"); },
    fh_sample_rate() { return 44100; },
    fh_fm6_api_version() { return 1; },
    fh_fm6_track_count() { return 4; },
    fh_fm6_patch_size() { return patchSize; },
    fh_fm6_factory_count() { return 8; },
    fh_fm6_factory_load(track, preset) { events.push(["factory", track, preset]); return track < 4 && preset < 8 ? 1 : 0; },
    fh_fm6_patch_buffer() { return patchPointer; },
    fh_fm6_patch_read(track, pointer, capacity) {
      if (track >= 4 || capacity < patchSize) return 0;
      new Uint8Array(memory.buffer, pointer, patchSize).fill(track + 1);
      return patchSize;
    },
    fh_fm6_patch_write(track, pointer, length) {
      events.push(["patch-write", track, length, new Uint8Array(memory.buffer, pointer, length)[0]]);
      return track < 4 && length === patchSize ? 1 : 0;
    },
    fh_fm6_carrier_mask(algorithm) { return algorithm === 1 ? 1 : 3; },
    fh_fm6_feedback_operator() { return 4; },
    fh_fm6_modulator_mask(algorithm, operator) { return operator === 1 ? 2 : 0; },
    fh_midi(status, data1, data2) { events.push(["midi", status, data1, data2]); return 1; },
    fh_render(count) {
      new Float32Array(memory.buffer, leftPointer, count).fill(0.25);
      new Float32Array(memory.buffer, rightPointer, count).fill(-0.25);
    },
    fh_out_l() { return leftPointer; },
    fh_out_r() { return rightPointer; }
  };

  let registeredName, Processor;
  class FakeAudioWorkletProcessor {
    constructor() {
      this.port = { messages: [], postMessage(message) { this.messages.push(message); } };
    }
  }
  const context = {
    AudioWorkletProcessor: FakeAudioWorkletProcessor,
    WebAssembly: { async instantiate() { return { instance: { exports } }; } },
    performance: { now: () => 10 },
    registerProcessor(name, constructor) { registeredName = name; Processor = constructor; }
  };
  vm.runInNewContext(source, context, { filename: "felucca-worklet.js" });
  assert.equal(registeredName, "fm1lab-felucca");
  const processor = new Processor();

  await processor.onMessage({ type: "load", wasm: new ArrayBuffer(0), master: 700 });
  assert.ok(processor.port.messages.some(message => message.type === "ready" && message.factoryCount === 8));
  assert.deepEqual(events.slice(0, 4), ["initialize", "erase", ["master", 700], "boot"]);

  await processor.onMessage({ type: "factory-load", id: 1, track: 2, preset: 5 });
  assert.ok(processor.port.messages.some(message => message.type === "factory-loaded" && message.id === 1));

  await processor.onMessage({ type: "patch-read", id: 2, track: 1 });
  const read = processor.port.messages.find(message => message.type === "patch" && message.id === 2);
  assert.equal(read.patch.length, patchSize);
  assert.equal(read.patch[0], 2);

  await processor.onMessage({ type: "patch-write", id: 3, track: 1, patch: new Uint8Array(patchSize).fill(77) });
  assert.ok(processor.port.messages.some(message => message.type === "patch-written" && message.id === 3));
  assert.ok(events.some(event => Array.isArray(event) && event[0] === "patch-write" && event[1] === 1 && event[3] === 77));

  await processor.onMessage({ type: "topology", id: 4, algorithm: 1 });
  const graph = processor.port.messages.find(message => message.type === "topology" && message.id === 4);
  assert.equal(graph.carriers, 1);
  assert.equal(graph.feedback, 4);
  assert.deepEqual(Array.from(graph.modulators), [2, 0, 0, 0, 0, 0]);

  await processor.onMessage({ type: "midi", data: [0x92, 64, 100] });
  assert.ok(events.some(event => Array.isArray(event) && event[0] === "midi" && event[1] === 0x92));

  const left = new Float32Array(128), right = new Float32Array(128);
  assert.equal(processor.process([], [[left, right]]), true);
  assert.equal(left[0], 0.25);
  assert.equal(right[0], -0.25);
});
