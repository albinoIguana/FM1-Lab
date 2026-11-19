/* SPDX-License-Identifier: GPL-3.0-only
 * Fixed-point candidate engines for FM1-Lab mixed-synthesis evaluation.
 * Engine patch parameters are copied into each voice at note start.
 */
#include "fm1lab_synth_engines.h"

#define OUT_LIMIT 32760
#define ENV_MAX_Q15 32767u

/* Q24 semitone ratios relative to C, with C4 phase increment at 44.1 kHz. */
static const uint32_t semitone_ratio_q24[12] = {
    16777216u, 17774841u, 18831813u, 19951693u,
    21138677u, 22396723u, 23730359u, 25143906u,
    26642354u, 28230670u, 29913627u, 31697061u
};
static const uint32_t c4_phase_inc = 25480119u;
static const int16_t sine64[64] = {
    0,3212,6393,9512,12539,15446,18204,20787,23170,25329,27245,28898,30273,31356,32137,32609,
    32767,32609,32137,31356,30273,28898,27245,25329,23170,20787,18204,15446,12539,9512,6393,3212,
    0,-3212,-6393,-9512,-12539,-15446,-18204,-20787,-23170,-25329,-27245,-28898,-30273,-31356,-32137,-32609,
    -32767,-32609,-32137,-31356,-30273,-28898,-27245,-25329,-23170,-20787,-18204,-15446,-12539,-9512,-6393,-3212
};

static int32_t clamp_q15(int64_t x)
{
    if (x > OUT_LIMIT) return OUT_LIMIT;
    if (x < -OUT_LIMIT) return -OUT_LIMIT;
    return (int32_t)x;
}

static uint32_t phase_for_pitch256(int16_t pitch256)
{
    int32_t offset=(int32_t)pitch256-60*256, octave=offset/3072, within=offset%3072;
    if(within<0){within+=3072;--octave;}
    uint32_t semitone=(uint32_t)within/256u, fraction=(uint32_t)within&255u;
    uint32_t next=semitone==11u?(2u<<24):semitone_ratio_q24[semitone+1u];
    uint64_t ratio=semitone_ratio_q24[semitone]+
        (((uint64_t)(next-semitone_ratio_q24[semitone])*fraction)>>8);
    uint64_t increment=((uint64_t)c4_phase_inc*ratio)>>24;
    if(octave>0)increment<<=(uint32_t)octave;
    else if(octave<0)increment>>=(uint32_t)(-octave);
    return increment>UINT32_MAX?UINT32_MAX:(uint32_t)increment;
}

static int32_t sine_lookup(uint32_t phase)
{
    uint32_t index=phase>>26, frac=(phase>>10)&0xffffu;
    int32_t a=sine64[index&63u], b=sine64[(index+1u)&63u];
    return a+(int32_t)(((int64_t)(b-a)*frac)>>16);
}

static int32_t oscillator(uint32_t phase,uint8_t waveform,uint8_t duty)
{
    switch(waveform&3u) {
    case 0: return sine_lookup(phase);
    case 1: {
        uint32_t p=phase>>16;
        return p<32768u?(int32_t)(p*2u)-32768:98303-(int32_t)(p*2u);
    }
    case 2: return (int32_t)(phase>>16)-32768;
    default: return (uint8_t)(phase>>24)<(uint8_t)((uint32_t)duty*255u/100u)?24576:-24576;
    }
}

void fm1lab_synth_patch_default(fm1lab_synth_engine_t engine,fm1lab_synth_patch_t *p)
{
    if(!p)return;
    p->waveform=0u;p->secondary_waveform=2u;p->duty=50u;p->noise_mix=0u;
    p->osc_mix=50u;p->detune=8u;p->cutoff=65u;p->attack=8u;p->sustain=75u;p->release=35u;
    if(engine==FM1LAB_SYNTH_PSG){p->waveform=3u;p->duty=50u;p->noise_mix=8u;}
    if(engine==FM1LAB_SYNTH_SUBTRACTIVE){p->waveform=2u;p->secondary_waveform=2u;p->osc_mix=35u;p->cutoff=55u;}
}

int fm1lab_synth_patch_valid(fm1lab_synth_engine_t engine,const fm1lab_synth_patch_t *p)
{
    if(!p||engine>=FM1LAB_SYNTH_ENGINE_COUNT||p->waveform>3u||p->secondary_waveform>3u||
       p->duty<5u||p->duty>95u||p->noise_mix>100u||p->osc_mix>100u||
       p->detune>100u||p->cutoff>100u||p->attack>100u||p->sustain>100u||p->release>100u)return 0;
    return 1;
}

int fm1lab_synth_set_patch(fm1lab_synth_voice_t *v,const fm1lab_synth_patch_t *p)
{
    if(!v||!fm1lab_synth_patch_valid((fm1lab_synth_engine_t)v->engine,p))return -1;
    v->patch=*p;
    /* Exponential-ish one-pole cutoff curve; no floating point in the renderer. */
    v->filter_alpha_q15=(uint16_t)(512u+((uint32_t)p->cutoff*p->cutoff*320u)/100u);
    return 0;
}

void fm1lab_synth_init(fm1lab_synth_voice_t *v,fm1lab_synth_engine_t engine)
{
    if(!v)return;
    uint8_t *bytes=(uint8_t *)v;
    for(uint32_t i=0;i<sizeof(*v);++i)bytes[i]=0u;
    v->engine=(uint8_t)engine;v->noise=0x2ce1u;
    fm1lab_synth_patch_default(engine,&v->patch);
    v->filter_alpha_q15=(uint16_t)(512u+((uint32_t)v->patch.cutoff*v->patch.cutoff*320u)/100u);
}

int fm1lab_synth_set_pitch(fm1lab_synth_voice_t *v,int16_t pitch256)
{
    if(!v||v->engine>=FM1LAB_SYNTH_ENGINE_COUNT)return -1;
    if (pitch256 < 0) pitch256 = 0;
    if (pitch256 > 127 * 256) pitch256 = 127 * 256;
    v->note=(uint8_t)((uint16_t)pitch256>>8);v->phase_inc=phase_for_pitch256(pitch256);
    uint32_t detune=(uint32_t)v->patch.detune;
    v->phase_inc_b=(uint32_t)(((uint64_t)v->phase_inc*(1000u+detune*2u))/1000u);
    return 0;
}

void fm1lab_synth_note_on(fm1lab_synth_voice_t *v,uint8_t note,uint8_t velocity)
{
    if(!v||v->engine>=FM1LAB_SYNTH_ENGINE_COUNT||!velocity)return;
    v->velocity=velocity;(void)fm1lab_synth_set_pitch(v,(int16_t)((note>127u?127u:note)*256u));
    v->phase=0u;v->phase_b=0u;v->envelope_q15=0u;v->gate=1u;v->active=1u;v->filter_state=0;
    if(!v->noise)v->noise=0x2ce1u;
}
void fm1lab_synth_note_off(fm1lab_synth_voice_t *v){if(v)v->gate=0u;}

static int32_t envelope_next(fm1lab_synth_voice_t *v)
{
    uint32_t attack=64u+(uint32_t)v->patch.attack*1984u/100u;
    uint32_t sustain=(uint32_t)v->patch.sustain*ENV_MAX_Q15/100u;
    uint32_t release=32u+(uint32_t)v->patch.release*992u/100u;
    if(v->gate){uint32_t next=(uint32_t)v->envelope_q15+attack;v->envelope_q15=(uint16_t)(next>sustain?sustain:next);}
    else if(v->envelope_q15)v->envelope_q15=(uint16_t)(v->envelope_q15>release?v->envelope_q15-release:0u);
    else v->active=0u;
    return v->envelope_q15;
}

static int32_t render_wavetable(fm1lab_synth_voice_t *v)
{
    int32_t y=oscillator(v->phase,v->patch.waveform,v->patch.duty);
    v->phase+=v->phase_inc;return y;
}
static int32_t render_psg(fm1lab_synth_voice_t *v)
{
    int32_t pulse=oscillator(v->phase,3u,v->patch.duty);
    uint32_t bit=((v->noise>>0)^(v->noise>>1))&1u;
    v->noise=(v->noise>>1)|(bit<<14);v->phase+=v->phase_inc;
    int32_t noise=(v->noise&1u)?24576:-24576;
    return (pulse*(100-v->patch.noise_mix)+noise*v->patch.noise_mix)/100;
}
static int32_t render_subtractive(fm1lab_synth_voice_t *v)
{
    int32_t a=oscillator(v->phase,v->patch.waveform,v->patch.duty);
    int32_t b=oscillator(v->phase_b,v->patch.secondary_waveform,v->patch.duty);
    int32_t x=(a*(100-v->patch.osc_mix)+b*v->patch.osc_mix)/100;
    int32_t delta=x-v->filter_state;
    v->filter_state+=(int32_t)(((int64_t)delta*v->filter_alpha_q15)>>15);
    v->phase+=v->phase_inc;v->phase_b+=v->phase_inc_b;return v->filter_state;
}

int fm1lab_synth_render(fm1lab_synth_voice_t *v,int16_t *out,uint32_t frames)
{
    if(!v||!out||v->engine>=FM1LAB_SYNTH_ENGINE_COUNT)return -1;
    for(uint32_t i=0;i<frames;++i){
        int32_t sample=0;
        if(v->active){int32_t env=envelope_next(v);if(v->active&&env){
            if(v->engine==FM1LAB_SYNTH_WAVETABLE)sample=render_wavetable(v);
            else if(v->engine==FM1LAB_SYNTH_PSG)sample=render_psg(v);
            else sample=render_subtractive(v);
            sample=(int32_t)(((int64_t)sample*env*v->velocity)/(32768LL*127LL));
        }}
        out[i]=(int16_t)clamp_q15(sample);
    }
    return 0;
}
size_t fm1lab_synth_voice_bytes(void){return sizeof(fm1lab_synth_voice_t);}
