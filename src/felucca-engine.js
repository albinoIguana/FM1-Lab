// FM1-Lab adapter for Felucca's actual firmware AudioWorklet.
// Felucca owns synthesis, note allocation, envelopes, FX and patch state.
export class FeluccaEngine {
  constructor({ onState = () => {} } = {}) {
    this.onState = onState;
    this.context = null;
    this.node = null;
    this.readyPromise = null;
    this.pending = new Map();
    this.sequence = 0;
    this.voices = new Set();
    this.api = null;
  }

  async ready() {
    if (this.readyPromise) return this.readyPromise;
    this.onState("STARTING FELUCCA FIRMWARE…");
    this.readyPromise = this.start().catch(async error => {
      this.node?.disconnect();
      this.node = null;
      if (this.context) await this.context.close().catch(() => {});
      this.context = null;
      this.pending.clear();
      this.readyPromise = null;
      this.onState("FELUCCA ENGINE OFFLINE");
      throw error;
    });
    return this.readyPromise;
  }

  async start() {
    const AudioContextClass = window.AudioContext || window.webkitAudioContext;
    if (!AudioContextClass) throw new Error("This browser does not support Web Audio.");
    this.context = new AudioContextClass({ sampleRate: 44100, latencyHint: "interactive" });
    if (this.context.sampleRate !== 44100) {
      throw new Error("Felucca requires a 44.1 kHz audio context; this browser opened " + this.context.sampleRate + " Hz.");
    }
    await this.context.audioWorklet.addModule(new URL("./felucca-worklet.js", import.meta.url));
    this.node = new AudioWorkletNode(this.context, "fm1lab-felucca", {
      numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2]
    });
    this.node.port.onmessage = event => this.onMessage(event.data);
    this.node.connect(this.context.destination);
    const response = await fetch(new URL("../felucca/felucca.wasm", import.meta.url));
    if (!response.ok) throw new Error("Could not load Felucca WASM (" + response.status + ").");
    const wasm = await response.arrayBuffer();
    const loaded = new Promise((resolve, reject) => {
      this.pending.set("boot", { resolve, reject, expected: "ready" });
    });
    this.node.port.postMessage({ type: "load", wasm, master: 700 }, [wasm]);
    this.api = await loaded;
    if (this.api.fm6Api !== 1 || this.api.tracks !== 4 || this.api.patchSize !== 155 || this.api.factoryCount < 8) {
      throw new Error("Unexpected Felucca FM6 bridge API.");
    }
    await this.context.resume();
    this.onState("FELUCCA FM6 ENGINE ACTIVE · 4 PARTS");
    return this;
  }

  onMessage(message) {
    if (message.type === "load" && Number.isFinite(message.load)) {
      this.onState("FELUCCA FM6 ACTIVE · DSP " + (message.load * 100).toFixed(1) + "%");
      return;
    }
    if (message.type === "error") {
      const boot = this.pending.get("boot");
      if (boot) { this.pending.delete("boot"); boot.reject(new Error(message.message)); }
      this.onState("FELUCCA ENGINE ERROR");
      return;
    }
    if (message.type === "ready") {
      const boot = this.pending.get("boot");
      if (boot) { this.pending.delete("boot"); boot.resolve(message); }
      return;
    }
    if (message.id !== undefined) {
      const pending = this.pending.get(message.id);
      if (!pending) return;
      this.pending.delete(message.id);
      if (message.type.endsWith("error")) pending.reject(new Error(message.message));
      else pending.resolve(message);
    }
  }

  request(type, payload = {}) {
    if (!this.node) return Promise.reject(new Error("Felucca is not initialized."));
    const id = ++this.sequence;
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      this.node.port.postMessage({ type, id, ...payload });
    });
  }

  async loadFactoryPreset(track, preset) {
    await this.request("factory-load", { track, preset });
  }

  async readPatch(track) {
    const result = await this.request("patch-read", { track });
    return new Uint8Array(result.patch);
  }

  async writePatch(track, patch) {
    const bytes = patch instanceof Uint8Array ? patch : new Uint8Array(patch);
    if (bytes.length !== 155) throw new Error("Native FM6 patches are 155 bytes.");
    await this.request("patch-write", { track, patch: bytes });
  }

  async getTopology(algorithm) {
    return this.request("topology", { algorithm });
  }

  noteOn({ note = 60, velocity = 100, track = 0, duration = null } = {}) {
    if (!this.node || this.context?.state !== "running") throw new Error("Felucca audio is not running.");
    const part = Math.max(0, Math.min(3, track | 0));
    const key = Math.max(0, Math.min(127, note | 0));
    const vel = Math.max(1, Math.min(127, velocity | 0));
    this.node.port.postMessage({ type: "midi", data: [0x90 | part, key, vel] });
    const voice = { track: part, note: key, timer: null, released: false };
    this.voices.add(voice);
    if (Number.isFinite(duration) && duration > 0) {
      voice.timer = window.setTimeout(() => this.noteOff(voice), duration * 1000);
    }
    return voice;
  }

  noteOff(voice) {
    if (!voice || voice.released) return;
    voice.released = true;
    if (voice.timer) window.clearTimeout(voice.timer);
    this.node?.port.postMessage({ type: "midi", data: [0x80 | voice.track, voice.note, 0] });
    this.voices.delete(voice);
  }

  allNotesOff() {
    for (const voice of [...this.voices]) this.noteOff(voice);
    for (let track = 0; track < 4; track++) {
      this.node?.port.postMessage({ type: "midi", data: [0xB0 | track, 123, 0] });
    }
  }

  async close() {
    this.allNotesOff();
    this.node?.disconnect();
    this.node = null;
    if (this.context) await this.context.close();
    this.context = null;
    this.readyPromise = null;
    this.onState("FELUCCA ENGINE OFFLINE");
  }
}
