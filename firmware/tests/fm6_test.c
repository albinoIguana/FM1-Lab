#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fm6.h"
#include "seq.h"
#include "storage_format.h"

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
    fm1lab_instrument_record_t chunk_in[6], chunk_out[6];
    uint8_t chunk_bytes[FM1LAB_STORE_CHUNK_HEADER_BYTES + 6u * sizeof(fm1lab_instrument_record_t)];
    uint32_t chunk_written = 0;
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

    /* Raw operator edits and algorithm routing are reflected by the editor API. */
    fm6_init();
    {
        uint8_t routes_before[6], changed = 0;
        uint32_t op;
        uint8_t level = fm6_operator_value(0u, 1u, FM6_OP_LEVEL);
        for (op = 1; op <= 6u; op++)
            routes_before[op - 1u] = fm6_operator_route(0u, op);
        require(fm6_operator_set_value(0u, 1u, FM6_OP_LEVEL,
                    (uint8_t)(level < 90u ? level + 5u : level - 5u)),
                "operator level edit accepted");
        require(fm6_operator_value(0u, 1u, FM6_OP_LEVEL) != level,
                "operator level edit changes the patch");
        require(fm6_instrument_dirty(0u), "raw operator edit marks Instrument dirty");
        fm6_patch_set_algorithm(0u, 31u);
        for (op = 1; op <= 6u; op++)
            if (routes_before[op - 1u] != fm6_operator_route(0u, op))
                changed = 1;
        require(changed, "algorithm change updates operator routing topology");
    }

    /* Chunk storage format checks header, version, length and CRC. */
    memset(chunk_in, 0, sizeof(chunk_in));
    for (i = 0; i < 6u; i++) {
        chunk_in[i].valid = 1;
        chunk_in[i].name[0] = (uint8_t)('A' + i);
        chunk_in[i].seq_tempo = 120u;
        chunk_in[i].seq_note[0][0] = (uint8_t)(60u + i);
        chunk_in[i].seq_velocity[0][0] = 100u;
    }
    require(fm1lab_store_chunk_encode(0u, 6u, chunk_in, chunk_bytes,
                sizeof(chunk_bytes), &chunk_written), "Instrument chunk encodes");
    require(chunk_written == sizeof(chunk_bytes), "Instrument chunk fits its sector budget");
    require(fm1lab_store_chunk_decode(0u, 6u, chunk_bytes, chunk_written, chunk_out),
            "Instrument chunk decodes with valid CRC");
    require(memcmp(chunk_in, chunk_out, sizeof(chunk_in)) == 0,
            "Instrument chunk round-trips every patch and sequence byte");
    chunk_bytes[FM1LAB_STORE_CHUNK_HEADER_BYTES + 10u] ^= 0x01u;
    require(!fm1lab_store_chunk_decode(0u, 6u, chunk_bytes, chunk_written, chunk_out),
            "Instrument chunk corruption is rejected by CRC");

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

    /* The sequencer is two lanes, saved per Instrument, with source-safe note-offs. */
    fm6_init();
    seq_init();
    seq_set_step(0u, 0u, 60u, 100u);
    seq_set_step(1u, 0u, 64u, 90u);
    seq_start();
    seq_process(32u);
    require(seq_running(), "sequencer transport starts");
    require(seq_current_step() == 0u, "sequencer starts on step zero");
    require(fm6_active_voices() == 2u, "two sequencer lanes create independent notes");
    seq_instrument_export_current(&record);
    require(record.seq_note[0][0] == 60u && record.seq_note[1][0] == 64u,
            "both lanes are included in Instrument save data");
    require(record.seq_tempo == 120u, "default sequence tempo is 120 BPM");
    seq_stop();
    seq_instrument_load(3u, &record);
    seq_select_instrument(3u);
    require(seq_step_note(0u, 0u) == 60u && seq_step_note(1u, 0u) == 64u,
            "two-lane sequence survives Instrument export/import");
    seq_set_tempo(250u);
    require(seq_tempo() == 240u, "tempo is clamped to supported maximum");
    seq_set_tempo(30u);
    require(seq_tempo() == 60u, "tempo is clamped to supported minimum");
    seq_select_instrument(4u);
    require(seq_step_note(0u, 0u) == 0xFFu, "each Instrument has its own sequence");

    /* Sequencer lane note-offs must not cut a same-pitch live keyboard note. */
    fm6_init();
    for (op = 1; op <= 6u; op++)
        fm6_operator_set_value(0u, op, FM6_OP_RELEASE, 99u);
    fm6_note_on_source(60u, 100u, 0u);
    fm6_note_on_source(60u, 100u, 1u);
    require(fm6_active_voices() == 2u, "same pitch can exist on two note sources");
    fm6_note_off_source(60u, 1u);
    for (b = 0; b < 64u; b++)
        fm6_render(audio, BLOCK, 4096);
    require(fm6_active_voices() == 1u, "lane A note-off leaves keyboard source held");
    fm6_note_off_source(60u, 0u);
    for (b = 0; b < 64u; b++)
        fm6_render(audio, BLOCK, 4096);
    require(fm6_active_voices() == 0u, "keyboard source releases independently");

    printf("FM6 host tests passed: 4 patches, 8 shared voices, deterministic audio, neutral macros, 24-bit limiter, saveable Instruments, two sequencer lanes.\n");
    return 0;
}
