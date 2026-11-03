import { FMEngine } from "./fm-engine.js";
import { createDefaultPatch, createFactoryInstruments, migrateLegacyPatch, normalizeInstrument, normalizePatch } from "./instrument.js";

const $=s=>document.querySelector(s);
const clamp=(v,a,b)=>Math.max(a,Math.min(b,Number(v)));
const factoryInstruments=createFactoryInstruments();
const STORAGE_KEY="fm1lab.instrument-library.v1";
let library=Object.fromEntries(factoryInstruments.map(item=>[item.id,item]));
let selectedInstrumentId=factoryInstruments[0].id;
let instrument=library[selectedInstrumentId];
let selectedPatchIndex=0;
let patch=instrument.patches[selectedPatchIndex];
const engine=new FMEngine({onState:state=>$("#engine-status").textContent=state});
const graphs={
 cascade:{caption:"SERIAL / each stage modulates the next",edges:[[6,5],[5,4],[4,3],[3,2],[2,1]],pos:[[70,45],[165,45],[260,45],[355,45],[450,45],[545,45]],carriers:[1]},
 pairs:{caption:"THREE PAIRS / three independent carrier voices",edges:[[2,1],[4,3],[6,5]],pos:[[110,30],[110,125],[305,30],[305,125],[500,30],[500,125]],carriers:[1,3,5]},
 split:{caption:"SPLIT TREE / two branches converge through operator 2",edges:[[6,4],[5,4],[4,2],[3,2],[2,1]],pos:[[305,150],[305,85],[155,35],[455,35],[235,12],[375,12]],carriers:[1]}
};
function setActivePatch(index){
 selectedPatchIndex=clamp(index,0,3);
 patch=instrument.patches[selectedPatchIndex];
}
function persist(){
 try{
  library[instrument.id]=normalizeInstrument(instrument);
  localStorage.setItem(STORAGE_KEY,JSON.stringify({format:"fm1-lab-library",version:1,selectedInstrumentId,instruments:library}));
 }catch(error){console.warn("Could not persist Instrument",error)}
}
function selectInstrument(index){
 const next=factoryInstruments[clamp(index,0,factoryInstruments.length-1)];
 selectedInstrumentId=next.id;
 if(!library[selectedInstrumentId])library[selectedInstrumentId]=structuredClone(next);
 instrument=library[selectedInstrumentId];
 setActivePatch(0);
 renderHome();
 renderAll();
}
function renderHome(){
 $("#instrument-name").value=instrument.name;
 const presetIndex=factoryInstruments.findIndex(item=>item.id===selectedInstrumentId);\n $("#preset-number").textContent=presetIndex<0?"USR":String(presetIndex+1).padStart(2,"0");
 $("#preset-name").textContent=instrument.name.toUpperCase();
 $("#preset-knob").value=Math.max(0,presetIndex);
 $("#patch-slots").innerHTML=instrument.patches.map((item,index)=>`
  <article class="patch-slot ${selectedPatchIndex===index?"active":""}">
   <div class="slot-top"><span>FM PATCH 0${index+1}</span><span class="slot-state">${item.topology.toUpperCase()}</span></div>
   <div class="slot-main"><button class="patch-knob" data-patch="${index}" aria-label="Select FM Patch ${index+1}"><span>KNOB</span><b>${index+1}</b></button><div class="slot-copy"><h3>${escapeHtml(item.name)}</h3><p>6 OPERATORS · ${item.operators.length} ACTIVE PARAMETERS</p><button class="edit-patch" data-patch="${index}">OPEN PATCH ↗</button></div></div>
  </article>`).join("");
 $("#patch-slots").querySelectorAll("[data-patch]").forEach(button=>button.onclick=()=>openPatch(+button.dataset.patch));
}
function escapeHtml(value){return String(value).replace(/[&<>"']/g,char=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[char]))}
function openPatch(index){
 setActivePatch(index);
 $("#home-screen").hidden=true;
 $("#editor-screen").hidden=false;
 renderAll();
}
function showHome(){
 engine.allNotesOff();
 $("#editor-screen").hidden=true;
 $("#home-screen").hidden=false;
 renderHome();
}
function renderMap(){
 const g=graphs[patch.topology];
 const edges=g.edges.map(([a,b])=>{const p=g.pos[a-1],q=g.pos[b-1];return `<path class="edge ${patch.selected===a||patch.selected===b?"hot":""}" d="M${p[0]} ${p[1]} Q${(p[0]+q[0])/2} ${(p[1]+q[1])/2-18} ${q[0]} ${q[1]}"/>`}).join("");
 const nodes=g.pos.map(([x,y],i)=>{const n=i+1;return `<g class="node ${g.carriers.includes(n)?"carrier":""} ${patch.selected===n?"selected":""}" data-op="${n}" tabindex="0" role="button" aria-label="Edit operator ${n}"><circle cx="${x}" cy="${y}" r="21"/><text x="${x}" y="${y}">OP ${n}</text><text class="kind" x="${x}" y="${y+32}">${g.carriers.includes(n)?"CARRIER":"MODULATOR"}</text></g>`}).join("");
 $("#map").innerHTML=`<svg viewBox="0 0 610 190" role="img" aria-label="Six operator FM routing diagram">${edges}${nodes}</svg>`;
 $("#map-caption").textContent=g.caption+" · Select a node to edit.";
 $("#map").querySelectorAll("[data-op]").forEach(node=>{const select=()=>selectOp(+node.dataset.op);node.onclick=select;node.onkeydown=event=>{if(event.key==="Enter"||event.key===" "){event.preventDefault();select()}}});
}
function selectOp(number){patch.selected=number;renderTabs();renderMap();renderEditor();persist()}
const fields=[["ratio","FREQUENCY RATIO",.25,16,.25],["level","OUTPUT LEVEL",0,100,1],["attack","ATTACK",0,100,1],["decay","DECAY",0,100,1],["sustain","SUSTAIN",0,100,1],["release","RELEASE",0,100,1],["feedback","FEEDBACK",0,100,1]];
function renderTabs(){
 $("#tabs").innerHTML=patch.operators.map((_,index)=>`<button data-op="${index+1}" role="tab" aria-selected="${patch.selected===index+1}">OPERATOR 0${index+1}</button>`).join("");
 $("#tabs").querySelectorAll("button").forEach(button=>button.onclick=()=>selectOp(+button.dataset.op));
}
function renderEditor(){
 const op=patch.operators[patch.selected-1];
 $("#editor").innerHTML=`<div class="op-title">OPERATOR 0${patch.selected}<span>LIVE SIX-OP DSP PARAMETERS</span></div>`+fields.map(([key,label,min,max,step])=>`<div class="param"><label for="f-${key}">${label}</label><output id="v-${key}">${op[key]}</output><input id="f-${key}" type="range" min="${min}" max="${max}" step="${step}" value="${op[key]}"></div>`).join("");
 fields.forEach(([key])=>$("#f-"+key).oninput=event=>{op[key]=+event.target.value;$("#v-"+key).textContent=op[key];renderSpectrum();persist()});
}
function renderMacros(){
 const labels={brightness:"BRIGHTNESS",body:"BODY",motion:"MOTION",edge:"FEEDBACK EDGE"};
 $("#macro-controls").innerHTML=Object.entries(labels).map(([key,label])=>`<label class="macro-item"><div><b>${label}</b><output id="m-${key}">${patch.macros[key]}</output></div><input id="macro-${key}" type="range" min="0" max="100" value="${patch.macros[key]}"></label>`).join("");
 Object.keys(labels).forEach(key=>$("#macro-"+key).oninput=event=>{patch.macros[key]=+event.target.value;$("#m-"+key).textContent=event.target.value;renderSpectrum();persist()});
}
function renderSpectrum(){
 const bars=Array.from({length:40},(_,index)=>{const harmonic=index+1;let height=0;patch.operators.forEach((operator,opIndex)=>{height+=(operator.level/100)*Math.exp(-Math.abs(harmonic-operator.ratio)*(1.05+patch.macros.brightness/26))/(1+opIndex*.12)});height*=.2+patch.macros.body/100*.8;return `<i class="bar" style="height:${clamp(height*17/(1+Math.log2(harmonic)*.5),2,98)}%"></i>`}).join("");
 $("#spectrum").innerHTML=bars;
}
function renderAll(){
 $("#patch-name").value=patch.name;
 $("#topology").value=patch.topology;
 $("#editor-instrument-name").textContent=instrument.name.toUpperCase();\n $("#patch-id").textContent="FM PATCH 0"+(selectedPatchIndex+1);
 $("#editor-patch-label").textContent="FM PATCH 0"+(selectedPatchIndex+1);
 renderMap();renderTabs();renderEditor();renderMacros();renderSpectrum();
}
function load(){
 try{
  const saved=JSON.parse(localStorage.getItem(STORAGE_KEY));
  if(saved?.format==="fm1-lab-library"&&saved.instruments){
   for(const [id,candidate] of Object.entries(saved.instruments)){
    try{library[id]=normalizeInstrument(candidate,id)}catch(error){console.warn("Skipped invalid saved Instrument",id,error)}
   }
   if(library[saved.selectedInstrumentId])selectedInstrumentId=saved.selectedInstrumentId;
  }else{
   const legacy=JSON.parse(localStorage.getItem("fm1lab.patch.v3")||localStorage.getItem("fm1lab.patch.v2"));
   if(legacy?.operators){const migrated=migrateLegacyPatch(legacy);migrated.id=factoryInstruments[0].id;library[migrated.id]=migrated;selectedInstrumentId=migrated.id;}
  }
 }catch(error){console.warn("Ignoring invalid saved Instrument library",error)}
 instrument=library[selectedInstrumentId]||factoryInstruments[0];
 setActivePatch(0);
}
async function playNote(note,velocity=100,duration=1.15){
 try{await engine.ready();engine.noteOn({note,velocity,duration,patch})}
 catch(error){console.error(error);alert(error.message)}
}
function renderKeys(){
 const notes=[60,62,64,65,67,69,71,72,74,76,77,79],black=[1,3,6,8,10];
 $("#keyboard").innerHTML=notes.map((note,index)=>`<button class="key ${black.includes(index)?"black":""}" data-note="${note}" aria-label="Play MIDI note ${note}">${["C","D","E","F","G","A","B","C","D","E","F","G"][index]}</button>`).join("");
 $("#keyboard").querySelectorAll("button").forEach(button=>button.addEventListener("pointerdown",event=>{event.preventDefault();playNote(+button.dataset.note,+$("#velocity").value,.75)}));
}
$("#enter").onclick=()=>{$("#boot").remove();$("#app").hidden=false};
$("#preset-knob").oninput=event=>selectInstrument(+event.target.value);
$("#instrument-name").onchange=event=>{instrument.name=event.target.value.trim().slice(0,32)||"Untitled Instrument";persist();renderHome();renderAll()};
$("#home-button").onclick=showHome;
$("#patch-name").oninput=event=>{patch.name=event.target.value;persist();renderHome()};
$("#topology").onchange=event=>{patch.topology=event.target.value;renderMap();renderSpectrum();persist();renderHome()};
$("#mutate").onclick=()=>{
 patch.operators=patch.operators.map((operator,index)=>({...operator,ratio:[.5,1,1.5,2,3,4,5,6][Math.floor(Math.random()*8)],level:20+Math.round(Math.random()*80),attack:Math.round(Math.random()*55),decay:20+Math.round(Math.random()*75),sustain:15+Math.round(Math.random()*85),release:10+Math.round(Math.random()*70),feedback:index===0?Math.round(Math.random()*35):0}));
 patch.name="Mutation "+(100+Math.floor(Math.random()*900));renderAll();persist();renderHome();
};
$("#reset").onclick=()=>{if(confirm("Reset this FM patch?")){engine.allNotesOff();patch=createDefaultPatch("FM Patch 0"+(selectedPatchIndex+1),selectedPatchIndex);instrument.patches[selectedPatchIndex]=patch;renderAll();persist();renderHome()}};
$("#export").onclick=()=>{
 const blob=new Blob([JSON.stringify(normalizeInstrument(instrument),null,2)],{type:"application/json"}),url=URL.createObjectURL(blob),anchor=document.createElement("a");
 anchor.href=url;anchor.download=(instrument.name.replace(/[^a-z0-9-_]/gi,"-")||"instrument")+".fm1lab.json";anchor.click();URL.revokeObjectURL(url);
};
$("#play").onclick=()=>playNote(+$(" #note").value,+$("#velocity").value,1.5);
load();renderHome();renderAll();renderKeys();
const toggleEmulator=$("#toggle-emulator"),frameWrap=$("#emulator-frame-wrap");
if(toggleEmulator&&frameWrap)toggleEmulator.onclick=()=>{frameWrap.hidden=!frameWrap.hidden;toggleEmulator.textContent=frameWrap.hidden?"LOAD OFFICIAL FM-1 EMULATOR ↓":"HIDE OFFICIAL FM-1 EMULATOR ↑"};
