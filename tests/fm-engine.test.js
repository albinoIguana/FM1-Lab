import test from "node:test";
import assert from "node:assert/strict";
import { FMEngine } from "../src/fm-engine.js";

const connections=[];
class FakeParam {
  constructor(){this.value=0;}
  setValueAtTime(v){this.value=v;}
  linearRampToValueAtTime(v){this.value=v;}
  cancelScheduledValues(){}
}
function fakeNode(kind){
  return {
    kind,gain:new FakeParam(),frequency:new FakeParam(),delayTime:new FakeParam(),
    connect(target){connections.push({from:this,to:target});},
    disconnect(){},start(){},stop(){}
  };
}
class FakeAudioContext {
  constructor(){this.currentTime=0;this.state="running";this.destination=fakeNode("destination");}
  createGain(){return fakeNode("gain");}
  createOscillator(){return fakeNode("oscillator");}
  createDelay(){return fakeNode("delay");}
  async resume(){}
  async close(){}
}
globalThis.window={
  AudioContext:FakeAudioContext,
  setTimeout:()=>1,
  clearTimeout:()=>{}
};
const makePatch=(topology,feedback=0)=>({
  topology,macros:{brightness:50,body:50,motion:30,edge:40},
  operators:Array.from({length:6},(_,i)=>({
    ratio:i+1,level:70,attack:10,decay:40,sustain:55,release:25,
    feedback:i===0?feedback:0
  }))
});

test("creates six oscillators and six envelope generators",async()=>{
  const engine=new FMEngine();await engine.ready();
  const voice=engine.noteOn({patch:makePatch("cascade"),duration:.1});
  assert.equal(voice.oscillators.length,6);
  assert.equal(voice.envelopes.length,6);
  assert.equal(voice.envelopeParams.length,6);
  engine.dispose(voice);
});

test("cascade and split topologies create five modulation edges; pairs creates three",async()=>{
  const expected={cascade:5,pairs:3,split:5};
  for(const [topology,count] of Object.entries(expected)){
    connections.length=0;
    const engine=new FMEngine();await engine.ready();
    const voice=engine.noteOn({patch:makePatch(topology),duration:.1});
    const oscillatorFrequencyTargets=voice.oscillators.map(o=>o.frequency);
    const edgeCount=connections.filter(c=>oscillatorFrequencyTargets.includes(c.to)).length;
    assert.equal(edgeCount,count,topology+" modulation edge count");
    engine.dispose(voice);
  }
});

test("feedback creates a delayed feedback path",async()=>{
  const engine=new FMEngine();await engine.ready();
  const voice=engine.noteOn({patch:makePatch("cascade",50),duration:.1});
  assert.equal(voice.feedbackNodes.length,2);
  assert.equal(voice.feedbackNodes[0].delayTime.value,.004);
  engine.dispose(voice);
});

test("rejects malformed patches instead of creating a partial voice",async()=>{
  const engine=new FMEngine();await engine.ready();
  assert.throws(()=>engine.noteOn({patch:{topology:"cascade",operators:[]}}),/six-operator patch/);
});
