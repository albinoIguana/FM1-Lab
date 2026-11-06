#ifndef FM1LAB_INSERT_STATE_H
#define FM1LAB_INSERT_STATE_H
#include <stdint.h>
#define FM1LAB_EQ_BAND_COUNT 3u

enum {
    FM6_INSERT_AMP_ENABLED=0, FM6_INSERT_DRIVE, FM6_INSERT_TONE, FM6_INSERT_OUTPUT,
    FM6_INSERT_EQ_ENABLED,
    FM6_INSERT_EQ_LOW_FREQUENCY, FM6_INSERT_EQ_LOW_GAIN, FM6_INSERT_EQ_LOW_Q, FM6_INSERT_EQ_LOW_ENABLE,
    FM6_INSERT_EQ_MID_FREQUENCY, FM6_INSERT_EQ_MID_GAIN, FM6_INSERT_EQ_MID_Q, FM6_INSERT_EQ_MID_ENABLE,
    FM6_INSERT_EQ_HIGH_FREQUENCY, FM6_INSERT_EQ_HIGH_GAIN, FM6_INSERT_EQ_HIGH_Q, FM6_INSERT_EQ_HIGH_ENABLE,
    FM6_INSERT_FIELD_COUNT
};
enum {
    FM6_COMP_ENABLED=0, FM6_COMP_THRESHOLD, FM6_COMP_RATIO,
    FM6_COMP_ATTACK, FM6_COMP_RELEASE, FM6_COMP_MAKEUP, FM6_COMP_FIELD_COUNT
};

typedef struct { uint8_t frequency, gain, q; } fm1lab_eq_band_state_t;
typedef struct {
    uint8_t amp_enabled, drive, tone, output, eq_enabled;
    fm1lab_eq_band_state_t eq[FM1LAB_EQ_BAND_COUNT];
} fm1lab_patch_insert_state_t;
typedef struct { fm1lab_patch_insert_state_t patches[4]; } fm1lab_insert_state_t;

static inline void fm1lab_insert_state_init(fm1lab_insert_state_t *s) {
    if (!s) return;
    for (unsigned p=0;p<4u;p++) {
        fm1lab_patch_insert_state_t *v=&s->patches[p];
        v->amp_enabled=0u; v->drive=0u; v->tone=50u; v->output=50u;
        v->eq_enabled=14u; /* three bands individually armed; global EQ bypassed */
        v->eq[0].frequency=18u; v->eq[1].frequency=50u; v->eq[2].frequency=82u;
        for (unsigned b=0;b<3u;b++){v->eq[b].gain=12u;v->eq[b].q=10u;}
    }
}
static inline int fm1lab_insert_state_valid(const fm1lab_insert_state_t *s) {
    if (!s) return 0;
    for (unsigned p=0;p<4u;p++) {
        const fm1lab_patch_insert_state_t *v=&s->patches[p];
        if(v->amp_enabled>1u||v->drive>100u||v->tone>100u||v->output>100u||v->eq_enabled>15u)return 0;
        for(unsigned b=0;b<3u;b++)
            if(v->eq[b].frequency>100u||v->eq[b].gain>24u||v->eq[b].q>31u)return 0;
    }
    return 1;
}
static inline void fm1lab_patch_insert_state_set(fm1lab_patch_insert_state_t *v,unsigned field,uint8_t value) {
    if(!v||field>=FM6_INSERT_FIELD_COUNT)return;
    if(field==FM6_INSERT_AMP_ENABLED)v->amp_enabled=value?1u:0u;
    else if(field==FM6_INSERT_DRIVE)v->drive=value>100u?100u:value;
    else if(field==FM6_INSERT_TONE)v->tone=value>100u?100u:value;
    else if(field==FM6_INSERT_OUTPUT)v->output=value>100u?100u:value;
    else if(field==FM6_INSERT_EQ_ENABLED){if(value)v->eq_enabled|=1u;else v->eq_enabled&=(uint8_t)~1u;}
    else {
        unsigned offset=field-FM6_INSERT_EQ_LOW_FREQUENCY,band=offset/4u,param=offset%4u;
        if(band>=3u)return;
        if(param==0u)v->eq[band].frequency=value>100u?100u:value;
        else if(param==1u)v->eq[band].gain=value>24u?24u:value;
        else if(param==2u)v->eq[band].q=value>31u?31u:value;
        else {uint8_t bit=(uint8_t)(1u<<(band+1u));if(value)v->eq_enabled|=bit;else v->eq_enabled&=(uint8_t)~bit;}
    }
}
static inline uint8_t fm1lab_patch_insert_state_get(const fm1lab_patch_insert_state_t *v,unsigned field) {
    if(!v||field>=FM6_INSERT_FIELD_COUNT)return 0u;
    if(field==FM6_INSERT_AMP_ENABLED)return v->amp_enabled;
    if(field==FM6_INSERT_DRIVE)return v->drive;
    if(field==FM6_INSERT_TONE)return v->tone;
    if(field==FM6_INSERT_OUTPUT)return v->output;
    if(field==FM6_INSERT_EQ_ENABLED)return (uint8_t)(v->eq_enabled&1u);
    unsigned offset=field-FM6_INSERT_EQ_LOW_FREQUENCY,band=offset/4u,param=offset%4u;
    if(band>=3u)return 0u;
    if(param==0u)return v->eq[band].frequency;
    if(param==1u)return v->eq[band].gain;
    if(param==2u)return v->eq[band].q;
    return (uint8_t)((v->eq_enabled>>(band+1u))&1u);
}
#endif
