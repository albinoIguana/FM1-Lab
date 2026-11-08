#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fm1lab_settings_record.h"

_Static_assert(offsetof(settings_v3_t, performance) == 28u, "legacy v3 settings prefix changed");
_Static_assert(sizeof(fm6_performance_state_v3_t) == 542u, "legacy v3 performance layout changed");

/* The record codec delegates performance-state validation to FM6. Keep this
 * unit test focused on the settings envelope and migration boundary. */
int fm6_performance_state_valid(const fm6_performance_state_t *s)
{
    if (!s || s->tempo_bpm < 30u || s->tempo_bpm > 300u ||
        s->seq_length < 1u || s->seq_length > 64u || s->arp_mode > 4u ||
        s->arp_division>5u || s->arp_gate<10u || s->arp_gate>100u ||
        s->arp_octaves<1u || s->arp_octaves>4u ||
        (s->arp_length!=8u&&s->arp_length!=16u) || s->arp_output>2u ||
        s->arp_random>100u || s->arp_macro_dest>3u || s->arp_depth>100u)
        return 0;
    for (unsigned lane = 0; lane < 2u; ++lane) {
        if (s->lane_patch[lane] >= FM1LAB_PATCH_COUNT) return 0;
        for (unsigned step = 0; step < 64u; ++step)
            if (s->seq_enabled[lane][step] > 1u || s->seq_velocity[lane][step] > 127u ||
                s->seq_gate[lane][step] > 100u || s->seq_note[lane][step] > 127u)
                return 0;
    }
    for (unsigned fx = 0; fx < FM6_FX_COUNT; ++fx)
        if (s->fx_enabled[fx] > 1u || s->fx_mix[fx] > 100u ||
            s->fx_time[fx] > 100u || s->fx_feedback[fx] > 100u || s->fx_depth[fx] > 100u)
            return 0;
    for (unsigned p = 0; p < FM1LAB_PATCH_COUNT; ++p)
        for (unsigned fx = 0; fx < FM6_FX_COUNT; ++fx)
            if (s->fx_send[p][fx] > 100u) return 0;
    if (s->compressor_enabled > 1u || s->compressor_threshold > 100u ||
        s->compressor_ratio > 100u || s->compressor_attack > 100u ||
        s->compressor_release > 100u || s->compressor_makeup > 100u) return 0;
    return 1;
}

static void make_valid(settings_t *s)
{
    memset(s, 0, sizeof *s);
    s->octave = -1; s->patch = 2u; s->enabled_mask = 0x0Fu; s->ble_off = 1u;
    for (unsigned p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        s->levels[p] = (uint8_t)(90u + p);
        for (unsigned m = 0; m < FM1LAB_MACRO_COUNT; ++m)
            s->macros[p][m] = (uint8_t)(40u + p + m);
    }
    s->performance.tempo_bpm = 123u;
    s->performance.seq_length = 64u;
    s->performance.arp_division=3u;s->performance.arp_gate=75u;
    s->performance.arp_octaves=1u;s->performance.arp_length=16u;
    s->performance.arp_output=0u;s->performance.arp_random=0u;
    s->performance.arp_macro_dest=0u;s->performance.arp_depth=50u;
    for(unsigned step=0;step<16u;step++){
        s->performance.arp_step_gain[step]=127u;
        s->performance.arp_step_accent[step]=0u;
        s->performance.arp_step_macro[step]=50u;
    }
    s->performance.lane_patch[0] = 1u;
    s->performance.lane_patch[1] = 3u;
    for (unsigned lane = 0; lane < 2u; ++lane)
        for (unsigned step = 0; step < 64u; ++step) {
            s->performance.seq_enabled[lane][step] = (uint8_t)(step & 1u);
            s->performance.seq_note[lane][step] = (uint8_t)(48u + step % 36u);
            s->performance.seq_velocity[lane][step] = 100u;
            s->performance.seq_gate[lane][step] = 75u;
        }
    settings_seal(s);
}

int main(void)
{
    settings_t good, copy;
    make_valid(&good);
    assert(settings_valid(&good));

    /* Every single-bit corruption of the persisted record must be rejected,
     * including corruption of either stored CRC byte. */
    for (size_t i = 0; i < sizeof good; ++i) {
        copy = good;
        ((uint8_t *)&copy)[i] ^= 1u;
        assert(!settings_valid(&copy));
    }

    /* A correctly re-sealed but semantically invalid record is also rejected. */
    copy = good; copy.octave = 3; settings_seal(&copy); assert(!settings_valid(&copy));
    copy = good; copy.macros[2][1] = 101u; settings_seal(&copy); assert(!settings_valid(&copy));
    copy = good; copy.performance.tempo_bpm = 29u; settings_seal(&copy); assert(!settings_valid(&copy));

    settings_v1_t v1 = {SET_MAGIC_V1, 2, 3u, 0x05u, 1u, {0u}};
    settings_t imported = good;
    assert(settings_v1_valid(&v1));
    assert(settings_import_v1(&imported, &v1) == 0);
    assert(imported.octave == 2 && imported.patch == 3u &&
           imported.enabled_mask == 0x05u && imported.ble_off == 1u);
    assert(imported.performance.tempo_bpm == good.performance.tempo_bpm);
    settings_v1_t bad_v1 = v1; bad_v1.patch = 4u;
    copy = imported; assert(settings_import_v1(&copy, &bad_v1) == -1);
    assert(memcmp(&copy, &imported, sizeof copy) == 0);

    settings_v2_t v2;
    memset(&v2, 0, sizeof v2);
    v2.magic = SET_MAGIC_V2; v2.octave = -2; v2.patch = 1u;
    v2.enabled_mask = 0x0Au; v2.ble_off = 0u;
    for (unsigned p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        v2.levels[p] = (uint8_t)(70u + p);
        for (unsigned m = 0; m < FM1LAB_MACRO_COUNT; ++m)
            v2.macros[p][m] = (uint8_t)(50u + p + m);
    }
    assert(settings_v2_valid(&v2));
    imported = good;
    assert(settings_import_v2(&imported, &v2) == 0);
    assert(imported.octave == -2 && imported.patch == 1u &&
           imported.enabled_mask == 0x0Au && imported.levels[3] == v2.levels[3] &&
           imported.macros[2][3] == v2.macros[2][3]);
    assert(imported.performance.tempo_bpm == good.performance.tempo_bpm);
    v2.macros[0][0] = 101u;
    copy = imported; assert(settings_import_v2(&copy, &v2) == -1);
    assert(memcmp(&copy, &imported, sizeof copy) == 0);

    /* V3 -> V4 migration keeps the three existing buses and initializes Phaser
     * disabled with a conservative depth; legacy records are CRC protected. */
    settings_v3_t v3;
    memset(&v3, 0, sizeof v3);
    v3.magic=SET_MAGIC_V3; v3.octave=0; v3.patch=1u; v3.enabled_mask=0x0Fu;
    v3.performance.tempo_bpm=128u; v3.performance.seq_length=16u;
    v3.performance.lane_patch[0]=0u; v3.performance.lane_patch[1]=2u;
    for(unsigned lane=0;lane<2u;++lane) for(unsigned step=0;step<64u;++step) {
        v3.performance.seq_note[lane][step]=60u;
        v3.performance.seq_velocity[lane][step]=100u;
        v3.performance.seq_gate[lane][step]=50u;
    }
    for(unsigned fx=0;fx<3u;++fx) {
        v3.performance.fx_enabled[fx]=1u;
        v3.performance.fx_mix[fx]=40u;
        v3.performance.fx_time[fx]=45u;
        v3.performance.fx_feedback[fx]=30u;
        for(unsigned p=0;p<FM1LAB_PATCH_COUNT;++p) v3.performance.fx_send[p][fx]=55u;
    }
    for(unsigned p=0;p<FM1LAB_PATCH_COUNT;++p) {
        v3.levels[p]=100u;
        for(unsigned m=0;m<FM1LAB_MACRO_COUNT;++m) v3.macros[p][m]=50u;
    }
    v3.crc=settings_crc16(&v3,offsetof(settings_v3_t,crc));
    imported=good;
    assert(settings_v3_valid(&v3));
    settings_v3_t damaged_v3=v3; damaged_v3.performance.fx_time[1]^=1u;
    assert(!settings_v3_valid(&damaged_v3));
    assert(settings_import_v3(&imported,&v3)==0);
    assert(imported.performance.tempo_bpm==128u);
    assert(imported.performance.fx_enabled[0]==1u && imported.performance.fx_send[2][2]==55u);
    assert(imported.performance.fx_enabled[FM6_FX_PHASER]==0u);
    assert(imported.performance.fx_send[2][FM6_FX_PHASER]==0u);
    assert(imported.performance.fx_depth[FM6_FX_PHASER]==65u);
    assert(imported.performance.compressor_enabled==0u);
    assert(imported.performance.compressor_threshold==65u);
    assert(imported.performance.compressor_ratio==40u);
    v3.performance.fx_send[0][0]=101u;
    copy=imported;
    assert(settings_import_v3(&copy,&v3)==-1);
    assert(memcmp(&copy,&imported,sizeof copy)==0);

    /* V4 is the last pre-compressor settings layout. It must migrate without
     * losing its four FX buses and must default the new compressor to bypass. */
    settings_v4_t v4;
    memset(&v4,0,sizeof v4);
    v4.magic=SET_MAGIC_V4; v4.octave=1; v4.patch=2u; v4.enabled_mask=0x0fu;
    v4.performance.tempo_bpm=141u; v4.performance.seq_length=32u;
    v4.performance.lane_patch[0]=1u; v4.performance.lane_patch[1]=3u;
    for(unsigned fx=0;fx<4u;fx++){
        v4.performance.fx_enabled[fx]=1u; v4.performance.fx_mix[fx]=40u;
        v4.performance.fx_time[fx]=45u; v4.performance.fx_feedback[fx]=30u;
        v4.performance.fx_depth[fx]=55u;
        for(unsigned p=0;p<FM1LAB_PATCH_COUNT;p++)v4.performance.fx_send[p][fx]=60u;
    }
    for(unsigned p=0;p<FM1LAB_PATCH_COUNT;p++){
        v4.levels[p]=31u;
        for(unsigned m=0;m<FM1LAB_MACRO_COUNT;m++)v4.macros[p][m]=50u;
    }
    v4.crc=settings_crc16(&v4,offsetof(settings_v4_t,crc));
    assert(settings_v4_valid(&v4));
    imported=good;
    assert(settings_import_v4(&imported,&v4)==0);
    assert(imported.performance.tempo_bpm==141u);
    assert(imported.performance.fx_enabled[FM6_FX_PHASER]==1u);
    assert(imported.performance.fx_send[3][FM6_FX_PHASER]==60u);
    assert(imported.performance.compressor_enabled==0u);
    assert(imported.performance.compressor_threshold==65u);
    assert(imported.performance.compressor_ratio==40u);
    assert(imported.performance.arp_division==3u&&imported.performance.arp_gate==75u);
    assert(imported.performance.arp_octaves==1u&&imported.performance.arp_length==16u);
    assert(imported.performance.arp_step_gain[0]==127u&&imported.performance.arp_step_macro[15]==50u);

    puts("FM1-Lab settings record CRC, validation and V1/V2/V3/V4/V5 migration tests passed");
    return 0;
}
