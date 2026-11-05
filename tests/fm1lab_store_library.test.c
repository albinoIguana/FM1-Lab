#include "fm1lab_store_library.h"
#include "app.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t objects[4][3840];
static int present[4];
int plat_store_size(uint32_t obj) {
    if(obj<OBJ_INSTRUMENTS0 || obj>OBJ_INSTRUMENTS3 || !present[obj-OBJ_INSTRUMENTS0]) return -1;
    return 3840;
}
int plat_store_load(uint32_t obj, void *dst, uint32_t max) {
    if(obj<OBJ_INSTRUMENTS0 || obj>OBJ_INSTRUMENTS3 || !present[obj-OBJ_INSTRUMENTS0] || max<3840) return -1;
    memcpy(dst,objects[obj-OBJ_INSTRUMENTS0],3840); return 3840;
}
int plat_store_save(uint32_t obj, const void *src, uint32_t len) {
    if(obj<OBJ_INSTRUMENTS0 || obj>OBJ_INSTRUMENTS3 || len!=3840) return -1;
    memcpy(objects[obj-OBJ_INSTRUMENTS0],src,3840); present[obj-OBJ_INSTRUMENTS0]=1; return 0;
}
static void make_instrument(fm1lab_instrument_t *in) {
    fm1lab_instrument_init(in,"TEST QUAD VOICE");
    for(unsigned p=0;p<4;p++) {
        uint8_t *v=in->patches[p].voice;
        for(unsigned i=0;i<156;i++) v[i]=(uint8_t)((i*3u+p*7u)&127u);
        v[134]=(uint8_t)(p+2u); v[135]=(uint8_t)(p+1u);
        in->patches[p].enabled=(uint8_t)(p&1u);
        in->patches[p].level=(uint8_t)(100u+p);
        for(unsigned m=0;m<4;m++) in->patches[p].macros[m]=(uint8_t)(20u+p*10u+m);
    }
}
int main(void) {
    fm1lab_instrument_t factory, source, loaded;
    make_instrument(&factory);
    assert(fm1lab_store_library_format(&factory)==0);
    for(unsigned i=0;i<32;i++) {
        assert(fm1lab_store_library_load(i,&loaded)==0);
        assert(loaded.patches[0].voice[134]==2);
    }
    make_instrument(&source);
    assert(fm1lab_store_library_save(31,&source)==0);
    assert(fm1lab_store_library_load(31,&loaded)==0);
    assert(memcmp(loaded.name,"TEST QUAD V",11)==0);
    for(unsigned p=0;p<4;p++) {
        assert(loaded.patches[p].voice[134]==source.patches[p].voice[134]);
        assert(loaded.patches[p].voice[135]==source.patches[p].voice[135]);
        assert(loaded.patches[p].enabled==source.patches[p].enabled);
        assert(loaded.patches[p].level==source.patches[p].level);
        assert(memcmp(loaded.patches[p].macros,source.patches[p].macros,4)==0);
        for(unsigned i=0;i<156;i++) assert(loaded.patches[p].voice[i]==source.patches[p].voice[i]);
    }
    assert(fm1lab_store_library_load(30,&loaded)==0);
    assert(memcmp(loaded.name,factory.name,11)==0);
    objects[3][100]^=1u;
    assert(fm1lab_store_library_load(31,&loaded)<0);
    return 0;
}
