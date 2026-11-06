#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fm1lab_settings_record.h"

/* The record codec delegates performance-state validation to FM6. Keep this
 * unit test focused on the settings envelope and migration boundary. */
int fm6_performance_state_valid(const fm6_performance_state_t *s)
{
    if (!s || s->tempo_bpm < 30u || s->tempo_bpm > 300u ||
        s->seq_length < 1u || s->seq_length > 64u || s->arp_mode > 3u)
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
    assert(settings_import_v3(&imported,&v3)==0);
    assert(imported.performance.tempo_bpm==128u);
    assert(imported.performance.fx_enabled[0]==1u && imported.performance.fx_send[2][2]==55u);
    assert(imported.performance.fx_enabled[FM6_FX_PHASER]==0u);
    assert(imported.performance.fx_send[2][FM6_FX_PHASER]==0u);
    assert(imported.performance.fx_depth[FM6_FX_PHASER]==65u);
    v3.performance.fx_send[0][0]=101u;
    copy=imported;
    assert(settings_import_v3(&copy,&v3)==-1);
    assert(memcmp(&copy,&imported,sizeof copy)==0);

    puts("FM1-Lab settings record CRC, validation and V1/V2/V3 migration tests passed");
    return 0;
}
