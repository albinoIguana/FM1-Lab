#include "fm1lab_store_library.h"
#include "app.h"

#define BANK_BYTES 3840u
#define BANK_HEADER 8u
#define RECORD_BYTES 479u
#define VOICE_BYTES 112u
#define NAME_BYTES 11u
#define PATCH_META_BYTES 5u
#define BANK_MAGIC 0x32424c46u /* FLB2 */
#define BANK_VERSION 2u

static PLAT_POOL uint8_t bank_work[BANK_BYTES];
#define INSERTS_V1_MAGIC 0x31534e49u
#define INSERTS_V1_SNAPSHOT_BYTES 1802u
#define INSERTS_V1_TOTAL_BYTES 3616u
#define INSERTS_V2_MAGIC 0x32534e49u
#define INSERTS_V2_SNAPSHOT_DATA_BYTES 1804u
#define INSERTS_V2_SNAPSHOT_BYTES 1806u
#define INSERTS_V2_TOTAL_BYTES 3624u
#define INSERTS_MAGIC 0x33534e49u
#define INSERTS_VERSION 3u
#define INSERTS_HEADER_BYTES 12u
#define INSERTS_SNAPSHOT_DATA_BYTES 460u
#define INSERTS_SNAPSHOT_BYTES 462u
#define INSERTS_TOTAL_BYTES 936u
#define INSERTS_STATE_OFFSET 12u
#define INSERTS_LEGACY_PATCH_BYTES 14u
#define INSERTS_LEGACY_INSTRUMENT_BYTES (4u * INSERTS_LEGACY_PATCH_BYTES)
_Static_assert(sizeof(fm1lab_insert_state_t)==14u, "Global insert state layout changed");
_Static_assert(INSERTS_TOTAL_BYTES<=3840u, "Insert sidecar exceeds one store sector");
/* Oversized read/scratch buffers intentionally accept the previous V1/V2
 * journal records before compacting them into the V3 global-Instrument form. */
static PLAT_POOL uint8_t inserts_work[INSERTS_V2_TOTAL_BYTES];
static PLAT_POOL uint8_t inserts_snapshot_work[INSERTS_V2_SNAPSHOT_BYTES];
static uint16_t inserts_bank_crc[FM1LAB_STORE_BANK_COUNT];
static uint8_t inserts_bank_generation[FM1LAB_STORE_BANK_COUNT];

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

static uint8_t *record_at(uint8_t *bank, unsigned slot)
{ return bank+BANK_HEADER+slot*RECORD_BYTES; }
static int bank_valid(const uint8_t *b, unsigned bank)
{
    return read32(b)==BANK_MAGIC && (b[4]==1u || b[4]==BANK_VERSION) && (b[5]&0x03u)==bank &&
        read16(b+6)==crc16(b+BANK_HEADER,BANK_BYTES-BANK_HEADER);
}
static void bank_init(uint8_t *b, unsigned bank)
{
    bytes_zero(b,BANK_BYTES);
    write32(b,BANK_MAGIC); b[4]=BANK_VERSION; b[5]=(uint8_t)bank;
    for(unsigned s=0;s<8;s++) {
        uint8_t *r=record_at(b,s);
        const char *prefix="INSTR ";
        for(unsigned n=0;n<6;n++) r[n]=(uint8_t)prefix[n];
        r[6]=(uint8_t)('0'+(s+1u)/10u); r[7]=(uint8_t)('0'+(s+1u)%10u);
        for(unsigned p=0;p<4;p++) {
            uint8_t *m=r+NAME_BYTES+448u+p*PATCH_META_BYTES;
            m[0]=50; m[1]=50; m[2]=50; m[3]=0xB2u; m[4]=31; /* macro 3=50 plus pan MSB for center; safe new patch level */
        }
    }
    write16(b+6,crc16(b+BANK_HEADER,BANK_BYTES-BANK_HEADER));
}
static int bank_read(unsigned bank, uint8_t *data)
{
    int n=plat_store_load(OBJ_INSTRUMENTS0+bank,data,BANK_BYTES);
    if(n<0) { bank_init(data,bank); return 0; }
    if(n!=(int)BANK_BYTES || !bank_valid(data,bank)) return -1;
    /* V1 used the same fixed record size but had no pan extension. Migrate
     * every old patch to centered pan using the reserved macro-byte MSBs. */
    if(data[4]==1u) {
        for(unsigned slot=0;slot<8u;slot++) {
            uint8_t *r=record_at(data,slot);
            for(unsigned p=0;p<4u;p++) {
                uint8_t *m=r+NAME_BYTES+448u+p*PATCH_META_BYTES;
                m[3]|=0x80u; /* pan 64: upper pan bit 3 */
            }
        }
        data[4]=BANK_VERSION;
        write16(data+6,crc16(data+BANK_HEADER,BANK_BYTES-BANK_HEADER));
    }
    return 0;
}
static void instrument_decode(const uint8_t *r, fm1lab_instrument_t *out)
{
    fm1lab_instrument_init(out,"");
    for(unsigned i=0;i<FM1LAB_INSTRUMENT_NAME_BYTES;i++)
        out->name[i]=i<NAME_BYTES?(char)r[i]:' ';
    for(unsigned p=0;p<4;p++) {
        uint8_t raw[128], voice[156];
        unpack7(raw,r+NAME_BYTES+p*VOICE_BYTES);
        raw_to_voice(voice,raw);
        (void)fm1lab_patch_set_voice(out,p,voice);
        const uint8_t *m=r+NAME_BYTES+448u+p*PATCH_META_BYTES;
        uint8_t pan=(uint8_t)((raw[111]>>4)&7u);
        for(unsigned k=0;k<4;k++) {
            out->patches[p].macros[k]=(uint8_t)(m[k]&127u);
            pan|=(uint8_t)(((m[k]>>7)&1u)<<(k+3u));
        }
        out->patches[p].voice[155]=pan;
        out->patches[p].enabled=(uint8_t)(m[4]>>7);
        out->patches[p].level=(uint8_t)(m[4]&127u);
    }
}
static void instrument_encode(uint8_t *r, const fm1lab_instrument_t *in)
{
    for(unsigned i=0;i<NAME_BYTES;i++) {
        unsigned char c=(unsigned char)in->name[i];
        r[i]=(uint8_t)(c>=32u&&c<=126u?c:' ');
    }
    for(unsigned p=0;p<4;p++) {
        uint8_t raw[128];
        voice_to_raw(raw,in->patches[p].voice);
        uint8_t pan=(uint8_t)(in->patches[p].voice[155]&127u);
        raw[111]=(uint8_t)((raw[111]&0x0Fu)|((pan&7u)<<4));
        pack7(r+NAME_BYTES+p*VOICE_BYTES,raw);
        uint8_t *m=r+NAME_BYTES+448u+p*PATCH_META_BYTES;
        for(unsigned k=0;k<4;k++)
            m[k]=(uint8_t)(in->patches[p].macros[k]|(((pan>>(k+3u))&1u)<<7));
        m[4]=(uint8_t)(in->patches[p].level|(in->patches[p].enabled?128u:0u));
    }
}


static uint16_t inserts_snapshot_crc(const uint8_t *s){return crc16(s,INSERTS_SNAPSHOT_DATA_BYTES);}
static int inserts_snapshot_valid(const uint8_t *s){return read16(s+INSERTS_SNAPSHOT_DATA_BYTES)==inserts_snapshot_crc(s);}
static void inserts_snapshot_seal(uint8_t *s){write16(s+INSERTS_SNAPSHOT_DATA_BYTES,inserts_snapshot_crc(s));}
static void inserts_header_seal(uint8_t *r){write32(r,INSERTS_MAGIC);write16(r+4,INSERTS_VERSION);write16(r+6,INSERTS_TOTAL_BYTES);write16(r+8,crc16(r,8));write16(r+10,0);}
static int inserts_record_valid(const uint8_t *r){
 const uint8_t *a=r+INSERTS_HEADER_BYTES,*f=a+INSERTS_SNAPSHOT_BYTES;
 return read32(r)==INSERTS_MAGIC&&read16(r+4)==INSERTS_VERSION&&read16(r+6)==INSERTS_TOTAL_BYTES&&
  read16(r+8)==crc16(r,8)&&(inserts_snapshot_valid(a)||inserts_snapshot_valid(f));
}
static int inserts_v1_snapshot_valid(const uint8_t *s){return read16(s+1800u)==crc16(s,1800u);}
static int inserts_v2_snapshot_valid(const uint8_t *s){return read16(s+INSERTS_V2_SNAPSHOT_DATA_BYTES)==crc16(s,INSERTS_V2_SNAPSHOT_DATA_BYTES);}

/* Old V1/V2 stored four independent patch inserts. Collapse to a single
 * Instrument-wide chain. Average controls for reference, but only retain
 * bypass states when all four patch settings agree; otherwise bypass avoids
 * unexpectedly applying one layer's old effect to the entire stack. */
static void inserts_legacy_collapse(fm1lab_insert_state_t *dst,const uint8_t *src){
 fm1lab_insert_state_init(dst);
 const uint8_t *p[4]={src,src+14u,src+28u,src+42u};
 unsigned sum_drive=0,sum_tone=0,sum_output=0;
 int amp_same=1,amp_all=1,eq_same=1,eq_all=1;
 for(unsigned i=0;i<4u;i++){
  sum_drive+=p[i][1];sum_tone+=p[i][2];sum_output+=p[i][3];
  if(i && (p[i][1]!=p[0][1]||p[i][2]!=p[0][2]||p[i][3]!=p[0][3]))amp_same=0;
  if(!p[i][0])amp_all=0;
  if(i && (p[i][4]!=p[0][4]))eq_same=0;
  if(!(p[i][4]&1u))eq_all=0;
 }
 dst->drive=(uint8_t)((sum_drive+2u)/4u);
 dst->tone=(uint8_t)((sum_tone+2u)/4u);
 dst->output=(uint8_t)((sum_output+2u)/4u);
 dst->amp_enabled=(uint8_t)(amp_same&&amp_all&&p[0][0]);
 dst->eq_enabled=(uint8_t)(eq_same&&eq_all?(p[0][4]&15u):14u);
 for(unsigned b=0;b<3u;b++){
  unsigned sf=0,sg=0,sq=0;int same=1,band_all=1;
  for(unsigned i=0;i<4u;i++){
   const uint8_t *v=p[i]+5u+b*3u;
   sf+=v[0];sg+=v[1];sq+=v[2];
   if(i && (v[0]!=p[0][5u+b*3u]||v[1]!=p[0][6u+b*3u]||v[2]!=p[0][7u+b*3u]))same=0;
   if(!(p[i][4]&(1u<<(b+1u))))band_all=0;
  }
  dst->eq[b].frequency=(uint8_t)((sf+2u)/4u);
  dst->eq[b].gain=(uint8_t)((sg+2u)/4u);
  dst->eq[b].q=(uint8_t)((sq+2u)/4u);
  if(!same || !band_all)dst->eq_enabled&=(uint8_t)~(1u<<(b+1u));
 }
}
static void inserts_snapshot_convert_v1(uint8_t *dst,const uint8_t *src){
 bytes_zero(dst,INSERTS_SNAPSHOT_BYTES);bytes_copy(dst,src,8u);
 for(unsigned b=0;b<FM1LAB_STORE_BANK_COUNT;b++)dst[8u+b]=0u;
 for(unsigned i=0;i<FM1LAB_STORE_INSTRUMENT_COUNT;i++){
  fm1lab_insert_state_t v;
  inserts_legacy_collapse(&v,src+8u+i*INSERTS_LEGACY_INSTRUMENT_BYTES);
  bytes_copy(dst+INSERTS_STATE_OFFSET+i*sizeof(v),&v,sizeof(v));
 }
 inserts_snapshot_seal(dst);
}
static void inserts_snapshot_convert_v2(uint8_t *dst,const uint8_t *src){
 bytes_zero(dst,INSERTS_SNAPSHOT_BYTES);bytes_copy(dst,src,12u);
 for(unsigned i=0;i<FM1LAB_STORE_INSTRUMENT_COUNT;i++){
  fm1lab_insert_state_t v;
  inserts_legacy_collapse(&v,src+12u+i*INSERTS_LEGACY_INSTRUMENT_BYTES);
  bytes_copy(dst+INSERTS_STATE_OFFSET+i*sizeof(v),&v,sizeof(v));
 }
 inserts_snapshot_seal(dst);
}
static int inserts_current_bank_crcs(void){
 for(unsigned b=0;b<FM1LAB_STORE_BANK_COUNT;b++){
  if(bank_read(b,bank_work)){inserts_bank_crc[b]=0u;inserts_bank_generation[b]=0u;continue;}
  inserts_bank_crc[b]=read16(bank_work+6u);
  inserts_bank_generation[b]=(uint8_t)(bank_work[5]>>2);
 }
 return 0;
}
static void inserts_record_load(uint8_t *r){
 int n=plat_store_load(OBJ_INSERTS,r,INSERTS_V2_TOTAL_BYTES);
 if(n==(int)INSERTS_V1_TOTAL_BYTES&&read32(r)==INSERTS_V1_MAGIC&&read16(r+4u)==1u&&
    read16(r+6u)==INSERTS_V1_TOTAL_BYTES&&read16(r+8u)==crc16(r,8u)){
  uint8_t *old_active=r+INSERTS_HEADER_BYTES,*old_fallback=old_active+INSERTS_V1_SNAPSHOT_BYTES;
  int active_ok=inserts_v1_snapshot_valid(old_active),fallback_ok=inserts_v1_snapshot_valid(old_fallback);
  if(fallback_ok){inserts_snapshot_convert_v1(inserts_snapshot_work,old_fallback);bytes_copy(r+INSERTS_HEADER_BYTES+INSERTS_SNAPSHOT_BYTES,inserts_snapshot_work,INSERTS_SNAPSHOT_BYTES);}
  else bytes_zero(r+INSERTS_HEADER_BYTES+INSERTS_SNAPSHOT_BYTES,INSERTS_SNAPSHOT_BYTES);
  if(active_ok){inserts_snapshot_convert_v1(inserts_snapshot_work,old_active);bytes_copy(r+INSERTS_HEADER_BYTES,inserts_snapshot_work,INSERTS_SNAPSHOT_BYTES);}
  else bytes_zero(r+INSERTS_HEADER_BYTES,INSERTS_SNAPSHOT_BYTES);
  inserts_header_seal(r);return;
 }
 if(n==(int)INSERTS_V2_TOTAL_BYTES&&read32(r)==INSERTS_V2_MAGIC&&read16(r+4u)==2u&&
    read16(r+6u)==INSERTS_V2_TOTAL_BYTES&&read16(r+8u)==crc16(r,8u)){
  uint8_t *old_active=r+INSERTS_HEADER_BYTES,*old_fallback=old_active+INSERTS_V2_SNAPSHOT_BYTES;
  int active_ok=inserts_v2_snapshot_valid(old_active),fallback_ok=inserts_v2_snapshot_valid(old_fallback);
  if(fallback_ok){inserts_snapshot_convert_v2(inserts_snapshot_work,old_fallback);bytes_copy(r+INSERTS_HEADER_BYTES+INSERTS_SNAPSHOT_BYTES,inserts_snapshot_work,INSERTS_SNAPSHOT_BYTES);}
  else bytes_zero(r+INSERTS_HEADER_BYTES+INSERTS_SNAPSHOT_BYTES,INSERTS_SNAPSHOT_BYTES);
  if(active_ok){inserts_snapshot_convert_v2(inserts_snapshot_work,old_active);bytes_copy(r+INSERTS_HEADER_BYTES,inserts_snapshot_work,INSERTS_SNAPSHOT_BYTES);}
  else bytes_zero(r+INSERTS_HEADER_BYTES,INSERTS_SNAPSHOT_BYTES);
  inserts_header_seal(r);return;
 }
 if(n!=(int)INSERTS_TOTAL_BYTES||!inserts_record_valid(r))bytes_zero(r,INSERTS_TOTAL_BYTES);
}
static void inserts_snapshot_resolve(uint8_t *dst,const uint8_t *r,
 const uint16_t crc[FM1LAB_STORE_BANK_COUNT],const uint8_t generation[FM1LAB_STORE_BANK_COUNT]){
 const uint8_t *a=r+INSERTS_HEADER_BYTES,*f=a+INSERTS_SNAPSHOT_BYTES;
 int av=inserts_snapshot_valid(a),fv=inserts_snapshot_valid(f);
 fm1lab_insert_state_t d;fm1lab_insert_state_init(&d);
 for(unsigned b=0;b<FM1LAB_STORE_BANK_COUNT;b++){
  int am=av&&read16(a+b*2u)==crc[b]&&a[8u+b]==generation[b];
  int fm=fv&&read16(f+b*2u)==crc[b]&&f[8u+b]==generation[b];
  const uint8_t *src=am?a:fm?f:0;
  write16(dst+b*2u,crc[b]);dst[8u+b]=generation[b];
  if(src)bytes_copy(dst+INSERTS_STATE_OFFSET+b*FM1LAB_STORE_INSTRUMENTS_PER_BANK*sizeof(d),
    src+INSERTS_STATE_OFFSET+b*FM1LAB_STORE_INSTRUMENTS_PER_BANK*sizeof(d),FM1LAB_STORE_INSTRUMENTS_PER_BANK*sizeof(d));
  else for(unsigned slot=0;slot<FM1LAB_STORE_INSTRUMENTS_PER_BANK;slot++){
   unsigned index=b*FM1LAB_STORE_INSTRUMENTS_PER_BANK+slot;
   bytes_copy(dst+INSERTS_STATE_OFFSET+index*sizeof(d),&d,sizeof(d));
  }
 }
 inserts_snapshot_seal(dst);
}
static int inserts_record_prepare(uint8_t *r,const uint16_t crc[FM1LAB_STORE_BANK_COUNT],
 const uint8_t generation[FM1LAB_STORE_BANK_COUNT],unsigned bank,unsigned slot,
 uint16_t new_crc,uint8_t new_generation,const fm1lab_insert_state_t *state){
 uint8_t *a=r+INSERTS_HEADER_BYTES,*f=a+INSERTS_SNAPSHOT_BYTES;
 if(!fm1lab_insert_state_valid(state)||bank>=FM1LAB_STORE_BANK_COUNT||slot>=FM1LAB_STORE_INSTRUMENTS_PER_BANK)return -1;
 inserts_snapshot_resolve(inserts_snapshot_work,r,crc,generation);
 bytes_copy(f,inserts_snapshot_work,INSERTS_SNAPSHOT_BYTES);
 bytes_copy(a,inserts_snapshot_work,INSERTS_SNAPSHOT_BYTES);
 bytes_copy(a+INSERTS_STATE_OFFSET+(bank*FM1LAB_STORE_INSTRUMENTS_PER_BANK+slot)*sizeof(*state),state,sizeof(*state));
 write16(a+bank*2u,new_crc);a[8u+bank]=new_generation;
 inserts_snapshot_seal(a);inserts_header_seal(r);return 0;
}
static int inserts_record_save(void){return plat_store_save(OBJ_INSERTS,inserts_work,INSERTS_TOTAL_BYTES);}

int fm1lab_store_library_load_full(unsigned index,fm1lab_instrument_t *out,fm1lab_insert_state_t *inserts){
 if(index>=FM1LAB_STORE_INSTRUMENT_COUNT||!out||!inserts)return -1;
 unsigned bank=index/FM1LAB_STORE_INSTRUMENTS_PER_BANK,slot=index%FM1LAB_STORE_INSTRUMENTS_PER_BANK;
 if(bank_read(bank,bank_work))return -1;
 instrument_decode(record_at(bank_work,slot),out);
 if(!fm1lab_instrument_valid(out))return -1;
 inserts_record_load(inserts_work);
 if(inserts_current_bank_crcs())return -1;
 inserts_snapshot_resolve(inserts_snapshot_work,inserts_work,inserts_bank_crc,inserts_bank_generation);
 bytes_copy(inserts,inserts_snapshot_work+INSERTS_STATE_OFFSET+index*sizeof(*inserts),sizeof(*inserts));
 return fm1lab_insert_state_valid(inserts)?0:-1;
}
int fm1lab_store_library_load(unsigned index,fm1lab_instrument_t *out){
 fm1lab_insert_state_t ignored;return fm1lab_store_library_load_full(index,out,&ignored);
}
int fm1lab_store_library_save_full(unsigned index,const fm1lab_instrument_t *in,const fm1lab_insert_state_t *inserts){
 if(index>=FM1LAB_STORE_INSTRUMENT_COUNT||!fm1lab_instrument_valid(in)||!fm1lab_insert_state_valid(inserts))return -1;
 unsigned bank=index/FM1LAB_STORE_INSTRUMENTS_PER_BANK,slot=index%FM1LAB_STORE_INSTRUMENTS_PER_BANK;
 if(inserts_current_bank_crcs()||bank_read(bank,bank_work))return -1;
 uint8_t new_generation=(uint8_t)((inserts_bank_generation[bank]+1u)&0x3fu);
 bank_work[5]=(uint8_t)((new_generation<<2)|bank);
 instrument_encode(record_at(bank_work,slot),in);
 write16(bank_work+6u,crc16(bank_work+BANK_HEADER,BANK_BYTES-BANK_HEADER));
 inserts_record_load(inserts_work);
 if(inserts_record_prepare(inserts_work,inserts_bank_crc,inserts_bank_generation,bank,slot,
    read16(bank_work+6u),new_generation,inserts))return -1;
 /* Journal first. CRC plus generation detects insert-only changes even when
  * the 479-byte voice bank itself would otherwise be byte-identical. */
 if(inserts_record_save())return -1;
 return plat_store_save(OBJ_INSTRUMENTS0+bank,bank_work,BANK_BYTES);
}
int fm1lab_store_library_save(unsigned index,const fm1lab_instrument_t *in){
 fm1lab_insert_state_t state;fm1lab_instrument_t ignored;
 if(fm1lab_store_library_load_full(index,&ignored,&state))fm1lab_insert_state_init(&state);
 return fm1lab_store_library_save_full(index,in,&state);
}
int fm1lab_store_library_format_bank(unsigned bank,const fm1lab_instrument_t *factory){
 fm1lab_insert_state_t defaults;
 if(bank>=FM1LAB_STORE_BANK_COUNT||!fm1lab_instrument_valid(factory)||inserts_current_bank_crcs())return -1;
 uint8_t new_generation=(uint8_t)((inserts_bank_generation[bank]+1u)&0x3fu);
 bank_init(bank_work,bank);
 bank_work[5]=(uint8_t)((new_generation<<2)|bank);
 for(unsigned slot=0;slot<FM1LAB_STORE_INSTRUMENTS_PER_BANK;slot++)
  instrument_encode(record_at(bank_work,slot),factory);
 write16(bank_work+6u,crc16(bank_work+BANK_HEADER,BANK_BYTES-BANK_HEADER));
 fm1lab_insert_state_init(&defaults);inserts_record_load(inserts_work);
 if(inserts_record_prepare(inserts_work,inserts_bank_crc,inserts_bank_generation,bank,0u,
    read16(bank_work+6u),new_generation,&defaults))return -1;
 inserts_snapshot_resolve(inserts_snapshot_work,inserts_work,inserts_bank_crc,inserts_bank_generation);
 for(unsigned slot=0;slot<FM1LAB_STORE_INSTRUMENTS_PER_BANK;slot++)
  bytes_copy(inserts_snapshot_work+INSERTS_STATE_OFFSET+(bank*FM1LAB_STORE_INSTRUMENTS_PER_BANK+slot)*sizeof(defaults),&defaults,sizeof(defaults));
 write16(inserts_snapshot_work+bank*2u,read16(bank_work+6u));
 inserts_snapshot_work[8u+bank]=new_generation;
 inserts_snapshot_seal(inserts_snapshot_work);
 bytes_copy(inserts_work+INSERTS_HEADER_BYTES,inserts_snapshot_work,INSERTS_SNAPSHOT_BYTES);
 inserts_header_seal(inserts_work);
 if(inserts_record_save())return -1;
 return plat_store_save(OBJ_INSTRUMENTS0+bank,bank_work,BANK_BYTES);
}
int fm1lab_store_library_format(const fm1lab_instrument_t *factory)
{
 if(!fm1lab_instrument_valid(factory))return -1;
 for(unsigned bank=0;bank<FM1LAB_STORE_BANK_COUNT;bank++)
  if(fm1lab_store_library_format_bank(bank,factory))return -1;
 return 0;
}
