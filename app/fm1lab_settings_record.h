/* SPDX-License-Identifier: GPL-3.0-only
 * FM1-Lab settings record codec. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "fm6.h"
#define SET_MAGIC_V1 0x4C314D46u
#define SET_MAGIC_V2 0x4C314D47u
#define SET_MAGIC 0x4C314D48u
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off, rsv[1]; } settings_v1_t;
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off; uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT]; uint8_t levels[FM1LAB_PATCH_COUNT]; } settings_v2_t;
typedef struct { uint32_t magic; int8_t octave; uint8_t patch, enabled_mask, ble_off; uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT]; uint8_t levels[FM1LAB_PATCH_COUNT]; fm6_performance_state_t performance; uint16_t crc; } settings_t;
static inline uint16_t settings_crc16(const void *data, size_t length) {
    const uint8_t *bytes=(const uint8_t *)data; uint16_t crc=0xFFFFu;
    while(length--) { crc^=(uint16_t)*bytes++<<8; for(uint32_t bit=0;bit<8u;++bit) crc=(uint16_t)((crc&0x8000u)?(((uint32_t)crc<<1)^0x1021u):((uint32_t)crc<<1)); }
    return crc;
}
static inline void settings_seal(settings_t *v) { v->magic=SET_MAGIC; v->crc=settings_crc16(v,offsetof(settings_t,crc)); }
static inline int settings_v1_valid(const settings_v1_t *v) {
    return v && v->magic==SET_MAGIC_V1 && v->octave>=-2 && v->octave<=2 && v->patch<FM1LAB_PATCH_COUNT && !(v->enabled_mask&0xF0u) && v->ble_off<=1u;
}
static inline int settings_v2_valid(const settings_v2_t *v) {
    if(!v || v->magic!=SET_MAGIC_V2 || v->octave < -2 || v->octave>2 || v->patch>=FM1LAB_PATCH_COUNT || (v->enabled_mask&0xF0u) || v->ble_off>1u) return 0;
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;++p) { if(v->levels[p]>127u) return 0; for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;++m) if(v->macros[p][m]>100u) return 0; }
    return 1;
}
static inline int settings_valid(const settings_t *v) {
    if(!v || v->magic!=SET_MAGIC || v->octave < -2 || v->octave>2 || v->patch>=FM1LAB_PATCH_COUNT || (v->enabled_mask&0xF0u) || v->ble_off>1u || !fm6_performance_state_valid(&v->performance) || v->crc!=settings_crc16(v,offsetof(settings_t,crc))) return 0;
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;++p) { if(v->levels[p]>127u) return 0; for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;++m) if(v->macros[p][m]>100u) return 0; }
    return 1;
}
static inline int settings_import_v2(settings_t *dst,const settings_v2_t *src) {
    if(!dst || !settings_v2_valid(src)) return -1;
    dst->octave=src->octave; dst->patch=src->patch; dst->enabled_mask=src->enabled_mask; dst->ble_off=src->ble_off;
    for(uint32_t p=0;p<FM1LAB_PATCH_COUNT;++p) { dst->levels[p]=src->levels[p]; for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;++m) dst->macros[p][m]=src->macros[p][m]; }
    return 0;
}
static inline int settings_import_v1(settings_t *dst,const settings_v1_t *src) {
    if(!dst || !settings_v1_valid(src)) return -1;
    dst->octave=src->octave; dst->patch=src->patch; dst->enabled_mask=src->enabled_mask; dst->ble_off=src->ble_off; return 0;
}
