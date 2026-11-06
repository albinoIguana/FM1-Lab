#ifndef FM1LAB_INSERT_STATE_H
#define FM1LAB_INSERT_STATE_H
#include <stdint.h>
#define FM1LAB_EQ_BAND_COUNT 3u
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
  v->amp_enabled=0; v->drive=0; v->tone=50; v->output=50; v->eq_enabled=0;
  v->eq[0].frequency=18; v->eq[1].frequency=50; v->eq[2].frequency=82;
  for(unsigned b=0;b<FM1LAB_EQ_BAND_COUNT;b++){v->eq[b].gain=12;v->eq[b].q=10;}
 }
}
static inline int fm1lab_insert_state_valid(const fm1lab_insert_state_t *s) {
 if(!s)return 0;
 for(unsigned p=0;p<4u;p++){
  const fm1lab_patch_insert_state_t *v=&s->patches[p];
  if(v->amp_enabled>1u||v->drive>100u||v->tone>100u||v->output>100u||v->eq_enabled>1u)return 0;
  for(unsigned b=0;b<FM1LAB_EQ_BAND_COUNT;b++)
   if(v->eq[b].frequency>100u||v->eq[b].gain>24u||v->eq[b].q>31u)return 0;
 }
 return 1;
}
#endif
