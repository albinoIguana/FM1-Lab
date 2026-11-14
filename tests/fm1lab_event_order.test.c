#include "../app/fm1lab_event_order.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

enum { MAX_EVENTS=63u, PATCHES=8u, MACROS=4u, ROUNDS=250u };
typedef struct { uint8_t type,a,b,c; } event_t;
typedef struct {
    int16_t last_note;
    uint16_t note_ons,note_offs;
    uint8_t levels[PATCHES];
    uint8_t macros[PATCHES][MACROS];
    uint8_t loaded_instrument[2];
    uint8_t performance[3];
    uint8_t voice_version[PATCHES];
    int8_t octave[PATCHES], fine[PATCHES];
} engine_model_t;
static uint32_t rng=0x6d2b79f5u;
static uint32_t random_u32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void apply_event(engine_model_t *s,const event_t *e)
{
    switch(e->type) {
    case FM1LAB_EVENT_NOTE_ON:s->last_note=e->a;s->note_ons++;break;
    case FM1LAB_EVENT_NOTE_OFF:s->last_note=-1;s->note_offs++;break;
    case FM1LAB_EVENT_PATCH_LEVEL:s->levels[e->a%PATCHES]=e->b;break;
    case FM1LAB_EVENT_MACRO_SET:s->macros[e->a%PATCHES][e->b%MACROS]=e->c;break;
    case FM1LAB_EVENT_INSTRUMENT_LOAD: {
        uint8_t lane=(uint8_t)(e->a&1u),id=e->b;
        s->loaded_instrument[lane]=id;
        for(uint32_t p=0;p<4u;p++) {
            uint32_t patch=(uint32_t)lane*4u+p;
            s->levels[patch]=(uint8_t)((id*7u+p*11u)%128u);
            for(uint32_t m=0;m<MACROS;m++)
                s->macros[patch][m]=(uint8_t)((id*13u+p*5u+m*3u)%101u);
        }
        break;
    }
    case FM1LAB_EVENT_PERFORMANCE_APPLY:
        s->performance[0]=e->a;s->performance[1]=e->b;s->performance[2]=e->c;break;
    case FM1LAB_EVENT_PATCH_VOICE:s->voice_version[e->a%PATCHES]=e->b;break;
    case FM1LAB_EVENT_PATCH_PITCH:
        s->octave[e->a%PATCHES]=(int8_t)e->b;s->fine[e->a%PATCHES]=(int8_t)e->c;break;
    case FM1LAB_EVENT_PANIC:s->last_note=-1;break;
    default:break;
    }
}
static void run_batched(const event_t *events,uint32_t count,engine_model_t *state)
{
    uint32_t cursor=0u,callback=0u,trace_count=0u;
    uint8_t trace[MAX_EVENTS];uint32_t trace_callback[MAX_EVENTS];
    while(cursor<count) {
        uint8_t heavy_done=0u;uint32_t in_callback=0u;
        callback++;
        while(cursor<count) {
            const event_t *e=&events[cursor];
            if(fm1lab_event_should_defer(e->type,heavy_done))break;
            assert(trace_count<MAX_EVENTS);
            trace[trace_count]=e->type;trace_callback[trace_count]=callback;trace_count++;
            apply_event(state,e);
            if(fm1lab_event_is_heavy(e->type)){heavy_done=1u;in_callback++;}
            cursor++;
        }
        assert(in_callback<=1u);
    }
    assert(trace_count==count);
    for(uint32_t i=0;i<count;i++)assert(trace[i]==events[i].type);
    for(uint32_t i=0;i<count;i++)
        for(uint32_t j=i+1u;j<count;j++)
            if(trace_callback[i]==trace_callback[j])
                assert(!(fm1lab_event_is_heavy(trace[i])&&fm1lab_event_is_heavy(trace[j])));
}
static event_t random_event(void)
{
    uint32_t r=random_u32();event_t e={(uint8_t)(r%8u),0u,0u,0u};
    switch(e.type) {
    case 0:e.type=FM1LAB_EVENT_NOTE_ON;e.a=(uint8_t)(36u+r%60u);e.b=(uint8_t)(40u+r%88u);break;
    case 1:e.type=FM1LAB_EVENT_NOTE_OFF;e.a=(uint8_t)(36u+r%60u);break;
    case 2:e.type=FM1LAB_EVENT_PATCH_LEVEL;e.a=(uint8_t)(r%PATCHES);e.b=(uint8_t)(r%128u);break;
    case 3:e.type=FM1LAB_EVENT_MACRO_SET;e.a=(uint8_t)(r%PATCHES);e.b=(uint8_t)((r>>8)%MACROS);e.c=(uint8_t)((r>>16)%101u);break;
    case 4:e.type=FM1LAB_EVENT_INSTRUMENT_LOAD;e.a=(uint8_t)((r>>8)&1u);e.b=(uint8_t)((r>>16)%32u);break;
    case 5:e.type=FM1LAB_EVENT_PERFORMANCE_APPLY;e.a=(uint8_t)r;e.b=(uint8_t)(r>>8);e.c=(uint8_t)(r>>16);break;
    case 6:e.type=FM1LAB_EVENT_PATCH_VOICE;e.a=(uint8_t)(r%PATCHES);e.b=(uint8_t)(r>>8);break;
    default:e.type=FM1LAB_EVENT_PATCH_PITCH;e.a=(uint8_t)(r%PATCHES);e.b=(uint8_t)(int8_t)((int)(r%9u)-4);e.c=(uint8_t)(int8_t)((int)((r>>8)%201u)-100);break;
    }
    return e;
}
static void test_required_deferred_order(void)
{
    const event_t events[]={
        {FM1LAB_EVENT_PERFORMANCE_APPLY,91u,22u,7u},
        {FM1LAB_EVENT_NOTE_ON,60u,100u,0u},
        {FM1LAB_EVENT_INSTRUMENT_LOAD,0u,3u,0u},
        {FM1LAB_EVENT_INSTRUMENT_LOAD,1u,9u,0u},
        {FM1LAB_EVENT_PATCH_LEVEL,4u,37u,0u},
        {FM1LAB_EVENT_MACRO_SET,4u,2u,88u},
        {FM1LAB_EVENT_PATCH_PITCH,4u,254u,35u},
        {FM1LAB_EVENT_PATCH_VOICE,4u,17u,0u},
        {FM1LAB_EVENT_NOTE_OFF,60u,0u,0u}
    };
    engine_model_t actual={0},expected={0};
    for(uint32_t i=0;i<sizeof(events)/sizeof(events[0]);i++)apply_event(&expected,&events[i]);
    run_batched(events,(uint32_t)(sizeof(events)/sizeof(events[0])),&actual);
    assert(memcmp(&actual,&expected,sizeof actual)==0);
    assert(actual.loaded_instrument[0]==3u&&actual.loaded_instrument[1]==9u);
    assert(actual.levels[4]==37u&&actual.macros[4][2]==88u);
    assert(actual.octave[4]==-2&&actual.fine[4]==35);
    assert(actual.voice_version[4]==17u&&actual.note_ons==1u&&actual.note_offs==1u);
}
static void test_random_event_sequences(void)
{
    for(uint32_t round=0;round<ROUNDS;round++) {
        event_t events[MAX_EVENTS];engine_model_t actual={0},expected={0};
        uint32_t count=8u+random_u32()%(MAX_EVENTS-7u);
        for(uint32_t i=0;i<count;i++)events[i]=random_event();
        for(uint32_t i=0;i<count;i++)apply_event(&expected,&events[i]);
        run_batched(events,count,&actual);
        assert(memcmp(&actual,&expected,sizeof actual)==0);
    }
}
int main(void){test_required_deferred_order();test_random_event_sequences();return 0;}
