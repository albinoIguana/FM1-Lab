#include "fm1lab_runtime.h"
#include "fm1lab_library.h"
#include "fm1lab_bank_metadata.h"
#include "fm_patch.h"
#include "instrument.h"
#include <assert.h>
#include <string.h>
static uint8_t bank_raw[4096], bank_meta[FM1LAB_BANK_META_BYTES];
int groove_fm_library_read(unsigned bank,uint8_t raw[4096]){assert(bank<4);memcpy(raw,bank_raw,sizeof bank_raw);return 0;}
int groove_fm_library_metadata_read(unsigned bank,uint8_t meta[FM1LAB_BANK_META_BYTES]){assert(bank<4);memcpy(meta,bank_meta,sizeof bank_meta);return 0;}
int groove_fm_library_instrument_write(unsigned bank,const uint8_t raw[4096],const uint8_t meta[FM1LAB_BANK_META_BYTES]){assert(bank<4);memcpy(bank_raw,raw,sizeof bank_raw);memcpy(bank_meta,meta,sizeof bank_meta);return 0;}
void groove_fm_unpack(uint8_t p[156],const uint8_t v[128]){memset(p,0,156);memcpy(p,v,128);p[134]=v[110]&31u;}
void groove_fm_pack(uint8_t v[128],const uint8_t p[156]){memcpy(v,p,128);v[110]=p[134];}
int main(void){
 fm1lab_bank_metadata_t m;fm1lab_bank_metadata_init(&m);assert(!fm1lab_bank_metadata_encode(&m,bank_meta));
 fm1lab_runtime_t r={0};assert(!fm1lab_runtime_load(&r,0));assert(r.loaded&&r.selected_patch==0&&!r.dirty);
 assert(!fm1lab_runtime_select_patch(&r,3)&&r.selected_patch==3);
 assert(!fm1lab_runtime_toggle_patch(&r,2)&&r.dirty);
 assert(fm1lab_runtime_select_instrument(&r,1)==-2);
 assert(!fm1lab_runtime_set_level(&r,1,-200)&&r.instrument.patches[1].level==0);
 assert(!fm1lab_runtime_set_macro(&r,1,FM1LAB_BRIGHTNESS,100));
 uint8_t voice[FM1LAB_PATCH_BYTES],level=255;
 assert(fm1lab_runtime_render(&r,2,voice,&level)==0&&level==127);
 assert(fm1lab_runtime_render(&r,1,voice,&level)==1&&level==0);
 assert(!fm1lab_runtime_save(&r)&&!r.dirty);
 assert(!fm1lab_runtime_select_instrument(&r,-1)&&r.instrument_index==31);
 return 0;
}
