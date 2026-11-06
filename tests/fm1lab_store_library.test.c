#include "fm1lab_store_library.h"
#include "app.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t objects[5][3840];
static uint16_t object_len[5];
static int present[5];
static uint16_t test_crc16(const uint8_t *p, unsigned n) {
    uint16_t crc=0xffffu;
    while(n--) {
        crc^=(uint16_t)((uint16_t)*p++<<8);
        for(unsigned b=0;b<8u;b++) {
            uint16_t shifted=(uint16_t)(crc<<1);
            if(crc&0x8000u) shifted^=0x1021u;
            crc=shifted;
        }
    }
    return crc;
}
int plat_store_size(uint32_t obj) {
    if(obj<OBJ_INSTRUMENTS0 || obj>OBJ_INSERTS || !present[obj-OBJ_INSTRUMENTS0]) return -1;
    return object_len[obj-OBJ_INSTRUMENTS0];
}
int plat_store_load(uint32_t obj, void *dst, uint32_t max) {
    if(obj<OBJ_INSTRUMENTS0 || obj>OBJ_INSERTS || !present[obj-OBJ_INSTRUMENTS0] || max<object_len[obj-OBJ_INSTRUMENTS0]) return -1;
    memcpy(dst,objects[obj-OBJ_INSTRUMENTS0],object_len[obj-OBJ_INSTRUMENTS0]); return object_len[obj-OBJ_INSTRUMENTS0];
}
int plat_store_save(uint32_t obj, const void *src, uint32_t len) {
    if(obj<OBJ_INSTRUMENTS0 || obj>OBJ_INSERTS || len>3840u) return -1;
    memcpy(objects[obj-OBJ_INSTRUMENTS0],src,len); object_len[obj-OBJ_INSTRUMENTS0]=(uint16_t)len; present[obj-OBJ_INSTRUMENTS0]=1; return 0;
}
static void make_instrument(fm1lab_instrument_t *in) {
    fm1lab_instrument_init(in,"TEST QUAD VOICE");
    for(unsigned p=0;p<4;p++) {
        uint8_t *v=in->patches[p].voice;
        for(unsigned op=0;op<6;op++) {
            uint8_t *q=v+op*21u;
            for(unsigned i=0;i<11;i++) q[i]=(uint8_t)((i*3u+p*7u)%100u);
            q[11]=1; q[12]=2; q[13]=3; q[14]=1; q[15]=3;
            q[16]=88; q[17]=1; q[18]=14; q[19]=77; q[20]=4;
        }
        for(unsigned i=126;i<156;i++) v[i]=(uint8_t)(i%90u);
        v[134]=(uint8_t)(p+2u); v[135]=(uint8_t)(p+1u);
        v[136]=1; v[141]=1; v[142]=4; v[143]=6; v[155]=63;
        in->patches[p].voice[155]=(uint8_t)(20u+p*30u);
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
        assert(fm1lab_patch_pan(&loaded,p)==fm1lab_patch_pan(&source,p));
        assert(memcmp(loaded.patches[p].macros,source.patches[p].macros,4)==0);
        for(unsigned i=0;i<156;i++) assert(loaded.patches[p].voice[i]==source.patches[p].voice[i]);
    }
    assert(fm1lab_store_library_load(30,&loaded)==0);
    assert(memcmp(loaded.name,factory.name,11)==0);

    /* A V1 bank has no pan bits. Loading it migrates all patches to center
     * without changing the 3,840-byte record layout; the next save seals V2. */
    memcpy(objects[1], objects[0], 3840u);
    present[1]=1; object_len[1]=3840u;
    objects[1][4]=1u;
    objects[1][5]=1u; /* bank identity is covered by the header CRC */
    for(unsigned slot=0;slot<8u;slot++) {
        uint8_t *record=objects[1]+8u+slot*479u;
        memset(record+11u,0,448u); /* legacy DX7 voices have no reserved pan bits */
        for(unsigned p=0;p<4u;p++) {
            uint8_t *m=record+11u+448u+p*5u;
            m[0]=50u; m[1]=50u; m[2]=50u; m[3]=50u; m[4]=31u;
        }
    }
    uint16_t crc=test_crc16(objects[1]+8u,3832u);
    objects[1][6]=(uint8_t)crc; objects[1][7]=(uint8_t)(crc>>8);
    assert(fm1lab_store_library_load(8u,&loaded)==0);
    for(unsigned p=0;p<4u;p++) assert(fm1lab_patch_pan(&loaded,p)==64u);
    assert(fm1lab_store_library_save(8u,&loaded)==0);
    assert(objects[1][4]==2u);

    objects[3][100]^=1u;
    assert(fm1lab_store_library_load(31,&loaded)<0);
    fm1lab_insert_state_t inserts, restored;
    fm1lab_insert_state_init(&inserts);
    assert(fm1lab_store_library_load_full(0u,&loaded,&restored)==0);
    inserts.patches[2].amp_enabled=1u; inserts.patches[2].drive=73u;
    inserts.patches[2].tone=29u; inserts.patches[2].output=61u;
    inserts.patches[2].eq_enabled=1u; inserts.patches[2].eq[1].frequency=67u;
    inserts.patches[2].eq[1].gain=19u; inserts.patches[2].eq[1].q=23u;
    assert(fm1lab_store_library_save_full(0u,&source,&inserts)==0);
    assert(fm1lab_store_library_load_full(0u,&loaded,&restored)==0);
    assert(memcmp(&inserts,&restored,sizeof(inserts))==0);
    return 0;
}
