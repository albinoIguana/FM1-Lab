import test from "node:test";
import assert from "node:assert/strict";
import {
  createTwoTrackSequence, normalizeSequence, addNote, updateNote, deleteNote,
  orderedNotes, selectNextNote, notesStartingAt, notesEndingAt, setTrackLength
} from "../src/sequencer-events.js";

const legacyTrack = (steps = []) => ({
  name: "LEGACY", patchIndex: 2, length: 16,
  steps: Array.from({length:64},(_,index)=>steps[index] || {active:false,note:60,velocity:100,gate:0,tie:false,alt:false})
});

test("new sequence stores two independent event tracks, not step cells", () => {
  const sequence = createTwoTrackSequence();
  assert.equal(sequence.version, 3);
  assert.equal(sequence.tracks.length, 2);
  assert.deepEqual(sequence.tracks.map(track=>track.events), [[],[]]);
});

test("polyphonic notes overlap and remain independent events", () => {
  let sequence = createTwoTrackSequence();
  sequence = addNote(sequence, 0, {id:"c",start:0,pitch:60,duration:8,velocity:90});
  sequence = addNote(sequence, 0, {id:"e",start:0,pitch:64,duration:8,velocity:88});
  sequence = addNote(sequence, 0, {id:"g",start:0,pitch:67,duration:8,velocity:86});
  assert.equal(sequence.tracks[0].events.length, 3);
  assert.deepEqual(notesStartingAt(sequence,0,0).map(note=>note.pitch).sort((a,b)=>a-b),[60,64,67]);
  assert.equal(notesEndingAt(sequence,0,8).length,3);
  assert.equal(sequence.tracks[1].events.length,0);
});

test("always-on quantization snaps starts and durations to the active grid", () => {
  let sequence = createTwoTrackSequence();
  sequence.quantize = 4;
  sequence = addNote(sequence,0,{start:5,duration:7,pitch:62});
  assert.equal(sequence.tracks[0].events[0].start,4);
  assert.equal(sequence.tracks[0].events[0].duration,8);
  sequence = updateNote(sequence,0,sequence.tracks[0].events[0].id,{start:11,duration:5});
  assert.equal(sequence.tracks[0].events[0].start,12);
  assert.equal(sequence.tracks[0].events[0].duration,4);
});

test("SELECT order follows piano-roll reading order: high to low, then left to right", () => {
  let sequence = createTwoTrackSequence();
  sequence = addNote(sequence,0,{id:"low-late",start:8,pitch:48});
  sequence = addNote(sequence,0,{id:"high-late",start:8,pitch:72});
  sequence = addNote(sequence,0,{id:"high-early",start:2,pitch:72});
  assert.deepEqual(orderedNotes(sequence,0).map(note=>note.id),["high-early","high-late","low-late"]);
  assert.equal(selectNextNote(sequence,0,"high-early"),"high-late");
  assert.equal(selectNextNote(sequence,0,"low-late"),"high-early");
});

test("K1-K4 property editing affects only the selected event", () => {
  let sequence = createTwoTrackSequence();
  sequence = addNote(sequence,0,{id:"a",start:0,pitch:60,duration:2,velocity:70});
  sequence = addNote(sequence,0,{id:"b",start:0,pitch:64,duration:4,velocity:80});
  sequence = updateNote(sequence,0,"a",{duration:6,pitch:62,accent:true,velocity:110});
  assert.deepEqual(sequence.tracks[0].events.find(note=>note.id==="a"),{
    id:"a",start:0,duration:6,pitch:62,velocity:110,accent:true
  });
  assert.deepEqual(sequence.tracks[0].events.find(note=>note.id==="b"),{
    id:"b",start:0,duration:4,pitch:64,velocity:80,accent:false
  });
});

test("EDIT deletion removes exactly one selected note with no undo state", () => {
  let sequence = createTwoTrackSequence();
  sequence = addNote(sequence,0,{id:"keep",start:0,pitch:60,duration:4});
  sequence = addNote(sequence,0,{id:"delete",start:0,pitch:64,duration:4});
  sequence = deleteNote(sequence,0,"delete");
  assert.deepEqual(sequence.tracks[0].events.map(note=>note.id),["keep"]);
});

test("V2 step patterns migrate into continuous note events, joining valid tie chains", () => {
  const steps = [];
  steps[0]={active:true,note:60,velocity:90,tie:false};
  steps[1]={active:true,note:60,velocity:90,tie:true};
  steps[2]={active:true,note:60,velocity:90,tie:true};
  steps[4]={active:true,note:64,velocity:80,tie:false};
  const legacy = {version:2,tempo:120,swing:0,selectedTrack:0,tracks:[legacyTrack(steps),legacyTrack()]};
  const migrated = normalizeSequence(legacy);
  assert.equal(migrated.version,3);
  assert.deepEqual(migrated.tracks[0].events,[
    {id:"t1n1",start:0,duration:3,pitch:60,velocity:90,accent:false},
    {id:"t1n2",start:4,duration:1,pitch:64,velocity:80,accent:false}
  ]);
});

test("loop length truncation drops out-of-range starts and bounds sustained durations", () => {
  let sequence = createTwoTrackSequence();
  sequence = addNote(sequence,0,{id:"inside",start:12,duration:8});
  sequence = addNote(sequence,0,{id:"outside",start:20,duration:2});
  sequence = setTrackLength(sequence,0,16);
  assert.deepEqual(sequence.tracks[0].events.map(note=>note.id),["inside"]);
  assert.equal(sequence.tracks[0].events[0].duration,8);
});

test("normalization rejects more than 128 events per track", () => {
  const sequence = createTwoTrackSequence();
  sequence.tracks[0].events = Array.from({length:129},(_,index)=>({id:String(index),start:0,duration:1}));
  assert.throws(()=>normalizeSequence(sequence),RangeError);
});
