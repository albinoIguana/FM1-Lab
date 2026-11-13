#include "fm1lab_store_library.h"
#include "app.h"

#define PATCH_BANK_HEADER 8u
#define PATCH_RECORD_BYTES 117u
#define VOICE_BYTES 112u
#define PATCH_BANK_MAGIC 0x33504C46u /* FLP3 */
#define PATCH_BANK_VERSION 3u
#define PATCH_BANK_BYTES FM1LAB_STORE_PATCH_BANK_BYTES
#define PATCH_RECORDS_BYTES (FM1LAB_STORE_PATCHES_PER_BANK * PATCH_RECORD_BYTES)
#define META_MAGIC 0x334D4C46u /* FLM3 */
#define META_VERSION 3u
#define META_HEADER_BYTES 12u
#define META_RECORD_BYTES (FM1LAB_INSTRUMENT_NAME_BYTES + FM1LAB_PATCH_COUNT + sizeof(fm1lab_insert_state_t))
#define META_SNAPSHOT_DATA_BYTES (FM1LAB_STORE_INSTRUMENT_COUNT * META_RECORD_BYTES)
#define META_SNAPSHOT_BYTES (META_SNAPSHOT_DATA_BYTES + 2u)
#define META_TOTAL_BYTES (META_HEADER_BYTES + 2u * META_SNAPSHOT_BYTES)

_Static_assert(sizeof(fm1lab_insert_state_t)==26u, "Instrument insert state layout changed");
_Static_assert(PATCH_BANK_HEADER + PATCH_RECORDS_BYTES == PATCH_BANK_BYTES, "Patch bank size mismatch");
_Static_assert(META_TOTAL_BYTES == FM1LAB_STORE_METADATA_BYTES, "Instrument directory size mismatch");
_Static_assert(META_TOTAL_BYTES <= 3840u, "Instrument directory exceeds pooled object capacity");

static PLAT_POOL uint8_t bank_work[PATCH_BANK_BYTES];
static PLAT_POOL uint8_t meta_work[META_TOTAL_BYTES];
static PLAT_POOL uint8_t meta_snapshot_work[META_SNAPSHOT_BYTES];

static void bytes_copy(void *dst, const void *src, unsigned n)
{ uint8_t *d=(uint8_t *)dst; const uint8_t *s=(const uint8_t *)src; while (n--) *d++ = *s++; }
static void bytes_zero(uint8_t *d, unsigned n)
{ while (n--) *d++ = 0; }
static uint16_t read16(const uint8_t *p)
{ return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static void write16(uint8_t *p, uint16_t v)
{ p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static uint32_t read32(const uint8_t *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
static void write32(uint8_t *p, uint32_t v)
{ p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }

static uint16_t crc16(const uint8_t *p, unsigned n)
{
    uint16_t crc=0xffffu;
    while(n--) {
        crc ^= (uint16_t)((uint16_t)*p++ << 8);
        for(unsigned b=0;b<8;b++) {
            uint16_t shifted=(uint16_t)(crc << 1);
            if(crc & 0x8000u) shifted ^= (uint16_t)0x1021u;
            crc=shifted;
        }
    }
    return crc;
}

/* FM1's standard seven-bit packed 128-byte DX7 VMEM representation. */
static void pack7(uint8_t out[112], const uint8_t in[128])
{
    bytes_zero(out,112);
    for(unsigned i=0;i<128;i++) {
        unsigned bit=i*7u, at=bit>>3, shift=bit&7u;
        out[at]|=(uint8_t)(in[i]<<shift);
        if(shift>1u) out[at+1u]|=(uint8_t)(in[i]>>(8u-shift));
    }
}
static void unpack7(uint8_t out[128], const uint8_t in[112])
{
    for(unsigned i=0;i<128;i++) {
        unsigned bit=i*7u, at=bit>>3, shift=bit&7u, v=in[at]>>shift;
        if(shift>1u) v|=(unsigned)in[at+1u]<<(8u-shift);
        out[i]=(uint8_t)(v&127u);
    }
}

/* Conversion follows the pinned M-VAVE app/fm_library.c VMEM mapping. */
static void raw_to_voice(uint8_t p[156], const uint8_t b[128])
{
    for(unsigned op=0;op<6;op++) {
        const uint8_t *v=b+op*17u; uint8_t *q=p+op*21u;
        bytes_copy(q,v,11); q[11]=v[11]&3u; q[12]=(v[11]>>2)&3u;
        q[13]=v[12]&7u; q[20]=(v[12]>>3)&15u;
        q[14]=v[13]&3u; q[15]=(v[13]>>2)&7u; q[16]=v[14];
        q[17]=v[15]&1u; q[18]=(v[15]>>1)&31u; q[19]=v[16];
    }
    bytes_copy(p+126,b+102,8); p[134]=b[110]&31u;
    p[135]=b[111]&7u; p[136]=(b[111]>>3)&1u;
    bytes_copy(p+137,b+112,4); p[141]=b[116]&1u;
    p[142]=(b[116]>>1)&7u; p[143]=(b[116]>>4)&7u;
    bytes_copy(p+144,b+117,11); p[155]=63;
}
static void voice_to_raw(uint8_t b[128], const uint8_t p[156])
{
    for(unsigned op=0;op<6;op++) {
        uint8_t *v=b+op*17u; const uint8_t *q=p+op*21u;
        bytes_copy(v,q,11); v[11]=q[11]|(q[12]<<2); v[12]=q[13]|(q[20]<<3);
        v[13]=q[14]|(q[15]<<2); v[14]=q[16]; v[15]=q[17]|(q[18]<<1); v[16]=q[19];
    }
    bytes_copy(b+102,p+126,8); b[110]=p[134]; b[111]=p[135]|(p[136]<<3);
    bytes_copy(b+112,p+137,4); b[116]=p[141]|(p[142]<<1)|(p[143]<<4);
    bytes_copy(b+117,p+144,11);
}


static uint8_t *patch_record_at(uint8_t *bank,unsigned slot)
{ return bank+PATCH_BANK_HEADER+slot*PATCH_RECORD_BYTES; }
static const uint8_t *meta_record_at(const uint8_t *snapshot,unsigned index)
{ return snapshot+index*META_RECORD_BYTES; }
static uint8_t *meta_record_at_mut(uint8_t *snapshot,unsigned index)
{ return snapshot+index*META_RECORD_BYTES; }

static void patch_bank_seal(uint8_t *bank,unsigned bank_index)
{
    write32(bank,PATCH_BANK_MAGIC);
    bank[4]=PATCH_BANK_VERSION;
    bank[5]=(uint8_t)bank_index;
    write16(bank+6,crc16(bank+PATCH_BANK_HEADER,PATCH_RECORDS_BYTES));
}
static int patch_bank_valid(const uint8_t *bank,unsigned bank_index)
{
    return read32(bank)==PATCH_BANK_MAGIC && bank[4]==PATCH_BANK_VERSION &&
        bank[5]==bank_index &&
        read16(bank+6)==crc16(bank+PATCH_BANK_HEADER,PATCH_RECORDS_BYTES);
}
static int patch_bank_load(unsigned bank)
{
    if(bank>=FM1LAB_STORE_BANK_COUNT)return -1;
    uint32_t object=OBJ_INSTRUMENTS0+bank;
    if(plat_store_size(object)!=(int)PATCH_BANK_BYTES)return -1;
    int n=plat_store_load(object,bank_work,sizeof bank_work);
    return n==(int)PATCH_BANK_BYTES && patch_bank_valid(bank_work,bank)?0:-1;
}
static int patch_bank_save(unsigned bank)
{
    patch_bank_seal(bank_work,bank);
    return plat_store_save(OBJ_INSTRUMENTS0+bank,bank_work,PATCH_BANK_BYTES);
}

/* A patch record is 112 packed DX7 VMEM bytes plus four macro bytes and
 * one level/on byte. Pan occupies reserved bits in the packed voice/macros. */
static void patch_decode(const uint8_t *record,fm1lab_patch_t *patch)
{
    uint8_t raw[128],voice[FM1LAB_PATCH_BYTES];
    unpack7(raw,record);
    raw_to_voice(voice,raw);
    bytes_copy(patch->voice,voice,FM1LAB_PATCH_BYTES);
    for(unsigned k=0;k<FM1LAB_MACRO_COUNT;k++)patch->macros[k]=50u;
    patch->enabled=1u;
    patch->level=31u;
    patch->octave=0;
    patch->fine_cents=0;
    const uint8_t *m=record+VOICE_BYTES;
    uint8_t pan=(uint8_t)((raw[111]>>4)&7u);
    for(unsigned k=0;k<FM1LAB_MACRO_COUNT;k++){
        patch->macros[k]=(uint8_t)(m[k]&127u);
        pan|=(uint8_t)(((m[k]>>7)&1u)<<(k+3u));
    }
    patch->voice[155]=pan;
    patch->enabled=(uint8_t)(m[4]>>7);
    patch->level=(uint8_t)(m[4]&127u);
}
static void patch_encode(uint8_t *record,const fm1lab_patch_t *patch)
{
    uint8_t raw[128];
    voice_to_raw(raw,patch->voice);
    uint8_t pan=(uint8_t)(patch->voice[155]&127u);
    raw[111]=(uint8_t)((raw[111]&0x0Fu)|((pan&7u)<<4));
    pack7(record,raw);
    uint8_t *m=record+VOICE_BYTES;
    for(unsigned k=0;k<FM1LAB_MACRO_COUNT;k++)
        m[k]=(uint8_t)(patch->macros[k]|(((pan>>(k+3u))&1u)<<7));
    m[4]=(uint8_t)(patch->level|(patch->enabled?128u:0u));
}

static void meta_snapshot_seal(uint8_t *snapshot)
{ write16(snapshot+META_SNAPSHOT_DATA_BYTES,crc16(snapshot,META_SNAPSHOT_DATA_BYTES)); }
static int meta_snapshot_valid(const uint8_t *snapshot)
{ return read16(snapshot+META_SNAPSHOT_DATA_BYTES)==crc16(snapshot,META_SNAPSHOT_DATA_BYTES); }
static void meta_header_seal(uint8_t *record)
{
    write32(record,META_MAGIC);
    record[4]=META_VERSION;
    record[5]=0u;
    write16(record+6,META_TOTAL_BYTES);
    write16(record+8,crc16(record,8u));
    write16(record+10,0u);
}
static int meta_header_valid(const uint8_t *record)
{
    return read32(record)==META_MAGIC && record[4]==META_VERSION &&
        read16(record+6)==META_TOTAL_BYTES && read16(record+8)==crc16(record,8u);
}
static void default_instrument_name(char *name,unsigned index)
{
    static const char prefix[]="INSTRUMENT ";
    for(unsigned i=0;i<sizeof(prefix)-1u;i++)name[i]=prefix[i];
    unsigned number=index+1u;
    name[sizeof(prefix)-1u]=(char)('0'+(number/10u));
    name[sizeof(prefix)]=(char)('0'+(number%10u));
    for(unsigned i=sizeof(prefix)+1u;i<FM1LAB_INSTRUMENT_NAME_BYTES;i++)name[i]=' ';
}
static void meta_snapshot_init(uint8_t *snapshot)
{
    bytes_zero(snapshot,META_SNAPSHOT_BYTES);
    for(unsigned i=0;i<FM1LAB_STORE_INSTRUMENT_COUNT;i++){
        uint8_t *r=meta_record_at_mut(snapshot,i);
        default_instrument_name((char *)r,i);
        for(unsigned p=0;p<FM1LAB_PATCH_COUNT;p++)r[FM1LAB_INSTRUMENT_NAME_BYTES+p]=(uint8_t)(i*4u+p);
        fm1lab_insert_state_t inserts;
        fm1lab_insert_state_init(&inserts);
        bytes_copy(r+FM1LAB_INSTRUMENT_NAME_BYTES+FM1LAB_PATCH_COUNT,&inserts,sizeof inserts);
    }
    meta_snapshot_seal(snapshot);
}
static int meta_snapshot_load(void)
{
    if(plat_store_size(OBJ_INSERTS)!=(int)META_TOTAL_BYTES)return -1;
    int n=plat_store_load(OBJ_INSERTS,meta_work,sizeof meta_work);
    if(n!=(int)META_TOTAL_BYTES || !meta_header_valid(meta_work))return -1;
    const uint8_t *active=meta_work+META_HEADER_BYTES;
    const uint8_t *fallback=active+META_SNAPSHOT_BYTES;
    if(meta_snapshot_valid(active))bytes_copy(meta_snapshot_work,active,META_SNAPSHOT_BYTES);
    else if(meta_snapshot_valid(fallback))bytes_copy(meta_snapshot_work,fallback,META_SNAPSHOT_BYTES);
    else return -1;
    return 0;
}
static int meta_snapshot_commit(void)
{
    uint8_t *active=meta_work+META_HEADER_BYTES;
    uint8_t *fallback=active+META_SNAPSHOT_BYTES;
    bytes_copy(fallback,meta_snapshot_work,META_SNAPSHOT_BYTES);
    bytes_copy(active,meta_snapshot_work,META_SNAPSHOT_BYTES);
    meta_snapshot_seal(active);
    meta_header_seal(meta_work);
    return plat_store_save(OBJ_INSERTS,meta_work,META_TOTAL_BYTES);
}
static void meta_snapshot_format(void)
{
    bytes_zero(meta_work,META_TOTAL_BYTES);
    meta_snapshot_init(meta_snapshot_work);
    bytes_copy(meta_work+META_HEADER_BYTES,meta_snapshot_work,META_SNAPSHOT_BYTES);
    bytes_copy(meta_work+META_HEADER_BYTES+META_SNAPSHOT_BYTES,meta_snapshot_work,META_SNAPSHOT_BYTES);
    meta_header_seal(meta_work);
}
static int meta_save_instrument(unsigned index,const fm1lab_instrument_t *in,
                                const fm1lab_insert_state_t *inserts)
{
    if(meta_snapshot_load()!=0)return -1;
    uint8_t *r=meta_record_at_mut(meta_snapshot_work,index);
    for(unsigned n=0;n<FM1LAB_INSTRUMENT_NAME_BYTES;n++){
        unsigned char c=(unsigned char)in->name[n];
        r[n]=(uint8_t)(c>=32u&&c<=126u?c:' ');
    }
    for(unsigned p=0;p<FM1LAB_PATCH_COUNT;p++)
        r[FM1LAB_INSTRUMENT_NAME_BYTES+p]=(uint8_t)(index*4u+p);
    bytes_copy(r+FM1LAB_INSTRUMENT_NAME_BYTES+FM1LAB_PATCH_COUNT,inserts,sizeof *inserts);
    return meta_snapshot_commit();
}
static int meta_load_instrument(unsigned index,fm1lab_instrument_t *out,
                                fm1lab_insert_state_t *inserts)
{
    if(meta_snapshot_load()!=0)return -1;
    const uint8_t *r=meta_record_at(meta_snapshot_work,index);
    for(unsigned n=0;n<FM1LAB_INSTRUMENT_NAME_BYTES;n++)out->name[n]=(char)r[n];
    for(unsigned p=0;p<FM1LAB_PATCH_COUNT;p++){
        uint8_t id=r[FM1LAB_INSTRUMENT_NAME_BYTES+p];
        if(id>=FM1LAB_PATCH_ID_COUNT || id!=(uint8_t)(index*4u+p))return -1;
        out->patch_ids[p]=id;
    }
    bytes_copy(inserts,r+FM1LAB_INSTRUMENT_NAME_BYTES+FM1LAB_PATCH_COUNT,sizeof *inserts);
    return fm1lab_insert_state_valid(inserts)?0:-1;
}

int fm1lab_store_library_load_full(unsigned index,fm1lab_instrument_t *out,
                                   fm1lab_insert_state_t *inserts)
{
    if(index>=FM1LAB_STORE_INSTRUMENT_COUNT || !out || !inserts)return -1;
    fm1lab_instrument_init(out,"");
    if(meta_load_instrument(index,out,inserts)!=0)return -1;
    unsigned bank=index/FM1LAB_STORE_INSTRUMENTS_PER_BANK;
    if(patch_bank_load(bank)!=0)return -1;
    unsigned first_slot=(index%FM1LAB_STORE_INSTRUMENTS_PER_BANK)*FM1LAB_PATCH_COUNT;
    for(unsigned p=0;p<FM1LAB_PATCH_COUNT;p++){
        patch_decode(patch_record_at(bank_work,first_slot+p),&out->patches[p]);
        out->patches[p].octave=inserts->patch_octave[p];
        out->patches[p].fine_cents=inserts->patch_fine_cents[p];
    }
    return fm1lab_instrument_valid(out)?0:-1;
}
int fm1lab_store_library_load(unsigned index,fm1lab_instrument_t *out)
{
    fm1lab_insert_state_t ignored;
    return fm1lab_store_library_load_full(index,out,&ignored);
}
int fm1lab_store_library_save_full(unsigned index,const fm1lab_instrument_t *in,
                                   const fm1lab_insert_state_t *inserts)
{
    if(index>=FM1LAB_STORE_INSTRUMENT_COUNT || !fm1lab_instrument_valid(in) ||
       !fm1lab_insert_state_valid(inserts))return -1;
    if(meta_snapshot_load()!=0)return -1;
    unsigned bank=index/FM1LAB_STORE_INSTRUMENTS_PER_BANK;
    if(patch_bank_load(bank)!=0)return -1;
    unsigned first_slot=(index%FM1LAB_STORE_INSTRUMENTS_PER_BANK)*FM1LAB_PATCH_COUNT;
    for(unsigned p=0;p<FM1LAB_PATCH_COUNT;p++)
        patch_encode(patch_record_at(bank_work,first_slot+p),&in->patches[p]);
    /* One pooled-object commit writes all four patch records for this Instrument. */
    if(patch_bank_save(bank)!=0)return -1;
    return meta_save_instrument(index,in,inserts);
}
int fm1lab_store_library_save(unsigned index,const fm1lab_instrument_t *in)
{
    fm1lab_insert_state_t inserts;
    fm1lab_instrument_t ignored;
    if(fm1lab_store_library_load_full(index,&ignored,&inserts)!=0)
        fm1lab_insert_state_init(&inserts);
    if(in){
        for(unsigned p=0;p<FM1LAB_PATCH_COUNT;p++){
            inserts.patch_octave[p]=in->patches[p].octave;
            inserts.patch_fine_cents[p]=in->patches[p].fine_cents;
        }
    }
    return fm1lab_store_library_save_full(index,in,&inserts);
}
int fm1lab_store_library_format_bank(unsigned bank,const fm1lab_instrument_t *factory)
{
    if(bank>=FM1LAB_STORE_BANK_COUNT || !fm1lab_instrument_valid(factory))return -1;
    bytes_zero(bank_work,PATCH_BANK_BYTES);
    for(unsigned instrument=0;instrument<FM1LAB_STORE_INSTRUMENTS_PER_BANK;instrument++)
        for(unsigned p=0;p<FM1LAB_PATCH_COUNT;p++)
            patch_encode(patch_record_at(bank_work,instrument*FM1LAB_PATCH_COUNT+p),
                         &factory->patches[p]);
    if(patch_bank_save(bank)!=0)return -1;

    if(meta_snapshot_load()!=0)meta_snapshot_format();
    for(unsigned slot=0;slot<FM1LAB_STORE_INSTRUMENTS_PER_BANK;slot++){
        unsigned index=bank*FM1LAB_STORE_INSTRUMENTS_PER_BANK+slot;
        uint8_t *r=meta_record_at_mut(meta_snapshot_work,index);
        for(unsigned n=0;n<FM1LAB_INSTRUMENT_NAME_BYTES;n++){
            unsigned char c=(unsigned char)factory->name[n];
            r[n]=(uint8_t)(c>=32u&&c<=126u?c:' ');
        }
        for(unsigned p=0;p<FM1LAB_PATCH_COUNT;p++)
            r[FM1LAB_INSTRUMENT_NAME_BYTES+p]=(uint8_t)(index*4u+p);
        fm1lab_insert_state_t inserts;
        fm1lab_insert_state_init(&inserts);
        for(unsigned p=0;p<FM1LAB_PATCH_COUNT;p++){
            inserts.patch_octave[p]=factory->patches[p].octave;
            inserts.patch_fine_cents[p]=factory->patches[p].fine_cents;
        }
        bytes_copy(r+FM1LAB_INSTRUMENT_NAME_BYTES+FM1LAB_PATCH_COUNT,&inserts,sizeof inserts);
    }
    return meta_snapshot_commit();
}
int fm1lab_store_library_format(const fm1lab_instrument_t *factory)
{
    if(!fm1lab_instrument_valid(factory))return -1;
    for(unsigned bank=0;bank<FM1LAB_STORE_BANK_COUNT;bank++)
        if(fm1lab_store_library_format_bank(bank,factory)!=0)return -1;
    return 0;
}
