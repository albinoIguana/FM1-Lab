// SPDX-License-Identifier: GPL-3.0-only
// FM1-Lab's lean AudioWorklet host for the pinned Felucca firmware.
// The custom FM1-Lab UI talks to this processor; Felucca owns the actual DSP.
class FM1LabFeluccaProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null;
    this.busyMilliseconds = 0;
    this.renderedFrames = 0;
    this.port.onmessage = event => this.onMessage(event.data);
  }

  async onMessage(message) {
    if (message.type === "load") {
      try {
        const { instance } = await WebAssembly.instantiate(message.wasm, {});
        const ex = instance.exports;
        ex._initialize();
        ex.fh_nor_erase();
        ex.fh_master(message.master ?? 700);
        ex.fh_boot();
        this.ex = ex;
        this.port.postMessage({
          type: "ready",
          rate: ex.fh_sample_rate(),
          fm6Api: ex.fh_fm6_api_version(),
          tracks: ex.fh_fm6_track_count(),
          patchSize: ex.fh_fm6_patch_size(),
          factoryCount: ex.fh_fm6_factory_count()
        });
      } catch (error) {
        this.port.postMessage({ type: "error", message: String(error && error.message || error) });
      }
      return;
    }

    const ex = this.ex;
    if (!ex) return;

    if (message.type === "factory-load") {
      const ok = ex.fh_fm6_factory_load(message.track >>> 0, message.preset >>> 0);
      this.port.postMessage({
        type: ok ? "factory-loaded" : "patch-error",
        id: message.id,
        track: message.track,
        message: ok ? undefined : "Felucca rejected factory preset."
      });
    } else if (message.type === "patch-read") {
      const pointer = ex.fh_fm6_patch_buffer();
      const size = ex.fh_fm6_patch_size();
      const count = ex.fh_fm6_patch_read(message.track >>> 0, pointer, size);
      if (count !== size) {
        this.port.postMessage({ type: "patch-error", id: message.id, message: "Felucca rejected FM6 patch read." });
        return;
      }
      const patch = new Uint8Array(ex.memory.buffer, pointer, size).slice();
      this.port.postMessage({ type: "patch", id: message.id, track: message.track, patch }, [patch.buffer]);
    } else if (message.type === "patch-write") {
      const pointer = ex.fh_fm6_patch_buffer();
      const size = ex.fh_fm6_patch_size();
      const patch = new Uint8Array(message.patch);
      if (patch.length !== size) {
        this.port.postMessage({ type: "patch-error", id: message.id, message: "FM6 patch must be exactly " + size + " bytes." });
        return;
      }
      new Uint8Array(ex.memory.buffer, pointer, size).set(patch);
      const ok = ex.fh_fm6_patch_write(message.track >>> 0, pointer, size);
      this.port.postMessage({
        type: ok ? "patch-written" : "patch-error",
        id: message.id,
        track: message.track,
        message: ok ? undefined : "Felucca rejected FM6 patch write."
      });
    } else if (message.type === "topology") {
      const algorithm = message.algorithm >>> 0;
      if (algorithm < 1 || algorithm > 32) {
        this.port.postMessage({ type: "topology-error", id: message.id, message: "FM6 algorithm must be 1–32." });
        return;
      }
      const carriers = ex.fh_fm6_carrier_mask(algorithm);
      const feedback = ex.fh_fm6_feedback_operator(algorithm);
      const modulators = Array.from({ length: 6 }, (_, index) => ex.fh_fm6_modulator_mask(algorithm, index + 1));
      this.port.postMessage({ type: "topology", id: message.id, algorithm, carriers, feedback, modulators });
    } else if (message.type === "midi") {
      ex.fh_midi(message.data[0] | 0, message.data[1] | 0, message.data[2] | 0);
    } else if (message.type === "master") {
      ex.fh_master(message.value | 0);
    }
  }

  process(inputs, outputs) {
    const ex = this.ex;
    const output = outputs[0];
    if (!output || !output[0]) return true;
    const left = output[0];
    const right = output[1];
    if (!ex) {
      left.fill(0);
      right?.fill(0);
      return true;
    }

    // AudioWorklet quantum sizes vary by browser; Felucca accepts up to 1024 frames per render call.
    const clock = () => globalThis.performance?.now?.() ?? currentTime * 1000;
    const started = clock();
    let offset = 0;
    while (offset < left.length) {
      const count = Math.min(1024, left.length - offset);
      ex.fh_render(count);
      const memory = ex.memory.buffer;
      left.set(new Float32Array(memory, ex.fh_out_l(), count), offset);
      if (right) right.set(new Float32Array(memory, ex.fh_out_r(), count), offset);
      offset += count;
    }
    this.busyMilliseconds += clock() - started;
    this.renderedFrames += left.length;
    if (this.renderedFrames >= 44100) {
      const load = this.busyMilliseconds / (this.renderedFrames / 44.1);
      this.port.postMessage({ type: "load", load });
      this.busyMilliseconds = 0;
      this.renderedFrames = 0;
    }
    return true;
  }
}

registerProcessor("fm1lab-felucca", FM1LabFeluccaProcessor);
