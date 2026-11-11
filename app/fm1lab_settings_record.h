/* SPDX-License-Identifier: GPL-3.0-only
 * FM1-Lab settings record codec. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "fm6.h"
#define SET_MAGIC_V1 0x4C314D46u
#define SET_MAGIC_V2 0x4C314D47u
#define SET_MAGIC_V3 0x4C314D48u
#define SET_MAGIC_V4 0x4C314D49u
#define SET_MAGIC_V5 0x4C314D4Au
#define SET_MAGIC_V6 0x4C314D4Bu
#define SET_MAGIC_V7 0x4C314D4Cu
#define SET_MAGIC_V8 0x4C314D4Du
#define SET_MAGIC_V9 0x4C314D4Eu
#define SET_MAGIC_V10 0x4C314D4Fu
#define SET_MAGIC_V11 0x4C314D50u
#define SET_MAGIC_V12 0x4C314D51u
#define SET_MAGIC SET_MAGIC_V12
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off, rsv[1]; } settings_v1_t;
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off; uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT]; uint8_t levels[FM1LAB_PATCH_COUNT]; } settings_v2_t;
typedef struct {
    uint16_t tempo_bpm; uint8_t seq_length; uint8_t lane_patch[2];
    uint8_t seq_enabled[2][64]; uint8_t seq_note[2][64];
    uint8_t seq_velocity[2][64]; uint8_t seq_gate[2][64]; uint8_t arp_mode;
    uint8_t fx_enabled[3], fx_mix[3], fx_time[3], fx_feedback[3];
    uint8_t fx_send[FM1LAB_PATCH_COUNT][3];
} fm6_performance_state_v3_t;
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off; uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT]; uint8_t levels[FM1LAB_PATCH_COUNT]; fm6_performance_state_v3_t performance; uint16_t crc; } settings_v3_t;

typedef struct {
    uint16_t tempo_bpm; uint8_t seq_length; uint8_t lane_patch[2];
    uint8_t seq_enabled[2][64]; uint8_t seq_note[2][64];
    uint8_t seq_velocity[2][64]; uint8_t seq_gate[2][64]; uint8_t arp_mode;
    uint8_t fx_enabled[4], fx_mix[4], fx_time[4], fx_feedback[4];
    uint8_t fx_send[FM1LAB_PATCH_COUNT][4]; uint8_t fx_depth[4];
} fm6_performance_state_v4_t;
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off; uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT]; uint8_t levels[FM1LAB_PATCH_COUNT]; fm6_performance_state_v4_t performance; uint16_t crc; } settings_v4_t;

typedef struct {
    uint16_t tempo_bpm; uint8_t seq_length; uint8_t lane_patch[2];
    uint8_t seq_enabled[2][64]; uint8_t seq_note[2][64];
    uint8_t seq_velocity[2][64]; uint8_t seq_gate[2][64]; uint8_t arp_mode;
    uint8_t fx_enabled[4],fx_mix[4],fx_time[4],fx_feedback[4];
    uint8_t fx_send[FM1LAB_PATCH_COUNT][4];uint8_t fx_depth[4];
    uint8_t compressor_enabled,compressor_threshold,compressor_ratio;
    uint8_t compressor_attack,compressor_release,compressor_makeup;
} fm6_performance_state_v5_t;
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off; uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT]; uint8_t levels[FM1LAB_PATCH_COUNT]; fm6_performance_state_v5_t performance; uint16_t reserved; uint16_t crc; } settings_v5_t;
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off; uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT]; uint8_t levels[FM1LAB_PATCH_COUNT]; fm6_performance_state_v10_t performance; uint16_t reserved; uint16_t padding; uint16_t crc; } settings_v10_t;
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off; uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT]; uint8_t levels[FM1LAB_PATCH_COUNT]; fm6_performance_state_t performance; uint16_t reserved; uint16_t padding; uint16_t crc; } settings_v11_t;
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off; uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT]; uint8_t levels[FM1LAB_PATCH_COUNT]; fm6_performance_state_t performance; uint16_t reserved; uint16_t swing; uint16_t crc; } settings_t;
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off; uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT]; uint8_t levels[FM1LAB_PATCH_COUNT]; fm6_performance_state_v9_t performance; uint16_t reserved; uint16_t crc; } settings_v6_t;
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off; uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT]; uint8_t levels[FM1LAB_PATCH_COUNT]; fm6_performance_state_v9_t performance; uint16_t reserved; uint16_t crc; } settings_v7_t;
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off; uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT]; uint8_t levels[FM1LAB_PATCH_COUNT]; fm6_performance_state_v9_t performance; uint16_t reserved; uint16_t crc; } settings_v8_t;
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off; uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT]; uint8_t levels[FM1LAB_PATCH_COUNT]; fm6_performance_state_v9_t performance; uint16_t reserved; uint16_t crc; } settings_v9_t;
static inline uint16_t settings_crc16(const void *data, size_t length) {
    const uint8_t *bytes=(const uint8_t *)data; uint16_t crc=0xFFFFu;
    while(length--) { crc^=(uint16_t)*bytes++<<8; for(uint32_t bit=0;bit<8u;++bit) crc=(uint16_t)((crc&0x8000u)?(((uint32_t)crc<<1)^0x1021u):((uint32_t)crc<<1)); }
    return crc;
}
static inline void settings_seal(settings_t *v) { v->magic=SET_MAGIC; v->reserved&=3u; if(v->swing<50u||v->swing>75u)v->swing=50u; v->crc=settings_crc16(v,offsetof(settings_t,crc)); }
static inline int settings_v1_valid(const settings_v1_t *v) {
    return v && v->magic==SET_MAGIC_V1 && v->octave>=-2 && v->octave<=2 && v->patch<FM1LAB_PATCH_COUNT && !(v->enabled_mask&0xF0u) && v->ble_off<=1u;
}
static inline int settings_v2_valid(const settings_v2_t *v) {
    if(!v || v->magic!=SET_MAGIC_V2 || v->octave < -2 || v->octave>2 || v->patch>=FM1LAB_PATCH_COUNT || (v->enabled_mask&0xF0u) || v->ble_off>1u) return 0;
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;++p) { if(v->levels[p]>127u) return 0; for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;++m) if(v->macros[p][m]>100u) return 0; }
    return 1;
}

static inline int settings_legacy_performance_valid(const fm6_performance_state_v9_t *legacy) {
    /* Called only by the serial startup migration path. Keep the full current
     * record out of the FM1 main stack; legacy records are checked one at a time. */
    static fm6_performance_state_t current;
    memset(&current,0,sizeof current);
    memcpy(&current,legacy,sizeof *legacy);
    return fm6_performance_state_valid(&current);
}
static inline void settings_clear_events(settings_t *dst) {
    memset(dst->performance.seq_event_count,0,sizeof dst->performance.seq_event_count);
    memset(dst->performance.seq_events,0,sizeof dst->performance.seq_events);
}
static inline void settings_migrate_step_events(settings_t *dst) {
    uint32_t length=dst->performance.seq_length;
    if(length<2u)length=2u;
    if(length>64u)length=64u;
    settings_clear_events(dst);
    for(uint32_t lane=0;lane<2u;lane++) {
        int previous=-1;uint8_t previous_note=255u;
        for(uint32_t step=0;step<length;step++) {
            if(!dst->performance.seq_enabled[lane][step]||!dst->performance.seq_velocity[lane][step]) {
                previous=-1;previous_note=255u;continue;
            }
            uint8_t note=dst->performance.seq_note[lane][step];
            uint8_t tied=(uint8_t)((dst->performance.seq_gate[lane][step]&0x80u)&&previous>=0&&previous_note==note);
            if(tied) {
                if(dst->performance.seq_events[lane][previous].duration<127u)
                    dst->performance.seq_events[lane][previous].duration+=2u;
            } else if(dst->performance.seq_event_count[lane]<128u) {
                uint8_t index=dst->performance.seq_event_count[lane]++;
                dst->performance.seq_events[lane][index]=(fm6_seq_event_t){
                    (uint8_t)(step*2u),2u,note,dst->performance.seq_velocity[lane][step],0u
                };
                previous=(int)index;
            } else previous=-1;
            previous_note=note;
        }
    }
}

static inline void settings_arp_defaults(settings_t *dst) {
    dst->performance.arp_division=3u;dst->performance.arp_gate=75u;
    dst->performance.arp_octaves=1u;dst->performance.arp_length=16u;
    dst->performance.arp_output=0u;dst->performance.arp_random=0u;
    dst->performance.arp_macro_dest=0u;dst->performance.arp_depth=50u;
    for(uint32_t step=0;step<16u;step++){
        dst->performance.arp_step_gain[step]=127u;
        dst->performance.arp_step_accent[step]=0u;
        dst->performance.arp_step_gate[step]=0u;
    }
}
static inline int settings_v5_valid(const settings_v5_t *v) {
    if(!v||v->magic!=SET_MAGIC_V5||v->octave < -2||v->octave>2||
       v->patch>=FM1LAB_PATCH_COUNT||(v->enabled_mask&0xF0u)||v->ble_off>1u||
       v->reserved!=0u||v->crc!=settings_crc16(v,offsetof(settings_v5_t,crc)))return 0;
    const fm6_performance_state_v5_t *p=&v->performance;
    if(p->tempo_bpm<30u||p->tempo_bpm>300u||p->seq_length<1u||p->seq_length>64u||p->arp_mode>4u)return 0;
    for(uint32_t lane=0;lane<2u;lane++){
        if(p->lane_patch[lane]>=FM1LAB_PATCH_COUNT)return 0;
        for(uint32_t step=0;step<64u;step++)
            if(p->seq_enabled[lane][step]>1u||p->seq_note[lane][step]>127u||
               p->seq_velocity[lane][step]>127u||p->seq_gate[lane][step]>100u)return 0;
    }
    for(uint32_t fx=0;fx<4u;fx++){
        if(p->fx_enabled[fx]>1u||p->fx_mix[fx]>100u||p->fx_time[fx]>100u||
           p->fx_depth[fx]>100u||p->fx_feedback[fx]>(fx==0u?70u:90u))return 0;
        for(uint32_t patch=0;patch<FM1LAB_PATCH_COUNT;patch++)
            if(p->fx_send[patch][fx]>100u)return 0;
    }
    if(p->compressor_enabled>1u||p->compressor_threshold>100u||p->compressor_ratio>100u||
       p->compressor_attack>100u||p->compressor_release>100u||p->compressor_makeup>100u)return 0;
    for(uint32_t patch=0;patch<FM1LAB_PATCH_COUNT;patch++){
        if(v->levels[patch]>127u)return 0;
        for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++)if(v->macros[patch][m]>100u)return 0;
    }
    return 1;
}
static inline int settings_import_v5(settings_t *dst,const settings_v5_t *src) {
    if(!dst||!settings_v5_valid(src))return -1;
    dst->swing=50u;
    dst->octave=src->octave;dst->patch=src->patch;dst->enabled_mask=src->enabled_mask;dst->ble_off=src->ble_off;
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;p++){
        dst->levels[p]=src->levels[p];
        for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++)dst->macros[p][m]=src->macros[p][m];
    }
    dst->performance.tempo_bpm=src->performance.tempo_bpm;
    dst->performance.seq_length=src->performance.seq_length;dst->performance.arp_mode=src->performance.arp_mode;
    for(uint32_t lane=0;lane<2u;lane++){
        dst->performance.lane_instrument[lane]=0u;
        for(uint32_t step=0;step<64u;step++){
            dst->performance.seq_enabled[lane][step]=src->performance.seq_enabled[lane][step];
            dst->performance.seq_note[lane][step]=src->performance.seq_note[lane][step];
            dst->performance.seq_velocity[lane][step]=src->performance.seq_velocity[lane][step];
            dst->performance.seq_gate[lane][step]=src->performance.seq_gate[lane][step];
        }
    }
    for(uint32_t fx=0;fx<4u;fx++){
        dst->performance.fx_enabled[fx]=src->performance.fx_enabled[fx];
        dst->performance.fx_mix[fx]=src->performance.fx_mix[fx];
        dst->performance.fx_time[fx]=src->performance.fx_time[fx];
        dst->performance.fx_feedback[fx]=src->performance.fx_feedback[fx];
        dst->performance.fx_depth[fx]=src->performance.fx_depth[fx];
        for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;p++)dst->performance.fx_send[p][fx]=src->performance.fx_send[p][fx];
    }
    dst->performance.compressor_enabled=src->performance.compressor_enabled;
    dst->performance.compressor_threshold=src->performance.compressor_threshold;
    dst->performance.compressor_ratio=src->performance.compressor_ratio;
    dst->performance.compressor_attack=src->performance.compressor_attack;
    dst->performance.compressor_release=src->performance.compressor_release;
    dst->performance.compressor_makeup=src->performance.compressor_makeup;
    settings_arp_defaults(dst);
    settings_migrate_step_events(dst);
    return 0;
}
static inline int settings_v3_valid(const settings_v3_t *v) {
    if (!v || v->magic != SET_MAGIC_V3 || v->octave < -2 || v->octave > 2 ||
        v->patch >= FM1LAB_PATCH_COUNT || (v->enabled_mask & 0xF0u) || v->ble_off > 1u ||
        v->crc != settings_crc16(v, offsetof(settings_v3_t, crc)))
        return 0;
    const fm6_performance_state_v3_t *p = &v->performance;
    if (p->tempo_bpm < 30u || p->tempo_bpm > 300u || p->seq_length < 1u ||
        p->seq_length > 64u || p->arp_mode > 4u) return 0;
    for (uint32_t lane=0; lane<2u; ++lane) {
        if (p->lane_patch[lane] >= FM1LAB_PATCH_COUNT) return 0;
        for (uint32_t step=0; step<64u; ++step)
            if (p->seq_enabled[lane][step] > 1u || p->seq_note[lane][step] > 127u ||
                p->seq_velocity[lane][step] > 127u || p->seq_gate[lane][step] > 100u) return 0;
    }
    for (uint32_t fx=0; fx<3u; ++fx) {
        if (p->fx_enabled[fx] > 1u || p->fx_mix[fx] > 100u || p->fx_time[fx] > 100u ||
            p->fx_feedback[fx] > (fx == 0u ? 70u : 90u)) return 0;
        for (uint32_t patch=0; patch<FM1LAB_PATCH_COUNT; ++patch)
            if (p->fx_send[patch][fx] > 100u) return 0;
    }
    for (uint32_t patch=0; patch<FM1LAB_PATCH_COUNT; ++patch) {
        if (v->levels[patch] > 127u) return 0;
        for (uint32_t m=0; m<FM1LAB_MACRO_COUNT; ++m)
            if (v->macros[patch][m] > 100u) return 0;
    }
    return 1;
}
static inline int settings_import_v3(settings_t *dst, const settings_v3_t *src) {
    if (!dst || !settings_v3_valid(src)) return -1;
    dst->octave=src->octave; dst->patch=src->patch; dst->enabled_mask=src->enabled_mask; dst->ble_off=src->ble_off;
    for (uint32_t p=0; p<FM1LAB_PATCH_COUNT; ++p) {
        dst->levels[p]=src->levels[p];
        for (uint32_t m=0; m<FM1LAB_MACRO_COUNT; ++m) dst->macros[p][m]=src->macros[p][m];
    }
    dst->performance.tempo_bpm=src->performance.tempo_bpm;
    dst->performance.seq_length=src->performance.seq_length;
    dst->performance.arp_mode=src->performance.arp_mode;
    for (uint32_t lane=0; lane<2u; ++lane) {
        dst->performance.lane_instrument[lane]=0u;
        for (uint32_t step=0; step<64u; ++step) {
            dst->performance.seq_enabled[lane][step]=src->performance.seq_enabled[lane][step];
            dst->performance.seq_note[lane][step]=src->performance.seq_note[lane][step];
            dst->performance.seq_velocity[lane][step]=src->performance.seq_velocity[lane][step];
            dst->performance.seq_gate[lane][step]=src->performance.seq_gate[lane][step];
        }
    }
    for (uint32_t fx=0; fx<3u; ++fx) {
        dst->performance.fx_enabled[fx]=src->performance.fx_enabled[fx];
        dst->performance.fx_mix[fx]=src->performance.fx_mix[fx];
        dst->performance.fx_time[fx]=src->performance.fx_time[fx];
        dst->performance.fx_feedback[fx]=src->performance.fx_feedback[fx];
        for (uint32_t p=0; p<FM1LAB_PATCH_COUNT; ++p)
            dst->performance.fx_send[p][fx]=src->performance.fx_send[p][fx];
    }
    dst->performance.fx_enabled[3]=0u;
    dst->performance.fx_mix[3]=35u;
    dst->performance.fx_time[3]=35u;
    dst->performance.fx_feedback[3]=20u;
    dst->performance.fx_depth[3]=65u;
    for (uint32_t p=0; p<FM1LAB_PATCH_COUNT; ++p) dst->performance.fx_send[p][3]=0u;
    dst->performance.compressor_enabled=0u; dst->performance.compressor_threshold=65u;
    dst->performance.compressor_ratio=40u; dst->performance.compressor_attack=20u;
    dst->performance.compressor_release=40u; dst->performance.compressor_makeup=0u;
    settings_arp_defaults(dst);
    return 0;
}


static inline int settings_v4_valid(const settings_v4_t *v) {
    if(!v||v->magic!=SET_MAGIC_V4||v->octave < -2||v->octave>2||v->patch>=FM1LAB_PATCH_COUNT||
       (v->enabled_mask&0xF0u)||v->ble_off>1u||v->crc!=settings_crc16(v,offsetof(settings_v4_t,crc)))return 0;
    const fm6_performance_state_v4_t *p=&v->performance;
    if(p->tempo_bpm<30u||p->tempo_bpm>300u||p->seq_length<1u||p->seq_length>64u||p->arp_mode>4u)return 0;
    for(uint32_t lane=0;lane<2u;lane++){
        if(p->lane_patch[lane]>=FM1LAB_PATCH_COUNT)return 0;
        for(uint32_t step=0;step<64u;step++)
            if(p->seq_enabled[lane][step]>1u||p->seq_note[lane][step]>127u||
               p->seq_velocity[lane][step]>127u||p->seq_gate[lane][step]>100u)return 0;
    }
    for(uint32_t fx=0;fx<4u;fx++){
        if(p->fx_enabled[fx]>1u||p->fx_mix[fx]>100u||p->fx_time[fx]>100u||
           p->fx_depth[fx]>100u||p->fx_feedback[fx]>(fx==0u?70u:90u))return 0;
        for(uint32_t patch=0;patch<FM1LAB_PATCH_COUNT;patch++)
            if(p->fx_send[patch][fx]>100u)return 0;
    }
    for(uint32_t patch=0;patch<FM1LAB_PATCH_COUNT;patch++){
        if(v->levels[patch]>127u)return 0;
        for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++)if(v->macros[patch][m]>100u)return 0;
    }
    return 1;
}
static inline int settings_import_v4(settings_t *dst,const settings_v4_t *src) {
    if(!dst||!settings_v4_valid(src))return -1;
    dst->swing=50u;
    dst->octave=src->octave;dst->patch=src->patch;dst->enabled_mask=src->enabled_mask;dst->ble_off=src->ble_off;
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;p++){
        dst->levels[p]=src->levels[p];
        for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++)dst->macros[p][m]=src->macros[p][m];
    }
    dst->performance.tempo_bpm=src->performance.tempo_bpm;
    dst->performance.seq_length=src->performance.seq_length;dst->performance.arp_mode=src->performance.arp_mode;
    for(uint32_t lane=0;lane<2u;lane++){
        dst->performance.lane_instrument[lane]=0u;
        for(uint32_t step=0;step<64u;step++){
            dst->performance.seq_enabled[lane][step]=src->performance.seq_enabled[lane][step];
            dst->performance.seq_note[lane][step]=src->performance.seq_note[lane][step];
            dst->performance.seq_velocity[lane][step]=src->performance.seq_velocity[lane][step];
            dst->performance.seq_gate[lane][step]=src->performance.seq_gate[lane][step];
        }
    }
    for(uint32_t fx=0;fx<4u;fx++){
        dst->performance.fx_enabled[fx]=src->performance.fx_enabled[fx];
        dst->performance.fx_mix[fx]=src->performance.fx_mix[fx];
        dst->performance.fx_time[fx]=src->performance.fx_time[fx];
        dst->performance.fx_feedback[fx]=src->performance.fx_feedback[fx];
        dst->performance.fx_depth[fx]=src->performance.fx_depth[fx];
        for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;p++)dst->performance.fx_send[p][fx]=src->performance.fx_send[p][fx];
    }
    dst->performance.compressor_enabled=0u;dst->performance.compressor_threshold=65u;
    dst->performance.compressor_ratio=40u;dst->performance.compressor_attack=20u;
    dst->performance.compressor_release=40u;dst->performance.compressor_makeup=0u;
    settings_arp_defaults(dst);
    return 0;
}

static inline int settings_v6_valid(const settings_v6_t *v) {
    if(!v||v->magic!=SET_MAGIC_V6||v->octave < -2||v->octave>2||v->patch>=FM1LAB_PATCH_COUNT||
       (v->enabled_mask&0xF0u)||v->ble_off>1u||v->reserved!=0u||
       v->crc!=settings_crc16(v,offsetof(settings_v6_t,crc)))return 0;
    fm6_performance_state_t p={0}; memcpy(&p,&v->performance,sizeof v->performance);
    /* V6's final per-step byte was macro amount; initialize the new tie flag off. */
    for(uint32_t step=0;step<16u;step++){
        if(p.arp_step_gate[step]>100u)return 0;
        p.arp_step_gate[step]=0u;
    }
    if(!fm6_performance_state_valid(&p))return 0;
    for(uint32_t patch=0;patch<FM1LAB_PATCH_COUNT;patch++){
        if(v->levels[patch]>127u)return 0;
        for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++)if(v->macros[patch][m]>100u)return 0;
    }
    return 1;
}
static inline int settings_import_v6(settings_t *dst,const settings_v6_t *src) {
    if(!dst||!settings_v6_valid(src))return -1;
    dst->swing=50u;
    dst->octave=src->octave;dst->patch=src->patch;dst->enabled_mask=src->enabled_mask;dst->ble_off=src->ble_off;
    for(uint32_t patch=0;patch<FM1LAB_PATCH_COUNT;patch++){
        dst->levels[patch]=src->levels[patch];
        for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++)dst->macros[patch][m]=src->macros[patch][m];
    }
    memcpy(&dst->performance,&src->performance,sizeof src->performance); settings_clear_events(dst);
    dst->performance.lane_instrument[0]=dst->performance.lane_instrument[1]=0u;
    for(uint32_t step=0;step<16u;step++)dst->performance.arp_step_gate[step]=0u;
    settings_migrate_step_events(dst);
    return 0;
}

static inline int settings_v7_valid(const settings_v7_t *v) {
    if(!v||v->magic!=SET_MAGIC_V7||v->octave < -2||v->octave>2||v->patch>=FM1LAB_PATCH_COUNT||
       (v->enabled_mask&0xF0u)||v->ble_off>1u||v->reserved!=0u||
       v->crc!=settings_crc16(v,offsetof(settings_v7_t,crc)))return 0;
    fm6_performance_state_t p={0}; memcpy(&p,&v->performance,sizeof v->performance);
    for(uint32_t step=0;step<16u;step++){
        if(p.arp_step_gate[step]<10u||p.arp_step_gate[step]>100u)return 0;
        p.arp_step_gate[step]=0u;
    }
    if(!fm6_performance_state_valid(&p))return 0;
    for(uint32_t patch=0;patch<FM1LAB_PATCH_COUNT;patch++){
        if(v->levels[patch]>127u)return 0;
        for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++)if(v->macros[patch][m]>100u)return 0;
    }
    return 1;
}
static inline int settings_import_v7(settings_t *dst,const settings_v7_t *src) {
    if(!dst||!settings_v7_valid(src))return -1;
    dst->swing=50u;
    dst->octave=src->octave;dst->patch=src->patch;dst->enabled_mask=src->enabled_mask;dst->ble_off=src->ble_off;
    for(uint32_t patch=0;patch<FM1LAB_PATCH_COUNT;patch++){
        dst->levels[patch]=src->levels[patch];
        for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++)dst->macros[patch][m]=src->macros[patch][m];
    }
    memcpy(&dst->performance,&src->performance,sizeof src->performance); settings_clear_events(dst);
    dst->performance.lane_instrument[0]=dst->performance.lane_instrument[1]=0u;
    for(uint32_t step=0;step<16u;step++)dst->performance.arp_step_gate[step]=0u;
    if(dst->performance.arp_step_gain[1]==69u&&dst->performance.arp_step_accent[1]==64u){
        dst->performance.arp_step_gain[1]=127u;
        dst->performance.arp_step_accent[1]=0u;
    }
    settings_migrate_step_events(dst);
    return 0;
}

/* V8 used the same persisted layout as V9, but could contain the known
 * accidental step-2 ARP seed (gain 69 / accent 64). Keep the repair in the
 * versioned migration so it is applied once and then saved as V9. */
static inline int settings_v8_valid(const settings_v8_t *v) {
    if(!v||v->magic!=SET_MAGIC_V8||v->octave < -2||v->octave>2||v->patch>=FM1LAB_PATCH_COUNT||
       (v->enabled_mask&0xF0u)||v->ble_off>1u||v->reserved>3u||
       !settings_legacy_performance_valid(&v->performance)||
       v->crc!=settings_crc16(v,offsetof(settings_v8_t,crc)))return 0;
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;p++){
        if(v->levels[p]>127u)return 0;
        for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++)if(v->macros[p][m]>100u)return 0;
    }
    return 1;
}
static inline int settings_import_v8(settings_t *dst,const settings_v8_t *src) {
    if(!dst||!settings_v8_valid(src))return -1;
    dst->swing=50u;
    dst->magic=SET_MAGIC;dst->octave=src->octave;dst->patch=src->patch;
    dst->enabled_mask=src->enabled_mask;dst->ble_off=src->ble_off;
    memcpy(dst->macros,src->macros,sizeof dst->macros);
    memcpy(dst->levels,src->levels,sizeof dst->levels);
    memcpy(&dst->performance,&src->performance,sizeof src->performance);
    dst->reserved=src->reserved;dst->crc=0u;
    /* V8 lane_patch values selected individual patches, not library Instruments. */
    dst->performance.lane_instrument[0]=dst->performance.lane_instrument[1]=0u;
    if(dst->performance.arp_step_gain[1]==69u&&dst->performance.arp_step_accent[1]==64u){
        dst->performance.arp_step_gain[1]=127u;
        dst->performance.arp_step_accent[1]=0u;
    }
    settings_migrate_step_events(dst);
    return 0;
}

static inline int settings_v9_valid(const settings_v9_t *v) {
    if(!v||v->magic!=SET_MAGIC_V9||v->octave < -2||v->octave>2||v->patch>=FM1LAB_PATCH_COUNT||
       (v->enabled_mask&0xF0u)||v->ble_off>1u||v->reserved>3u||
       !settings_legacy_performance_valid(&v->performance)||
       v->crc!=settings_crc16(v,offsetof(settings_v9_t,crc)))return 0;
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;p++) {
        if(v->levels[p]>127u)return 0;
        for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++)if(v->macros[p][m]>100u)return 0;
    }
    return 1;
}
static inline int settings_import_v9(settings_t *dst,const settings_v9_t *src) {
    if(!dst||!settings_v9_valid(src))return -1;
    dst->swing=50u;
    dst->magic=SET_MAGIC;dst->octave=src->octave;dst->patch=src->patch;
    dst->enabled_mask=src->enabled_mask;dst->ble_off=src->ble_off;
    memcpy(dst->macros,src->macros,sizeof dst->macros);
    memcpy(dst->levels,src->levels,sizeof dst->levels);
    memcpy(&dst->performance,&src->performance,sizeof src->performance);
    dst->reserved=src->reserved;dst->crc=0u;
    settings_migrate_step_events(dst);
    return 0;
}

static inline int settings_v10_valid(const settings_v10_t *v) {
    if(!v || v->magic!=SET_MAGIC_V10 || v->octave < -2 || v->octave>2 ||
       v->patch>=FM1LAB_PATCH_COUNT || (v->enabled_mask&0xF0u) || v->ble_off>1u ||
       v->reserved>3u || v->padding!=0u ||
       v->crc!=settings_crc16(v,offsetof(settings_v10_t,crc))) return 0;
    settings_t migrated={0};
    memcpy(&migrated.performance,&v->performance,sizeof v->performance);
    migrated.performance.chaos_amount=0u;migrated.performance.chaos_rate=42u;
    migrated.performance.chaos_tone=55u;migrated.performance.chaos_shape=0u;
    if(!fm6_performance_state_valid(&migrated.performance))return 0;
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;p++){
        if(v->levels[p]>127u)return 0;
        for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++)if(v->macros[p][m]>100u)return 0;
    }
    return 1;
}
static inline int settings_import_v10(settings_t *dst,const settings_v10_t *src) {
    if(!dst || !settings_v10_valid(src))return -1;
    dst->swing=50u;
    dst->magic=SET_MAGIC;dst->octave=src->octave;dst->patch=src->patch;
    dst->enabled_mask=src->enabled_mask;dst->ble_off=src->ble_off;
    memcpy(dst->macros,src->macros,sizeof dst->macros);
    memcpy(dst->levels,src->levels,sizeof dst->levels);
    memcpy(&dst->performance,&src->performance,sizeof src->performance);
    dst->performance.chaos_amount=0u;dst->performance.chaos_rate=42u;
    dst->performance.chaos_tone=55u;dst->performance.chaos_shape=0u;
    for(uint32_t lane=0;lane<2u;lane++) {
        for(uint32_t i=0;i<dst->performance.seq_event_count[lane];i++) {
            fm6_seq_event_t *event=&dst->performance.seq_events[lane][i];
            event->start=(uint8_t)(event->start*2u);
            event->duration=(uint8_t)(event->duration*2u);
        }
    }
    dst->reserved=src->reserved;dst->swing=50u;dst->crc=0u;
    return 0;
}
static inline int settings_v11_valid(const settings_v11_t *v) {
    if(!v || v->magic!=SET_MAGIC_V11 || v->octave < -2 || v->octave>2 ||
       v->patch>=FM1LAB_PATCH_COUNT || (v->enabled_mask&0xF0u) || v->ble_off>1u ||
       v->reserved>3u || v->padding!=0u ||
       v->crc!=settings_crc16(v,offsetof(settings_v11_t,crc)) ||
       !fm6_performance_state_valid(&v->performance)) return 0;
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;p++){
        if(v->levels[p]>127u)return 0;
        for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++)if(v->macros[p][m]>100u)return 0;
    }
    return 1;
}
static inline int settings_import_v11(settings_t *dst,const settings_v11_t *src) {
    if(!dst || !settings_v11_valid(src))return -1;
    dst->swing=50u;
    dst->magic=SET_MAGIC;dst->octave=src->octave;dst->patch=src->patch;
    dst->enabled_mask=src->enabled_mask;dst->ble_off=src->ble_off;
    memcpy(dst->macros,src->macros,sizeof dst->macros);
    memcpy(dst->levels,src->levels,sizeof dst->levels);
    memcpy(&dst->performance,&src->performance,sizeof src->performance);
    /* V11 stored positions in sixteenth steps. V12 stores 32nd-note ticks. */
    for(uint32_t lane=0;lane<2u;lane++) {
        for(uint32_t i=0;i<dst->performance.seq_event_count[lane];i++) {
            fm6_seq_event_t *event=&dst->performance.seq_events[lane][i];
            event->start=(uint8_t)(event->start*2u);
            event->duration=(uint8_t)(event->duration*2u);
        }
    }
    dst->reserved=src->reserved;dst->swing=50u;dst->crc=0u;
    return 0;
}

static inline int settings_valid(const settings_t *v) {
    if(!v || v->magic!=SET_MAGIC || v->octave < -2 || v->octave>2 || v->patch>=FM1LAB_PATCH_COUNT || (v->enabled_mask&0xF0u) || v->ble_off>1u || v->reserved>3u || v->swing<50u || v->swing>75u || !fm6_performance_state_valid(&v->performance) || v->crc!=settings_crc16(v,offsetof(settings_t,crc))) return 0;
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;++p) { if(v->levels[p]>127u) return 0; for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;++m) if(v->macros[p][m]>100u) return 0; }
    return 1;
}
static inline int settings_import_v2(settings_t *dst,const settings_v2_t *src) {
    if(!dst || !settings_v2_valid(src)) return -1;
    dst->swing=50u;
    dst->octave=src->octave; dst->patch=src->patch; dst->enabled_mask=src->enabled_mask; dst->ble_off=src->ble_off;
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;++p) { dst->levels[p]=src->levels[p]; for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;++m) dst->macros[p][m]=src->macros[p][m]; }
    return 0;
}
static inline int settings_import_v1(settings_t *dst,const settings_v1_t *src) {
    if(!dst || !settings_v1_valid(src)) return -1;
    dst->swing=50u;
    dst->octave=src->octave; dst->patch=src->patch; dst->enabled_mask=src->enabled_mask; dst->ble_off=src->ble_off; return 0;
}
