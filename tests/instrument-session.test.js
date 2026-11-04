import test from "node:test";
import assert from "node:assert/strict";
import { InstrumentSession } from "../src/instrument-session.js";
import { createPatch, createInstrument } from "../src/instrument-model.js";

function mockEngine() {
  const writes = [];
  return {
    writes,
    carrierMask: () => 1,
    operatorDepth: () => 1,
    async writePatch(track, voice) { writes.push({ track, voice: Uint8Array.from(voice) }); },
    async selectTrack(track) { this.selectedTrack = track; },
    async factoryPatch(program) { return new Uint8Array(156).fill(program); }
  };
}

test("session renders with engine topology and writes all four tracks in order", async () => {
  const engine = mockEngine();
  const session = new InstrumentSession(engine);
  const instrument = createInstrument("TEST", Array.from({ length: 4 }, (_, i) =>
    createPatch("PATCH " + i, new Uint8Array(156).fill(i))
  ));
  await session.loadInstrument(instrument);
  assert.deepEqual(engine.writes.map(write => write.track), [0, 1, 2, 3]);
  assert.equal(engine.writes[0].voice.length, 156);
  assert.equal(engine.writes[3].voice[0], 3);
});

test("queued writes snapshot the voice at request time", async () => {
  const engine = mockEngine();
  let release;
  const originalWrite = engine.writePatch.bind(engine);
  engine.writePatch = async (track, voice) => {
    if (track === 0) await new Promise(resolve => { release = resolve; });
    return originalWrite(track, voice);
  };
  const session = new InstrumentSession(engine);
  const patch = createPatch("SNAPSHOT", new Uint8Array(156).fill(12));
  const first = session.writePatch(0, patch);
  patch.baseData.fill(88);
  const second = session.writePatch(1, patch);
  release();
  await Promise.all([first, second]);
  assert.equal(engine.writes[0].voice[0], 12);
  assert.equal(engine.writes[1].voice[0], 88);
});

test("session rejects invalid patch tracks and malformed Instruments", async () => {
  const session = new InstrumentSession(mockEngine());
  await assert.rejects(session.writePatch(4, createPatch()));
  await assert.rejects(session.loadInstrument({ patches: [] }));
  await assert.rejects(session.selectPatch(4));
});

test("factory voice loading stays independent of UI and returns requested count", async () => {
  const session = new InstrumentSession(mockEngine());
  const voices = await session.factoryVoices(4);
  assert.equal(voices.length, 4);
  assert.equal(voices[3][0], 3);
});
