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

static void bytes_copy(uint8_t *d, const uint8_t *s, unsigned n)
{ while (n--) *d++ = *s++; }
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
    return read32(b)==BANK_MAGIC && (b[4]==1u || b[4]==BANK_VERSION) && b[5]==bank &&
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
int fm1lab_store_library_load(unsigned index, fm1lab_instrument_t *out)
{
    if(index>=32u||!out) return -1;
    unsigned b=index/8u, slot=index%8u;
    if(bank_read(b,bank_work)) return -1;
    instrument_decode(record_at(bank_work,slot),out);
    return fm1lab_instrument_valid(out)?0:-1;
}
int fm1lab_store_library_save(unsigned index, const fm1lab_instrument_t *in)
{
    if(index>=32u||!fm1lab_instrument_valid(in)) return -1;
    unsigned b=index/8u, slot=index%8u;
    if(bank_read(b,bank_work)) return -1;
    instrument_encode(record_at(bank_work,slot),in);
    write16(bank_work+6,crc16(bank_work+BANK_HEADER,BANK_BYTES-BANK_HEADER));
    return plat_store_save(OBJ_INSTRUMENTS0+b,bank_work,BANK_BYTES);
}

int fm1lab_store_library_format_bank(unsigned b, const fm1lab_instrument_t *factory)
{
    if(b>=FM1LAB_STORE_BANK_COUNT || !fm1lab_instrument_valid(factory)) return -1;
    bank_init(bank_work,b);
    for(unsigned slot=0;slot<FM1LAB_STORE_INSTRUMENTS_PER_BANK;slot++)
        instrument_encode(record_at(bank_work,slot),factory);
    write16(bank_work+6,crc16(bank_work+BANK_HEADER,BANK_BYTES-BANK_HEADER));
    return plat_store_save(OBJ_INSTRUMENTS0+b,bank_work,BANK_BYTES);
}
int fm1lab_store_library_format(const fm1lab_instrument_t *factory)
{
    if(!fm1lab_instrument_valid(factory)) return -1;
    for(unsigned b=0;b<FM1LAB_STORE_BANK_COUNT;b++)
        if(fm1lab_store_library_format_bank(b,factory)) return -1;
    return 0;
}
