#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fm6.h"

#define BLOCK 256u
#define BLOCKS 24u

static int32_t audio[BLOCK * 2u];

typedef struct {
    uint32_t hash;
    uint64_t energy;
    uint32_t peak;
} result_t;

static uint32_t hash_sample(uint32_t h, int32_t sample)
{
    uint32_t v = (uint32_t)sample;
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        h ^= (v >> (i * 8u)) & 0xFFu;
        h *= 16777619u;
    }
    return h;
}

static result_t render_note(uint32_t macro, uint8_t value)
{
    result_t r = {2166136261u, 0, 0};
    uint32_t b, i;
    fm6_init();
    if (macro < FM1LAB_MACRO_COUNT)
        fm6_macro_set(0, macro, value);
    fm6_note_on(60, 100);
    for (b = 0; b < BLOCKS; b++) {
        fm6_render(audio, BLOCK, 4096);
        for (i = 0; i < BLOCK; i++) {
            int32_t s = audio[i * 2u];
            uint32_t a = (uint32_t)(s < 0 ? -(int64_t)s : s);
            if (a > r.peak)
                r.peak = a;
            r.energy += (uint64_t)a * a;
            r.hash = hash_sample(r.hash, s);
        }
    }
    fm6_note_off(60);
    for (b = 0; b < 512u && fm6_active_voices(); b++)
        fm6_render(audio, BLOCK, 4096);
    return r;
}

static void require(int ok, const char *message)
{
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

int main(void)
{
    result_t base, repeat, neutral, bright, body, dirt;
    fm1lab_instrument_record_t record;
    uint32_t p, op, i, b;
    uint32_t active_peak = 0;

    fm6_init();
    require(fm6_patch_count() == 4u, "Instrument has exactly four FM patches");
    require(fm6_patch_name(0)[0] != 0, "factory patch 1 has a name");
    require(fm6_patch_name(1)[0] != 0, "factory patch 2 has a name");
    require(fm6_patch_name(2)[0] != 0, "factory patch 3 has a name");
    require(fm6_patch_name(3)[0] != 0, "factory patch 4 has a name");
    for (p = 0; p < 4u; p++) {
        require(fm6_patch_algorithm(p) < 32u, "algorithm index is in range");
        for (op = 1; op <= 6u; op++)
            require(fm6_operator_is_carrier(p, op) <= 1u, "operator role is a bit");
        for (i = 0; i < 4u; i++)
            require(fm6_macro_get(p, i) == 50u, "all four macros start neutral at 50");
    }

    base = render_note(99u, 50u);
    repeat = render_note(99u, 50u);
    neutral = render_note(FM1LAB_MACRO_BRIGHTNESS, 50u);
    bright = render_note(FM1LAB_MACRO_BRIGHTNESS, 80u);
    body = render_note(FM1LAB_MACRO_BODY, 80u);
    dirt = render_note(FM1LAB_MACRO_DIRT, 80u);
    require(base.energy > 0, "FM6 note produces non-silent audio");
    require(base.peak < 8388608u, "single-voice output stays within 24-bit range");
    require(base.hash == repeat.hash, "FM6 render is deterministic");
    require(base.hash == neutral.hash, "macro value 50 is non-destructive/neutral");
    require(base.hash != bright.hash, "BRIGHTNESS changes the spectrum");
    require(base.hash != body.hash, "BODY changes modulator ratio/envelopes");
    require(base.hash != dirt.hash, "DIRT changes feedback/detune");

    /* Instrument slots preserve all four patches, macros, enable state and name. */
    fm6_init();
    require(fm6_current_instrument() == 0u, "default Instrument slot is zero");
    fm6_select_instrument(1u);
    require(fm6_current_instrument() == 1u, "preset selection changes Instrument");
    require(!fm6_instrument_valid(1u), "unused Instrument slot starts empty");
    fm6_patch_set_algorithm(0u, 7u);
    fm6_macro_set(0u, FM1LAB_MACRO_BODY, 73u);
    fm6_instrument_set_name_char(0u, 'T');
    require(fm6_instrument_dirty(1u), "editing an Instrument marks it dirty");
    fm6_instrument_export_current(&record);
    require(record.valid && record.name[0] == 'T', "snapshot includes valid user name");
    require(record.macros[0][FM1LAB_MACRO_BODY] == 73u, "snapshot includes macro state");
    fm6_instrument_mark_saved(1u);
    require(!fm6_instrument_dirty(1u), "successful save clears dirty state");
    fm6_instrument_load(2u, &record);
    fm6_select_instrument(2u);
    require(fm6_instrument_valid(2u), "stored Instrument loads into another slot");
    require(fm6_instrument_name(2u)[0] == 'T', "Instrument name survives export/import");
    require(fm6_patch_algorithm(0u) == 7u, "patch algorithm survives export/import");
    require(fm6_macro_get(0u, FM1LAB_MACRO_BODY) == 73u, "macro value survives export/import");
    require(!fm6_instrument_dirty(2u), "loaded Instrument starts clean");

    fm6_init();
    for (p = 0; p < 4u; p++)
        fm6_patch_set_enabled(p, 1);
    fm6_note_on(60, 100);
    fm6_note_on(64, 100);
    require(fm6_active_voices() == FM1LAB_VOICE_LIMIT, "four layers share one eight-voice budget");
    fm6_note_on(67, 100); /* Must steal, not allocate beyond the fixed pool. */
    require(fm6_active_voices() == FM1LAB_VOICE_LIMIT, "voice stealing respects the shared limit");
    for (b = 0; b < 32u; b++) {
        fm6_render(audio, BLOCK, 4096);
        for (i = 0; i < BLOCK; i++) {
            uint32_t a = (uint32_t)(audio[i * 2u] < 0 ? -(int64_t)audio[i * 2u] : audio[i * 2u]);
            if (a > active_peak)
                active_peak = a;
        }
    }
    require(active_peak < 8388608u, "eight-voice mix is soft-limited to 24-bit output");
    fm6_note_off(60);
    fm6_note_off(64);
    fm6_note_off(67);
    for (b = 0; b < 512u && fm6_active_voices(); b++)
        fm6_render(audio, BLOCK, 4096);
    require(fm6_active_voices() == 0u, "note-off releases every voice without hanging");

    printf("FM6 host tests passed: 4 patches, 8 shared voices, deterministic audio, neutral macros, 24-bit limiter.\n");
    return 0;
}
