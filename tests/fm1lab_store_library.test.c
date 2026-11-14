#include "fm1lab_store_library.h"
#include "app.h"
#include "fm1lab_input_dispatch.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t objects[5][3840];
static uint16_t object_len[5];
static int present[5];
static int fail_obj=-1;
static int fail_size_obj=-1;
static int wrong_size_obj=-1;
static int fail_load_obj=-1;

static uint16_t test_crc16(const uint8_t *p, unsigned n)
{
    uint16_t crc=0xffffu;
    while(n--) {
        crc ^= (uint16_t)((uint16_t)*p++ << 8);
        for(unsigned b=0;b<8;b++) {
            uint16_t shifted=(uint16_t)(crc << 1);
            if(crc & 0x8000u) shifted ^= 0x1021u;
            crc=shifted;
        }
    }
    return crc;
}
static void reseal_metadata_snapshot(void)
{
    unsigned data_bytes=FM1LAB_STORE_METADATA_BYTES-12u-2u;
    uint16_t crc=test_crc16(objects[4]+12u,data_bytes);
    objects[4][FM1LAB_STORE_METADATA_BYTES-2u]=(uint8_t)crc;
    objects[4][FM1LAB_STORE_METADATA_BYTES-1u]=(uint8_t)(crc>>8);
}

int plat_store_size(uint32_t obj) {
    if((int)obj==fail_size_obj)return -1;
    if((int)obj==wrong_size_obj && obj>=OBJ_INSTRUMENTS0 && obj<=OBJ_INSERTS && present[obj-OBJ_INSTRUMENTS0]) return (int)object_len[obj-OBJ_INSTRUMENTS0]-1;
    if(obj<OBJ_INSTRUMENTS0 || obj>OBJ_INSERTS || !present[obj-OBJ_INSTRUMENTS0]) return -1;
    return object_len[obj-OBJ_INSTRUMENTS0];
}
int plat_store_load(uint32_t obj, void *dst, uint32_t max) {
    if((int)obj==fail_load_obj)return -1;
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

static fm1lab_instrument_t dispatch_loaded;
static fm1lab_insert_state_t dispatch_inserts;
static int dispatch_load(void *ctx,uint8_t lane,uint8_t id,int mode)
{
    (void)ctx;(void)lane;(void)mode;
    return fm1lab_store_library_load_full(id,&dispatch_loaded,&dispatch_inserts);
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

    /* Exercise the same route + dispatcher used by app.c, ending in the
     * production store loader rather than a mocked successful callback. */
    {
        fm1lab_input_state_t state={0};
        fm1lab_input_ops_t ops={0};
        state.instrument=0u; state.track_instrument[0]=0u;
        ops.load_instrument=dispatch_load;
        enum fm1lab_input_action action=fm1lab_input_route(
            FM1LAB_INPUT_PAGE_HOME,FM1LAB_ENCODER_PRESETS,0u);
        assert(action==FM1LAB_INPUT_INSTRUMENT_STEP);
        assert(fm1lab_input_apply(&state,action,1,FM1LAB_STORE_INSTRUMENT_COUNT,
            FM1LAB_PATCH_COUNT,&ops,0)==FM1LAB_INPUT_RESULT_APPLIED);
        assert(state.instrument==1u);
        assert(memcmp(dispatch_loaded.name,"FACTORY QUAD",12u)==0);
        assert(fm1lab_store_library_last_load_stage()==FM1LAB_STORE_LOAD_OK);
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

    /* Fault injection identifies lookup/read/validation failures precisely. */
    fail_size_obj=OBJ_INSERTS;
    assert(fm1lab_store_library_load_full(0u,&loaded,&restored)<0);
    assert(fm1lab_store_library_last_load_stage()==FM1LAB_STORE_LOAD_META_LOOKUP);
    fail_size_obj=-1;
    wrong_size_obj=OBJ_INSERTS;
    assert(fm1lab_store_library_load_full(0u,&loaded,&restored)<0);
    assert(fm1lab_store_library_last_load_stage()==FM1LAB_STORE_LOAD_META_SIZE);
    wrong_size_obj=-1;
    fail_load_obj=OBJ_INSERTS;
    assert(fm1lab_store_library_load_full(0u,&loaded,&restored)<0);
    assert(fm1lab_store_library_last_load_stage()==FM1LAB_STORE_LOAD_META_READ);
    fail_load_obj=-1;
    fail_size_obj=OBJ_INSTRUMENTS0;
    assert(fm1lab_store_library_load_full(0u,&loaded,&restored)<0);
    assert(fm1lab_store_library_last_load_stage()==FM1LAB_STORE_LOAD_BANK_LOOKUP);
    fail_size_obj=-1;
    wrong_size_obj=OBJ_INSTRUMENTS0;
    assert(fm1lab_store_library_load_full(0u,&loaded,&restored)<0);
    assert(fm1lab_store_library_last_load_stage()==FM1LAB_STORE_LOAD_BANK_SIZE);
    wrong_size_obj=-1;
    fail_load_obj=OBJ_INSTRUMENTS0;
    assert(fm1lab_store_library_load_full(0u,&loaded,&restored)<0);
    assert(fm1lab_store_library_last_load_stage()==FM1LAB_STORE_LOAD_BANK_READ);
    fail_load_obj=-1;

    /* Initial formatting reports whether the patch-bank write or the
     * Instrument-directory commit failed; a generic META LOOKUP later must
     * not hide the original startup write failure. */
    fail_obj=OBJ_INSTRUMENTS0;
    assert(fm1lab_store_library_format_bank(0u,&factory)<0);
    assert(fm1lab_store_library_last_format_stage()==FM1LAB_STORE_FORMAT_BANK_SAVE);
    fail_obj=OBJ_INSERTS;
    assert(fm1lab_store_library_format_bank(0u,&factory)<0);
    assert(fm1lab_store_library_last_format_stage()==FM1LAB_STORE_FORMAT_META_SAVE);
    assert(fm1lab_store_library_format_bank(FM1LAB_STORE_BANK_COUNT,&factory)<0);
    assert(fm1lab_store_library_last_format_stage()==FM1LAB_STORE_FORMAT_BAD_ARGUMENT);
    assert(fm1lab_store_library_format_bank(0u,&factory)==0);
    assert(fm1lab_store_library_last_format_stage()==FM1LAB_STORE_FORMAT_OK);

    /* Header, snapshot CRC, patch references and insert sidecar validation
     * each fail at their own boundary rather than collapsing into LOAD FAILED. */
    uint8_t saved_meta[FM1LAB_STORE_METADATA_BYTES];
    memcpy(saved_meta,objects[4],sizeof saved_meta);
    objects[4][0]^=1u;
    assert(fm1lab_store_library_load(0u,&loaded)<0);
    assert(fm1lab_store_library_last_load_stage()==FM1LAB_STORE_LOAD_META_HEADER);
    memcpy(objects[4],saved_meta,sizeof saved_meta);
    objects[4][100]^=1u;
    assert(fm1lab_store_library_load(0u,&loaded)<0);
    assert(fm1lab_store_library_last_load_stage()==FM1LAB_STORE_LOAD_META_CRC);
    memcpy(objects[4],saved_meta,sizeof saved_meta);
    objects[4][12u+FM1LAB_INSTRUMENT_NAME_BYTES]=127u;
    reseal_metadata_snapshot();
    assert(fm1lab_store_library_load(0u,&loaded)<0);
    assert(fm1lab_store_library_last_load_stage()==FM1LAB_STORE_LOAD_PATCH_IDS);
    memcpy(objects[4],saved_meta,sizeof saved_meta);
    objects[4][12u+FM1LAB_INSTRUMENT_NAME_BYTES+FM1LAB_PATCH_COUNT]=2u;
    reseal_metadata_snapshot();
    assert(fm1lab_store_library_load(0u,&loaded)<0);
    assert(fm1lab_store_library_last_load_stage()==FM1LAB_STORE_LOAD_INSERTS);
    memcpy(objects[4],saved_meta,sizeof saved_meta);
    assert(fm1lab_store_library_load_full(FM1LAB_STORE_INSTRUMENT_COUNT,&loaded,&restored)<0);
    assert(fm1lab_store_library_last_load_stage()==FM1LAB_STORE_LOAD_BAD_ARGUMENT);
    assert(fm1lab_store_library_load_full(0u,0,&restored)<0);
    assert(fm1lab_store_library_last_load_stage()==FM1LAB_STORE_LOAD_BAD_ARGUMENT);

    /* CRC damage is reported; it is not silently replaced by factory data. */
    objects[0][100]^=1u;
    assert(fm1lab_store_library_load(0u,&loaded)<0);
    assert(fm1lab_store_library_last_load_stage()==FM1LAB_STORE_LOAD_BANK_DATA);
    return 0;
}
