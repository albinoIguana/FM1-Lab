import { MvaveEngine } from "./m-vave-engine.js";
import { createPatch, createInstrument, normalizeInstrument, patchAlgorithm } from "./instrument-model.js";
import { InstrumentSession } from "./instrument-session.js";
import { renderPatchGrid } from "./ui/home-view.js";
import { renderSequencer } from "./ui/sequencer-view.js";
import { createTwoTrackSequence, normalizeSequence, setTrackLength, setTrackDivision, setTrackSwing, setStep, selectSequenceTrack, setGlobalKeyShift } from "./sequencer-model.js";
import { patchName, setPatchName, readOperator, writeOperator, readGlobal, writeGlobal } from "./dx7-voice.js";

const $ = s => document.querySelector(s);
const KEY = "fm1lab.native-instruments.v1";
const SEQ_KEY = "fm1lab.two-track-sequencer.v1";
const BANKS = [{name:"GLASS CURRENT",programs:[0,1,2,3]},{name:"NEON ORCHARD",programs:[4,5,6,7]}];
const MACROS = [
  ["brightness","BRIGHTNESS","Modulator output / harmonic energy"],
  ["body","BODY","Carrier sustain / presence"],
  ["motion","MOTION","Modulator envelope movement"],
  ["dirt","DIRT","Bounded feedback / spectral aggression"]
];
let engine, session, library=[], instrument, instrumentIndex=0, selectedPatchIndex=0, selectedOperator=1;
let sequence=createTwoTrackSequence(), selectedStep=0, sequenceAdvanced=false;

const patch=()=>instrument.patches[selectedPatchIndex];
const persist=()=>{try{localStorage.setItem(KEY,JSON.stringify({version:1,instrumentIndex,library}));}catch(e){console.warn(e);}};
function writeCurrent(){
  persist();
  return session.writePatch(selectedPatchIndex,patch()).catch(e=>{
    $("#engine-status").textContent="PATCH WRITE ERROR";
    console.error(e);
  });
}
async function factoryLibrary(){
  const voices=await session.factoryVoices(8);
  return BANKS.map(bank=>createInstrument(bank.name,bank.programs.map(i=>createPatch(patchName(voices[i]),voices[i]))));
}
async function loadLibrary(){
  try{
    const saved=JSON.parse(localStorage.getItem(KEY)||"null");
    if(saved?.version===1&&Array.isArray(saved.library)&&saved.library.length){
      library=saved.library.map(normalizeInstrument);
      instrumentIndex=Math.max(0,Math.min(library.length-1,Number(saved.instrumentIndex)||0));
    }
  }catch(e){console.warn("Saved Instruments could not be loaded",e);}
  if(!library.length)library=await factoryLibrary();
  instrument=library[instrumentIndex];persist();
}
async function loadInstrument(){
  await session.loadInstrument(instrument);
}
function renderHome(){
  $("#instrument-index").textContent=String(instrumentIndex+1).padStart(2,"0")+" / "+String(library.length).padStart(2,"0");
  $("#instrument-name").value=instrument.name;
  $("#patch-grid").innerHTML=renderPatchGrid(instrument.patches,selectedPatchIndex);
  $("#patch-grid").querySelectorAll("[data-patch]").forEach(b=>b.onclick=()=>openPatch(+b.dataset.patch));
}
function persistSequence(){
  try{localStorage.setItem(SEQ_KEY,JSON.stringify(sequence));}catch(error){console.warn("Sequencer state could not be saved",error);}
}
function loadSequence(){
  try{
    const saved=JSON.parse(localStorage.getItem(SEQ_KEY)||"null");
    if(saved)sequence=normalizeSequence(saved);
  }catch(error){console.warn("Saved sequence could not be loaded",error);sequence=createTwoTrackSequence();}
}
function renderSequence(){
  $("#sequencer-view").innerHTML=renderSequencer(sequence,selectedStep,sequenceAdvanced);
  $("#sequencer-view").querySelectorAll("[data-seq-select]").forEach(button=>button.onclick=()=>{
    sequence=selectSequenceTrack(sequence,+button.dataset.seqSelect);selectedStep=0;renderSequence();persistSequence();
  });
  $("#sequencer-view").querySelectorAll("[data-seq-step]").forEach(button=>button.onclick=()=>{
    const track=+button.dataset.seqTrack,step=+button.dataset.seqStep;
    sequence=setStep(sequence,track,step,{active:!sequence.tracks[track].steps[step].active});
    selectedStep=step;sequence=selectSequenceTrack(sequence,track);renderSequence();persistSequence();
  });
  const length=$("#sequencer-view [data-seq-length]");
  length.oninput=()=>{length.previousElementSibling.textContent=length.value;};
  length.onchange=()=>{sequence=setTrackLength(sequence,sequence.selectedTrack,+length.value);renderSequence();persistSequence();};
  const division=$("#sequencer-view [data-seq-division]");
  division.onchange=()=>{sequence=setTrackDivision(sequence,sequence.selectedTrack,+division.value);renderSequence();persistSequence();};
  const swing=$("#sequencer-view [data-seq-swing]");
  swing.oninput=()=>{swing.previousElementSibling.textContent=swing.value;};
  swing.onchange=()=>{sequence=setTrackSwing(sequence,sequence.selectedTrack,+swing.value);renderSequence();persistSequence();};
  const keyShift=$("#sequencer-view [data-seq-key-shift]");
  keyShift.oninput=()=>{keyShift.previousElementSibling.querySelector("output").textContent=keyShift.value;};
  keyShift.onchange=()=>{sequence=setGlobalKeyShift(sequence,+keyShift.value);renderSequence();persistSequence();};
  $("#sequencer-view [data-seq-advanced]").onclick=()=>{sequenceAdvanced=!sequenceAdvanced;renderSequence();};
  $("#sequencer-view").querySelectorAll("[data-seq-note]").forEach(button=>button.onclick=()=>{
    sequence=setStep(sequence,sequence.selectedTrack,selectedStep,{note:+button.dataset.seqNote});renderSequence();persistSequence();
  });
  const velocity=$("#sequencer-view [data-seq-velocity]");
  if(velocity){velocity.oninput=()=>{velocity.previousElementSibling.querySelector("output").textContent=velocity.value;};velocity.onchange=()=>{sequence=setStep(sequence,sequence.selectedTrack,selectedStep,{velocity:+velocity.value});renderSequence();persistSequence();};}
  const gate=$("#sequencer-view [data-seq-gate]");
  if(gate){gate.oninput=()=>{gate.previousElementSibling.querySelector("output").textContent=gate.value;};gate.onchange=()=>{sequence=setStep(sequence,sequence.selectedTrack,selectedStep,{gate:+gate.value});renderSequence();persistSequence();};}
  const tie=$("#sequencer-view [data-seq-tie]");
  if(tie)tie.onchange=()=>{sequence=setStep(sequence,sequence.selectedTrack,selectedStep,{tie:tie.checked});persistSequence();};
  const slide=$("#sequencer-view [data-seq-slide]");
  if(slide)slide.onchange=()=>{sequence=setStep(sequence,sequence.selectedTrack,selectedStep,{slide:slide.checked});persistSequence();};
}
function showSequencer(){
  $("#home-screen").hidden=true;$("#editor-screen").hidden=true;$("#sequencer-screen").hidden=false;renderSequence();
}
function closeSequencer(){
  $("#sequencer-screen").hidden=true;$("#home-screen").hidden=false;renderHome();
}
function renderMacros(){
  const p=patch();
  $("#macros").innerHTML=MACROS.map(([key,label,desc])=>`<label class="macro-control">
    <span class="macro-label"><b>${label}</b><output id="macro-${key}-value">${p.macros[key]}</output></span>
    <input data-macro="${key}" type="range" min="0" max="100" value="${p.macros[key]}"><small>${desc}</small></label>`).join("");
  $("#macros").querySelectorAll("[data-macro]").forEach(input=>input.oninput=()=>{
    p.macros[input.dataset.macro]=+input.value;
    $("#macro-"+input.dataset.macro+"-value").textContent=input.value;
    writeCurrent();
  });
}
function range(id,label,value,min,max){
  return `<label class="parameter"><span>${label}<output id="${id}-value">${value}</output></span><input id="${id}" type="range" min="${min}" max="${max}" value="${value}"></label>`;
}
function bindRange(id,fn){
  const input=$("#"+id);
  input.oninput=()=>{fn(+input.value);$("#"+id+"-value").textContent=input.value;};
}
function editOperator(field,value){
  patch().baseData=Array.from(writeOperator(Uint8Array.from(patch().baseData),selectedOperator,field,value));
  writeCurrent();
}
function renderOperator(){
  const op=readOperator(Uint8Array.from(patch().baseData),selectedOperator);
  $("#operator-editor").innerHTML=`<div class="operator-heading"><b>OPERATOR 0${selectedOperator}</b><span>${op.mode?"FIXED":"RATIO"} OSCILLATOR</span></div>
    <label class="parameter"><span>OSCILLATOR MODE</span><select id="op-mode"><option value="0" ${op.mode?"":"selected"}>RATIO</option><option value="1" ${op.mode?"selected":""}>FIXED</option></select></label>
    ${range("op-coarse","COARSE",op.coarse,0,31)}${range("op-fine","FINE",op.fine,0,99)}
    ${range("op-output","OUTPUT LEVEL",op.output,0,99)}
    <div class="envelope-grid">${op.rates.map((v,i)=>range("op-rate-"+(i+1),"RATE "+(i+1),v,0,99)).join("")}
    ${op.levels.map((v,i)=>range("op-level-"+(i+1),"LEVEL "+(i+1),v,0,99)).join("")}</div>`;
  $("#op-mode").onchange=e=>editOperator("mode",+e.target.value);
  bindRange("op-coarse",v=>editOperator("coarse",v));bindRange("op-fine",v=>editOperator("fine",v));
  bindRange("op-output",v=>editOperator("output",v));
  for(let i=1;i<=4;i++){bindRange("op-rate-"+i,v=>editOperator("rate"+i,v));bindRange("op-level-"+i,v=>editOperator("level"+i,v));}
}
function renderEditor(){
  const p=patch(),g=readGlobal(Uint8Array.from(p.baseData));
  $("#editor-title").textContent="PATCH 0"+(selectedPatchIndex+1)+" · "+p.name;
  $("#patch-name").value=p.name;$("#algorithm").value=String(g.algorithm);
  $("#feedback").value=g.feedback;$("#feedback-value").textContent=g.feedback;
  $("#operator-tabs").innerHTML=Array.from({length:6},(_,i)=>`<button data-op="${i+1}" class="${selectedOperator===i+1?"active":""}">OP ${i+1}</button>`).join("");
  $("#operator-tabs").querySelectorAll("[data-op]").forEach(b=>b.onclick=()=>{selectedOperator=+b.dataset.op;renderEditor();});
  renderMacros();renderOperator();
}
async function openPatch(i){
  selectedPatchIndex=i;selectedOperator=1;await session.selectPatch(i);
  $("#home-screen").hidden=true;$("#sequencer-screen").hidden=true;$("#editor-screen").hidden=false;renderEditor();
}
function showHome(){$("#editor-screen").hidden=true;$("#sequencer-screen").hidden=true;$("#home-screen").hidden=false;renderHome();}
async function changeInstrument(i){
  instrumentIndex=(i+library.length)%library.length;instrument=library[instrumentIndex];
  selectedPatchIndex=0;await loadInstrument();renderHome();persist();
}
async function playNote(){
  await engine.startAudio();
  const voice=await engine.noteOn({track:selectedPatchIndex,note:60,velocity:105});
  setTimeout(()=>engine.noteOff(voice),900);
}
async function initialize(){
  engine=new MvaveEngine({onState:s=>{$("#engine-status").textContent=s;$("#boot-status").textContent=s;}});
  session=new InstrumentSession(engine,{onError:error=>console.error("Instrument session:",error)});
  await engine.ready();loadSequence();await loadLibrary();await loadInstrument();
  $("#boot").hidden=true;$("#app").hidden=false;renderHome();
}
$("#enter").onclick=async()=>{ $("#enter").disabled=true;try{await initialize();}catch(e){$("#boot-status").textContent=e.message;$("#enter").disabled=false;} };
$("#instrument-prev").onclick=()=>changeInstrument(instrumentIndex-1);
$("#instrument-next").onclick=()=>changeInstrument(instrumentIndex+1);
$("#instrument-name").onchange=e=>{instrument.name=String(e.target.value||"UNTITLED").slice(0,24);persist();renderHome();};
$("#back-home").onclick=showHome;$("#open-sequencer").onclick=showSequencer;$("#seq-back").onclick=closeSequencer;$("#brand-home").onclick=e=>{e.preventDefault();showHome();};
$("#algorithm").innerHTML=Array.from({length:32},(_,i)=>`<option value="${i+1}">ALGORITHM ${String(i+1).padStart(2,"0")}</option>`).join("");
$("#algorithm").onchange=e=>{patch().baseData=Array.from(writeGlobal(Uint8Array.from(patch().baseData),"algorithm",+e.target.value-1));renderEditor();writeCurrent();};
$("#feedback").oninput=e=>{patch().baseData=Array.from(writeGlobal(Uint8Array.from(patch().baseData),"feedback",+e.target.value));$("#feedback-value").textContent=e.target.value;writeCurrent();};
$("#patch-name").onchange=e=>{patch().name=String(e.target.value||"INIT").toUpperCase().slice(0,10);patch().baseData=Array.from(setPatchName(Uint8Array.from(patch().baseData),patch().name));renderEditor();renderHome();writeCurrent();};
$("#audio-start").onclick=async()=>{try{await engine.startAudio();$("#audio-start").textContent="AUDIO ACTIVE";$("#audio-start").disabled=true;}catch(e){$("#engine-status").textContent=e.message;}};
$("#play-note").onclick=playNote;
$("#export-instrument").onclick=()=>{
  const blob=new Blob([JSON.stringify(instrument,null,2)],{type:"application/json"}),url=URL.createObjectURL(blob),a=document.createElement("a");
  a.href=url;a.download=instrument.name.toLowerCase().replace(/[^a-z0-9]+/g,"-")+".fm1lab.json";a.click();URL.revokeObjectURL(url);
};
$("#import-instrument").onclick=()=>$("#import-file").click();
$("#import-file").onchange=async e=>{
  const file=e.target.files?.[0];if(!file)return;
  try{instrument=normalizeInstrument(JSON.parse(await file.text()));library.push(instrument);instrumentIndex=library.length-1;await loadInstrument();renderHome();persist();}
  catch(error){$("#engine-status").textContent="IMPORT ERROR: "+error.message;}
  e.target.value="";
};
