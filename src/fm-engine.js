// FM1-Lab local six-operator FM engine.
// Independent Web Audio implementation; not a bit-accurate stock FM-1 emulator.
const ROUTES={
 cascade:{edges:[[6,5],[5,4],[4,3],[3,2],[2,1]],carriers:[1]},
 pairs:{edges:[[2,1],[4,3],[6,5]],carriers:[1,3,5]},
 split:{edges:[[6,4],[5,4],[4,2],[3,2],[2,1]],carriers:[1]}
};
const midiHz=n=>440*Math.pow(2,(n-69)/12);
const clamp=(v,min,max)=>Math.max(min,Math.min(max,Number(v)));
const seconds=(v,scale,max)=>clamp(v/100*scale,0,max);
function envelopeLevelAt(t,p){
 if(p.attack>0&&t<p.attack)return p.peak*t/p.attack;
 if(p.decay>0&&t<p.attack+p.decay){const x=(t-p.attack)/p.decay;return p.peak+(p.peak*p.sustain-p.peak)*x;}
 return p.peak*p.sustain;
}
export class FMEngine{
 constructor({onState=()=>{},maxVoices=12}={}){this.context=null;this.voices=new Set();this.onState=onState;this.maxVoices=Math.max(1,Math.floor(maxVoices));}
 async ready(){
  if(!this.context){const AC=window.AudioContext||window.webkitAudioContext;if(!AC)throw new Error("Web Audio is not supported in this browser.");this.context=new AC();}
  if(this.context.state==="suspended")await this.context.resume();
  this.onState("LOCAL 6-OP FM ENGINE ACTIVE");return this.context;
 }
 noteOn({note=60,velocity=100,patch,duration=1.2}={}){
  if(!this.context||this.context.state!=="running")throw new Error("Audio engine is not ready. Press Play to initialize audio.");
  if(!patch||!Array.isArray(patch.operators)||patch.operators.length!==6)throw new Error("A six-operator patch is required.");
  if(this.voices.size>=this.maxVoices){const oldest=this.voices.values().next().value;if(oldest)this.dispose(oldest);}\n  const ctx=this.context,now=ctx.currentTime,freq=midiHz(note),route=ROUTES[patch.topology]||ROUTES.cascade;
  const macros=patch.macros||{},brightness=clamp(macros.brightness??50,0,100)/100;
  const body=clamp(macros.body??50,0,100)/100,motion=clamp(macros.motion??30,0,100)/100;
  const edge=clamp(macros.edge??0,0,100)/100,velocityGain=Math.pow(clamp(velocity,1,127)/127,1.35);
  const master=ctx.createGain();master.gain.value=.65;master.connect(ctx.destination);
  const oscillators=[],envelopes=[],feedbackNodes=[],envelopeParams=[];
  for(let i=0;i<6;i++){
   const op=patch.operators[i],osc=ctx.createOscillator(),env=ctx.createGain();
   const ratio=clamp(op.ratio??1,.25,16),peak=clamp(op.level??70,0,100)/100*velocityGain;
   const attack=seconds(op.attack??10,.8,.8),decay=seconds(op.decay??45,1.8,1.8)*(.65+motion*.7);
   const sustain=clamp(op.sustain??55,0,100)/100,release=seconds(op.release??30,2.5,2.5);
   osc.type="sine";osc.frequency.setValueAtTime(freq*ratio,now);
   env.gain.setValueAtTime(.0001,now);
   env.gain.linearRampToValueAtTime(Math.max(.0001,peak),now+Math.max(.002,attack));
   env.gain.linearRampToValueAtTime(Math.max(.0001,peak*sustain),now+Math.max(.002,attack)+Math.max(.002,decay));
   osc.connect(env);osc.start(now);
   oscillators.push(osc);envelopes.push(env);envelopeParams.push({attack,decay,sustain,release,peak});
  }
  // Real audio-rate modulation: each edge routes one operator's signal to another's frequency input.
  for(const [source,target] of route.edges){
   const si=source-1,ti=target-1,depth=ctx.createGain();
   const targetHz=freq*clamp(patch.operators[ti].ratio??1,.25,16);
   const sourceLevel=clamp(patch.operators[si].level??70,0,100)/100;
   depth.gain.value=targetHz*(.04+brightness*2.2)*(.25+sourceLevel);
   envelopes[si].connect(depth);depth.connect(oscillators[ti].frequency);
  }
  // Carrier operators are mixed into one output bus.
  for(const carrier of route.carriers){
   const sum=ctx.createGain();sum.gain.value=(.18+body*.42)/Math.sqrt(route.carriers.length);
   envelopes[carrier-1].connect(sum);sum.connect(master);
  }
  // Feedback paths use a short delay so the Web Audio graph's cycle is valid.
  for(let i=0;i<6;i++){
   const amount=clamp(patch.operators[i].feedback??0,0,100)/100;if(amount<=0)continue;
   const delay=ctx.createDelay(.02),fb=ctx.createGain();delay.delayTime.value=.004;
   const selfHz=freq*clamp(patch.operators[i].ratio??1,.25,16);
   fb.gain.value=Math.min(12000,selfHz*.75)*amount*(.08+edge*.7);
   envelopes[i].connect(delay);delay.connect(fb);fb.connect(oscillators[i].frequency);feedbackNodes.push(delay,fb);
  }
  const voice={oscillators,envelopes,feedbackNodes,master,envelopeParams,stopped:false};
  this.voices.add(voice);this.noteOff(voice,duration);
  const longestRelease=Math.max(...envelopeParams.map(p=>p.release));
  const cleanupAt=now+Math.max(0,duration)+longestRelease+.2;
  voice.cleanupTimer=window.setTimeout(()=>this.dispose(voice),Math.max(50,(cleanupAt-ctx.currentTime)*1000));
  return voice;
 }
 noteOff(voice,duration=0){
  if(!voice||voice.stopped)return;
  const off=this.context.currentTime+Math.max(0,duration),t=Math.max(0,duration);
  voice.envelopes.forEach((env,i)=>{
   const p=voice.envelopeParams[i],level=envelopeLevelAt(t,p);
   env.gain.cancelScheduledValues(off);env.gain.setValueAtTime(Math.max(.0001,level),off);
   env.gain.linearRampToValueAtTime(.0001,off+p.release);
  });
 }
 dispose(voice){
  if(!voice||voice.stopped)return;voice.stopped=true;
  if(voice.cleanupTimer)window.clearTimeout(voice.cleanupTimer);
  const at=this.context?.currentTime??0;
  voice.oscillators.forEach(osc=>{try{osc.stop(at+.01);osc.disconnect();}catch{}});
  [...voice.envelopes,...voice.feedbackNodes,voice.master].forEach(node=>{try{node.disconnect();}catch{}});
  this.voices.delete(voice);
 }
 allNotesOff(){for(const voice of [...this.voices])this.dispose(voice);}
 close(){this.allNotesOff();if(this.context){this.context.close();this.context=null;}}
}
