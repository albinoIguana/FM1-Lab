import { FMEngine } from "./fm-engine.js";

const $=s=>document.querySelector(s);
const clamp=(v,a,b)=>Math.max(a,Math.min(b,Number(v)));
const defaults=()=>({name:"Glass Current",topology:"cascade",selected:1,macros:{brightness:62,body:48,motion:24,edge:36},operators:Array.from({length:6},(_,i)=>({ratio:[1,2,3,1.5,.5,1][i],level:[86,58,42,65,34,72][i],attack:[8,14,22,4,36,10][i],decay:[54,42,62,30,72,48][i],sustain:[55,48,62,35,70,50][i],release:[30,24,42,18,55,32][i],feedback:i===0?18:0}))});
let patch=defaults();
const engine=new FMEngine({onState:state=>$("#engine-status").textContent=state});
const graphs={
 cascade:{caption:"SERIAL / each stage modulates the next",edges:[[6,5],[5,4],[4,3],[3,2],[2,1]],pos:[[70,45],[165,45],[260,45],[355,45],[450,45],[545,45]],carriers:[1]},
 pairs:{caption:"THREE PAIRS / three independent carrier voices",edges:[[2,1],[4,3],[6,5]],pos:[[110,30],[110,125],[305,30],[305,125],[500,30],[500,125]],carriers:[1,3,5]},
 split:{caption:"SPLIT TREE / two branches converge through operator 2",edges:[[6,4],[5,4],[4,2],[3,2],[2,1]],pos:[[305,150],[305,85],[155,35],[455,35],[235,12],[375,12]],carriers:[1]}
};
function renderMap(){
 const g=graphs[patch.topology];
 const edges=g.edges.map(([a,b])=>{const p=g.pos[a-1],q=g.pos[b-1];return `<path class="edge ${patch.selected===a||patch.selected===b?'hot':''}" d="M${p[0]} ${p[1]} Q${(p[0]+q[0])/2} ${(p[1]+q[1])/2-18} ${q[0]} ${q[1]}"/>`}).join("");
 const nodes=g.pos.map(([x,y],i)=>{const n=i+1;return `<g class="node ${g.carriers.includes(n)?'carrier':''} ${patch.selected===n?'selected':''}" data-op="${n}" tabindex="0" role="button" aria-label="Edit operator ${n}"><circle cx="${x}" cy="${y}" r="21"/><text x="${x}" y="${y}">OP ${n}</text><text class="kind" x="${x}" y="${y+32}">${g.carriers.includes(n)?'CARRIER':'MODULATOR'}</text></g>`}).join("");
 $("#map").innerHTML=`<svg viewBox="0 0 610 190" role="img" aria-label="Six operator FM routing diagram">${edges}${nodes}</svg>`;
 $("#map-caption").textContent=g.caption+" · Select a node to edit.";
 $("#map").querySelectorAll("[data-op]").forEach(n=>{const f=()=>selectOp(+n.dataset.op);n.onclick=f;n.onkeydown=e=>{if(e.key==="Enter"||e.key===" "){e.preventDefault();f()}}});
}
function selectOp(n){patch.selected=n;renderTabs();renderMap();renderEditor()}
const fields=[["ratio","FREQUENCY RATIO",.25,16,.25],["level","OUTPUT LEVEL",0,100,1],["attack","ATTACK",0,100,1],["decay","DECAY",0,100,1],["sustain","SUSTAIN",0,100,1],["release","RELEASE",0,100,1],["feedback","FEEDBACK",0,100,1]];
function renderTabs(){
 $("#tabs").innerHTML=patch.operators.map((_,i)=>`<button data-op="${i+1}" role="tab" aria-selected="${patch.selected===i+1}">OPERATOR 0${i+1}</button>`).join("");
 $("#tabs").querySelectorAll("button").forEach(b=>b.onclick=()=>selectOp(+b.dataset.op));
}
function renderEditor(){
 const op=patch.operators[patch.selected-1];
 $("#editor").innerHTML=`<div class="op-title">OPERATOR 0${patch.selected}<span>LIVE SIX-OP DSP PARAMETERS</span></div>`+fields.map(([k,label,min,max,step])=>`<div class="param"><label for="f-${k}">${label}</label><output id="v-${k}">${op[k]}</output><input id="f-${k}" type="range" min="${min}" max="${max}" step="${step}" value="${op[k]}"></div>`).join("");
 fields.forEach(([k])=>$("#f-"+k).oninput=e=>{op[k]=+e.target.value;$("#v-"+k).textContent=op[k];renderSpectrum();persist()});
}
function renderMacros(){
 const labels={brightness:"BRIGHTNESS",body:"BODY",motion:"MOTION",edge:"FEEDBACK EDGE"};
 $("#macro-controls").innerHTML=Object.entries(labels).map(([k,label])=>`<label class="macro-item"><div><b>${label}</b><output id="m-${k}">${patch.macros[k]}</output></div><input id="macro-${k}" type="range" min="0" max="100" value="${patch.macros[k]}"></label>`).join("");
 Object.keys(labels).forEach(k=>$("#macro-"+k).oninput=e=>{patch.macros[k]=+e.target.value;$("#m-"+k).textContent=e.target.value;renderSpectrum();persist()});
}
function renderSpectrum(){
 const bars=Array.from({length:40},(_,i)=>{const harmonic=i+1;let h=0;patch.operators.forEach((o,index)=>{h+=(o.level/100)*Math.exp(-Math.abs(harmonic-o.ratio)*(1.05+patch.macros.brightness/26))/(1+index*.12)});h*=.2+patch.macros.body/100*.8;return `<i class="bar" style="height:${clamp(h*17/(1+Math.log2(harmonic)*.5),2,98)}%"></i>`}).join("");
 $("#spectrum").innerHTML=bars;
}
function renderAll(){$("#patch-name").value=patch.name;$("#topology").value=patch.topology;renderMap();renderTabs();renderEditor();renderMacros();renderSpectrum()}
function persist(){try{localStorage.setItem("fm1lab.patch.v3",JSON.stringify(patch))}catch(e){console.warn("Could not persist patch",e)}}
function load(){
 try{
  const p=JSON.parse(localStorage.getItem("fm1lab.patch.v3")||localStorage.getItem("fm1lab.patch.v2"));
  if(p&&Array.isArray(p.operators)&&p.operators.length===6){const d=defaults();patch={...d,...p,macros:{...d.macros,...p.macros},operators:p.operators.map((o,i)=>({...d.operators[i],...o}))};}
 }catch(e){console.warn("Ignoring invalid saved patch",e)}
}
async function playNote(note,velocity=100,duration=1.15){
 try{await engine.ready();engine.noteOn({note,velocity,duration,patch})}
 catch(e){console.error(e);alert(e.message)}
}
function renderKeys(){
 const notes=[60,62,64,65,67,69,71,72,74,76,77,79],black=[1,3,6,8,10];
 $("#keyboard").innerHTML=notes.map((n,i)=>`<button class="key ${black.includes(i)?'black':''}" data-note="${n}" aria-label="Play MIDI note ${n}">${["C","D","E","F","G","A","B","C","D","E","F","G"][i]}</button>`).join("");
 $("#keyboard").querySelectorAll("button").forEach(b=>b.addEventListener("pointerdown",e=>{e.preventDefault();playNote(+b.dataset.note,+$("#velocity").value,.75)}));
}
$("#enter").onclick=()=>{$("#boot").remove();$("#app").hidden=false};
$("#patch-name").oninput=e=>{patch.name=e.target.value;persist()};
$("#topology").onchange=e=>{patch.topology=e.target.value;renderMap();renderSpectrum();persist()};
$("#mutate").onclick=()=>{patch.operators=patch.operators.map((o,i)=>({...o,ratio:[.5,1,1.5,2,3,4,5,6][Math.floor(Math.random()*8)],level:20+Math.round(Math.random()*80),attack:Math.round(Math.random()*55),decay:20+Math.round(Math.random()*75),sustain:15+Math.round(Math.random()*85),release:10+Math.round(Math.random()*70),feedback:Math.round(Math.random()*35)}));patch.name="Mutation "+(100+Math.floor(Math.random()*900));renderAll();persist()};
$("#reset").onclick=()=>{if(confirm("Reset this patch?")){engine.allNotesOff();patch=defaults();renderAll();persist()}};
$("#export").onclick=()=>{const blob=new Blob([JSON.stringify({format:"fm1-lab-patch",version:3,engine:"fm1-lab-six-op-web-audio",patch},null,2)],{type:"application/json"}),url=URL.createObjectURL(blob),a=document.createElement("a");a.href=url;a.download=(patch.name.replace(/[^a-z0-9-_]/gi,"-")||"patch")+".json";a.click();URL.revokeObjectURL(url)};
$("#play").onclick=()=>playNote(+$ ("#note").value,+$("#velocity").value,1.5);
load();renderAll();renderKeys();
const toggleEmulator=$("#toggle-emulator"),frameWrap=$("#emulator-frame-wrap");
if(toggleEmulator&&frameWrap)toggleEmulator.onclick=()=>{frameWrap.hidden=!frameWrap.hidden;toggleEmulator.textContent=frameWrap.hidden?"LOAD OFFICIAL FM-1 EMULATOR ↓":"HIDE OFFICIAL FM-1 EMULATOR ↑"};
