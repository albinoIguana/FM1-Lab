// FM1-Lab local six-operator FM engine.
// Independent Web Audio implementation; it does not emulate stock FM-1 firmware.
const ROUTES = {
  cascade: { edges:[[6,5],[5,4],[4,3],[3,2],[2,1]], carriers:[1] },
  pairs:   { edges:[[2,1],[4,3],[6,5]], carriers:[1,3,5] },
  split:   { edges:[[6,4],[5,4],[4,2],[3,2],[2,1]], carriers:[1] }
};
const midiHz = n => 440 * Math.pow(2,(n-69)/12);
const clamp = (v,min,max) => Math.max(min,Math.min(max,v));
const seconds = (v,scale,max) => clamp(v/100*scale,0,max);

function envelopeLevelAt(t, attack, decay, sustain, peak) {
  if (attack > 0 && t < attack) return peak * t / attack;
  if (decay > 0 && t < attack + decay) {
    const p=(t-attack)/decay;
    return peak + (peak*sustain-peak)*p;
  }
  return peak*sustain;
}

export class FMEngine {
  constructor({onState=()=>{}}={}) {
    this.context = null;
    this.voices = new Set();
    this.onState = onState;
  }
  async ready() {
    if (!this.context) {
      const AudioContextClass=window.AudioContext||window.webkitAudioContext;
      if (!AudioContextClass) throw new Error("Web Audio is not supported in this browser.");
      this.context=new AudioContextClass();
    }
    if (this.context.state==="suspended") await this.context.resume();
    this.onState("LOCAL 6-OP FM ENGINE ACTIVE");
    return this.context;
  }
  noteOn({note=60,velocity=100,patch,duration=1.2}={}) {
    if (!this.context || this.context.state!=="running") throw new Error("Audio engine is not ready. Press Play to initialize audio.");
    const ctx=this.context, now=ctx.currentTime, freq=midiHz(note);
    const route=ROUTES[patch.topology]||ROUTES.cascade;
    const macros=patch.macros||{};
    const brightness=clamp(macros.brightness??50,0,100)/100;
    const body=clamp(macros.body??50,0,100)/100;
    const edge=clamp(macros.edge??0,0,100)/100;
    const motion=clamp(macros.motion??30,0,100)/100;
    const master=ctx.createGain();
    master.gain.value=0.65;
    master.connect(ctx.destination);
    const oscillators=[], envelopes=[], feedbackNodes=[];
    const velocityGain=Math.pow(clamp(velocity,1,127)/127,1.35);
    const ops=patch.operators;
    for(let i=0;i<6;i++){
      const op=ops[i]||{};
      const osc=ctx.createOscillator(), env=ctx.createGain();
      const ratio=clamp(op.ratio??1,.25,16);
      const peak=clamp(op.level??70,0,100)/100;
      const attack=seconds(op.attack??10,.8,0.8);
      const decay=seconds(op.decay??45,1.8,1.8);
      const sustain=clamp(op.sustain??55,0,100)/100;
      const release=seconds(op.release??30,2.5,2.5);
      osc.type="sine";
      osc.frequency.setValueAtTime(freq*ratio,now);
      env.gain.setValueAtTime(0.0001,now);
      env.gain.linearRampToValueAtTime(Math.max(.0001,peak*velocityGain),now+Math.max(.002,attack));
      env.gain.linearRampToValueAtTime(Math.max(.0001,peak*velocityGain*sustain),now+Math.max(.002,attack)+Math.max(.002,decay));
      osc.connect(env);
      osc.start(now);
      oscillators.push(osc); envelopes.push(env);
      op._release=release;
      op._attack=attack; op._decay=decay; op._sustain=sustain; op._peak=peak*velocityGain;
    }
    // Every edge is a genuine audio-rate FM connection: source operator -> target frequency.
    for(const [source,target] of route.edges){
      const sourceIndex=source-1,targetIndex=target-1;
      const depth=ctx.createGain();
      const targetRatio=clamp(ops[targetIndex]?.ratio??1,.25,16);
      const targetHz=freq*targetRatio;
      const sourceLevel=clamp(ops[sourceIndex]?.level??70,0,100)/100;
      depth.gain.value=targetHz*(.04+brightness*2.2)*(0.25+sourceLevel);
      envelopes[sourceIndex].connect(depth);
      depth.connect(oscillators[targetIndex].frequency);
    }
    // Carrier operators are summed into the output bus.
    for(const carrier of route.carriers){
      const index=carrier-1, sum=ctx.createGain();
      sum.gain.value=(.18+body*.42)/Math.max(1,Math.sqrt(route.carriers.length));
      envelopes[index].connect(sum); sum.connect(master);
    }
    // A short delay makes the feedback loop legal in Web Audio's cyclic graph model.
    for(let i=0;i<6;i++){
      const amount=clamp(ops[i]?.feedback??0,0,100)/100;
      if(amount<=0) continue;
      const delay=ctx.createDelay(.02), fb=ctx.createGain();
      delay.delayTime.value=.004;
      fb.gain.value=Math.min(.88,amount*(.12+edge*.48));
      envelopes[i].connect(delay); delay.connect(fb); fb.connect(oscillators[i].frequency);
      feedbackNodes.push(delay,fb);
    }
    const voice={oscillators,envelopes,feedbackNodes,master,stopped:false};
    this.voices.add(voice);
    this.noteOff(voice,duration);
    const longestRelease=Math.max(...ops.map(o=>seconds(o.release??30,2.5,2.5)));
    const cleanupAt=now+duration+longestRelease+.15;
    voice.cleanupTimer=window.setTimeout(()=>this.dispose(voice),Math.max(50,(cleanupAt-ctx.currentTime)*1000));
    return voice;
  }
  noteOff(voice,duration=0) {
    if(!voice||voice.stopped)return;
    const ctx=this.context, off=ctx.currentTime+Math.max(0,duration);
    voice.envelopes.forEach((env,i)=>{
      const op=this._currentPatch?.operators?.[i];
      // ADSR parameters are attached to the voice when it is constructed.
      const p=voice.envelopeParams[i], t=Math.max(0,duration);
      const level=envelopeLevelAt(t,p.attack,p.decay,p.sustain,p.peak);
      env.gain.cancelScheduledValues(off);
      env.gain.setValueAtTime(Math.max(.0001,level),off);
      env.gain.linearRampToValueAtTime(.0001,off+p.release);
    });
  }
  dispose(voice) {
    if(!voice||voice.stopped)return;
    voice.stopped=true;
    if(voice.cleanupTimer)window.clearTimeout(voice.cleanupTimer);
    const at=this.context?.currentTime??0;
    voice.oscillators.forEach(osc=>{try{osc.stop(at+.01);osc.disconnect()}catch{}});
    [...voice.envelopes,...voice.feedbackNodes,voice.master].forEach(node=>{try{node.disconnect()}catch{}});
    this.voices.delete(voice);
  }
  allNotesOff() { for(const voice of [...this.voices])this.dispose(voice); }
  close() { this.allNotesOff(); if(this.context){this.context.close();this.context=null;} }
}
