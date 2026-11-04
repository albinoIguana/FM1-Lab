export class MvaveEngine {
  constructor({ onState = () => {}, wasmUrl = "./m-vave/simulator.wasm", scriptUrl = "./m-vave/Simulator.js" } = {}) {
    this.onState = onState;
    this.wasmUrl = wasmUrl;
    this.scriptUrl = scriptUrl;
    this.core = null;
    this.wasm = null;
    this.context = null;
    this.node = null;
    this.gain = null;
    this.pending = new Map();
    this.sequence = 0;
    this.carrierMasks = [];
    this.voices = new Set();
    this.readyPromise = null;
  }

  async loadRuntimeScript() {
    if (globalThis.FM1SimulatorCore && globalThis.FM1SimulatorAudioSource) return;
    globalThis.FM1_LAB_MODE = true;
    await new Promise((resolve, reject) => {
      const script = document.createElement("script");
      script.src = this.scriptUrl;
      script.onload = resolve;
      script.onerror = () => reject(new Error("Could not load the official M-VAVE simulator runtime."));
      document.head.append(script);
    });
    if (!globalThis.FM1SimulatorCore || !globalThis.FM1SimulatorAudioSource) {
      throw new Error("The official M-VAVE simulator runtime did not initialize.");
    }
  }

  async ready() {
    if (this.readyPromise) return this.readyPromise;
    this.readyPromise = (async () => {
      this.onState("LOADING OFFICIAL M-VAVE FM1 ENGINE…");
      await this.loadRuntimeScript();
      const response = await fetch(this.wasmUrl);
      if (!response.ok) throw new Error("Could not load the official M-VAVE simulator (" + response.status + ").");
      this.wasm = await response.arrayBuffer();
      this.core = new globalThis.FM1SimulatorCore(this.wasm, null);
      for (let algorithm = 0; algorithm < 32; algorithm++) {
        this.carrierMasks[algorithm] = this.core.c.sim_fm_carriers(algorithm);
      }
      this.onState("M-VAVE FM1 ENGINE READY · DX7 / 6 OPERATORS");
      return this;
    })().catch(error => {
      this.readyPromise = null;
      this.onState("M-VAVE ENGINE ERROR");
      throw error;
    });
    return this.readyPromise;
  }

  command(message) {
    if (this.node) {
      const id = ++this.sequence;
      return new Promise((resolve, reject) => {
        this.pending.set(id, { resolve, reject });
        this.node.port.postMessage({ ...message, id });
      });
    }
    if (!this.core) return Promise.reject(new Error("The M-VAVE engine is not initialized."));
    try { return Promise.resolve(this.core.command(message)); }
    catch (error) { return Promise.reject(error); }
  }

  async selectTrack(track) {
    if (!Number.isInteger(track) || track < 0 || track > 7) throw new RangeError("FM1 track must be 1–8.");
    await this.command({ type: "select", track, source: 0 });
  }

  async factoryPatch(program) {
    await this.ready();
    const patch = await this.command({ type: "fm-factory", program });
    if (!(patch instanceof Uint8Array) || patch.length !== 156) {
      throw new Error("M-VAVE returned an invalid 156-byte DX7 factory voice.");
    }
    return patch;
  }

  async loadFactoryPatch(track, program) {
    await this.selectTrack(track);
    const patch = await this.factoryPatch(program);
    await this.command({ type: "import", patch: true, data: patch });
    return patch;
  }

  async readPatch(track) {
    await this.selectTrack(track);
    return this.command({ type: "export", patch: true });
  }

  async writePatch(track, patch) {
    const bytes = patch instanceof Uint8Array ? patch : Uint8Array.from(patch);
    if (bytes.length !== 156) throw new Error("M-VAVE DX7 patches are 156 bytes.");
    await this.selectTrack(track);
    await this.command({ type: "import", patch: true, data: bytes });
  }

  carrierMask(algorithm) {
    return this.carrierMasks[Math.max(0, Math.min(31, algorithm | 0))] ?? 0x3f;
  }

  noteOn({ note = 60, velocity = 100, track = 0 } = {}) {
    const channel = Math.max(0, Math.min(3, track | 0));
    const pitch = Math.max(0, Math.min(127, note | 0));
    const level = Math.max(1, Math.min(127, velocity | 0));
    const voice = { track: channel, note: pitch, released: false };
    this.voices.add(voice);
    return this.command({ type: "midi", data: [0x90 | channel, pitch, level] }).then(() => voice);
  }

  noteOff(voice) {
    if (!voice || voice.released) return Promise.resolve();
    voice.released = true;
    this.voices.delete(voice);
    return this.command({ type: "midi", data: [0x80 | voice.track, voice.note, 0] });
  }

  async allNotesOff() {
    const voices = [...this.voices];
    this.voices.clear();
    await Promise.all(voices.map(voice => this.noteOff(voice)));
    for (let track = 0; track < 4; track++) {
      await this.command({ type: "midi", data: [0xb0 | track, 123, 0] });
    }
  }

  async startAudio() {
    await this.ready();
    if (this.node) {
      await this.context?.resume();
      return;
    }
    const AudioContextClass = window.AudioContext || window.webkitAudioContext;
    if (!AudioContextClass || !window.AudioWorkletNode) throw new Error("This browser does not support FM1 audio worklets.");
    this.context = new AudioContextClass({ sampleRate: 44100, latencyHint: "interactive" });
    await this.context.resume();
    const moduleUrl = "data:text/javascript;charset=utf-8," + encodeURIComponent(globalThis.FM1SimulatorAudioSource);
    await this.context.audioWorklet.addModule(moduleUrl);
    const snapshot = this.core.c.memory.buffer.slice(0);
    const candidate = new AudioWorkletNode(this.context, "fm1-engine", {
      numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2],
      processorOptions: { wasm: this.wasm, snapshot }
    });
    this.gain = this.context.createGain();
    this.gain.gain.value = 0.8;
    candidate.connect(this.gain).connect(this.context.destination);
    await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error("M-VAVE audio worklet did not start.")), 5000);
      candidate.onprocessorerror = () => { clearTimeout(timeout); reject(new Error("M-VAVE audio worklet stopped unexpectedly.")); };
      candidate.port.onmessage = event => {
        const message = event.data;
        if (message.type === "ready") { clearTimeout(timeout); resolve(); return; }
        if (message.id !== undefined) {
          const pending = this.pending.get(message.id);
          if (!pending) return;
          this.pending.delete(message.id);
          if (message.error) pending.reject(new Error(message.error));
          else pending.resolve(message.data);
        }
      };
    });
    this.node = candidate;
    this.core = null;
    this.onState("M-VAVE FM1 ENGINE ACTIVE · LIVE AUDIO");
  }

  setVolume(value) {
    if (this.gain) this.gain.gain.setTargetAtTime(Math.max(0, Math.min(1, Number(value))), this.context.currentTime, 0.015);
  }

  async close() {
    await this.allNotesOff().catch(() => {});
    this.node?.disconnect();
    this.node = null;
    if (this.context) await this.context.close();
    this.context = null;
    this.gain = null;
    this.core = null;
    this.readyPromise = null;
    this.onState("M-VAVE ENGINE OFFLINE");
  }
}
