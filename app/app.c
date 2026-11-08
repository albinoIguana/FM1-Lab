/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* FM1-Lab's four-layer FM instrument interface for the native FM1 platform.
 * Physical controls navigate patch, operator, envelope, LFO, sequencer, arp and
 * master-FX pages. Keybed and incoming MIDI feed the FM6 engine; the audio
 * interrupt owns synthesis state while UI edits are published through events.
 * Part of the unity build (device/unity.c, host/plat_host.c), after the platform:
 * gfx.c and announce.c are in scope. */
#include "fm6.h"
#include "fm1lab_settings_record.h"
#include "fm1lab_instrument.h"
#include "fm1lab_voice_editor.h"
#include "fm1lab_store_library.h"
#include "../firmware/fm1lab_physical_controls.h"
#include <stddef.h>
#include <string.h>

static settings_t set;
static uint8_t set_dirty;
static fm1lab_instrument_t current_instrument;
static fm1lab_insert_state_t current_inserts;
static fm1lab_instrument_t factory_instrument;
static uint8_t instrument_index, instrument_dirty, instrument_loaded;
static volatile uint8_t instrument_apply_pending;
static uint8_t voice_update_snapshot[FM1LAB_PATCH_BYTES];
static volatile uint8_t voice_update_pending, voice_update_patch, voice_dirty;
enum { USER_INSTRUMENT_COUNT = FM1LAB_STORE_INSTRUMENT_COUNT };
enum { PAGE_HOME = 0, PAGE_MACRO, PAGE_GLOBAL, PAGE_OPERATOR_MAP, PAGE_ENV, PAGE_LFO, PAGE_LFO_MOD, PAGE_SEQ, PAGE_SEQ_OPTIONS, PAGE_SEQ_PIANO, PAGE_ARP, PAGE_ARP_STEPS, PAGE_ARP_LATCH, PAGE_FX_AMP, PAGE_FX_EQ, PAGE_FX_COMPRESSOR, PAGE_FX_CHORUS, PAGE_FX_DELAY, PAGE_FX_REVERB, PAGE_FX_PHASER, PAGE_FX_ROUTING, PAGE_INSTRUMENT_MACRO, PAGE_COUNT };
static uint8_t page = PAGE_HOME;
/* HOME starts in whole-instrument mode; selecting a layer enters patch focus. */
static uint8_t patch_focus;
static uint8_t eq_ui_band = 1u;
static uint8_t env_operator = 1u, env_levels, operator_detune_mode;
static uint8_t seq_ui_lane, seq_ui_step, seq_ui_instrument[2]={0u,0u}, seq_ui_note[2][64], seq_ui_velocity[2][64],
               seq_ui_gate[2][64], seq_ui_enabled[2][64], seq_ui_accent[2][64], seq_ui_patch[2]={0u,1u};
static char seq_ui_instrument_name[2][FM1LAB_INSTRUMENT_NAME_BYTES+1u]={{'F','A','C','T','O','R','Y',0},{'F','A','C','T','O','R','Y',0}};
static fm1lab_instrument_t instrument_apply_snapshot;
static fm1lab_insert_state_t inserts_apply_snapshot;
static uint8_t seq_record_armed, seq_record_mode, seq_record_last_step=255u;
static uint8_t seq_clear_prompt;
static int16_t seq_clear_turn;
static uint8_t seq_record_active[NKEYS], seq_record_step[NKEYS], seq_record_lane[NKEYS], seq_record_note[NKEYS];
static uint32_t seq_record_started[NKEYS];
static uint8_t seq_record_redraw;
static uint16_t transport_ui_bpm=120u;
static uint8_t arp_ui_latch, arp_ui_mode, arp_ui_step, arp_ui_length=16u, arp_ui_gain[16], arp_ui_accent[16], arp_ui_step_gate[16];
static uint8_t arp_ui_division=3u, arp_ui_gate=75u, arp_ui_octaves=1u;
static uint8_t arp_ui_output, arp_ui_random, arp_ui_macro_dest, arp_ui_depth=50u;

static const char *patch_display_name(uint32_t patch);
static const char *instrument_display_name(void);
static uint16_t panel_fill(void);
static uint8_t instrument_macro_effective(uint32_t patch, uint32_t macro);

/* the controls by their names in plat.h, in its order */
static const char *const BTN_NAME[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                         "PLAY", "REC", "OCT-", "OCT+"};
static const char *const ENC_NAME[NE] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};

/* ------------------------------------------------------------------ voice --- */
/* one voice, the last note wins; the main loop and MIDI in set it, the audio interrupt plays it */
static volatile int32_t voice_note = -1;     /* display-only: last pressed MIDI/key note */
enum { EV_NOTE_ON = 1, EV_NOTE_OFF, EV_PANIC, EV_MACRO_SET, EV_PATCH_LEVEL, EV_INSTRUMENT_LOAD, EV_PATCH_VOICE, EV_MASTER_INSERT, EV_MASTER_COMP, EV_PERFORMANCE_APPLY, EV_SEQ_RUN };
static fm6_performance_state_t performance_update_snapshot;
static volatile uint8_t performance_update_pending, performance_update_dirty;
typedef struct { volatile uint8_t type, a, b, c; } ui_event_t;
static volatile ui_event_t ui_events[64];
static volatile uint8_t ui_event_head, ui_event_tail, ui_event_overflow;

/* Main-loop controls never write FM voice state directly. They publish compact events;
 * the audio interrupt is the sole runtime writer of the synthesizer state. */

/* M-VAVE's 16 compact factory DX7 recipes, expanded to the same 156-byte
 * voice layout used by FM1-Lab/Felucca. PRESET loads one recipe into one patch. */
enum { FM1LAB_FACTORY_PRESET_COUNT = 16u };
struct fm1lab_factory_pair {
    uint8_t level, coarse : 2, detune : 4, mod, ratio, fine, decay, body;
};
struct fm1lab_factory_voice {
    uint8_t attack, decay, settle, release, body, sustain, mod_attack, mod_release, feedback;
    struct fm1lab_factory_pair pair[3];
};
static const char fm1lab_factory_names[FM1LAB_FACTORY_PRESET_COUNT][11] = {
    "NEON KEYS ","ORBIT BELL","SOFT ORGAN","ROUND BASS","WARM AIR  ","WOOD REED ",
    "SOFT EP   ","DRY CLAV  ","FM GUITAR ","MARIMBA   ","VIBES     ","SOFT BRASS",
    "FM STRINGS","SUB BASS  ","METAL HIT ","SOFT LEAD "
};
static const struct fm1lab_factory_voice fm1lab_factory_voices[FM1LAB_FACTORY_PRESET_COUNT] = {
    {96,52,30,62,84,0,99,66,0, {{98,1,7,54,1,0,65,38},{91,1,8,60,1,0,57,58},{79,1,6,48,7,0,80,0}}},
    {99,50,28,53,70,0,99,62,0, {{96,1,7,58,3,50,59,0},{82,2,7,48,7,0,68,0},{69,3,7,35,11,0,80,0}}},
    {95,65,45,78,99,99,95,78,0, {{90,1,7,0,1,0,65,0},{87,2,7,0,1,0,65,0},{74,3,7,0,1,0,65,0}}},
    {97,72,46,82,91,84,99,86,0, {{99,1,7,42,1,0,76,32},{74,2,7,32,1,0,82,20},{0,1,7,0,1,0,80,0}}},
    {53,47,36,52,96,92,49,55,0, {{90,1,5,42,1,0,55,82},{84,1,9,34,2,0,53,76},{73,2,7,27,1,0,52,78}}},
    {84,61,46,72,94,89,78,77,1, {{95,1,7,61,2,0,63,86},{82,1,8,38,1,0,68,75},{70,3,7,29,1,0,72,60}}},
    {94,49,28,60,86,0,96,64,0, {{98,1,7,44,1,0,60,35},{86,1,8,38,3,0,66,20},{72,2,6,31,1,0,72,0}}},
    {99,75,47,80,42,0,99,83,1, {{96,1,7,67,1,0,82,18},{81,2,7,55,2,0,86,0},{0,1,7,0,1,0,80,0}}},
    {97,57,32,66,69,0,99,74,0, {{97,1,7,55,1,0,72,18},{83,2,7,46,3,0,81,0},{66,3,7,38,5,0,85,0}}},
    {99,67,37,72,34,0,99,81,0, {{98,1,7,60,3,0,85,0},{74,2,7,46,5,0,88,0},{58,3,7,32,7,0,91,0}}},
    {98,52,25,55,78,0,99,65,0, {{94,1,7,56,4,0,76,0},{76,2,8,39,3,0,81,0},{66,3,6,28,2,0,84,0}}},
    {81,62,42,72,96,90,67,75,1, {{95,1,7,67,1,0,62,90},{84,1,8,46,1,0,65,83},{70,2,6,40,2,0,68,75}}},
    {61,48,39,54,96,94,57,58,0, {{89,1,5,54,1,0,55,89},{83,1,9,36,2,0,55,83},{72,2,7,30,4,0,58,76}}},
    {95,72,44,84,96,94,99,86,0, {{99,1,7,0,1,0,80,0},{0,1,7,0,1,0,80,0},{0,1,7,0,1,0,80,0}}},
    {99,65,40,72,48,0,99,76,0, {{89,1,7,65,7,31,67,0},{70,2,7,58,11,47,73,0},{56,3,7,48,13,79,78,0}}},
    {88,60,40,71,96,90,82,75,1, {{96,1,7,45,1,0,65,83},{80,1,8,31,2,0,70,72},{71,2,6,24,1,0,74,66}}}
};
static void fm1lab_factory_voice_build(uint8_t out[FM1LAB_PATCH_BYTES], uint8_t preset)
{
    const struct fm1lab_factory_voice *sound = &fm1lab_factory_voices[preset % FM1LAB_FACTORY_PRESET_COUNT];
    for (uint32_t i=0;i<FM1LAB_PATCH_BYTES;i++) out[i]=0u;
    for (uint32_t i=0;i<10u;i++) out[145u+i]=(uint8_t)fm1lab_factory_names[preset % FM1LAB_FACTORY_PRESET_COUNT][i];
    for (uint32_t op=0;op<6u;op++) {
        uint8_t *v=out+21u*op;
        const struct fm1lab_factory_pair *pair=&sound->pair[op/2u];
        uint8_t carrier=(uint8_t)(op&1u);
        v[0]=carrier?sound->attack:sound->mod_attack;
        v[1]=carrier?sound->decay:pair->decay;
        v[2]=sound->settle; v[3]=carrier?sound->release:sound->mod_release;
        v[4]=99u; v[5]=carrier?sound->body:pair->body;
        v[6]=carrier?sound->sustain:(sound->sustain?pair->body:0u);
        v[8]=50u; v[10]=carrier?8u:18u; v[13]=1u;
        v[15]=carrier?2u:4u; v[16]=carrier?pair->level:pair->mod;
        v[18]=carrier?pair->coarse:pair->ratio;
        v[19]=carrier?0u:pair->fine; v[20]=pair->detune;
        if (preset==2u) {
            static const uint8_t levels[6]={89,77,84,68,59,52};
            static const uint8_t ratios[6]={1,2,3,4,6,8};
            v[16]=levels[op]; v[18]=ratios[op]; v[19]=0u;
            v[5]=v[6]=99u; v[15]=1u; v[20]=7u;
        }
    }
    for(uint32_t i=126u;i<130u;i++)out[i]=99u;
    for(uint32_t i=130u;i<134u;i++)out[i]=50u;
    out[134]=(uint8_t)(preset==2u?31u:4u);
    out[135]=sound->feedback; out[136]=1u;
    out[137]=28u; out[138]=35u;
    if(preset==4u||preset==5u||preset==12u){out[139]=2u;out[143]=2u;}
    out[141]=1u; out[142]=4u; out[144]=24u; out[155]=63u;
}


/* Turn a factory recipe into a genuinely long-form layer. DX-style EG rates
 * are inverse-time controls: lower values create slower stages. Two contrasting
 * evolvers are used per Instrument; the remaining voices keep the attack clear. */
static void fm1lab_make_evolving_voice(uint8_t voice[FM1LAB_PATCH_BYTES], uint8_t texture)
{
    static const char *const names[2]={"SLOW AIR","EVOLVE FX"};
    uint8_t carriers=fm1lab_carrier_mask(voice[134]&31u);
    for(uint32_t op=0;op<6u;op++) {
        uint8_t *v=voice+(5u-op)*21u;
        uint8_t carrier=(uint8_t)((carriers>>op)&1u);
        uint8_t spread=(uint8_t)(op*2u);
        if(!texture) {
            /* Broad, slow swell; modulator envelopes move at different rates
             * so the harmonic color opens after the fundamental arrives. */
            v[0]=(uint8_t)(carrier?13u+spread:20u+spread);
            v[1]=(uint8_t)(carrier?24u+spread:31u+spread);
            v[2]=(uint8_t)(carrier?35u+spread:43u+spread);
            v[3]=(uint8_t)(carrier?19u+spread:27u+spread);
            v[4]=99u;
            v[5]=(uint8_t)(carrier?88u:76u);
            v[6]=(uint8_t)(carrier?72u:48u);
            v[7]=0u;
        } else {
            /* More spectral movement and a slower, uneven fade; deliberately
             * contrasts with SLOW AIR instead of doubling its envelope. */
            v[0]=(uint8_t)(carrier?19u+spread:11u+spread);
            v[1]=(uint8_t)(carrier?34u+spread:24u+spread);
            v[2]=(uint8_t)(carrier?46u+spread:37u+spread);
            v[3]=(uint8_t)(carrier?23u+spread:16u+spread);
            v[4]=99u;
            v[5]=(uint8_t)(carrier?82u:68u);
            v[6]=(uint8_t)(carrier?58u:36u);
            v[7]=0u;
            if(!carrier && v[18]<28u) v[18]=(uint8_t)(v[18]+(op%3u)+1u);
        }
    }
    voice[137]=(uint8_t)(texture?29u:17u); /* slow LFO */
    voice[138]=(uint8_t)(texture?24u:48u); /* let the note bloom before modulation */
    voice[139]=(uint8_t)(texture?7u:3u);  /* restrained pitch movement */
    voice[140]=(uint8_t)(texture?42u:26u); /* audible but gradual level movement */
    voice[141]=0u;
    voice[142]=0u; /* sine */
    voice[143]=(uint8_t)(texture?4u:2u);
    for(uint32_t i=0;i<10u;i++) voice[145u+i]=(uint8_t)(i<strlen(names[texture])?names[texture][i]:' ');
}

/* Four dark FM creatures. Each patch is a purpose-built six-operator voice:
 * low throat, grinding growl, sharp teeth, and a slow spectral howl. */
static void fm1lab_make_beast_voice(uint8_t voice[FM1LAB_PATCH_BYTES], uint8_t beast, uint8_t role)
{
    static const uint8_t algorithms[4][4] = {
        {15u, 16u, 20u, 24u}, {17u, 15u, 21u, 23u},
        {16u, 18u, 20u, 24u}, {15u, 17u, 22u, 24u}
    };
    static const uint8_t coarse[4][6] = {
        {1u,2u,3u,5u,7u,1u}, {1u,3u,2u,6u,9u,1u},
        {1u,2u,4u,7u,11u,1u}, {1u,3u,5u,8u,13u,1u}
    };
    static const uint8_t fine[4][6] = {
        {0u,7u,19u,31u,11u,0u}, {0u,13u,27u,8u,37u,0u},
        {0u,9u,23u,41u,16u,0u}, {0u,17u,33u,5u,29u,0u}
    };
    uint8_t algorithm=algorithms[beast&3u][role&3u];
    uint8_t carriers=fm1lab_carrier_mask(algorithm);
    voice[134]=algorithm;
    voice[135]=(uint8_t)(role==0u?7u:role==1u?6u:role==2u?5u:4u);
    for(uint32_t op=0;op<6u;op++) {
        uint8_t *v=voice+(5u-op)*21u;
        uint8_t carrier=(uint8_t)((carriers>>op)&1u);
        uint8_t spread=(uint8_t)((op*3u+beast*2u)%13u);
        /* A quick bite followed by an uneven, vocal-like decay. */
        v[0]=(uint8_t)(carrier?82u+spread:91u-spread/2u);
        v[1]=(uint8_t)(role==0u?48u+spread:role==1u?31u+spread:58u+spread);
        v[2]=(uint8_t)(role==3u?22u+spread:45u+spread);
        v[3]=(uint8_t)(role==2u?35u+spread:62u+spread);
        v[4]=99u;
        v[5]=(uint8_t)(carrier?94u:82u);
        v[6]=(uint8_t)(carrier?(role==0u?84u:72u):(role==1u?65u:42u+spread));
        v[7]=0u;
        v[10]=(uint8_t)(carrier?8u:18u);
        v[15]=(uint8_t)(carrier?1u:4u);
        v[16]=(uint8_t)(carrier?94u:(role==0u?88u:role==1u?99u:78u+spread));
        v[18]=coarse[beast&3u][op];
        v[19]=fine[beast&3u][op];
        v[20]=(uint8_t)(7u+(op%4u)*2u);
    }
    /* Slow, slightly unstable pitch/level movement keeps the held note alive. */
    voice[137]=(uint8_t)(role==3u?31u:role==1u?47u:38u+beast*3u);
    voice[138]=(uint8_t)(role==3u?12u:role==1u?5u:18u);
    voice[139]=(uint8_t)(role==2u?12u:role==3u?24u:18u);
    voice[140]=(uint8_t)(role==3u?34u:role==1u?12u:18u);
    voice[141]=0u;
    voice[142]=0u;
    voice[143]=(uint8_t)(role==3u?5u:3u);
    {
        static const char *const names[4]={"THROAT","GROWL","TEETH","HOWL"};
        for(uint32_t i=0;i<10u;i++) voice[145u+i]=(uint8_t)(i<strlen(names[role&3u])?names[role&3u][i]:' ');
    }
}
typedef struct { const char *name; uint8_t source[4]; uint8_t level[4]; } fm1lab_beast_instrument_t;
static const fm1lab_beast_instrument_t fm1lab_beasts[4]={
    {"HOUND",{3u,5u,14u,12u},{22u,16u,11u,9u}},
    {"RAVAGE",{14u,11u,15u,5u},{20u,15u,10u,8u}},
    {"ABYSS",{13u,12u,14u,4u},{21u,16u,10u,8u}},
    {"WRAITH",{5u,12u,10u,14u},{20u,16u,11u,9u}}
};
static void fm1lab_seed_beast(uint32_t index,fm1lab_instrument_t *out)
{
    const fm1lab_beast_instrument_t *beast=&fm1lab_beasts[(index-16u)&3u];
    fm1lab_instrument_init(out,beast->name);
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;p++) {
        uint8_t voice[FM1LAB_PATCH_BYTES];
        fm1lab_factory_voice_build(voice,beast->source[p]);
        fm1lab_make_beast_voice(voice,(uint8_t)((index-16u)&3u),(uint8_t)p);
        (void)fm1lab_patch_set_voice(out,p,voice);
        out->patches[p].level=beast->level[p];
        out->patches[p].enabled=1u;
        out->patches[p].voice[155]=(uint8_t)(p==0u?48u:p==1u?64u:p==2u?80u:72u);
    }
}

/* Starter library: a deliberate four-voice combination of M-VAVE recipes. */
typedef struct { const char *name; uint8_t voice[4]; uint8_t level[4]; } fm1lab_seed_instrument_t;
static const fm1lab_seed_instrument_t fm1lab_seed_instruments[16] = {
 {"GLASS",{1,4,10,0},{25,18,12,8}},
 {"PLUSH",{6,2,12,10},{24,18,14,10}},
 {"PULSE",{15,3,0,14},{23,20,12,8}},
 {"TIDE",{12,5,4,1},{22,18,14,10}},
 {"DUST",{7,8,13,14},{24,18,12,8}},
 {"HALO",{10,1,6,11},{22,18,14,10}},
 {"STATIC",{14,15,7,13},{24,18,12,8}},
 {"VELVET",{2,12,4,6},{23,18,14,10}},
 {"BASSLINE",{3,13,7,0},{25,20,12,8}},
 {"PRISM",{9,10,1,6},{22,18,14,10}},
 {"REED",{5,11,12,4},{22,18,14,10}},
 {"SPARK",{0,14,9,15},{24,18,12,8}},
 {"NIGHT",{13,12,1,2},{25,18,12,8}},
 {"CHIME",{1,9,10,6},{22,18,14,10}},
 {"MOTOR",{3,15,14,7},{24,18,12,8}},
 {"BLOOM",{4,11,12,10},{22,18,14,10}}
};
static void fm1lab_seed_instrument(uint32_t index, fm1lab_instrument_t *out)
{
    const fm1lab_seed_instrument_t *seed=&fm1lab_seed_instruments[index&15u];
    char name[FM1LAB_INSTRUMENT_NAME_BYTES+1u];
    uint32_t n=0u;
    while(n<10u && seed->name[n]) { name[n]=seed->name[n]; ++n; }
    if(index>=16u && n<10u) { name[n++]=' '; name[n++]='2'; }
    name[n]='\\0';
    fm1lab_instrument_init(out,name);
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;p++) {
        uint8_t voice[FM1LAB_PATCH_BYTES];
        uint32_t source=(index>=16u)?((p+1u)&3u):p;
        fm1lab_factory_voice_build(voice,seed->voice[source]);
        if(p==1u) fm1lab_make_evolving_voice(voice,0u);
        else if(p==3u) fm1lab_make_evolving_voice(voice,1u);
        (void)fm1lab_patch_set_voice(out,p,voice);
        out->patches[p].level=seed->level[source];
        out->patches[p].enabled=1u;
        out->patches[p].voice[155]=64u;
    }
}
static int fm1lab_is_untouched_factory(const fm1lab_instrument_t *in)
{
    if(!in || !fm1lab_instrument_valid(in)) return 0;
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;p++) {
        if(memcmp(in->patches[p].voice,factory_instrument.patches[p].voice,FM1LAB_PATCH_BYTES)!=0 ||
           in->patches[p].level!=31u || !in->patches[p].enabled) return 0;
        for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++)
            if(in->patches[p].macros[m]!=50u) return 0;
    }
    return 1;
}
static void fm1lab_seed_factory_library(void)
{
    for(uint32_t index=0;index<FM1LAB_STORE_INSTRUMENT_COUNT;index++) {
        fm1lab_instrument_t existing, seed;
        fm1lab_insert_state_t inserts;
        if(fm1lab_store_library_load_full(index,&existing,&inserts)!=0) continue;
        if(index>=16u && index<20u) {
            fm1lab_instrument_t previous;
            fm1lab_seed_instrument(index,&previous);
            if(!fm1lab_is_untouched_factory(&existing) &&
               memcmp(existing.patches,previous.patches,sizeof existing.patches)!=0) continue;
            fm1lab_seed_beast(index,&seed);
        } else {
            if(!fm1lab_is_untouched_factory(&existing)) continue;
            fm1lab_seed_instrument(index,&seed);
        }
        if(fm1lab_store_library_save_full(index,&seed,&inserts)!=0)
            app_say("LIBRARY SEED","SAVE FAILED");
    }
}

static uint8_t fm1lab_factory_preset_for_voice(const uint8_t *voice)
{
    for(uint32_t p=0;p<FM1LAB_FACTORY_PRESET_COUNT;p++) {
        uint8_t name_matches=1u;
        for(uint32_t i=0;i<10u;i++)
            if(voice[145u+i]!=(uint8_t)fm1lab_factory_names[p][i]) name_matches=0u;
        if(name_matches) return (uint8_t)p;
    }
    return 0u;
}

static void ui_event_push(uint8_t type, uint8_t a, uint8_t b, uint8_t c)
{
    uint8_t head = ui_event_head, next = (uint8_t)((head + 1u) & 63u);
    if (next == ui_event_tail) {
        ui_event_overflow = 1;
        return;
    }
    ui_events[head].type = type;
    ui_events[head].a = a;
    ui_events[head].b = b;
    ui_events[head].c = c;
    ui_event_head = next;
}

static void performance_publish(void)
{
    if (performance_update_pending) { performance_update_dirty=1u; return; }
    set.performance.tempo_bpm = transport_ui_bpm;
    set.performance.arp_mode = arp_ui_mode;
    set.performance.arp_division=arp_ui_division;set.performance.arp_gate=arp_ui_gate;
    set.performance.arp_octaves=arp_ui_octaves;set.performance.arp_length=arp_ui_length;
    set.performance.arp_output=arp_ui_output;set.performance.arp_random=arp_ui_random;
    set.performance.arp_macro_dest=arp_ui_macro_dest;set.performance.arp_depth=arp_ui_depth;
    for(uint32_t step=0;step<16u;step++){
        set.performance.arp_step_gain[step]=arp_ui_gain[step];
        set.performance.arp_step_accent[step]=arp_ui_accent[step];
        set.performance.arp_step_gate[step]=arp_ui_step_gate[step];
    }
    for (uint32_t lane=0; lane<2u; ++lane) {
        set.performance.lane_instrument[lane] = seq_ui_instrument[lane];
        for (uint32_t step=0; step<64u; ++step) {
            set.performance.seq_enabled[lane][step] = seq_ui_enabled[lane][step];
            set.performance.seq_note[lane][step] = seq_ui_note[lane][step];
            set.performance.seq_velocity[lane][step] = seq_ui_velocity[lane][step];
            set.performance.seq_gate[lane][step] = seq_ui_gate[lane][step];
        }
    }
    performance_update_snapshot = set.performance;
    performance_update_dirty = 0u;
    performance_update_pending = 1u;
    ui_event_push(EV_PERFORMANCE_APPLY,0u,0u,0u);
}

static uint8_t instrument_macro_effective_from(const fm1lab_instrument_t *instrument,
        const fm1lab_insert_state_t *inserts,uint32_t patch,uint32_t macro)
{
    int32_t value;
    if(!instrument||!inserts||patch>=FM1LAB_PATCH_COUNT||macro>=FM1LAB_MACRO_COUNT)return 50u;
    value=(int32_t)instrument->patches[patch].macros[macro]+(int32_t)inserts->instrument_macros[macro]-50;
    return (uint8_t)clamp(value,0,100);
}

static void instrument_apply_engine_bank(uint8_t lane,const fm1lab_instrument_t *source,
        const fm1lab_insert_state_t *inserts,uint8_t apply_master)
{
    uint32_t base=(uint32_t)(lane&1u)*FM1LAB_PATCH_COUNT;
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;++p){
        const fm1lab_patch_t *part=&source->patches[p];
        uint32_t engine_patch=base+p;
        (void)fm6_patch_set_voice(engine_patch,part->voice);
        fm6_patch_set_enabled(engine_patch,1u);
        fm6_patch_set_level(engine_patch,part->level);
        fm6_patch_set_pan(engine_patch,fm1lab_patch_pan(source,p));
        for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;++m)
            fm6_macro_set(engine_patch,m,instrument_macro_effective_from(source,inserts,p,m));
    }
    if(apply_master){
        fm6_master_insert_reset_state();
        for(uint32_t field=0;field<FM6_INSERT_FIELD_COUNT;++field)
            fm6_master_insert_set(field,fm1lab_insert_state_get(inserts,field));
    }
}

static void instrument_apply_engine(void)
{
    instrument_apply_engine_bank(seq_ui_lane,&current_instrument,&current_inserts,1u);
}

static uint8_t instrument_macro_effective(uint32_t patch, uint32_t macro)
{
    int32_t value;
    if (patch >= FM1LAB_PATCH_COUNT || macro >= FM1LAB_MACRO_COUNT) return 50u;
    value = (int32_t)current_instrument.patches[patch].macros[macro] +
        (int32_t)current_inserts.instrument_macros[macro] - 50;
    return (uint8_t)clamp(value, 0, 100);
}

static void instrument_factory_capture(void)
{
    fm1lab_instrument_init(&factory_instrument, "FACTORY");
    for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        const uint8_t *voice = fm6_patch_data(p);
        if (voice) (void)fm1lab_patch_set_voice(&factory_instrument, p, voice);
        factory_instrument.patches[p].enabled = 1u;
        factory_instrument.patches[p].level = 31u; /* safe factory level */
    }
}

static void instrument_factory_defaults(void)
{
    current_instrument = factory_instrument;
    fm1lab_insert_state_init(&current_inserts);
}

static void instrument_sync_settings(void)
{
    set.enabled_mask = 0x0fu; /* compatibility field; layer volume is authoritative */
    for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        fm1lab_patch_t *part = &current_instrument.patches[p];
        part->enabled = 1u; /* normalize legacy records to volume-controlled layers */
        set.levels[p] = part->level;
        for (uint32_t m = 0; m < FM1LAB_MACRO_COUNT; ++m)
            set.macros[p][m] = part->macros[m];
    }
}

static void instrument_load(uint32_t index, int from_audio)
{
    fm1lab_instrument_t candidate;
    fm1lab_insert_state_t candidate_inserts;
    int loaded = fm1lab_store_library_load_full(index, &candidate, &candidate_inserts);
    if (!loaded && fm1lab_instrument_valid(&candidate) && fm1lab_insert_state_valid(&candidate_inserts)) {
        current_instrument = candidate;
        current_inserts = candidate_inserts;
    } else {
        instrument_factory_defaults();
    }
    instrument_index = (uint8_t)index;
    seq_ui_instrument[seq_ui_lane]=instrument_index;
    fm6_sequencer_set_lane(seq_ui_lane,(uint8_t)(seq_ui_lane*FM1LAB_PATCH_COUNT));
    for(uint32_t n=0;n<FM1LAB_INSTRUMENT_NAME_BYTES;n++)seq_ui_instrument_name[seq_ui_lane][n]=current_instrument.name[n];seq_ui_instrument_name[seq_ui_lane][FM1LAB_INSTRUMENT_NAME_BYTES]='\0';
    instrument_loaded = 1u;
    instrument_dirty = 0u;
    instrument_sync_settings();
    if (from_audio) {
        instrument_apply_snapshot=current_instrument;
        inserts_apply_snapshot=current_inserts;
        instrument_apply_pending = 1u;
        ui_event_push(EV_INSTRUMENT_LOAD, seq_ui_lane, 0, 0);
    } else {
        instrument_apply_engine();
    }
}

static int instrument_save(void)
{
    if (!instrument_loaded || !fm1lab_instrument_valid(&current_instrument))
        return -1;
    if (fm1lab_store_library_save_full(instrument_index, &current_instrument, &current_inserts))
        return -1;
    instrument_dirty = 0u;
    seq_ui_instrument[seq_ui_lane]=instrument_index;
    return 0;
}

static void ui_events_drain(void)
{
    uint8_t tail;
    if (ui_event_overflow) {
        ui_event_tail = ui_event_head;
        ui_event_overflow = 0;
        fm6_panic();
        fm6_sequencer_set_running(0u);
        if (instrument_loaded) instrument_apply_engine();
        else for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
            fm6_patch_set_enabled(p, 1u);
            fm6_patch_set_level(p, set.levels[p]);
            for (uint32_t m = 0; m < FM1LAB_MACRO_COUNT; ++m)
                fm6_macro_set(p, m, set.macros[p][m]);
        }
        instrument_apply_pending = 0u;
        voice_update_pending = 0u;
        voice_dirty = 0u;
        performance_update_pending = 0u;
        performance_update_dirty = 0u;
        (void)fm6_performance_state_set(&set.performance);
        return;
    }
    while ((tail = ui_event_tail) != ui_event_head) {
        uint8_t type = ui_events[tail].type, a = ui_events[tail].a;
        uint8_t b = ui_events[tail].b, c = ui_events[tail].c;
        ui_event_tail = (uint8_t)((tail + 1u) & 63u);
        if (type == EV_NOTE_ON)
            fm6_note_on_mask(a, b, c);
        else if (type == EV_NOTE_OFF)
            fm6_note_off_mask(a,c);
        else if (type == EV_PANIC)
            fm6_panic();
        else if (type == EV_MACRO_SET)
            fm6_macro_set(a, b, c);
        else if (type == EV_PATCH_LEVEL)
            fm6_patch_set_level(a, b);
        else if (type == EV_INSTRUMENT_LOAD) {
            /* Replace only this track's four-patch bank; preserve the other
             * Instrument and any notes it currently has sounding. */
            instrument_apply_engine_bank(a,&instrument_apply_snapshot,&inserts_apply_snapshot,1u);
            instrument_apply_pending = 0u;
            voice_dirty = 0u;
        } else if (type == EV_MASTER_INSERT)
            fm6_master_insert_set(a,b);
        else if (type == EV_MASTER_COMP)
            fm6_master_comp_set(a,b);
        else if (type == EV_PERFORMANCE_APPLY) {
            (void)fm6_performance_state_update(&performance_update_snapshot);
            performance_update_pending = 0u;
        } else if (type == EV_SEQ_RUN)
            fm6_sequencer_set_running(a);
        else if (type == EV_PATCH_VOICE) {
            (void)fm6_patch_set_voice(a, voice_update_snapshot);
            fm6_patch_set_pan(a, fm1lab_patch_pan(&current_instrument, a%FM1LAB_PATCH_COUNT));
            voice_update_pending = 0u;
        }
    }
}

static void voice_update_publish(void)
{
    if (!voice_dirty || voice_update_pending || !instrument_loaded) return;
    const uint8_t *voice = current_instrument.patches[set.patch].voice;
    for (uint32_t i = 0; i < FM1LAB_PATCH_BYTES; ++i)
        voice_update_snapshot[i] = voice[i];
    voice_update_patch = (uint8_t)(seq_ui_lane*FM1LAB_PATCH_COUNT+set.patch);
    voice_dirty = 0u;
    voice_update_pending = 1u;
    ui_event_push(EV_PATCH_VOICE, voice_update_patch, 0, 0);
}

static void voice_edit_changed(void)
{
    instrument_dirty = 1u;
    set_dirty = 1u;
    voice_dirty = 1u;
}

static void instrument_insert_edit(uint32_t field,uint8_t value)
{
    if(field>=FM6_INSERT_FIELD_COUNT)return;
    fm1lab_insert_state_set(&current_inserts,field,value);
    instrument_dirty=1u;set_dirty=1u;
    ui_event_push(EV_MASTER_INSERT,(uint8_t)field,fm1lab_insert_state_get(&current_inserts,field),0u);
}
static uint8_t compressor_value(uint32_t field)
{
    if(field==FM6_COMP_ENABLED)return set.performance.compressor_enabled;
    if(field==FM6_COMP_THRESHOLD)return set.performance.compressor_threshold;
    if(field==FM6_COMP_RATIO)return set.performance.compressor_ratio;
    if(field==FM6_COMP_ATTACK)return set.performance.compressor_attack;
    if(field==FM6_COMP_RELEASE)return set.performance.compressor_release;
    if(field==FM6_COMP_MAKEUP)return set.performance.compressor_makeup;
    return 0u;
}
static void compressor_edit(uint32_t field,uint8_t value)
{
    if(field>=FM6_COMP_FIELD_COUNT)return;
    if(field==FM6_COMP_ENABLED)set.performance.compressor_enabled=value?1u:0u;
    else if(field==FM6_COMP_THRESHOLD)set.performance.compressor_threshold=value>100u?100u:value;
    else if(field==FM6_COMP_RATIO)set.performance.compressor_ratio=value>100u?100u:value;
    else if(field==FM6_COMP_ATTACK)set.performance.compressor_attack=value>100u?100u:value;
    else if(field==FM6_COMP_RELEASE)set.performance.compressor_release=value>100u?100u:value;
    else set.performance.compressor_makeup=value>100u?100u:value;
    set_dirty=1u;
    ui_event_push(EV_MASTER_COMP,(uint8_t)field,compressor_value(field),0u);
}

static void voice_edit_global(fm1lab_global_field_t field, int delta)
{
    uint8_t *voice = current_instrument.patches[set.patch].voice;
    uint8_t value;
    unsigned maximum = field == FM1LAB_GLOBAL_ALGORITHM ? 32u :
        field == FM1LAB_GLOBAL_LFO_WAVE ? 5u :
        field == FM1LAB_GLOBAL_PITCH_SENS ? 7u :
        (field == FM1LAB_GLOBAL_LFO_SYNC || field == FM1LAB_GLOBAL_OSC_SYNC) ? 1u : 99u;
    unsigned minimum = field == FM1LAB_GLOBAL_ALGORITHM ? 1u : 0u;
    if (!delta || fm1lab_voice_global_get(voice, field, &value)) return;
    int next = (int)value + delta;
    if (next < (int)minimum) next = (int)minimum;
    if ((unsigned)next > maximum) next = (int)maximum;
    if ((uint8_t)next != value &&
        !fm1lab_voice_global_set(voice, field, (unsigned)next))
        voice_edit_changed();
}

static void voice_edit_operator(fm1lab_operator_field_t field, int delta)
{
    uint8_t *voice = current_instrument.patches[set.patch].voice;
    uint8_t value;
    if (!delta || fm1lab_voice_operator_get(voice, env_operator, field, &value)) return;
    unsigned maximum = field == FM1LAB_OP_MODE ? 1u :
        field == FM1LAB_OP_COARSE ? 31u :
        field == FM1LAB_OP_DETUNE ? 14u : 99u;
    int next = (int)value + delta;
    if (next < 0) next = 0;
    if ((unsigned)next > maximum) next = (int)maximum;
    if ((uint8_t)next != value &&
        !fm1lab_voice_operator_set(voice, env_operator, field, (unsigned)next))
        voice_edit_changed();
}

static void sequencer_record_capture(uint32_t key, uint8_t note, uint8_t velocity)
{
    if(!seq_record_armed || (page!=PAGE_SEQ && page!=PAGE_SEQ_OPTIONS))return;
    uint8_t lane=seq_ui_lane;
    uint8_t step=fm6_sequencer_running()?fm6_sequencer_playing_step():seq_ui_step;
    seq_ui_enabled[lane][step]=1u;seq_ui_note[lane][step]=note;seq_ui_velocity[lane][step]=velocity;
    if(!seq_ui_gate[lane][step])seq_ui_gate[lane][step]=75u;
    if(fm6_sequencer_running()) {
        seq_record_active[key]=1u;seq_record_step[key]=step;seq_record_lane[key]=lane;
        seq_record_note[key]=note;seq_record_started[key]=plat_ms();
    } else seq_ui_step=(uint8_t)((step+1u)%64u);
    performance_publish();set_dirty=1u;seq_record_redraw=1u;
}
static void sequencer_record_release(uint32_t key, uint8_t note)
{
    if(!seq_record_active[key] || seq_record_note[key]!=note)return;
    uint32_t elapsed=plat_ms()-seq_record_started[key];
    uint32_t step_ms=60000u/(transport_ui_bpm?transport_ui_bpm:120u)/4u;
    uint8_t gate=(uint8_t)(elapsed>=step_ms?100u:clamp((int32_t)(elapsed*100u/(step_ms?step_ms:1u)),10,100));
    seq_ui_gate[seq_record_lane[key]][seq_record_step[key]]=gate;
    seq_record_active[key]=0u;performance_publish();set_dirty=1u;seq_record_redraw=1u;
}
static void sequencer_record_step_advance(void)
{
    if(!seq_record_armed || !seq_record_mode ||
       (page!=PAGE_SEQ && page!=PAGE_SEQ_OPTIONS) || !fm6_sequencer_running()) {
        seq_record_last_step=255u;return;
    }
    uint8_t step=fm6_sequencer_playing_step();
    if(step==seq_record_last_step)return;
    seq_record_last_step=step;
    uint8_t lane=seq_ui_lane;
    seq_ui_enabled[lane][step]=0u;seq_ui_note[lane][step]=60u;
    seq_ui_velocity[lane][step]=100u;seq_ui_gate[lane][step]=75u;
    performance_publish();set_dirty=1u;seq_record_redraw=1u;
}
static void note_on(int32_t note, uint32_t vel)
{
    voice_note = note;
    ui_event_push(EV_NOTE_ON, (uint8_t)note, (uint8_t)vel, (uint8_t)(0x0fu << (seq_ui_lane*FM1LAB_PATCH_COUNT)));
}
static void note_off(int32_t note)
{
    ui_event_push(EV_NOTE_OFF, (uint8_t)note, 0, (uint8_t)(0x0fu << (seq_ui_lane*FM1LAB_PATCH_COUNT)));
    if (voice_note == note)
        voice_note = -1;
}

static volatile uint32_t midi_last, midi_last_port, midi_count;   /* the last message in (shown), and how many */
static void midi_in(void)                    /* audio interrupt: the packets that came in */
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24;
        if (st != 0xF8u && st != 0xFEu) {    /* (clock and active sensing would flood the screen) */
            midi_last = pkt;
            midi_last_port = (uint32_t)port;
            midi_count++;
        }
        if ((st & 0xF0u) == 0x90u && d2) {
            voice_note = (int32_t)d1;
            fm6_note_on_mask((uint8_t)d1,(uint8_t)d2,(uint8_t)(0x0fu<<(seq_ui_lane*FM1LAB_PATCH_COUNT))); /* selected Instrument only */
        } else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u) {
            fm6_note_off_mask((uint8_t)d1,(uint8_t)(0x0fu<<(seq_ui_lane*FM1LAB_PATCH_COUNT)));
            if (voice_note == (int32_t)d1)
                voice_note = -1;
        }
    }
}

/* Read-only diagnostics from fm1-base's measured audio ISR counters. These
 * deliberately do not add UI work or logging to the real-time render path. */
uint32_t app_audio_cpu_pct(void) { return plat_cpu_pct(); }
uint32_t app_audio_xruns(void) { return plat_xruns(); }
uint32_t app_audio_overload_shed_count(void) { return fm6_overload_shed_count(); }

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    ui_events_drain();
    midi_in();
    fm6_render(lr, n, master_q12);
}

void app_panic(void)
{
    voice_note = -1;
    fm6_panic();
    ui_event_tail = ui_event_head;
    ui_event_overflow = 0;
}

/* --------------------------------------------------------------- settings --- */
static void settings_defaults(void)
{
    memset(&set, 0, sizeof set);
    set.magic = SET_MAGIC;
    set.octave = 0;
    set.patch = 0;
    set.enabled_mask = 0x0fu; /* all four factory layers active */
    set.ble_off = 0;
    for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        set.levels[p] = 31u; /* safe defaults; saved user levels are loaded below */
        for (uint32_t m = 0; m < FM1LAB_MACRO_COUNT; ++m)
            set.macros[p][m] = 50u;
    }
    fm6_performance_state_get(&set.performance);
    settings_seal(&set);
}

/* Called by fm1-base after a measured audio block exceeds 90% of its
 * 5.8 ms deadline. The DSP fades the globally oldest voice over 32 samples;
 * no allocation, flash access, UI work, or cross-patch allocator mutation. */
void app_overload(void)
{
    fm6_overload();
}

void app_init(void)
{
    settings_v1_t old_v1;
    settings_v2_t old_v2;
    settings_v3_t old_v3;
    settings_v4_t old_v4;
    settings_v5_t old_v5;
    settings_v6_t old_v6;
    settings_v7_t old_v7;
    settings_v8_t old_v8;
    int performance_loaded = 0;
    int loaded;
    fm6_init();
    settings_defaults();
    loaded = plat_store_load(OBJ_SETTINGS, &set, sizeof set);
    if (loaded == (int)sizeof set && settings_valid(&set)) {
        performance_loaded = 1;
    } else {
        settings_defaults();
        if (plat_store_load(OBJ_SETTINGS, &old_v8, sizeof old_v8) == (int)sizeof old_v8 &&
            settings_import_v8(&set, &old_v8) == 0) {
            performance_loaded = 1;
            set_dirty = 1u; /* V8 -> V9 repairs the accidental ARP step-2 seed */
        } else if (plat_store_load(OBJ_SETTINGS, &old_v7, sizeof old_v7) == (int)sizeof old_v7 &&
            settings_import_v7(&set, &old_v7) == 0) {
            performance_loaded = 1;
            set_dirty = 1u;
        } else if (plat_store_load(OBJ_SETTINGS, &old_v6, sizeof old_v6) == (int)sizeof old_v6 &&
            settings_import_v6(&set, &old_v6) == 0) {
            performance_loaded = 1;
            set_dirty = 1u; /* V6 macro-step values become neutral full-length gates */
        } else if (plat_store_load(OBJ_SETTINGS, &old_v5, sizeof old_v5) == (int)sizeof old_v5 &&
            settings_import_v5(&set, &old_v5) == 0) {
            performance_loaded = 1;
            set_dirty = 1u; /* migrate previous current record; initialize arp controls */
        } else if (plat_store_load(OBJ_SETTINGS, &old_v4, sizeof old_v4) == (int)sizeof old_v4 &&
            settings_import_v4(&set, &old_v4) == 0) {
            performance_loaded = 1;
            set_dirty = 1u; /* migrate v4 settings; compressor starts bypassed */
        } else if (plat_store_load(OBJ_SETTINGS, &old_v3, sizeof old_v3) == (int)sizeof old_v3 &&
            settings_import_v3(&set, &old_v3) == 0) {
            performance_loaded = 1;
            set_dirty = 1u; /* migrate v3 three-bus performance into v4 four-bus state */
        } else if (plat_store_load(OBJ_SETTINGS, &old_v2, sizeof old_v2) == (int)sizeof old_v2 &&
                   settings_import_v2(&set, &old_v2) == 0) {
            set_dirty = 1u; /* migrate v2 settings into the CRC-protected performance record */
        } else if (plat_store_load(OBJ_SETTINGS, &old_v1, sizeof old_v1) == (int)sizeof old_v1 &&
                   settings_import_v1(&set, &old_v1) == 0) {
            set_dirty = 1u; /* migrate the original compact settings record */
        }
        settings_seal(&set);
    }
    page = PAGE_HOME;
    for(uint32_t i=0;i<16u;i++){arp_ui_gain[i]=127u;arp_ui_accent[i]=0u;arp_ui_step_gate[i]=100u;}
    instrument_factory_capture();
    seq_record_mode=(uint8_t)(set.reserved&1u);
    arp_ui_latch=(uint8_t)((set.reserved>>1)&1u);
    fm6_arpeggiator_set_latch(arp_ui_latch);
    if (performance_loaded && fm6_performance_state_set(&set.performance) == 0) {
        transport_ui_bpm = set.performance.tempo_bpm;
        arp_ui_mode=set.performance.arp_mode;
        arp_ui_division=set.performance.arp_division;arp_ui_gate=set.performance.arp_gate;
        arp_ui_octaves=set.performance.arp_octaves;arp_ui_length=set.performance.arp_length;
        arp_ui_output=set.performance.arp_output;arp_ui_random=set.performance.arp_random;
        arp_ui_macro_dest=set.performance.arp_macro_dest;arp_ui_depth=set.performance.arp_depth;
        for(uint32_t step=0;step<16u;step++){
            arp_ui_gain[step]=set.performance.arp_step_gain[step];
            arp_ui_accent[step]=set.performance.arp_step_accent[step];
            arp_ui_step_gate[step]=set.performance.arp_step_gate[step];
        }
        for (uint32_t lane = 0; lane < 2u; ++lane) {
            seq_ui_instrument[lane] = set.performance.lane_instrument[lane];
            seq_ui_patch[lane] = 0u;
            for (uint32_t step = 0; step < 64u; ++step) {
                seq_ui_enabled[lane][step] = set.performance.seq_enabled[lane][step];
                 seq_ui_accent[lane][step] = 50u;
                seq_ui_note[lane][step] = set.performance.seq_note[lane][step];
                seq_ui_velocity[lane][step] = set.performance.seq_velocity[lane][step];
                seq_ui_gate[lane][step] = set.performance.seq_gate[lane][step];
            }
        }
    } else {
        for (uint32_t lane = 0; lane < 2u; ++lane) {
            seq_ui_instrument[lane] = 0u;
            seq_ui_patch[lane] = 0u;
            for (uint32_t step = 0; step < 64u; ++step) {
                seq_ui_note[lane][step] = (uint8_t)(lane ? 67u : 48u);
                seq_ui_velocity[lane][step] = 100u;
                seq_ui_gate[lane][step] = 50u;
                seq_ui_enabled[lane][step] = (uint8_t)((step < 16u) && (step % 4u == (lane ? 2u : 0u)));
                 seq_ui_accent[lane][step] = 50u;
                /* Performance state is applied below from one coherent snapshot. */
            }
        }
        performance_publish();
    }
    for (uint32_t bank = 0; bank < FM1LAB_STORE_BANK_COUNT; ++bank) {
        int size = plat_store_size(OBJ_INSTRUMENTS0 + bank);
        fm1lab_instrument_t probe;
        /* Only a genuinely absent object may be initialized automatically.
         * A present but malformed bank is user data: leave it untouched so a
         * CRC/read failure cannot silently factory-reset eight Instruments. */
        if (size < 0) {
            (void)fm1lab_store_library_format_bank(bank, &factory_instrument);
            continue;
        }
        if (size == 3840) {
            if (fm1lab_store_library_load(bank * 8u, &probe) != 0)
                app_say("BANK ERROR", "DATA PRESERVED");
            continue;
        }
        if (bank < 3u && size == (int)sizeof(fm1lab_instrument_t)) {
            fm1lab_instrument_t legacy;
            if (plat_store_load(OBJ_INSTRUMENTS0 + bank, &legacy, sizeof legacy) ==
                    (int)sizeof legacy && fm1lab_instrument_valid(&legacy)) {
                /* V1's final voice byte was only a 63 sentinel, not pan. */
                for (uint32_t p=0; p<FM1LAB_PATCH_COUNT; ++p)
                    fm1lab_patch_set_pan(&legacy,p,64u);
                if (!fm1lab_store_library_format_bank(bank, &factory_instrument))
                    (void)fm1lab_store_library_save(bank, &legacy);
                continue;
            }
        }
        /* Unexpected non-empty object sizes are not blank storage. Preserve
         * them for recovery rather than replacing them with factory data. */
        app_say("BANK ERROR", "DATA PRESERVED");
    }
    /* Replace only untouched factory placeholders; edited Instruments survive. */
    fm1lab_seed_factory_library();
    set.patch = seq_ui_patch[seq_ui_lane];
    patch_focus = 0u;
    instrument_load(seq_ui_instrument[seq_ui_lane], 0);
    {
        fm1lab_instrument_t other;
        fm1lab_insert_state_t other_inserts;
        uint8_t other_lane=(uint8_t)(seq_ui_lane^1u);
        uint32_t other_index=seq_ui_instrument[other_lane];
        if(fm1lab_store_library_load_full(other_index,&other,&other_inserts)!=0 ||
           !fm1lab_instrument_valid(&other) || !fm1lab_insert_state_valid(&other_inserts)){
            other=factory_instrument;
            fm1lab_insert_state_init(&other_inserts);
        }
        for(uint32_t n=0;n<FM1LAB_INSTRUMENT_NAME_BYTES;n++)seq_ui_instrument_name[other_lane][n]=other.name[n];seq_ui_instrument_name[other_lane][FM1LAB_INSTRUMENT_NAME_BYTES]='\0';
        fm6_sequencer_set_lane(other_lane,FM1LAB_PATCH_COUNT);
        instrument_apply_engine_bank(other_lane,&other,&other_inserts,0u);
    }
    for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        fm6_patch_set_enabled(p, 1u);
        fm6_patch_set_level(p, set.levels[p]);
        for (uint32_t m = 0; m < FM1LAB_MACRO_COUNT; ++m)
            fm6_macro_set(p, m, set.macros[p][m]);
    }
}

/* --------------------------------------------------------------------- UI --- */
static struct {
    char msg[2][24];                     /* a short message over the header */
    uint32_t msg_until;
    uint32_t keys, btn;                  /* held at the last input */
    uint8_t vel[NKEYS], prs[NKEYS];      /* each key's last velocity, and its pressure while held */
    int32_t last_key;                    /* the key pressed last: its numbers in the header */
    uint8_t looking;                     /* Bluetooth MIDI devices: REC pressed, the scan's list up (lib/btdev) */
    uint32_t rec_t;                      /* REC went down (0: not held, or its hold already acted) */
    uint32_t arp_hold_t, seq_hold_t, delete_until;
    uint8_t arp_hold_fired, seq_hold_fired, delete_kind, delete_index;
    int32_t turns[NE];                   /* detents since boot, per encoder */
    uint32_t turned_at[NE];              /* when each was last turned: it shows lit for a moment */
    uint8_t redraw;
} ui;

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)     /* signed, no padding */
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw = 1;
    if (ann_on()) {                          /* and to a screen reader (announce.c, fm1-reader) */
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void octave_text(char *t)
{
    t[0] = set.octave < 0 ? '-' : '+';
    t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave));
    t[2] = 0;
}

/* a screen reader that has just started listening (or asks) hears the whole screen */
static void speak(void)
{
    char t[96], o[4];
    if (ann_on() && ann_where()) {
        octave_text(o);
        t[0] = 0;
        text_cat(t, APP_NAME ". OCTAVE ");
        text_cat(t, o);
        text_cat(t, ". WAVE ");
        text_cat(t, patch_display_name(set.patch));
        if (midi_count) {                    /* the last MIDI message in, as the screen shows it */
            static const char *const PORT[NPORTS] = {"USB", "BLE", "TRS"};
            static const char HEX[] = "0123456789ABCDEF";
            uint32_t m = midi_last, b;
            char h[10];
            for (b = 0; b < 3u; b++) {
                uint32_t v = (m >> (8u + 8u * b)) & 0xFFu;
                h[3 * b] = HEX[v >> 4];
                h[3 * b + 1] = HEX[v & 15u];
                h[3 * b + 2] = b < 2u ? ' ' : 0;
            }
            text_cat(t, ". MIDI IN ");
            text_cat(t, midi_last_port < NPORTS ? PORT[midi_last_port] : "");
            text_cat(t, " ");
            text_cat(t, h);
        }
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

static void note_name(int32_t note, char *b)
{
    static const char *const N[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    char o[12];
    b[0] = 0;
    text_cat(b, N[note % 12]);
    int_text(note / 12 - 1, o);
    text_cat(b, o);
}

static void octave_step(int32_t d)
{
    int32_t o = clamp(set.octave + d, -2, 2);
    char t[4];
    if (o == set.octave)
        return;
    voice_note = -1;                         /* a key held across the change would not find its note off */
    ui_event_push(EV_PANIC, 0, 0, 0);
    set.octave = (int8_t)o;
    set_dirty = 1;
    octave_text(t);
    app_say("OCTAVE", t);
}

int fm1lab_ble_off(void) { return set.ble_off; }

/* REC held a second: Bluetooth off or on (kept with the settings, so it stays so at the next power-on) */
static void ble_toggle(void)
{
    set.ble_off = (uint8_t)!set.ble_off;
    set_dirty = 1;
    plat_ble_enable(!set.ble_off);
    app_say("BLUETOOTH", set.ble_off ? "OFF" : "ON");
}

/* REC: a Bluetooth MIDI device (FM1B_BLE_CENTRAL, lib/btdev): look for devices, then connect to the one ALGORITHM
 * picks, then let it go */
static void ble_device_say(void)
{
    char name[21];
    if (btdev_found(btdev_pick(), name, 0))
        app_say("DEVICE", name);
    else
        app_say("BLE", "SCANNING");
}
static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k;
    int32_t e;
    touch_poll();                            /* the sensed keys' presses, velocities and pressure */
    keys = touch_keys();
    sequencer_record_step_advance();
    for (k = 0; k < NKEYS; k++) {            /* key 0 is F3 (MIDI 53) */
        uint32_t bit = 1u << k, v;
        int32_t note = 53 + (int32_t)k + 12 * set.octave;
        if ((keys & bit) && !(ui.keys & bit)) {
            v = touch_vel(k);
            ui.vel[k] = (uint8_t)v;
            ui.prs[k] = 0;
            ui.last_key = (int32_t)k;
            sequencer_record_capture(k,(uint8_t)note,(uint8_t)v);
            note_on(note, v);
            plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)note << 16 | v << 24, TO_USB | TO_BLE);   /* BLE: if built with it */
        } else if (!(keys & bit) && (ui.keys & bit)) {
            ui.prs[k] = 0;
            sequencer_record_release(k,(uint8_t)note);
            note_off(note);
            plat_midi_send(0x08u | 0x80u << 8 | (uint32_t)note << 16, TO_USB | TO_BLE);
        } else if ((keys & bit) && touch_pressure(k) != ui.prs[k]) {   /* pressure: poly aftertouch */
            ui.prs[k] = touch_pressure(k);
            ui.redraw = 1;
            plat_midi_send(0x0Au | 0xA0u << 8 | (uint32_t)note << 16 | (uint32_t)ui.prs[k] << 24, TO_USB | TO_BLE);
        }
    }
    for (k = 0; k < NB; k++) {               /* a button pressed: its name, and what it does here */
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        if (k == B_OCTDN)
            octave_step(-1);
        else if (k == B_OCTUP)
            octave_step(1);
        else if (k == B_REC)
            ui.rec_t = plat_ms();                /* short release arms recording; long hold retains Bluetooth toggle */
        else if (k == B_HOME) {
            page = PAGE_HOME;
            set.patch = seq_ui_patch[seq_ui_lane];
            patch_focus = 0u;
            app_say("INSTRUMENT", seq_ui_lane ? "TRACK 2 PATCH" : "TRACK 1 PATCH");
        } else if (k == B_GLO) {
            page = page == PAGE_GLOBAL ? PAGE_HOME : PAGE_GLOBAL;
            app_say(page==PAGE_GLOBAL?"GLOBAL":"HOME","SETTINGS");
        } else if (k == B_ENV) {
            if (page == PAGE_ENV) env_levels ^= 1u;
            else page = PAGE_ENV;
            app_say("ENVELOPE", env_levels ? "LEVELS" : "RATES");
        } else if (k == B_LFO) {
            page = page == PAGE_LFO ? PAGE_LFO_MOD :
                page == PAGE_LFO_MOD ? PAGE_LFO : PAGE_LFO;
            app_say("LFO", page == PAGE_LFO_MOD ? "MODULATION" : "OSCILLATOR");
        } else if (k == B_SEL) {
            if (page == PAGE_SEQ || page == PAGE_SEQ_OPTIONS) {
                page = PAGE_SEQ_PIANO;
                seq_clear_prompt=0u;seq_clear_turn=0;
                app_say("PIANO ROLL",seq_ui_lane?"TRACK 2":"TRACK 1");
            } else if (page == PAGE_SEQ_PIANO) {
                page = PAGE_SEQ;
                seq_clear_prompt=0u;seq_clear_turn=0;
                app_say("SEQUENCER","TWO LANES");
            } else if (page == PAGE_MACRO || page == PAGE_INSTRUMENT_MACRO) {
                page = PAGE_HOME;
                app_say("HOME", "ALL FOUR PATCHES");
            } else {
                page = patch_focus ? PAGE_MACRO : PAGE_INSTRUMENT_MACRO;
                app_say(page == PAGE_MACRO ? "PATCH MACROS" : "INSTRUMENT MACROS",
                        page == PAGE_MACRO ? patch_display_name(set.patch) : "FOUR-PATCH CONTROL");
            }
        } else if (k == B_EDIT) {
            if(page==PAGE_SEQ_PIANO) {
                uint8_t lane=seq_ui_lane, step=seq_ui_step;
                seq_ui_enabled[lane][step]=0u;
                seq_ui_note[lane][step]=60u;
                seq_ui_velocity[lane][step]=100u;
                seq_ui_gate[lane][step]=75u;
                seq_ui_accent[lane][step]=0u;
                performance_publish();set_dirty=1u;
                app_say("NOTE DELETED","STEP CLEARED");
            } else {
                page = page == PAGE_OPERATOR_MAP ? PAGE_MACRO : PAGE_OPERATOR_MAP;
                app_say("OPERATOR ROLES", patch_display_name(set.patch));
            }
        } else if (k == B_SEQ) {
            ui.seq_hold_t=plat_ms();ui.seq_hold_fired=0u;
        } else if (k == B_ARP) {
            ui.arp_hold_t=plat_ms();ui.arp_hold_fired=0u;
        } else if (k == B_FX) {
            if (page < PAGE_FX_AMP || page > PAGE_FX_ROUTING) page = PAGE_FX_AMP;
            else page = page == PAGE_FX_ROUTING ? PAGE_FX_AMP : (uint8_t)(page + 1u);
            app_say("FX SECTION",page==PAGE_FX_AMP?"AMP / DRIVE":page==PAGE_FX_EQ?"PARAMETRIC EQ":page==PAGE_FX_COMPRESSOR?"COMPRESSOR":page==PAGE_FX_CHORUS?"CHORUS":page==PAGE_FX_DELAY?"DELAY":page==PAGE_FX_REVERB?"REVERB":page==PAGE_FX_PHASER?"PHASER":"SEND ROUTING");
        } else if (k == B_PLAY) {
            if (page == PAGE_SEQ || page == PAGE_SEQ_OPTIONS || page == PAGE_SEQ_PIANO) {
                uint8_t starting=(uint8_t)!fm6_sequencer_running();
                ui_event_push(EV_SEQ_RUN,starting,0u,0u);
                if(starting)seq_record_last_step=255u;
                app_say("SEQUENCER", starting ? (seq_record_armed?"RECORDING":"PLAYING") : "STOPPED");
            } else {
                app_say("PLAY", "SELECT SEQUENCER");
            }
        } else if (k == B_SAVE) {
            int instrument_result = instrument_save();
            /* set.performance is the UI-owned source of truth; queued audio
             * updates must not be read back over edits that have not rendered yet. */
            settings_seal(&set);
            int settings_result = plat_store_save(OBJ_SETTINGS, &set, sizeof set);
            if (instrument_result == 0 && settings_result == 0) {
                set_dirty = 0;
                app_say("INSTRUMENT", "SAVED");
            } else {
                app_say("SAVE", "FAILED");
            }
        } else {
            app_say("BUTTON", BTN_NAME[k]);
        }
    }
    if(ui.arp_hold_t) {
        if(btn&(1u<<B_ARP)) {
            if(!ui.arp_hold_fired && (uint32_t)(plat_ms()-ui.arp_hold_t)>=3000u) {
                for(uint32_t step=0;step<16u;step++){arp_ui_gain[step]=127u;arp_ui_accent[step]=0u;arp_ui_step_gate[step]=0u;}
                arp_ui_step=0u;performance_publish();set_dirty=1u;
                fm6_arpeggiator_reset_steps();
                ui.arp_hold_fired=1u;ui.delete_kind=1u;ui.delete_until=plat_ms()+2100u;
                app_say("ARPEGGIATOR","STEPS CLEARED");
            }
        } else {
            if(!ui.arp_hold_fired) {
                if(page==PAGE_ARP)page=PAGE_ARP_STEPS;
                else if(page==PAGE_ARP_STEPS)page=PAGE_ARP_LATCH;
                else page=PAGE_ARP;
                app_say("ARPEGGIATOR",page==PAGE_ARP?"ENGINE":page==PAGE_ARP_STEPS?"STEP EDITOR":"LATCH");
            }
            ui.arp_hold_t=0u;ui.arp_hold_fired=0u;
        }
    }
    if(ui.seq_hold_t) {
        if(btn&(1u<<B_SEQ)) {
            if(!ui.seq_hold_fired && (uint32_t)(plat_ms()-ui.seq_hold_t)>=3000u) {
                uint8_t lane=seq_ui_lane;
                for(uint32_t step=0;step<64u;step++){
                    seq_ui_enabled[lane][step]=0u;seq_ui_note[lane][step]=60u;
                    seq_ui_velocity[lane][step]=100u;seq_ui_gate[lane][step]=75u;
                }
                performance_publish();set_dirty=1u;fm6_sequencer_clear_lane(lane);
                ui.seq_hold_fired=1u;ui.delete_kind=2u;ui.delete_index=(uint8_t)(lane+1u);
                ui.delete_until=plat_ms()+2100u;
                app_say("SEQUENCE CLEARED",lane?"SEQUENCE 2":"SEQUENCE 1");
            }
        } else {
            if(!ui.seq_hold_fired) {
                if(page==PAGE_SEQ)page=PAGE_SEQ_OPTIONS;
                else page=PAGE_SEQ;
                app_say("SEQUENCER",page==PAGE_SEQ_OPTIONS?"RECORD MODE":"TWO LANES");
            }
            ui.seq_hold_t=0u;ui.seq_hold_fired=0u;
        }
    }
    if (ui.rec_t) {
        if (btn >> B_REC & 1u) {
            if (FM1B_BLE && plat_ms()-ui.rec_t>=1000u) {ui.rec_t=0u;ble_toggle();}
        } else {
            uint32_t held=plat_ms()-ui.rec_t;ui.rec_t=0u;
            if(held<1000u || !FM1B_BLE) {
                seq_record_armed^=1u;seq_record_last_step=255u;
                for(uint32_t key=0;key<NKEYS;key++)seq_record_active[key]=0u;
                app_say("RECORD",seq_record_armed?"ARMED":"DISARMED");
            }
        }
    }
    for (k = 0; k < NE; k++) {               /* an encoder turned: its count; PRESET also changes the wave */
        char t[12];
        if ((e = plat_enc((int)k)) == 0)
            continue;
        ui.turns[k] += e;
        ui.turned_at[k] = plat_ms();
        if(seq_clear_prompt && !(page==PAGE_SEQ && k==EN_K2)) {
            seq_clear_prompt=0u;seq_clear_turn=0;
        }
        ui.redraw = 1;
        if (k == EN_ALGO && FM1B_BLE_CENTRAL && ui.looking) {
            btdev_choose(btdev_pick() + e);
            ble_device_say();
        } else if (k == EN_ALGO && (page == PAGE_HOME || page == PAGE_OPERATOR_MAP)) {
            /* ALGO edits the selected patch's actual FM algorithm. PRESET
             * owns Instrument selection; the two controls stay independent. */
            voice_edit_global(FM1LAB_GLOBAL_ALGORITHM, e);
            uint8_t algorithm = 0u;
            (void)fm1lab_voice_global_get(current_instrument.patches[set.patch].voice,
                FM1LAB_GLOBAL_ALGORITHM, &algorithm);
            int_text(algorithm, t);
            app_say("FM ALGORITHM", t);
        } else if (k == EN_SELECT && page == PAGE_OPERATOR_MAP) {
            int32_t next = ((int32_t)env_operator - 1 + e) % 6;
            if (next < 0) next += 6;
            env_operator = (uint8_t)(next + 1);
            app_say("OPERATOR", fm1lab_voice_operator_is_carrier(
                current_instrument.patches[set.patch].voice, env_operator) ? "CARRIER" : "MODULATOR");
        } else if (k == EN_ALGO && page == PAGE_FX_EQ) {
            uint8_t field=(uint8_t)(FM6_INSERT_EQ_LOW_FREQUENCY+eq_ui_band*4u+2u);
            uint8_t value=fm1lab_insert_state_get(&current_inserts,field);
            instrument_insert_edit(field,(uint8_t)clamp((int32_t)value+e,0,31));
            app_say("EQ Q","UPDATED");
        } else if (k == EN_ALGO && page == PAGE_ENV) {
            int32_t next = ((int32_t)env_operator - 1 + e) % 6;
            if (next < 0) next += 6;
            env_operator = (uint8_t)(next + 1);
            app_say("ENVELOPE OPERATOR", fm1lab_voice_operator_is_carrier(
                current_instrument.patches[set.patch].voice, env_operator) ? "CARRIER" : "MODULATOR");
        } else if (k == EN_SELECT && (page == PAGE_SEQ || page == PAGE_SEQ_PIANO)) {
            if(instrument_apply_pending)continue;
            if(instrument_dirty && instrument_save()!=0){app_say("SAVE FAILED","TRACK NOT CHANGED");continue;}
            seq_ui_lane = (uint8_t)fm1lab_sequencer_lane_advance(seq_ui_lane, e);
            set.patch = seq_ui_patch[seq_ui_lane];
            patch_focus = 0u;
            instrument_load(seq_ui_instrument[seq_ui_lane],1);
            performance_publish();set_dirty=1u;
            app_say("SEQUENCER TRACK", seq_ui_lane ? "T2 INSTRUMENT" : "T1 INSTRUMENT");

        } else if (k == EN_ALGO && page == PAGE_FX_ROUTING) {
            int32_t next=((int32_t)set.patch+e)%4;
            if(next<0) next+=4;
            set.patch=(uint8_t)next;
            set_dirty=1u;
            app_say("ROUTING PATCH",patch_display_name(set.patch));
        } else if (k == EN_ALGO && page == PAGE_FX_COMPRESSOR) {
            compressor_edit(FM6_COMP_MAKEUP,(uint8_t)clamp((int32_t)compressor_value(FM6_COMP_MAKEUP)+e,0,100));
            app_say("COMP MAKEUP","UPDATED");
        } else if (k == EN_SELECT && page >= PAGE_FX_AMP && page <= PAGE_FX_ROUTING) {
            int32_t next = (int32_t)(page - PAGE_FX_AMP) + e;
            next %= 8;
            if (next < 0) next += 8;
            page = (uint8_t)(PAGE_FX_AMP + next);
            app_say("FX SECTION",page==PAGE_FX_AMP?"AMP / DRIVE":page==PAGE_FX_EQ?"PARAMETRIC EQ":page==PAGE_FX_COMPRESSOR?"COMPRESSOR":page==PAGE_FX_CHORUS?"CHORUS":page==PAGE_FX_DELAY?"DELAY":page==PAGE_FX_REVERB?"REVERB":page==PAGE_FX_PHASER?"PHASER":"SEND ROUTING");
        } else if (k == EN_SELECT) {
            int32_t next = ((int32_t)set.patch + e) % (int32_t)FM1LAB_PATCH_COUNT;
            if (next < 0) next += (int32_t)FM1LAB_PATCH_COUNT;
            set.patch = (uint8_t)next;
            if (page == PAGE_HOME) {
                patch_focus = 1u;
                seq_ui_patch[seq_ui_lane] = set.patch;
                performance_publish();
            }
            set_dirty = 1;
            app_say("PATCH", patch_display_name(set.patch));
        } else if (k == EN_PRESET && page == PAGE_ARP_STEPS) {
            arp_ui_length=arp_ui_length==16u?8u:16u;
            if(arp_ui_step>=arp_ui_length)arp_ui_step=(uint8_t)(arp_ui_length-1u);
            performance_publish();set_dirty=1u;
            app_say("ARP LENGTH",arp_ui_length==8u?"8 STEPS":"16 STEPS");
        } else if (k == EN_PRESET && (page == PAGE_SEQ || page == PAGE_SEQ_PIANO)) {
            if(instrument_apply_pending)continue;
            if(instrument_dirty && instrument_save()!=0){app_say("SAVE FAILED","INSTRUMENT NOT CHANGED");continue;}
            int32_t next=((int32_t)seq_ui_instrument[seq_ui_lane]+e)%(int32_t)USER_INSTRUMENT_COUNT;
            if(next<0)next+=(int32_t)USER_INSTRUMENT_COUNT;
            patch_focus=0u;
            instrument_load((uint32_t)next,1);
            performance_publish();set_dirty=1u;
            app_say("TRACK INSTRUMENT","LOADING");
        } else if (k == EN_PRESET && page == PAGE_OPERATOR_MAP) {
            int32_t mode = ((int32_t)operator_detune_mode + e) % 2;
            if (mode < 0) mode += 2;
            operator_detune_mode = (uint8_t)mode;
            app_say("OPERATOR PITCH", operator_detune_mode ? "DETUNE" : "FINE");
        } else if (k == EN_PRESET) {
            if (page == PAGE_HOME && patch_focus) {
                uint8_t voice[FM1LAB_PATCH_BYTES];
                uint8_t *selected = current_instrument.patches[set.patch].voice;
                int32_t next = ((int32_t)fm1lab_factory_preset_for_voice(selected) + e) %
                    (int32_t)FM1LAB_FACTORY_PRESET_COUNT;
                if (next < 0) next += (int32_t)FM1LAB_FACTORY_PRESET_COUNT;
                fm1lab_factory_voice_build(voice, (uint8_t)next);
                voice[155] = selected[155]; /* keep this patch's pan */
                if (fm1lab_patch_set_voice(&current_instrument, set.patch, voice) == 0) {
                    voice_edit_changed();
                    app_say("PATCH PRESET", fm1lab_factory_names[next]);
                } else {
                    app_say("PRESET ERROR", "PATCH UNCHANGED");
                }
            } else if (instrument_apply_pending) {
                app_say("INSTRUMENT", "LOADING");
            } else {
                /* PRESET must never appear inert: preserve edits before
                 * moving to another Instrument slot. */
                if (instrument_dirty && instrument_save() != 0) {
                    app_say("SAVE FAILED", "PRESET NOT CHANGED");
                    continue;
                }
                int32_t next = ((int32_t)seq_ui_instrument[seq_ui_lane] + e) % (int32_t)FM1LAB_STORE_INSTRUMENT_COUNT;
                if (next < 0) next += (int32_t)FM1LAB_STORE_INSTRUMENT_COUNT;
                if ((uint32_t)next != instrument_index) {
                    instrument_load((uint32_t)next, 1);
                    performance_publish();set_dirty=1u;
                    int_text(next + 1, t);
                    app_say("INSTRUMENT", t);
                }
            }
        } else if (k >= EN_K1 && k <= EN_K4) {
            uint32_t index = (uint32_t)k - EN_K1;
            if (page == PAGE_GLOBAL) {
                if(index==2u) {
                    transport_ui_bpm=(uint16_t)clamp((int32_t)transport_ui_bpm+e,30,300);
                    performance_publish(); set_dirty=1u;
                    app_say("MASTER TEMPO","UPDATED");
                }
            } else if (page == PAGE_SEQ_OPTIONS) {
                if(index==0u && (e%2)!=0) {
                    seq_record_mode^=1u;set.reserved=(uint16_t)((set.reserved&2u)|seq_record_mode);set_dirty=1u;
                    app_say("LOOP RECORD",seq_record_mode?"OVERWRITE":"OVERDUB");
                }
            } else if (page == PAGE_SEQ || page == PAGE_SEQ_PIANO) {
                uint8_t lane=seq_ui_lane, step=seq_ui_step;
                if(index==0u) {
                    int32_t next=((int32_t)step+e)%64;
                    if(next<0)next+=64;
                    seq_ui_step=(uint8_t)next;
                    seq_clear_prompt=0u;
                } else if(index==1u && page==PAGE_SEQ_PIANO) {
                    seq_ui_note[lane][step]=(uint8_t)clamp((int32_t)seq_ui_note[lane][step]+e,0,127);
                    seq_ui_enabled[lane][step]=1u;
                    performance_publish();set_dirty=1u;
                    app_say("NOTE PITCH","UPDATED");
                } else if(index==1u) {
                    if(!seq_clear_prompt) {
                        seq_clear_prompt=1u;
                        seq_clear_turn=0;
                        app_say("CLEAR STEP?","TURN K2 FURTHER TO CONFIRM");
                    } else {
                        int32_t movement=e<0?-e:e;
                        seq_clear_turn=(int16_t)(seq_clear_turn+movement);
                        if(seq_clear_turn>=4) {
                            seq_ui_enabled[lane][step]=0u;
                            seq_ui_note[lane][step]=60u;
                            seq_ui_velocity[lane][step]=100u;
                            seq_ui_gate[lane][step]=75u;
                            seq_ui_accent[lane][step]=0u;
                            seq_clear_prompt=0u;seq_clear_turn=0;
                            performance_publish();set_dirty=1u;
                            app_say("STEP CLEARED","");
                        }
                    }
                } else if(index==2u) {
                    if(page==PAGE_SEQ_PIANO) {
                        seq_ui_gate[lane][step]=(uint8_t)clamp((int32_t)seq_ui_gate[lane][step]+e,10,100);
                        seq_ui_enabled[lane][step]=1u;
                        app_say("NOTE LENGTH","UPDATED");
                    } else {
                        seq_ui_velocity[lane][step]=(uint8_t)clamp((int32_t)seq_ui_velocity[lane][step]+e,0,127);
                        seq_clear_prompt=0u;
                        app_say("STEP VELOCITY","UPDATED");
                    }
                    performance_publish();set_dirty=1u;
                } else {
                    seq_ui_velocity[lane][step]=(uint8_t)clamp((int32_t)seq_ui_velocity[lane][step]+e,0,127);
                    seq_ui_enabled[lane][step]=1u;
                    seq_clear_prompt=0u;
                    performance_publish();set_dirty=1u;
                    app_say(page==PAGE_SEQ_PIANO?"NOTE VELOCITY":"STEP GATE","UPDATED");
                }
                if(index==0u)app_say("STEP","SELECTED");
                if(index==1u && seq_clear_prompt)ui.redraw=1u;
            } else if (page == PAGE_ARP) {
                if(index==0u) {
                    int32_t mode=((int32_t)arp_ui_mode+e)%5;
                    if(mode<0) mode+=5;
                    arp_ui_mode=(uint8_t)mode;
                    performance_publish(); set_dirty=1u;
                    app_say("ARP MODE",arp_ui_mode==0u?"OFF":arp_ui_mode==1u?"UP":arp_ui_mode==2u?"DOWN":arp_ui_mode==3u?"UP / DOWN":"RANDOM");
                } else if(index==1u) {
                    arp_ui_division=(uint8_t)(((int32_t)arp_ui_division+e+6)%6);
                    app_say("ARP DIVISION",arp_ui_division==0u?"1/1":arp_ui_division==1u?"1/2":arp_ui_division==2u?"1/4":arp_ui_division==3u?"1/8":arp_ui_division==4u?"1/16":"1/32");
                } else if(index==2u) {
                    arp_ui_gate=(uint8_t)clamp((int32_t)arp_ui_gate+e,10,100);
                    app_say("ARP GATE","UPDATED");
                } else {
                    arp_ui_octaves=(uint8_t)clamp((int32_t)arp_ui_octaves+e,1,4);
                    app_say("ARP OCTAVES","UPDATED");
                }
                performance_publish();set_dirty=1u;
            } else if (page == PAGE_ARP_LATCH) {
                if(index==0u && (e%2)!=0){arp_ui_latch^=1u;set.reserved=(uint16_t)((set.reserved&1u)|((uint16_t)arp_ui_latch<<1));fm6_arpeggiator_set_latch(arp_ui_latch);set_dirty=1u;app_say("ARP LATCH",arp_ui_latch?"ON":"OFF");}
            } else if (page == PAGE_ARP_STEPS) {
                uint8_t step=arp_ui_step;
                if(index==0u) { int32_t next=((int32_t)step+e)%arp_ui_length; if(next<0)next+=arp_ui_length; arp_ui_step=(uint8_t)next; }
                else if(index==1u) arp_ui_gain[step]=(uint8_t)clamp((int32_t)arp_ui_gain[step]+e,0,127);
                else if(index==2u) arp_ui_accent[step]=(uint8_t)clamp((int32_t)arp_ui_accent[step]+e,0,100);
                else if(e) arp_ui_step_gate[step]=(uint8_t)!arp_ui_step_gate[step];
                performance_publish();set_dirty=1u;
                app_say("ARP STEP", "UPDATED");
            } else if (page == PAGE_FX_ROUTING) {
                uint8_t fx=(uint8_t)index;
                set.performance.fx_send[set.patch][fx]=(uint8_t)clamp((int32_t)set.performance.fx_send[set.patch][fx]+e,0,100);
                performance_publish();
                set_dirty=1u;
                app_say("PATCH FX SEND",fx==0u?"CHORUS":fx==1u?"DELAY":fx==2u?"REVERB":"PHASER");
            } else if (page >= PAGE_FX_CHORUS && page <= PAGE_FX_PHASER) {
                uint8_t fx=(uint8_t)(page-PAGE_FX_CHORUS);
                if(index==0u) {
                    set.performance.fx_enabled[fx]=(uint8_t)!set.performance.fx_enabled[fx];
                    app_say("FX BYPASS",set.performance.fx_enabled[fx]?"ON":"OFF");
                } else if(index==1u && fx==FM6_FX_REVERB) {
                    uint8_t type=set.performance.fx_depth[2];
                    if(type>2u)type=1u;
                    int32_t next=((int32_t)type+e)%3;
                    if(next<0)next+=3;
                    set.performance.fx_depth[2]=(uint8_t)next;
                    app_say("REVERB TYPE",next==0?"ROOM":next==1?"HALL":"PLATE");
                } else if(index==1u) {
                    set.performance.fx_time[fx]=(uint8_t)clamp((int32_t)set.performance.fx_time[fx]+e,0,100);
                    app_say(fx==FM6_FX_CHORUS?"CHORUS RATE":fx==FM6_FX_DELAY?"DELAY TIME":"PHASER RATE","UPDATED");
                } else if(index==2u) {
                    set.performance.fx_mix[fx]=(uint8_t)clamp((int32_t)set.performance.fx_mix[fx]+e,0,100);
                    app_say("FX WET","UPDATED");
                } else {
                    set.performance.fx_feedback[fx]=(uint8_t)clamp((int32_t)set.performance.fx_feedback[fx]+e,0,fx==FM6_FX_CHORUS?70:90);
                    app_say(fx==FM6_FX_REVERB?"REVERB DECAY":fx==FM6_FX_PHASER?"PHASER RESONANCE":"FX FEEDBACK","UPDATED");
                }
                performance_publish();
                set_dirty=1u;
            } else if (page == PAGE_FX_AMP) {
                if(index==0u) {
                    uint8_t value=(uint8_t)!fm1lab_insert_state_get(&current_inserts,FM6_INSERT_AMP_ENABLED);
                    instrument_insert_edit(FM6_INSERT_AMP_ENABLED,value);
                    app_say("AMP BYPASS",value?"ON":"OFF");
                } else {
                    static const uint8_t fields[3]={FM6_INSERT_DRIVE,FM6_INSERT_TONE,FM6_INSERT_OUTPUT};
                    uint8_t field=fields[index-1u];
                    instrument_insert_edit(field,(uint8_t)clamp((int32_t)fm1lab_insert_state_get(&current_inserts,field)+e,0,100));
                    app_say(index==1u?"AMP DRIVE":index==2u?"AMP TONE":"AMP OUTPUT","UPDATED");
                }
            } else if (page == PAGE_FX_EQ) {
                if(index==0u) {
                    uint8_t value=(uint8_t)!fm1lab_insert_state_get(&current_inserts,FM6_INSERT_EQ_ENABLED);
                    instrument_insert_edit(FM6_INSERT_EQ_ENABLED,value);
                    app_say("EQ BYPASS",value?"ON":"OFF");
                } else if(index==1u) {
                    int32_t next=((int32_t)eq_ui_band+e)%3;if(next<0)next+=3;
                    eq_ui_band=(uint8_t)next;
                    app_say("EQ BAND",eq_ui_band==0u?"LOW":eq_ui_band==1u?"MID":"HIGH");
                } else {
                    uint8_t field=(uint8_t)(FM6_INSERT_EQ_LOW_FREQUENCY+eq_ui_band*4u+(index==2u?0u:1u));
                    uint8_t value=fm1lab_insert_state_get(&current_inserts,field);
                    instrument_insert_edit(field,(uint8_t)clamp((int32_t)value+e,0,index==2u?100:24));
                    app_say(index==2u?"EQ FREQUENCY":"EQ GAIN","UPDATED");
                }
            } else if (page == PAGE_FX_COMPRESSOR) {
                if(index==0u) {
                    uint8_t value=(uint8_t)!compressor_value(FM6_COMP_ENABLED);
                    compressor_edit(FM6_COMP_ENABLED,value);
                    app_say("COMP BYPASS",value?"ON":"OFF");
                } else {
                    static const uint8_t fields[3]={FM6_COMP_RATIO,FM6_COMP_THRESHOLD,FM6_COMP_ATTACK};
                    uint8_t field=fields[index-1u];
                    compressor_edit(field,(uint8_t)clamp((int32_t)compressor_value(field)+e,0,100));
                    app_say(index==1u?"COMP RATIO":index==2u?"COMP THRESHOLD":"COMP ATTACK","UPDATED");
                }
            } else if (page == PAGE_OPERATOR_MAP) {
                static const fm1lab_operator_field_t fields[4] = {
                    FM1LAB_OP_OUTPUT, FM1LAB_OP_MODE, FM1LAB_OP_COARSE, FM1LAB_OP_FINE
                };
                voice_edit_operator(index == 3u && operator_detune_mode ?
                    FM1LAB_OP_DETUNE : fields[index], e);
                app_say(index == 0u ? "OPERATOR OUTPUT" : index == 1u ? "OSCILLATOR MODE" :
                    index == 2u ? "COARSE FREQUENCY" : operator_detune_mode ? "OPERATOR DETUNE" : "FINE FREQUENCY", "UPDATED");
            } else if (page == PAGE_ENV) {
                voice_edit_operator((fm1lab_operator_field_t)((env_levels ? FM1LAB_OP_LEVEL_1 : FM1LAB_OP_RATE_1) + index), e);
                app_say(env_levels ? "ENVELOPE LEVEL" : "ENVELOPE RATE", "UPDATED");
            } else if (page == PAGE_LFO) {
                static const fm1lab_global_field_t fields[4] = {
                    FM1LAB_GLOBAL_LFO_WAVE, FM1LAB_GLOBAL_LFO_SPEED,
                    FM1LAB_GLOBAL_LFO_PITCH_DEPTH, FM1LAB_GLOBAL_LFO_AMP_DEPTH
                };
                voice_edit_global(fields[index], e);
                app_say("LFO", "UPDATED");
            } else if (page == PAGE_LFO_MOD) {
                static const fm1lab_global_field_t fields[4] = {
                    FM1LAB_GLOBAL_LFO_DELAY, FM1LAB_GLOBAL_PITCH_SENS,
                    FM1LAB_GLOBAL_LFO_SYNC, FM1LAB_GLOBAL_OSC_SYNC
                };
                voice_edit_global(fields[index], e);
                app_say("LFO MODULATION", "UPDATED");
            } else if (page == PAGE_INSTRUMENT_MACRO) {
                int32_t value = (int32_t)current_inserts.instrument_macros[index] + e;
                if (value < 0) value = 0;
                if (value > 100) value = 100;
                current_inserts.instrument_macros[index] = (uint8_t)value;
                instrument_dirty = 1u;
                for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p)
                    ui_event_push(EV_MACRO_SET, (uint8_t)(seq_ui_lane*FM1LAB_PATCH_COUNT+p), (uint8_t)index,
                                  instrument_macro_effective(p, index));
                set_dirty = 1u;
                app_say("INSTRUMENT MACRO", "UPDATED");
            } else if (page == PAGE_MACRO) {
                int32_t value = (int32_t)set.macros[set.patch][index] + e;
                if (value < 0) value = 0;
                if (value > 100) value = 100;
                set.macros[set.patch][index] = (uint8_t)value;
                current_instrument.patches[set.patch].macros[index] = (uint8_t)value;
                instrument_dirty = 1u;
                ui_event_push(EV_MACRO_SET, (uint8_t)(seq_ui_lane*FM1LAB_PATCH_COUNT+set.patch), (uint8_t)index, (uint8_t)value);
                set_dirty = 1;
                app_say("MACRO", "UPDATED");
            } else if (page == PAGE_HOME) {
                /* Selecting a layer enters patch focus; this encoder also
                 * continues to adjust that layer's level. */
                patch_focus = 1u;
                set.patch = (uint8_t)index;
                seq_ui_patch[seq_ui_lane] = set.patch;
                performance_publish();
                int32_t value = (int32_t)set.levels[index] + e;
                if (value < 0) value = 0;
                if (value > 127) value = 127;
                set.levels[index] = (uint8_t)value;
                current_instrument.patches[index].level = (uint8_t)value;
                instrument_dirty = 1u;
                ui_event_push(EV_PATCH_LEVEL, (uint8_t)(seq_ui_lane*FM1LAB_PATCH_COUNT+index), (uint8_t)value, 0);
                set_dirty = 1;
                app_say("PATCH", patch_display_name(index));
            } else {
                int_text(ui.turns[k], t);
                app_say(ENC_NAME[k], t);
            }
        } else {
            int_text(ui.turns[k], t);
            app_say(ENC_NAME[k], t);
        }
    }
    voice_update_publish();
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    plat_glow(1);                            /* the unlit buttons glow, readable in the dark */
    plat_leds(btn | (set_dirty ? 1u << B_SAVE : 0u) |
              (seq_record_armed ? 1u << B_REC : 0u), keys);
}

void app_input_only(void) { input(); }

static void cell(int32_t x, int32_t y, int32_t w, const char *s, int on)   /* a labelled box, lit when on */
{
    cv_rect(x, y, w, 18, on ? C_HI : C_LINE);
    cv_text(x + (w - text_w(&FONT_XS, s)) / 2, y + 2, &FONT_XS, s, on ? C_BLACK : C_GRAY);
}

static void key_cell(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t k, uint16_t off)
{
    int32_t v, p;
    cv_rect(x, y, w, h, off);
    if (!(ui.keys >> k & 1u))
        return;
    v = (ui.vel[k] * h + 126) / 127;
    p = (ui.prs[k] * h + 126) / 127;
    cv_rect(x, y + h - v, w, v, C_HI);
    if (p)
        cv_rect(x + 2, y + h - p, w - 4, p, C_WHITE);
}

static void outline(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color)
{
    cv_rect(x, y, w, 1, color);
    cv_rect(x, y + h - 1, w, 1, color);
    cv_rect(x, y, 1, h, color);
    cv_rect(x + w - 1, y, 1, h, color);
}

static void rounded_panel(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t fill, uint16_t border)
{
    /* FM1's display is a tiny instrument panel, not a miniature phone UI:
     * clipped corners, a quiet inset, and one precise highlight line. */
    (void)r;
    cv_rect(x + 2, y, w - 4, h, border);
    cv_rect(x, y + 2, 1, h - 4, border);
    cv_rect(x + w - 1, y + 2, 1, h - 4, border);
    cv_rect(x + 1, y + 1, w - 2, h - 2, fill);
    cv_rect(x + 3, y + 2, w - 6, 1, RGB(31, 47, 59));
    cv_rect(x + 2, y + h - 2, w - 4, 1, RGB(5, 9, 14));
}
static void instrument_card(int32_t x, int32_t y, int32_t w, int32_t h,
                            uint16_t fill, uint16_t border, uint16_t accent)
{
    rounded_panel(x, y, w, h, 3, fill, border);
    /* A narrow color rail gives every editor the same instrument-panel identity
     * without spending scarce pixels on extra labels or decorative chrome. */
    cv_rect(x + 3, y + 6, 2, h - 12, accent);
    cv_rect(x + 7, y + 2, w - 14, 1, RGB(31, 47, 59));
}

/* Shared 2-column panel layout. Individual pages supply content; these
 * helpers own the common card grid so spacing and sizing stay consistent. */
enum { UI_GRID_X = 2, UI_GRID_COL_STEP = 120, UI_GRID_TOP = 7,
       UI_GRID_ROW_STEP = 85, UI_GRID_CARD_W = 116, UI_GRID_CARD_H = 85 };

static int32_t panel_grid_x(uint32_t index)
{
    return UI_GRID_X + (int32_t)(index & 1u) * UI_GRID_COL_STEP;
}

static int32_t panel_grid_y(uint32_t index, int32_t top, int32_t row_step)
{
    return top + (int32_t)(index >> 1u) * row_step;
}

static void panel_grid_card(uint32_t index, int32_t top, int32_t row_step,
                            uint16_t accent)
{
    instrument_card(panel_grid_x(index), panel_grid_y(index, top, row_step),
                    UI_GRID_CARD_W, UI_GRID_CARD_H, panel_fill(), accent, accent);
}

static uint16_t patch_color(uint32_t p)
{
    static const uint16_t colors[4] = {RGB(36,215,232), RGB(227,75,210), RGB(242,212,59), RGB(255,82,101)};
    return colors[p & 3u];
}
static uint16_t panel_fill(void) { return RGB(17,29,39); }
static uint16_t screen_bg(void) { return RGB(8,13,19); }

static int algorithm_preview_active(void)
{
    return page == PAGE_HOME && plat_ms() < ui.msg_until &&
        ui.msg[0][0]=='F' && ui.msg[0][1]=='M' && ui.msg[0][2]==' ' &&
        ui.msg[0][3]=='A' && ui.msg[0][4]=='L';
}

/* M-VAVE-style six-operator topology preview. Connections come from the
 * selected voice's actual algorithm routing table; carrier nodes are filled. */
static void draw_algorithm_diagram(void)
{
    static const int16_t nx[6]={20,102,184,20,102,184};
    static const int16_t ny[6]={39,39,39,89,89,89};
    const uint8_t *voice=current_instrument.patches[set.patch].voice;
    uint8_t algorithm=1u;
    uint16_t accent=patch_color(set.patch);
    char t[12];
    (void)fm1lab_voice_global_get(voice,FM1LAB_GLOBAL_ALGORITHM,&algorithm);
    cv_text(7,5,&FONT_XS,"OPERATOR ROUTING",C_GRAY);
    cv_text(178,5,&FONT_XS,"ALG",C_GRAY);
    int_text(algorithm,t); cv_text(205,5,&FONT_XS,t,accent);

    /* Draw modulation links behind the operator blocks. */
    for (unsigned source=1;source<=6u;++source) {
        for (unsigned target=1;target<=6u;++target) {
            if (source==target ||
                fm1lab_voice_operator_modulates(voice,source,target)<=0) continue;
            unsigned si=source-1u, ti=target-1u;
            int32_t sx=nx[si]+18, sy=ny[si]+13;
            int32_t tx=nx[ti]+18, ty=ny[ti]+13;
            uint16_t ink=RGB(88,112,132);
            if (sy==ty) {
                int32_t source_edge=sx<tx?sx+18:sx-18;
                int32_t target_edge=sx<tx?tx-18:tx+18;
                int32_t left=source_edge<target_edge?source_edge:target_edge;
                int32_t right=source_edge<target_edge?target_edge:source_edge;
                cv_rect(left,sy-1,right-left+1,2,ink);
                cv_rect(target_edge-2,ty-3,2,6,accent);
            } else {
                int32_t source_edge=sy<ty?sy+13:sy-13;
                int32_t target_edge=sy<ty?ty-13:ty+13;
                int32_t mid_y=(source_edge+target_edge)/2;
                int32_t left=sx<tx?sx:tx, right=sx<tx?tx:sx;
                int32_t top=source_edge<mid_y?source_edge:mid_y;
                int32_t bottom=source_edge<mid_y?mid_y:source_edge;
                cv_rect(sx-1,top,2,bottom-top+1,ink);
                cv_rect(left,mid_y-1,right-left+1,2,ink);
                top=target_edge<mid_y?target_edge:mid_y;
                bottom=target_edge<mid_y?mid_y:target_edge;
                cv_rect(tx-1,top,2,bottom-top+1,ink);
                cv_rect(tx-3,target_edge-2,6,2,accent);
            }
        }
    }
    for (unsigned op=1;op<=6u;++op) {
        unsigned i=op-1u;
        int carrier=fm1lab_voice_operator_is_carrier(voice,op)>0;
        uint16_t edge=carrier?accent:C_LINE;
        uint16_t fill=carrier?RGB(28,57,67):panel_fill();
        rounded_panel(nx[i],ny[i],36,26,3,fill,edge);
        cv_text(nx[i]+7,ny[i]+7,&FONT_B,op==1?"OP1":op==2?"OP2":op==3?"OP3":op==4?"OP4":op==5?"OP5":"OP6",
            carrier?accent:C_WHITE);
        cv_rect(nx[i]+3,ny[i]+22,30,1,carrier?accent:C_DIM);
    }
    cv_text(8,126,&FONT_XS,"CARRIER",accent);
    cv_text(75,126,&FONT_XS,"MODULATOR",C_GRAY);
    cv_text(166,126,&FONT_XS,"ALGO KNOB",C_GRAY);
}

static int patch_name_is_factory(const char *name)
{
    if (!name) return 0;
    return (name[0]=='f' && name[1]=='a' && name[2]=='c' && name[3]=='t' &&
            name[4]=='o' && name[5]=='r' && name[6]=='y' && name[7]=='\\0') ||
           (name[0]=='F' && name[1]=='A' && name[2]=='C' && name[3]=='T' &&
            name[4]=='O' && name[5]=='R' && name[6]=='Y' && name[7]=='\\0') ||
           (name[0]=='F' && name[1]=='a' && name[2]=='c' && name[3]=='t' &&
            name[4]=='o' && name[5]=='r' && name[6]=='y' && name[7]=='\\0');
}

static const char *instrument_display_name(void)
{
    static char name[FM1LAB_INSTRUMENT_NAME_BYTES + 1u];
    uint32_t i=0u; while(i<FM1LAB_INSTRUMENT_NAME_BYTES && current_instrument.name[i]) { name[i]=current_instrument.name[i]; ++i; }
    while(i && name[i-1u]==' ') --i;
    if(!i) { name[0]='I';name[1]='N';name[2]='S';name[3]='T';name[4]='R';name[5]='U';name[6]='M';name[7]='E';name[8]='N';name[9]='T';i=10u; }
    name[i]='\\0'; return name;
}

static const char *patch_display_name(uint32_t patch)
{
    const char *name = fm6_patch_name(patch);
    static char fallback[FM1LAB_PATCH_COUNT][12];
    if (name && name[0] && !patch_name_is_factory(name))
        return name;
    if (patch >= FM1LAB_PATCH_COUNT) patch = 0u;
    fallback[patch][0] = 'P'; fallback[patch][1] = 'A';
    fallback[patch][2] = 'T'; fallback[patch][3] = 'C';
    fallback[patch][4] = 'H'; fallback[patch][5] = ' ';
    int_text((int32_t)patch + 1, fallback[patch] + 6);
    return fallback[patch];
}

static void draw_home_panel(void)
{
    char t[24];
    if (algorithm_preview_active()) {
        draw_algorithm_diagram();
        return;
    }
    for (uint32_t p = 0; p < 4u; ++p) {
        int32_t x = 2 + (int32_t)(p & 1u) * 120;
        int32_t y = 7 + (int32_t)(p >> 1u) * 85;
        uint16_t accent = patch_color(p);
        int enabled = set.levels[p] != 0u;
        rounded_panel(x, y, 116, 85, 3, panel_fill(),
                      (!patch_focus || p == set.patch) ? accent : C_LINE);
        cv_rect(x + 5, y + 7, 2, 70, accent);
        t[0] = 'P'; t[1] = 'A'; t[2] = 'T'; t[3] = 'C'; t[4] = 'H'; t[5] = ' ';
        int_text((int32_t)p + 1, t + 6);
        cv_text(x + 12, y + 5, &FONT_XS, t, accent);
        cv_text(x + 65, y + 5, &FONT_XS, enabled ? "ON" : "MUTED",
                enabled ? RGB(75,235,135) : C_GRAY);
        cv_text(x + 12, y + 23, &FONT_B, patch_display_name(p), C_WHITE);
        cv_text(x + 12, y + 49, &FONT_XS, "ALG", C_GRAY);
        int_text((int32_t)fm6_patch_algorithm(p) + 1, t);
        cv_text(x + 34, y + 49, &FONT_XS, t, accent);
        cv_text(x + 59, y + 49, &FONT_XS, "LEVEL", C_GRAY);
        int_text(set.levels[p], t);
        cv_text(x + 91, y + 49, &FONT_XS, t, C_WHITE);
        cv_rect(x + 12, y + 69, 96, 5, RGB(31, 43, 55));
        cv_rect(x + 12, y + 69, (int32_t)set.levels[p] * 96 / 127, 5, accent);
        if (p == set.patch) cv_rect(x + 12, y + 80, 96, 1, accent);
    }
}
static void draw_macro_panel(void)
{
    static const char *const names[4] = {"BRIGHT", "BODY", "MOTION", "EDGE"};
    uint32_t now = plat_ms();
    uint16_t accent = patch_color(set.patch);
    char t[24];
    for (uint32_t m = 0; m < 4u; ++m) {
        int32_t x = panel_grid_x(m);
        int32_t y = panel_grid_y(m, UI_GRID_TOP, UI_GRID_ROW_STEP);
        uint32_t value = set.macros[set.patch][m];
        uint16_t border = ui.turned_at[EN_K1 + m] && now - ui.turned_at[EN_K1 + m] < 650u ? accent : C_LINE;
        instrument_card(x, y, UI_GRID_CARD_W, UI_GRID_CARD_H, panel_fill(), border, accent);
        cv_text(x + 9, y + 9, &FONT_XS, names[m], C_WHITE);
        int_text((int32_t)value, t);
        cv_text(x + 88, y + 7, &FONT_B, t, accent);
        cv_rect(x + 9, y + 42, 98, 6, RGB(31, 43, 55));
        cv_rect(x + 9, y + 42, (int32_t)value * 98 / 100, 6, accent);
        cv_rect(x + 9 + (int32_t)value * 98 / 100, y + 39, 2, 12, C_WHITE);
    }
}
static void draw_instrument_macro_panel(void)
{
    static const char *const names[4] = {"BRIGHT", "BODY", "MOTION", "EDGE"};
    const uint16_t accent = RGB(150,120,255);
    const uint32_t now = plat_ms();
    char t[24];
    for (uint32_t m = 0; m < 4u; ++m) {
        int32_t x = panel_grid_x(m);
        int32_t y = panel_grid_y(m, UI_GRID_TOP, UI_GRID_ROW_STEP);
        uint32_t value = current_inserts.instrument_macros[m];
        uint16_t border = ui.turned_at[EN_K1 + m] && now - ui.turned_at[EN_K1 + m] < 650u ? accent : C_LINE;
        instrument_card(x, y, UI_GRID_CARD_W, UI_GRID_CARD_H, panel_fill(), border, accent);
        cv_text(x + 9, y + 9, &FONT_XS, names[m], C_WHITE);
        int_text((int32_t)value, t);
        cv_text(x + 88, y + 7, &FONT_B, t, accent);
        cv_rect(x + 9, y + 42, 98, 6, RGB(31,43,55));
        cv_rect(x + 9 + 49, y + 40, 1, 10, C_GRAY);
        if (value >= 50u) cv_rect(x + 9 + 49, y + 42, (int32_t)(value - 50u) * 49 / 50, 6, accent);
        else cv_rect(x + 9 + (int32_t)value * 49 / 50, y + 42, (int32_t)(50u - value) * 49 / 50, 6, accent);
    }
}
/* Four operator quadrants; roles come from the selected algorithm's actual data. */
static void draw_operator_map(void)
{
    static const char *const labels[4] = {"OUTPUT", "MODE", "COARSE", "FINE"};
    const fm1lab_operator_field_t fields[4] = {
        FM1LAB_OP_OUTPUT, FM1LAB_OP_MODE, FM1LAB_OP_COARSE, FM1LAB_OP_FINE
    };
    uint8_t maxima[4] = {99u, 1u, 31u, operator_detune_mode ? 14u : 99u};
    const uint8_t *voice = current_instrument.patches[set.patch].voice;
    uint8_t values[4] = {0u, 0u, 0u, 0u};
    char t[16];
    uint16_t accent = patch_color(set.patch);
    for (uint32_t i = 0; i < 4u; ++i) {
        int32_t x = panel_grid_x(i);
        int32_t y = panel_grid_y(i, UI_GRID_TOP, UI_GRID_ROW_STEP);
        (void)fm1lab_voice_operator_get(voice, env_operator,
            i == 3u && operator_detune_mode ? FM1LAB_OP_DETUNE : fields[i], &values[i]);
        panel_grid_card(i, UI_GRID_TOP, UI_GRID_ROW_STEP, accent);
        cv_text(x + 9, y + 8, &FONT_XS,
            i == 3u && operator_detune_mode ? "DETUNE" : labels[i], C_WHITE);
        if (i == 1u) {
            cv_text(x + 9, y + 27, &FONT_B, values[i] ? "FIXED" : "RATIO", accent);
        } else {
            int_text(values[i], t);
            cv_text(x + 9, y + 27, &FONT_B, t, accent);
            cv_rect(x + 9, y + 46, 94, 5, C_LINE);
            cv_rect(x + 9, y + 46, (int32_t)values[i] * 94 / maxima[i], 5, accent);
        }
    }
}


static void draw_amp_panel(void)
{
    static const char *const labels[4]={"K1  BYPASS","K2  DRIVE","K3  TONE","K4  OUTPUT"};
    const fm1lab_insert_state_t *v=&current_inserts;
    char t[16];uint16_t accent=RGB(36,215,232);
    uint8_t values[4]={v->amp_enabled,v->drive,v->tone,v->output};
    for(uint32_t i=0;i<4u;i++){
        int32_t x=panel_grid_x(i),y=panel_grid_y(i,UI_GRID_TOP,UI_GRID_ROW_STEP);
        panel_grid_card(i,UI_GRID_TOP,UI_GRID_ROW_STEP,accent);
        cv_text(x+7,y+6,&FONT_XS,labels[i],C_GRAY);
        if(i==0u)cv_text(x+7,y+23,&FONT_B,values[i]?"ON":"OFF",values[i]?RGB(75,235,135):RGB(255,82,101));
        else {int_text(values[i],t);cv_text(x+7,y+23,&FONT_B,t,accent);cv_rect(x+48,y+29,55,5,C_LINE);cv_rect(x+48,y+29,(int32_t)values[i]*55/100,5,accent);}
    }
}
static uint32_t eq_frequency_hz(uint8_t index)
{
    static const uint16_t anchors[11]={20,40,80,160,320,630,1260,2520,5000,10000,20000};
    uint32_t step=index/10u,rem=index%10u;
    if(step>=10u)return 20000u;
    return anchors[step]+((uint32_t)(anchors[step+1u]-anchors[step])*rem)/10u;
}
static void draw_eq_panel(void)
{
    static const char *const bands[3]={"LOW","MID","HIGH"};
    const fm1lab_insert_state_t *v=&current_inserts;
    const fm1lab_eq_band_state_t *b=&v->eq[eq_ui_band];
    char t[16];uint16_t accent=RGB(36,215,232);
    for(uint32_t i=0;i<4u;i++){
        int32_t x=panel_grid_x(i),y=panel_grid_y(i,UI_GRID_TOP,UI_GRID_ROW_STEP);
        panel_grid_card(i,UI_GRID_TOP,UI_GRID_ROW_STEP,accent);
        cv_text(x+7,y+6,&FONT_XS,i==0u?"K1  BYPASS":i==1u?"K2  BAND":i==2u?"K3  FREQUENCY":"K4  GAIN",C_GRAY);
        if(i==0u) cv_text(x+7,y+23,&FONT_B,(v->eq_enabled&1u)?"ON":"OFF",(v->eq_enabled&1u)?RGB(75,235,135):RGB(255,82,101));
        else if(i==1u) cv_text(x+7,y+23,&FONT_B,bands[eq_ui_band],accent);
        else if(i==2u) {int_text((int32_t)eq_frequency_hz(b->frequency),t);text_cat(t,"Hz");cv_text(x+7,y+23,&FONT_B,t,accent);}
        else {int_text((int32_t)b->gain-12,t);text_cat(t,"dB");cv_text(x+7,y+23,&FONT_B,t,accent);}
    }
}
static void draw_compressor_panel(void)
{
    uint8_t enabled=compressor_value(FM6_COMP_ENABLED);
    uint8_t values[4]={enabled,compressor_value(FM6_COMP_RATIO),
        compressor_value(FM6_COMP_THRESHOLD),compressor_value(FM6_COMP_ATTACK)};
    char t[16];uint16_t accent=RGB(36,215,232);
    for(uint32_t i=0;i<4u;i++){
        int32_t x=panel_grid_x(i),y=panel_grid_y(i,UI_GRID_TOP,UI_GRID_ROW_STEP);
        panel_grid_card(i,UI_GRID_TOP,UI_GRID_ROW_STEP,accent);
        cv_text(x+7,y+6,&FONT_XS,i==0u?"K1  BYPASS":i==1u?"K2  RATIO":i==2u?"K3  THRESHOLD":"K4  ATTACK",C_GRAY);
        if(i==0u) cv_text(x+7,y+23,&FONT_B,enabled?"ON":"OFF",enabled?RGB(75,235,135):RGB(255,82,101));
        else {
            if(i==1u){uint32_t ratio=1u+values[i]*11u/100u;int_text((int32_t)ratio,t);text_cat(t,":1");}
            else if(i==3u){uint32_t ms=1u+(uint32_t)values[i]*49u/100u;int_text((int32_t)ms,t);text_cat(t,"ms");}
            else int_text(values[i],t);
            cv_text(x+7,y+23,&FONT_B,t,accent);
        }
    }
}

static void draw_global_panel(uint32_t pct)
{
    char t[24];
    const char *labels[4] = {"MASTER", "OCTAVE", "TEMPO", "MIDI / BLE"};
    uint16_t colors[4] = {RGB(36,215,232), RGB(227,75,210), RGB(242,212,59), RGB(255,82,101)};
    for (uint32_t i = 0; i < 4u; ++i) {
        int32_t x = panel_grid_x(i);
        int32_t y = panel_grid_y(i, UI_GRID_TOP, UI_GRID_ROW_STEP);
        instrument_card(x, y, UI_GRID_CARD_W, UI_GRID_CARD_H, panel_fill(), colors[i], colors[i]);
        cv_text(x + 9, y + 8, &FONT_XS, labels[i], C_GRAY);
        if (i == 0) {
            int_text((int32_t)pct, t); text_cat(t, "%");
            cv_text(x + 9, y + 27, &FONT_B, t, colors[i]);
        } else if (i == 1) {
            octave_text(t);
            cv_text(x + 9, y + 27, &FONT_B, t, colors[i]);
        } else if (i == 2) {
            int_text((int32_t)transport_ui_bpm, t); text_cat(t, " BPM");
            cv_text(x + 9, y + 27, &FONT_B, t, colors[i]);
        } else {
            int_text((int32_t)midi_count, t);
            cv_text(x + 9, y + 27, &FONT_B, t, colors[i]);
        }
    }
}

static void draw_envelope_panel(void)
{
    static const char *const rates[4] = {"RATE 1", "RATE 2", "RATE 3", "RATE 4"};
    static const char *const levels[4] = {"LEVEL 1", "LEVEL 2", "LEVEL 3", "LEVEL 4"};
    char t[16];
    const uint8_t *voice = current_instrument.patches[set.patch].voice;
    uint16_t accent = patch_color(set.patch);
    for (uint32_t i = 0; i < 4u; ++i) {
        int32_t x = 4 + (int32_t)(i & 1u) * 118;
        int32_t y = panel_grid_y(i, UI_GRID_TOP, UI_GRID_ROW_STEP);
        uint8_t value = 0;
        (void)fm1lab_voice_operator_get(voice, env_operator,
            (fm1lab_operator_field_t)((env_levels ? FM1LAB_OP_LEVEL_1 : FM1LAB_OP_RATE_1) + i), &value);
        instrument_card(x, y, UI_GRID_CARD_W, UI_GRID_CARD_H, panel_fill(), accent, accent);
        cv_text(x + 9, y + 10, &FONT_XS, env_levels ? levels[i] : rates[i], C_WHITE);
        int_text(value, t); cv_text(x + 88, y + 10, &FONT_XS, t, accent);
        cv_rect(x + 9, y + 39, 98, 6, C_LINE);
        cv_rect(x + 9, y + 39, (int32_t)value * 98 / 99, 6, accent);
        cv_text(x + 9, y + 64, &FONT_XS, env_levels ? "0=OFF  99=MAX" : "0=SLOW  99=FAST", C_GRAY);
    }
}

static void draw_lfo_panel(uint32_t modulation)
{
    static const char *const labels[2][4] = {
        {"WAVEFORM", "SPEED", "PITCH DEPTH", "AMP DEPTH"},
        {"DELAY", "PITCH SENS", "LFO KEY SYNC", "OSC KEY SYNC"}
    };
    static const char *const waves[6] = {"TRIANGLE", "SAW DOWN", "SAW UP", "SQUARE", "SINE", "S/HOLD"};
    static const fm1lab_global_field_t fields[2][4] = {
        {FM1LAB_GLOBAL_LFO_WAVE, FM1LAB_GLOBAL_LFO_SPEED, FM1LAB_GLOBAL_LFO_PITCH_DEPTH, FM1LAB_GLOBAL_LFO_AMP_DEPTH},
        {FM1LAB_GLOBAL_LFO_DELAY, FM1LAB_GLOBAL_PITCH_SENS, FM1LAB_GLOBAL_LFO_SYNC, FM1LAB_GLOBAL_OSC_SYNC}
    };
    static const uint8_t maxima[2][4] = {{5,99,99,99},{99,7,1,1}};
    char t[20];
    const uint8_t *voice = current_instrument.patches[set.patch].voice;
    uint16_t accent = patch_color(set.patch);
    for (uint32_t i = 0; i < 4u; ++i) {
        int32_t x = panel_grid_x(i);
        int32_t y = panel_grid_y(i, UI_GRID_TOP, UI_GRID_ROW_STEP);
        uint8_t value = 0;
        (void)fm1lab_voice_global_get(voice, fields[modulation][i], &value);
        instrument_card(x, y, UI_GRID_CARD_W, UI_GRID_CARD_H, panel_fill(), accent, accent);
        cv_text(x + 9, y + 8, &FONT_XS, labels[modulation][i], C_WHITE);
        if (!modulation && i == 0u) {
            cv_text(x + 9, y + 26, &FONT_B, waves[value % 6u], accent);
        } else {
            if (modulation && i >= 2u) cv_text(x + 9, y + 26, &FONT_B, value ? "ON" : "OFF", accent);
            else {
                int_text(value, t); cv_text(x + 9, y + 26, &FONT_B, t, accent);
                cv_rect(x + 9, y + 46, 94, 5, C_LINE);
                cv_rect(x + 9, y + 46, (int32_t)value * 94 / maxima[modulation][i], 5, accent);
            }
        }
    }
}

static void draw_step_columns(uint8_t lane, uint8_t first, uint8_t count,
                               int32_t top, int32_t height, uint8_t selected,
                               uint8_t playing, const uint8_t *velocity,
                               const uint8_t *enabled, const uint8_t *accent_values,
                               uint16_t accent)
{
    int32_t gap=2, width=(224-(int32_t)(count-1u)*gap)/(int32_t)count;
    for(uint32_t i=0;i<count;i++) {
        uint8_t step=(uint8_t)(first+i), active=enabled?enabled[step]:1u;
        uint8_t value=velocity?velocity[step]:100u;
        int32_t x=8+(int32_t)i*(width+gap);
        int32_t bar_h=(int32_t)value*(height-18)/127;
        uint8_t accent_value=accent_values?accent_values[step]:50u;
        int32_t bar_w=active?(width/2+(int32_t)accent_value*(width/2-2)/100):width/2;
        int32_t bx=x+(width-bar_w)/2;
        uint16_t color=active?accent:C_LINE;
        cv_rect(x,top,width,height,C_LINE);
        if(bar_h<3 && active)bar_h=3;
        if(active) cv_rect(bx,top+height-4-bar_h,bar_w,bar_h,color);
        if(step==selected) rounded_panel(x-1,top-2,width+2,height+4,3,0u,RGB(255,255,255));
        if(step==playing) rounded_panel(x-2,top-3,width+4,height+6,3,0u,RGB(242,212,59));
    }
    (void)lane;
}
static void draw_sequencer_panel(void)
{
    uint8_t playing=fm6_sequencer_playing_step();
    const uint8_t count=16u;
    cv_text(6,0,&FONT_XS,"TRACK 1",patch_color(seq_ui_patch[0]));
    cv_text(78,0,&FONT_XS,seq_ui_instrument_name[0],C_WHITE);
    draw_step_columns(0u,0u,count,20,48,seq_ui_lane==0u?seq_ui_step:255u,
                      fm6_sequencer_running()?playing:255u,seq_ui_velocity[0],
                      seq_ui_enabled[0],seq_ui_accent[0],patch_color(seq_ui_patch[0]));
    cv_text(6,77,&FONT_XS,"TRACK 2",patch_color(seq_ui_patch[1]));
    cv_text(78,77,&FONT_XS,seq_ui_instrument_name[1],C_WHITE);
    draw_step_columns(1u,0u,count,94,48,seq_ui_lane==1u?seq_ui_step:255u,
                      fm6_sequencer_running()?playing:255u,seq_ui_velocity[1],
                      seq_ui_enabled[1],seq_ui_accent[1],patch_color(seq_ui_patch[1]));
    cv_text(6,151,&FONT_XS,fm6_sequencer_running()?(seq_record_armed?"LIVE RECORD":"PLAYING"):(seq_record_armed?"RECORD ARMED":"STOPPED"),
            fm6_sequencer_running()?RGB(75,235,135):(seq_record_armed?RGB(255,80,90):C_GRAY));
    cv_text(82,151,&FONT_XS,"K1 STEP  K2 CLEAR  K3 VEL  K4 GATE",C_GRAY);
}
static void draw_sequencer_piano_panel(void)
{
    char value[20];
    const uint8_t lane=seq_ui_lane, first=(uint8_t)((seq_ui_step/16u)*16u);
    const uint8_t selected=(uint8_t)(seq_ui_step-first);
    const uint16_t accent=patch_color(seq_ui_patch[lane]);
    const int32_t gx=24, gy=32, gw=208, gh=108, cw=13, rh=4;
    int32_t low=(int32_t)seq_ui_note[lane][seq_ui_step]-12;
    if(low<0)low=0;
    if(low>103)low=103;
    const int32_t high=low+23;
    instrument_card(4,2,232,174,panel_fill(),accent,accent);
    cv_text(12,8,&FONT_XS,lane?"TRACK 2  /  NOTES":"TRACK 1  /  NOTES",accent);
    cv_text(148,8,&FONT_XS,seq_ui_instrument_name[lane],C_WHITE);
    /* Sixteen-step, two-octave note grid modelled on established FM-1 piano-roll editors. */
    for(uint32_t row=0;row<24u;row++) {
        int32_t note=high-(int32_t)row, y=gy+(int32_t)row*rh;
        uint8_t pc=(uint8_t)(note%12);
        uint8_t black=(uint8_t)(pc==1u||pc==3u||pc==6u||pc==8u||pc==10u);
        cv_rect(gx,y,gw,rh-1,black?RGB(20,29,42):RGB(34,45,57));
        if(pc==0u||pc==5u)cv_rect(gx,y,gw,1,RGB(75,86,101));
    }
    for(uint32_t col=0;col<=16u;col++) {
        int32_t x=gx+(int32_t)col*cw;
        cv_rect(x,gy,1,gh,((col%4u)==0u)?RGB(86,98,112):RGB(49,61,75));
    }
    for(uint32_t col=0;col<16u;col++) {
        uint8_t step=(uint8_t)(first+col);
        int32_t x=gx+(int32_t)col*cw;
        if(step==seq_ui_step)rounded_panel(x,gy,cw,gh,1u,0u,C_WHITE);
        if(fm6_sequencer_running()&&step==fm6_sequencer_playing_step())
            cv_rect(x+5,gy,2,gh,RGB(242,212,59));
        if(seq_ui_enabled[lane][step]) {
            int32_t note=seq_ui_note[lane][step];
            if(note>=low&&note<=high) {
                int32_t y=gy+(high-note)*rh+1;
                int32_t barw=(int32_t)(seq_ui_gate[lane][step]?seq_ui_gate[lane][step]:75u)*(cw-2)/100;
                int32_t barh=2+(int32_t)seq_ui_velocity[lane][step]*2/127;
                if(barw<2)barw=2;
                if(barh>3)barh=3;
                cv_rect(x+1,y,barw,barh,accent);
                if(step==seq_ui_step)cv_rect(x+1,y,barw,1,C_WHITE);
            }
        }
        if((col%4u)==0u) {
            int_text((int32_t)step+1,value);
            cv_text(x+1,142,&FONT_XS,value,C_GRAY);
        }
    }
    cv_text(12,153,&FONT_XS,"NOTE",C_GRAY);
    int_text(seq_ui_enabled[lane][seq_ui_step]?seq_ui_note[lane][seq_ui_step]:0,value);
    cv_text(43,153,&FONT_B,seq_ui_enabled[lane][seq_ui_step]?value:"--",accent);
    cv_text(82,153,&FONT_XS,"VEL",C_GRAY);
    int_text(seq_ui_velocity[lane][seq_ui_step],value);
    cv_text(108,153,&FONT_B,value,C_WHITE);
    cv_text(144,153,&FONT_XS,"GATE",C_GRAY);
    int_text(seq_ui_gate[lane][seq_ui_step],value);
    cv_text(177,153,&FONT_B,value,C_WHITE);
    cv_text(10,166,&FONT_XS,"K1 STEP  K2 PITCH  K3 LEN  K4 VEL   EDIT DELETE",C_GRAY);
}

static void draw_sequencer_options_panel(void)
{
    uint16_t accent=RGB(242,212,59);
    instrument_card(8,22,224,126,panel_fill(),accent,accent);
    cv_text(20,34,&FONT_XS,"LOOP RECORD BEHAVIOR",C_GRAY);
    cv_text(20,61,&FONT_B,seq_record_mode?"OVERWRITE":"OVERDUB",accent);
    cv_text(20,94,&FONT_XS,seq_record_mode?"CLEAR UNPLAYED STEPS":"KEEP UNPLAYED STEPS",C_WHITE);
    cv_text(20,126,&FONT_XS,"K1  CHANGE MODE",C_GRAY);
    cv_text(8,166,&FONT_XS,seq_record_armed?"REC ARMED":"REC DISARMED",seq_record_armed?RGB(255,80,90):C_GRAY);
}
static void draw_arp_card(uint32_t index, const char *label, const char *value,
                          const char *hint, uint16_t accent)
{
    int32_t x=panel_grid_x(index), y=panel_grid_y(index,UI_GRID_TOP,UI_GRID_ROW_STEP);
    instrument_card(x,y,UI_GRID_CARD_W,UI_GRID_CARD_H,panel_fill(),accent,accent);
    cv_text(x+9,y+9,&FONT_XS,label,C_GRAY);
    cv_text(x+9,y+28,&FONT_B,value,accent);
    cv_text(x+9,y+67,&FONT_XS,hint,C_WHITE);
}
static void draw_arp_panel(void)
{
    static const char *const modes[5]={"OFF","UP","DOWN","UP/DOWN","RANDOM"};
    static const char *const divisions[6]={"1/1","1/2","1/4","1/8","1/16","1/32"};
    char t[16];
    uint16_t accent=RGB(227,75,210);
    draw_arp_card(0u,"K1  MODE",modes[arp_ui_mode<5u?arp_ui_mode:0u],"ORDER / ENABLE",accent);
    draw_arp_card(1u,"K2  DIVISION",divisions[arp_ui_division<6u?arp_ui_division:3u],"NOTE RATE",RGB(36,215,232));
    int_text(arp_ui_gate,t); text_cat(t,"%");
    draw_arp_card(2u,"K3  GATE",t,"NOTE LENGTH",RGB(242,212,59));
    int_text(arp_ui_octaves,t); text_cat(t," OCT");
    draw_arp_card(3u,"K4  OCTAVES",t,"RANGE",RGB(255,130,100));
}
static uint16_t arp_step_gradient_color(uint8_t accent, uint32_t band, uint32_t bands, uint8_t playing)
{
    /* Accent is visual-only: it controls a bottom-up color ramp. */
    uint32_t progress=bands>1u?band*100u/(bands-1u):100u;
    uint32_t level=18u+((uint32_t)accent*progress*82u)/10000u;
    uint32_t r=20u+(207u*level)/100u, g=12u+(63u*level)/100u, b=34u+(176u*level)/100u;
    if(playing){r+=r/2u;g+=g/2u;b+=b/2u;}
    if(r>255u)r=255u;if(g>255u)g=255u;if(b>255u)b=255u;
    return RGB(r,g,b);
}
static void draw_arp_latch_panel(void)
{
    uint16_t accent=RGB(227,75,210);
    instrument_card(8,22,224,126,panel_fill(),accent,accent);
    cv_text(20,34,&FONT_XS,"NOTE HOLD BEHAVIOR",C_GRAY);
    cv_text(20,62,&FONT_B,arp_ui_latch?"LATCH ON":"LATCH OFF",arp_ui_latch?RGB(75,235,135):accent);
    cv_text(20,96,&FONT_XS,arp_ui_latch?"RELEASED KEYS STAY IN THE ARP":"RELEASED KEYS LEAVE THE ARP",C_WHITE);
    cv_text(20,126,&FONT_XS,"K1  TOGGLE LATCH",C_GRAY);
}
static void draw_arp_steps_panel(void)
{
    char t[16];
    uint8_t playing=fm6_arpeggiator_playing_step(), active=fm6_arpeggiator_active();
    int32_t gap=2, width=(224-(int32_t)(arp_ui_length-1u)*gap)/(int32_t)arp_ui_length;
    int32_t top=29, height=91, bottom=top+height;
    uint16_t gain_color=RGB(36,215,232), accent_color=RGB(227,75,210), tie_color=RGB(242,212,59);
    cv_text(5,2,&FONT_XS,arp_ui_length==8u?"8 STEP LOOP":"16 STEP LOOP",C_GRAY);
    cv_text(151,2,&FONT_XS,active?"PLAYING":"STANDBY",active?RGB(75,235,135):C_GRAY);
    cv_rect(5,18,214,1,RGB(35,51,64));
    for(uint32_t i=0;i<arp_ui_length;i++) {
        int32_t x=8+(int32_t)i*(width+gap);
        uint8_t gain=arp_ui_gain[i], emphasis=arp_ui_accent[i];
        uint8_t tied=arp_ui_step_gate[i]?1u:0u;
        int32_t bar_h=(int32_t)gain*(height-10)/127;
        uint8_t is_playing=(uint8_t)(active && i==(uint32_t)(playing%arp_ui_length));
        uint8_t is_selected=(uint8_t)(i==arp_ui_step);
        uint32_t bands=7u;
        int_text((int32_t)i+1,t);
        cv_text(x+(width-text_w(&FONT_XS,t))/2,21,&FONT_XS,t,is_selected?C_WHITE:C_GRAY);
        cv_rect(x,top,width,height,is_selected?C_WHITE:RGB(43,59,72));
        cv_rect(x+1,top+1,width-2,height-2,RGB(12,20,29));
        cv_rect(x+2,top+2,width-4,2,emphasis?accent_color:RGB(34,49,61));
        if(bar_h<2&&gain)bar_h=2;
        if(gain) {
            int32_t fill_x=x+2, fill_y=bottom-4-bar_h;
            int32_t bar_w=width-4;
            int32_t band_h=(bar_h+(int32_t)bands-1)/(int32_t)bands;
            for(uint32_t band=0;band<bands;band++) {
                int32_t yy=fill_y+(int32_t)band*band_h, hh=band_h;
                if(yy+hh>bottom-4)hh=bottom-4-yy;
                if(hh>0)cv_rect(fill_x,yy,bar_w,hh,arp_step_gradient_color(emphasis,band,bands,is_playing));
            }
        }
        if(tied && i+1u<arp_ui_length) {
            int32_t bridge_y=bottom-1;
            cv_rect(x+width-1,bridge_y,gap+2,2,tie_color);
            cv_rect(x+width-1,bridge_y-2,1,4,tie_color);
            cv_rect(x+width+gap,bridge_y-2,1,4,tie_color);
        }
        if(is_playing) {
            cv_rect(x+1,top+1,width-2,2,RGB(255,150,255));
            cv_rect(x+1,bottom-3,width-2,1,RGB(255,150,255));
        }
        if(is_selected) {
            cv_rect(x,top,width,1,C_WHITE);cv_rect(x,bottom-1,width,1,C_WHITE);
            cv_rect(x,top,1,height,C_WHITE);cv_rect(x+width-1,top,1,height,C_WHITE);
        }
    }
    cv_text(5,128,&FONT_XS,"STEP",C_GRAY);
    int_text((int32_t)arp_ui_step+1,t);cv_text(31,128,&FONT_XS,t,C_WHITE);
    cv_text(55,128,&FONT_XS,"GAIN",gain_color);
    int_text(arp_ui_gain[arp_ui_step],t);cv_text(83,128,&FONT_XS,t,C_WHITE);
    cv_text(112,128,&FONT_XS,"ACC",accent_color);
    int_text(arp_ui_accent[arp_ui_step],t);cv_text(135,128,&FONT_XS,t,C_WHITE);
    cv_text(163,128,&FONT_XS,"TIE",tie_color);
    cv_text(193,128,&FONT_XS,arp_ui_step_gate[arp_ui_step]?"ON":"OFF",arp_ui_step_gate[arp_ui_step]?tie_color:C_GRAY);
    cv_text(5,151,&FONT_XS,"K1 STEP  K2 GAIN  K3 ACC  K4 TIE",C_GRAY);
}

static void draw_delete_overlay(void)
{
    uint32_t phase=(plat_ms()/180u)&3u;
    uint16_t accent=ui.delete_kind==1u?RGB(227,75,210):RGB(242,212,59);
    const char *label=ui.delete_kind==1u?"CLEARING ARP":
        (ui.delete_index==2u?"CLEARING SEQUENCE 2":"CLEARING SEQUENCE 1");
    uint32_t remaining=(uint32_t)(ui.delete_until-plat_ms());
    uint32_t elapsed=remaining<2100u?2100u-remaining:2100u;
    int32_t progress=(int32_t)(elapsed*176u/2100u);
    rounded_panel(18,48,204,91,3,RGB(5,9,16),accent);
    rounded_panel(21,51,198,85,3,RGB(12,18,29),RGB(31,47,59));
    cv_rect(96,62,32,2,RGB(43,59,72));
    cv_rect(96,62+(int32_t)(phase&1u)*2,8,2,accent);
    cv_rect(108,70,24,16,RGB(20,31,43));
    cv_rect(111,73,4,10,accent);
    cv_rect(120,73,4,10,phase&1u?C_WHITE:accent);
    cv_rect(129,73,4,10,accent);
    cv_text(120-text_w(&FONT_B,label)/2,96,&FONT_B,label,C_WHITE);
    cv_rect(34,119,176,3,RGB(31,47,59));
    if(progress>176)progress=176;
    if(progress>0)cv_rect(34,119,progress,3,accent);
}
static void draw_seq_clear_prompt(void)
{
    char t[20];
    rounded_panel(17,55,206,78,4,RGB(5,9,16),RGB(242,212,59));
    rounded_panel(20,58,200,72,4,RGB(12,18,29),RGB(57,66,72));
    cv_text(120-text_w(&FONT_B,"CLEAR STEP?")/2,72,&FONT_B,"CLEAR STEP?",RGB(255,226,95));
    cv_text(120-text_w(&FONT_XS,"TURN K2 TO CONFIRM")/2,96,&FONT_XS,"TURN K2 TO CONFIRM",C_WHITE);
    t[0]='T';t[1]=(char)('1'+seq_ui_lane);t[2]=' ';t[3]='S';t[4]='T';t[5]='E';t[6]='P';t[7]=' ';
    int_text((int32_t)seq_ui_step+1,t+8);
    cv_text(120-text_w(&FONT_XS,t)/2,115,&FONT_XS,t,C_GRAY);
}

static void draw_fx_panel(uint8_t fx)
{
    static const char *const names[4]={"CHORUS","DELAY","REVERB","PHASER"};
    static const char *const types[3]={"ROOM","HALL","PLATE"};
    char t[16];
    uint16_t accent=fx==0u?RGB(36,215,232):fx==1u?RGB(227,75,210):fx==2u?RGB(242,212,59):RGB(85,190,255);
    uint8_t enabled=fm6_fx_enabled(fx);
    uint8_t values[4]={enabled,fx==FM6_FX_REVERB?set.performance.fx_depth[2]:fm6_fx_time(fx),
        fm6_fx_mix(fx),fx==FM6_FX_PHASER?fm6_fx_feedback(fx):fm6_fx_feedback(fx)};
    const char *labels[4]={"K1  BYPASS",fx==FM6_FX_REVERB?"K2  TYPE":
        fx==FM6_FX_CHORUS?"K2  RATE":fx==FM6_FX_DELAY?"K2  TIME":"K2  RATE",
        "K3  WET","K4  FEEDBACK"};
    for(uint32_t k=0;k<4u;++k) {
        int32_t x=panel_grid_x(k),y=panel_grid_y(k,UI_GRID_TOP,UI_GRID_ROW_STEP);
        panel_grid_card(k,UI_GRID_TOP,UI_GRID_ROW_STEP,accent);
        cv_text(x+7,y+6,&FONT_XS,labels[k],C_GRAY);
        if(k==0u) cv_text(x+7,y+23,&FONT_B,enabled?"ON":"OFF",enabled?RGB(75,235,135):RGB(255,82,101));
        else if(k==1u && fx==FM6_FX_REVERB) {
            uint8_t type=values[k]<=2u?values[k]:1u;
            cv_text(x+7,y+23,&FONT_B,types[type],accent);
            cv_text(x+7,y+43,&FONT_XS,"ROOM / HALL / PLATE",C_GRAY);
        } else {
            int_text(values[k],t);
            cv_text(x+7,y+23,&FONT_B,t,accent);
            cv_rect(x+48,y+29,55,5,C_LINE);
            cv_rect(x+48,y+29,(int32_t)values[k]*55/100,5,accent);
        }
    }
}

static void draw_fx_routing_panel(void)
{
    static const char *const names[FM6_FX_COUNT]={"CHORUS SEND","DELAY SEND","REVERB SEND","PHASER SEND"};
    char t[16];
    uint16_t accent=patch_color(set.patch);
    for(uint32_t fx=0;fx<FM6_FX_COUNT;++fx) {
        uint8_t value=fm6_fx_get_send(set.patch,(uint8_t)fx);
        int32_t x=panel_grid_x(fx);
        int32_t y=panel_grid_y(fx,UI_GRID_TOP,UI_GRID_ROW_STEP);
        uint16_t color=fx==0u?RGB(36,215,232):fx==1u?RGB(227,75,210):fx==2u?RGB(242,212,59):RGB(85,190,255);
        panel_grid_card(fx, UI_GRID_TOP, UI_GRID_ROW_STEP, color);
        cv_text(x+7,y+6,&FONT_XS,names[fx],C_GRAY);
        int_text(value,t);
        cv_text(x+7,y+23,&FONT_B,t,color);
        cv_rect(x+48,y+29,55,5,C_LINE);
        cv_rect(x+48,y+29,(int32_t)value*55/100,5,color);
    }
}

/* The top identity line is the current page title, not the instrument's
 * generic FACTORY storage name. Page accents carry through to the footer. */
static const char *page_title(uint8_t current)
{
    switch (current) {
    case PAGE_HOME: return "HOME";
    case PAGE_MACRO: return patch_focus ? "PATCH MACROS" : "MACROS";
    case PAGE_GLOBAL: return "GLOBAL";
    case PAGE_OPERATOR_MAP: return "OPERATOR MAP";
    case PAGE_ENV: return "ENVELOPE";
    case PAGE_LFO: return "LFO";
    case PAGE_LFO_MOD: return "LFO MODULATION";
    case PAGE_SEQ: return "SEQUENCER";
    case PAGE_SEQ_OPTIONS: return "SEQ RECORD";
    case PAGE_SEQ_PIANO: return "PIANO ROLL";
    case PAGE_ARP: return "ARP ENGINE";
    case PAGE_ARP_STEPS: return "ARP STEP EDIT";
    case PAGE_ARP_LATCH: return "ARP LATCH";
    case PAGE_FX_AMP: return "AMP / DRIVE";
    case PAGE_FX_EQ: return "PARAMETRIC EQ";
    case PAGE_FX_COMPRESSOR: return "COMPRESSOR";
    case PAGE_FX_CHORUS: return "CHORUS";
    case PAGE_FX_DELAY: return "DELAY";
    case PAGE_FX_REVERB: return "REVERB";
    case PAGE_FX_PHASER: return "PHASER";
    case PAGE_FX_ROUTING: return "PATCH SENDS";
    case PAGE_INSTRUMENT_MACRO: return "INSTRUMENT MACROS";
    default: return "FM1-LAB";
    }
}

static uint16_t page_accent(uint8_t current)
{
    switch (current) {
    case PAGE_HOME: return patch_color(set.patch);
    case PAGE_MACRO: return patch_color(set.patch);
    case PAGE_INSTRUMENT_MACRO: return RGB(150,120,255);
    case PAGE_GLOBAL: return RGB(85,190,255);
    case PAGE_OPERATOR_MAP: return RGB(36,215,232);
    case PAGE_ENV: return RGB(255,170,75);
    case PAGE_LFO: return RGB(75,220,150);
    case PAGE_LFO_MOD: return RGB(75,190,180);
    case PAGE_SEQ: case PAGE_SEQ_OPTIONS: case PAGE_SEQ_PIANO: return RGB(242,212,59);
    case PAGE_ARP: case PAGE_ARP_STEPS: case PAGE_ARP_LATCH: return RGB(227,75,210);
    case PAGE_FX_AMP: return RGB(255,140,70);
    case PAGE_FX_EQ: return RGB(85,190,255);
    case PAGE_FX_COMPRESSOR: return RGB(255,170,75);
    case PAGE_FX_CHORUS: return RGB(36,215,232);
    case PAGE_FX_DELAY: return RGB(85,160,255);
    case PAGE_FX_REVERB: return RGB(150,120,255);
    case PAGE_FX_PHASER: return RGB(227,75,210);
    case PAGE_FX_ROUTING: return patch_color(set.patch);
    default: return C_GRAY;
    }
}

/* Page counters are local to the active page family, not the full enum.
 * Keep each family contiguous so adding/removing a page updates the total. */
static void page_counter(uint8_t current, char *out, uint32_t out_size)
{
    uint32_t index = 1u, total = 1u;
    if (current >= PAGE_FX_AMP && current <= PAGE_FX_ROUTING) {
        index = (uint32_t)(current - PAGE_FX_AMP) + 1u;
        total = (uint32_t)(PAGE_FX_ROUTING - PAGE_FX_AMP) + 1u;
    } else if (current >= PAGE_OPERATOR_MAP && current <= PAGE_LFO_MOD) {
        index = (uint32_t)(current - PAGE_OPERATOR_MAP) + 1u;
        total = (uint32_t)(PAGE_LFO_MOD - PAGE_OPERATOR_MAP) + 1u;
    } else if (current == PAGE_SEQ || current == PAGE_SEQ_OPTIONS) {
        index=(uint32_t)(current-PAGE_SEQ)+1u;total=2u;
    } else if (current == PAGE_SEQ_PIANO) {
        index=1u;total=1u;
    } else if (current >= PAGE_ARP && current <= PAGE_ARP_LATCH) {
        index=(uint32_t)(current-PAGE_ARP)+1u;total=3u;
    } else if (current <= PAGE_GLOBAL) {
        index = (uint32_t)current + 1u;
        total = (uint32_t)PAGE_GLOBAL + 1u;
    }
    int_text((int32_t)index, out);
    text_cat(out, "/");
    char total_text[8];
    int_text((int32_t)total, total_text);
    text_cat(out, total_text);
    (void)out_size;
}

static void draw(void)
{
    char t[24];
    uint32_t now = plat_ms(), pct = plat_master() * 100u / 4096u;
    uint16_t accent = page_accent(page);
    cv_begin(240, 37, screen_bg());

    /* Global status strip: tempo, physical battery gauge, and active arp. */
    int_text(transport_ui_bpm, t);
    text_cat(t, " BPM");
    cv_text(4, 1, &FONT_XS, t, C_GRAY);

    int battery = plat_battery(); /* 0..3 battery bars, 4 USB powered, -1 unknown */
    cv_rect(183, 2, 23, 7, C_GRAY);
    cv_rect(206, 4, 2, 3, C_GRAY);
    if (battery >= 0) {
        uint32_t bars = battery > 3 ? 4u : (uint32_t)battery;
        if (bars) cv_rect(185, 4, (int32_t)(bars * 5u), 3,
                          battery >= 3 ? RGB(75,235,135) : accent);
    }
    if (arp_ui_mode) cv_text(216, 1, &FONT_XS, "ARP", RGB(75,235,135));

    /* Page identity replaces the generic FACTORY instrument label. */
    cv_text(8, 15, &FONT_B, page_title(page), accent);
    if (page == PAGE_HOME) {
        cv_text(151, 17, &FONT_XS, seq_ui_lane ? "T2" : "T1", patch_color(seq_ui_patch[seq_ui_lane]));
        int_text((int32_t)instrument_index + 1, t);
        text_cat(t, "/32");
        cv_text(198, 17, &FONT_XS, t, C_GRAY);
    }
    cv_rect(0, 34, 240, 3, accent);
    cv_blit(0, 0);
    cv_begin(240, 180, screen_bg());
    if (page == PAGE_HOME) draw_home_panel();
    else if (page == PAGE_MACRO) draw_macro_panel();
    else if (page == PAGE_INSTRUMENT_MACRO) draw_instrument_macro_panel();
    else if (page == PAGE_FX_AMP) draw_amp_panel();
    else if (page == PAGE_FX_EQ) draw_eq_panel();
    else if (page == PAGE_FX_COMPRESSOR) draw_compressor_panel();
    else if (page == PAGE_OPERATOR_MAP) draw_operator_map();
    else if (page == PAGE_ENV) draw_envelope_panel();
    else if (page == PAGE_LFO) draw_lfo_panel(0);
    else if (page == PAGE_LFO_MOD) draw_lfo_panel(1);
    else if (page == PAGE_SEQ) draw_sequencer_panel();
    else if (page == PAGE_SEQ_OPTIONS) draw_sequencer_options_panel();
    else if (page == PAGE_SEQ_PIANO) draw_sequencer_piano_panel();
    else if (page == PAGE_ARP) draw_arp_panel();
    else if (page == PAGE_ARP_STEPS) draw_arp_steps_panel();
    else if (page == PAGE_ARP_LATCH) draw_arp_latch_panel();
    else if (page == PAGE_FX_CHORUS) draw_fx_panel(0u);
    else if (page == PAGE_FX_DELAY) draw_fx_panel(1u);
    else if (page == PAGE_FX_REVERB) draw_fx_panel(2u);
    else if (page == PAGE_FX_PHASER) draw_fx_panel(3u);
    else if (page == PAGE_FX_ROUTING) draw_fx_routing_panel();
    else draw_global_panel(pct);
    if(ui.delete_until && (int32_t)(ui.delete_until-plat_ms())>0)draw_delete_overlay();
    if(page==PAGE_SEQ && seq_clear_prompt)draw_seq_clear_prompt();
    cv_blit(0, 41);

    /* Compact control strip. HOME and ENV already show these values in
     * their main panels, so avoid duplicating them in a second bar. */
    cv_begin(240, 19, screen_bg());
    page_counter(page, t, sizeof(t));
    cv_text(240 - text_w(&FONT_XS, t) - 6, 4, &FONT_XS, t, accent);
    cv_blit(0, 221);
}

void app_ui_init(void)
{
    touch_on(1);                             /* velocity and pressure: the keys' readings on */
    lcd_fill(0, 0, 240, 240, screen_bg());
    ui.redraw = 1;
}

void app_frame(void)
{
    static uint32_t last_pct, lit_until, last_ble, last_midi, last_arp_redraw;
    uint32_t i, now = plat_ms(), pct = plat_master() * 100u / 4096u;
    input();
    if(seq_record_redraw){ui.redraw=1u;seq_record_redraw=0u;}
    if (performance_update_dirty && !performance_update_pending) performance_publish();
    speak();
    for (i = 0; i < NE; i++)                 /* an encoder's light goes out */
        if (ui.turned_at[i] && now - ui.turned_at[i] < 400u && now > lit_until)
            lit_until = ui.turned_at[i] + 300u;
    if (lit_until && now >= lit_until) {
        lit_until = 0;
        ui.redraw = 1;
    }
    if (FM1B_BLE_CENTRAL) {                  /* looking: a device found is named at once (REC connects to it) */
        static int found;
        int n = ui.looking ? btdev_count() : 0, ev;
        btdev_page(ui.looking && !set.ble_off);
        if (n > found && btdev_pick() < n)
            ble_device_say();
        found = n;
        while ((ev = btdev_event()) != BTDEV_NONE)   /* a device connected, given up or lost */
            if (ev == BTDEV_CONNECTED || ev == BTDEV_NO_ANSWER || ev == BTDEV_LOST)
                app_say("DEVICE", btdev_word(ev));
    }
    if (midi_count != last_midi) {
        last_midi = midi_count;
        ui.redraw = 1;
    }
    /* Transport playheads advance in the audio callback, independently of
     * UI input. Refresh sequencer and ARP views at a steady cadence so their
     * highlights follow the actual audio-owned step position. */
    if (((page == PAGE_SEQ || page == PAGE_SEQ_PIANO) && fm6_sequencer_running()) ||
        (page == PAGE_ARP_STEPS && fm6_arpeggiator_active())) {
        if ((uint32_t)(now - last_arp_redraw) >= 30u) {
            last_arp_redraw = now;
            ui.redraw = 1u;
        }
    }
    if(ui.delete_until && (int32_t)(ui.delete_until-now)>0 &&
       (uint32_t)(now-last_arp_redraw)>=70u){last_arp_redraw=now;ui.redraw=1u;}
    if(ui.delete_until && (int32_t)(now-ui.delete_until)>=0){ui.delete_until=0u;ui.redraw=1u;}
    if (pct != last_pct || (uint32_t)(plat_ble_connected() | plat_ble_device() << 1) != last_ble ||
        (ui.msg_until && now >= ui.msg_until)) {
        last_pct = pct;
        last_ble = (uint32_t)(plat_ble_connected() | plat_ble_device() << 1);
        if (now >= ui.msg_until)
            ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (ui.redraw) {
        ui.redraw = 0;
        draw();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "octave"))
        *got = set.octave;
    else if (!strcmp(what, "patch") || !strcmp(what, "wave"))
        *got = set.patch;
    else if (!strcmp(what, "note"))
        *got = voice_note;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "page"))
        *got = page;
    else if (!strcmp(what, "cpu"))
        *got = (int)app_audio_cpu_pct();
    else if (!strcmp(what, "xruns"))
        *got = (int)app_audio_xruns();
    else if (!strcmp(what, "shed"))
        *got = (int)app_audio_overload_shed_count();
    else if (!strcmp(what, "vel"))           /* the last key's velocity and pressure */
        *got = ui.vel[ui.last_key];
    else if (!strcmp(what, "prs"))
        *got = ui.prs[ui.last_key];
    else if (!strncmp(what, "turns", 5))    /* turnsN: encoder N's count (plat.h EN_*) */
        *got = ui.turns[atoi(what + 5) % NE];
    else
        return 0;
    return 1;
}
#endif
