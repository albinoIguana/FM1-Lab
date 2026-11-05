import { renderPatch, patchAlgorithm } from "./instrument-model.js";

/**
 * Application service between the Instrument model and the M-VAVE engine.
 * It owns topology-aware voice rendering and serializes track writes because
 * the underlying engine selects a mutable track before importing a voice.
 * UI modules must not calculate DX7 topology or write directly to the engine.
 */
export class InstrumentSession {
  constructor(engine, { onError = () => {} } = {}) {
    if (!engine) throw new TypeError("InstrumentSession requires an FM engine.");
    this.engine = engine;
    this.onError = onError;
    this.writeQueue = Promise.resolve();
  }

  renderVoice(patch) {
    const algorithm = patchAlgorithm(patch);
    const depths = Array.from({ length: 6 }, (_, index) =>
      this.engine.operatorDepth(algorithm, index + 1)
    );
    return renderPatch(patch, this.engine.carrierMask(algorithm), depths);
  }

  writePatch(track, patch) {
    if (!Number.isInteger(track) || track < 0 || track > 3) {
      return Promise.reject(new RangeError("Instrument patch track must be 0–3."));
    }
    // Snapshot at request time: later edits must not mutate a queued write.
    const voice = this.renderVoice(patch);
    const operation = this.writeQueue.catch(() => {}).then(() =>
      this.engine.writePatch(track, voice)
    );
    this.writeQueue = operation.catch(error => {
      this.onError(error);
    });
    return operation;
  }

  async loadInstrument(instrument) {
    if (!instrument || !Array.isArray(instrument.patches) || instrument.patches.length !== 4) {
      throw new TypeError("An Instrument must contain exactly four patches.");
    }
    // The M-VAVE engine has mutable track selection; writes must remain ordered.
    for (let track = 0; track < 4; track++) {
      await this.writePatch(track, instrument.patches[track]);
    }
  }

  async factoryVoices(count = 8) {
    if (!Number.isInteger(count) || count < 1 || count > 32) {
      throw new RangeError("Factory voice count must be between 1 and 32.");
    }
    return Promise.all(Array.from({ length: count }, (_, program) =>
      this.engine.factoryPatch(program)
    ));
  }

  selectPatch(index) {
    if (!Number.isInteger(index) || index < 0 || index > 3) {
      return Promise.reject(new RangeError("Instrument patch index must be 0–3."));
    }
    return this.engine.selectTrack(index);
  }
}
