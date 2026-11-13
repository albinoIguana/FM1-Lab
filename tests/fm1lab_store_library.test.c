#include "fm1lab_store_library.h"
#include "app.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t objects[5][3840];
static uint16_t object_len[5];
static int present[5];
static int fail_obj=-1;

int plat_store_size(uint32_t obj) {
    if(obj<OBJ_INSTRUMENTS0 || obj>OBJ_INSERTS || !present[obj-OBJ_INSTRUMENTS0]) return -1;
    return object_len[obj-OBJ_INSTRUMENTS0];
}
int plat_store_load(uint32_t obj, void *dst, uint32_t max) {
    if(obj<OBJ_INSTRUMENTS0 || obj>OBJ_INSERTS || !present[obj-OBJ_INSTRUMENTS0] ||
       max<object_len[obj-OBJ_INSTRUMENTS0]) return -1;
    memcpy(dst,objects[obj-OBJ_INSTRUMENTS0],object_len[obj-OBJ_INSTRUMENTS0]);
    return object_len[obj-OBJ_INSTRUMENTS0];
}
int plat_store_save(uint32_t obj, const void *src, uint32_t len) {
    if((int)obj==fail_obj){fail_obj=-1;return -1;}
    if(obj<OBJ_INSTRUMENTS0 || obj>OBJ_INSERTS || len>3840u)return -1;
    memcpy(objects[obj-OBJ_INSTRUMENTS0],src,len);
    object_len[obj-OBJ_INSTRUMENTS0]=(uint16_t)len;
    present[obj-OBJ_INSTRUMENTS0]=1;
    return 0;
}

static void make_instrument(fm1lab_instrument_t *in,const char *name,unsigned seed)
{
    fm1lab_instrument_init(in,name);
    for(unsigned p=0;p<FM1LAB_PATCH_COUNT;p++){
        uint8_t *v=in->patches[p].voice;
        for(unsigned op=0;op<6;op++){
            uint8_t *q=v+op*21u;
            for(unsigned i=0;i<11;i++)q[i]=(uint8_t)((i*3u+p*7u+seed)%100u);
            q[11]=1;q[12]=2;q[13]=3;q[14]=1;q[15]=3;
            q[16]=(uint8_t)(70u+seed);q[17]=1;q[18]=14;q[19]=77;q[20]=4;
        }
        for(unsigned i=126;i<156;i++)v[i]=(uint8_t)(i%90u);
        v[134]=(uint8_t)(p+2u);v[135]=(uint8_t)(p+1u);
        v[136]=1;v[141]=1;v[142]=4;v[143]=6;v[155]=(uint8_t)(20u+p*30u);
        in->patches[p].voice[155]=(uint8_t)(20u+p*30u);
        in->patches[p].enabled=(uint8_t)(p&1u);
        in->patches[p].level=(uint8_t)(80u+p);
        for(unsigned m=0;m<4;m++)in->patches[p].macros[m]=(uint8_t)(20u+p*10u+m);
    }
}

int main(void)
{
    fm1lab_instrument_t factory,one,two,loaded;
    fm1lab_insert_state_t inserts,restored;
    make_instrument(&factory,"FACTORY QUAD",0u);
    assert(fm1lab_store_library_format(&factory)==0);
    for(unsigned b=0;b<4;b++)assert(object_len[b]==FM1LAB_STORE_PATCH_BANK_BYTES);
    assert(object_len[4]==FM1LAB_STORE_METADATA_BYTES);

    for(unsigned i=0;i<FM1LAB_STORE_INSTRUMENT_COUNT;i++){
        assert(fm1lab_store_library_load_full(i,&loaded,&restored)==0);
        for(unsigned p=0;p<FM1LAB_PATCH_COUNT;p++)
            assert(loaded.patch_ids[p]==(uint8_t)(i*4u+p));
    }

    /* IDs are stable save addresses: editing Instrument 0 never mutates
     * Instrument 1, even though both live in the same physical patch-bank object. */
    assert(fm1lab_store_library_load_full(0u,&one,&inserts)==0);
    assert(fm1lab_store_library_load_full(1u,&two,&restored)==0);
    uint8_t untouched=two.patches[0].voice[0];
    one.patches[0].voice[0]=(uint8_t)((one.patches[0].voice[0]+1u)%100u);
    one.patches[0].macros[0]=91u;
    one.patches[0].level=17u;
    one.patches[0].enabled=1u;
    one.patches[0].octave=-2;
    one.patches[0].fine_cents=37;
    inserts.patch_octave[0]=-2;
    inserts.patch_fine_cents[0]=37;
    assert(fm1lab_store_library_save_full(0u,&one,&inserts)==0);

    assert(fm1lab_store_library_load_full(1u,&loaded,&restored)==0);
    assert(loaded.patches[0].voice[0]==untouched);
    assert(fm1lab_store_library_load_full(0u,&loaded,&restored)==0);
    assert(loaded.patch_ids[0]==0u);
    assert(loaded.patches[0].voice[0]==one.patches[0].voice[0]);
    assert(loaded.patches[0].macros[0]==91u);
    assert(loaded.patches[0].level==17u);
    assert(loaded.patches[0].octave==-2 && loaded.patches[0].fine_cents==37);

    /* Instrument names and insert state are separate from patch payloads. */
    memcpy(loaded.name,"MY SAVED INSTRUMENT",19u);
    inserts.drive=73u;
    inserts.eq[1].gain=19u;
    assert(fm1lab_store_library_save_full(0u,&loaded,&inserts)==0);
    assert(fm1lab_store_library_load_full(0u,&loaded,&restored)==0);
    assert(memcmp(loaded.name,"MY SAVED INSTRUMENT",19u)==0);
    assert(restored.drive==73u && restored.eq[1].gain==19u);

    /* A failed patch-bank commit leaves the previous saved patch intact. */
    uint8_t saved=loaded.patches[1].voice[0];
    loaded.patches[1].voice[0]=(uint8_t)((saved+1u)%100u);
    fail_obj=OBJ_INSTRUMENTS0;
    assert(fm1lab_store_library_save_full(0u,&loaded,&restored)<0);
    assert(fm1lab_store_library_load_full(0u,&one,&inserts)==0);
    assert(one.patches[1].voice[0]==saved);

    /* CRC damage is reported; it is not silently replaced by factory data. */
    objects[0][100]^=1u;
    assert(fm1lab_store_library_load(0u,&loaded)<0);
    return 0;
}
