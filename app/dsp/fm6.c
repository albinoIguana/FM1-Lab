/* SPDX-License-Identifier: GPL-3.0-only
 * FM1-Lab FM6 voice layer.
 *
 * FM6 core: Felucca's integer-only MSFA/Dexed-derived core, Apache-2.0.
 * The core is included in this translation unit because its compact API is
 * intentionally static; this wrapper supplies the four-patch instrument, four independent eight-voice
 * pools, and per-patch note allocation.
 */
#include <stdint.h>
#include "fm6.h"
#include "felucca_tables.h"
#include "felucca_fm6.h"

/* FM6 core source: Apache-2.0, Copyright Google / Pascal Gauthier / Leo Kuroshita.
 * Do not remove its SPDX header or attribution. */
#include "../../upstream/felucca/firmware/src/fm6_core.c"

static void fm6_copy_bytes(void *dst, const void *src, uint32_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--)
        *d++ = *s++;
}
static void fm6_zero_bytes(void *dst, uint32_t n)
{
    uint8_t *d = (uint8_t *)dst;
    while (n--)
        *d++ = 0;
}

#define PATCHES FM1LAB_PATCH_COUNT
#define VOICES (PATCHES * FM1LAB_VOICE_LIMIT)
#define Q24_ONE (1 << 24)
#define Q24_DB6 (1 << 24)
#define OUTPUT_LIMIT 8388607

typedef struct {
    uint8_t base[FP_SIZE + 1u];
    uint8_t effective[FP_SIZE + 1u];
    uint8_t macro[FM1LAB_MACRO_COUNT];
    uint8_t enabled;
    uint8_t level;
    int32_t mod_level;
    int32_t detune[6];
    uint8_t feedback;
    uint8_t ready;
} fm1lab_patch_t;

typedef struct {
    fm6_note_t state;
    uint32_t age;
    uint8_t patch;
    uint8_t note;
    uint8_t used;
    uint8_t fade_in;
    int16_t tune16;
} fm1lab_voice_t;

static fm1lab_patch_t instrument[PATCHES];
static fm1lab_voice_t voices[VOICES];
static fm6_lfo_t patch_lfo[PATCHES];
static int32_t patch_lfo_value[PATCHES];
static int32_t patch_lfo_delay[PATCHES];
static uint32_t age_counter;
static int32_t output_gain_q12;
static int16_t pitch_bend_16;

static int32_t clamp_i32(int32_t x, int32_t lo, int32_t hi)
{
    return x < lo ? lo : x > hi ? hi : x;
}

/* Felucca FM6 patch range validation, retained from its GPL engine wrapper. */
static uint32_t fm6_max(uint32_t i)
{
    static const uint8_t OPMAX[21] = {99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 3, 3, 7, 3, 7, 99, 1, 31, 99, 14};
    static const uint8_t VMAX[19] = {99, 99, 99, 99, 99, 99, 99, 99, 31, 7, 1, 99, 99, 99, 99, 1, 5, 7, 48};
    if (i < 126u)
        return OPMAX[i % 21u];
    if (i < FP_NAME)
        return VMAX[i - 126u];
    return 126u;
}
static void fm6_sanitize(uint8_t *v)
{
    uint32_t i;
    for (i = 0; i < FP_SIZE; i++) {
        if (i >= FP_NAME)
            v[i] = v[i] < 32u || v[i] > 126u ? ' ' : v[i];
        else if (v[i] > fm6_max(i))
            v[i] = (uint8_t)fm6_max(i);
    }
    v[FP_SIZE] = 0;
}

/* 128-byte Felucca bank record -> the 155-byte FM6 voice used by the core. */
static void fm6_unpack(const uint8_t *b, uint8_t *v)
{
    uint32_t k, i;
    for (k = 0; k < 6u; k++) {
        const uint8_t *o = b + k * 17u;
        uint8_t *d = v + k * FP_OP;
        for (i = 0; i < 11u; i++)
            d[i] = o[i] & 0x7Fu;
        d[FP_LC] = o[11] & 3u;
        d[FP_RC] = (o[11] >> 2) & 3u;
        d[FP_RS] = o[12] & 7u;
        d[FP_DET] = (o[12] >> 3) & 15u;
        d[FP_AMS] = o[13] & 3u;
        d[FP_KVS] = (o[13] >> 2) & 7u;
        d[FP_OL] = o[14] & 0x7Fu;
        d[FP_MODE] = o[15] & 1u;
        d[FP_FC] = (o[15] >> 1) & 31u;
        d[FP_FF] = o[16] & 0x7Fu;
    }
    for (i = 0; i < 9u; i++)
        v[FP_PR1 + i] = b[102 + i] & 0x7Fu;
    v[FP_ALG] &= 31u;
    v[FP_FB] = b[111] & 7u;
    v[FP_OKS] = (b[111] >> 3) & 1u;
    for (i = 0; i < 4u; i++)
        v[FP_LFS + i] = b[112 + i] & 0x7Fu;
    v[FP_LKS] = b[116] & 1u;
    v[FP_LFW] = (b[116] >> 1) & 7u;
    v[FP_LPMS] = (b[116] >> 4) & 7u;
    v[FP_TRNSP] = b[117] & 0x7Fu;
    for (i = 0; i < 10u; i++)
        v[FP_NAME + i] = b[118 + i] & 0x7Fu;
    fm6_sanitize(v);
}

static void fm6_rebuild_patch(uint32_t index)
{
    fm1lab_patch_t *p = &instrument[index];
    uint8_t *e = p->effective;
    const uint8_t *b = p->base;
    int32_t body = (int32_t)p->macro[FM1LAB_MACRO_BODY] - 50;
    int32_t motion = p->macro[FM1LAB_MACRO_MOTION];
    int32_t dirt = (int32_t)p->macro[FM1LAB_MACRO_EDGE] - 50;
    int32_t ratio = body * 4 / 50;
    int32_t meg = body * 40 / 50;
    uint32_t carriers, k, j, carrier_n = 0;
    static const int8_t spread[6] = {0, 1, -1, 2, -2, 3};
    int32_t cents_step = dirt * 70908 / 50; /* 12 cents at either macro extreme; Q24 log2 */
    fm6_copy_bytes(e, b, FP_SIZE + 1u);
    carriers = fm6_carriers(e[FP_ALG]);

    p->mod_level = ((int32_t)p->macro[FM1LAB_MACRO_BRIGHTNESS] - 50) * (6 << 24) / 50;
    p->feedback = (uint8_t)clamp_i32((int32_t)b[FP_FB] + dirt * 3 / 50, 0, 7);

    /* MOTION is a neutral-at-50 multiplier on the patch's LFO speed and depth. */
    e[FP_LFS] = (uint8_t)clamp_i32((int32_t)b[FP_LFS] * motion / 50, 0, 99);
    e[FP_LPMD] = (uint8_t)clamp_i32((int32_t)b[FP_LPMD] * motion / 50, 0, 99);
    e[FP_LAMD] = (uint8_t)clamp_i32((int32_t)b[FP_LAMD] * motion / 50, 0, 99);

    for (k = 0; k < 6u; k++) {
        uint32_t ki = 5u - k; /* UI order OP1..OP6; core storage is OP6..OP1. */
        uint8_t *op = e + ki * FP_OP;
        p->detune[ki] = 0;
        if ((carriers >> ki) & 1u) {
            p->detune[ki] = spread[carrier_n++] * cents_step;
            continue;
        }
        /* BODY is topology-aware: only modulators change ratio and envelope time. */
        if (!op[FP_MODE])
            op[FP_FC] = (uint8_t)clamp_i32((int32_t)op[FP_FC] + ratio, 0, 31);
        for (j = 0; j < 4u; j++)
            op[FP_R1 + j] = (uint8_t)clamp_i32((int32_t)op[FP_R1 + j] - meg, 0, 99);
    }
    fm6_lfo_reset(&patch_lfo[index], e);
    p->ready = 1;
}

static int32_t fm6_soft_clip(int64_t x)
{
    uint64_t a = x < 0 ? (uint64_t)(-(x + 1)) + 1u : (uint64_t)x;
    uint32_t index = (uint32_t)(a >> 16);
    int32_t y;
    if (index > 256u)
        index = 256u;
    y = (int32_t)(((int64_t)TANH_Q15[index] * OUTPUT_LIMIT) >> 15);
    return x < 0 ? -y : y;
}

/* Each patch owns a fixed, independent voice pool. Layering four patches can
 * therefore never steal a voice from another patch. */
static fm1lab_voice_t *fm6_allocate_voice(uint32_t patch)
{
    uint32_t first = patch * FM1LAB_VOICE_LIMIT;
    uint32_t end = first + FM1LAB_VOICE_LIMIT;
    uint32_t i, oldest = first;
    for (i = first; i < end; ++i)
        if (!voices[i].used)
            return &voices[i];
    for (i = first + 1u; i < end; ++i)
        if ((int32_t)(voices[i].age - voices[oldest].age) < 0)
            oldest = i;
    return &voices[oldest];
}

void fm6_init(void)
{
    uint32_t i;
    fm6_zero_bytes(instrument, sizeof(instrument));
    fm6_zero_bytes(voices, sizeof(voices));
    fm6_zero_bytes(patch_lfo, sizeof(patch_lfo));
    fm6_zero_bytes(patch_lfo_value, sizeof(patch_lfo_value));
    fm6_zero_bytes(patch_lfo_delay, sizeof(patch_lfo_delay));
    age_counter = 0;
    output_gain_q12 = 0;
    pitch_bend_16 = 0;
    for (i = 0; i < PATCHES; i++) {
        fm6_unpack(FM6_FACTORY[i], instrument[i].base);
        instrument[i].macro[0] = 50;
        instrument[i].macro[1] = 50;
        instrument[i].macro[2] = 50;
        instrument[i].macro[3] = 50;
        instrument[i].enabled = (i == 0u); /* one layer by default; enable more as desired */
        instrument[i].level = 127u;
        fm6_rebuild_patch(i);
    }
}

void fm6_panic(void)
{
    fm6_zero_bytes(voices, sizeof(voices));
    pitch_bend_16 = 0;
}

void fm6_note_on_tuned_mask(uint8_t note, uint8_t velocity,
                                           uint8_t patch_mask, int16_t tune16)
{
    uint32_t i;
    if (note > 127u || !velocity)
        return;
    if (tune16 > 32) tune16 = 32;
    if (tune16 < -32) tune16 = -32;
    for (i = 0; i < PATCHES; i++) {
        fm1lab_patch_t *p = &instrument[i];
        fm1lab_voice_t *v;
        if (!(patch_mask & (1u << i)) || !p->enabled || !p->level)
            continue;
        v = fm6_allocate_voice(i);
        v->patch = (uint8_t)i;
        v->note = note;
        v->used = 1;
        v->age = ++age_counter;
        v->fade_in = 32;
        v->tune16 = tune16;
        fm6_note_init(&v->state, p->effective, (int32_t)note + p->effective[FP_TRNSP] - 24, velocity, 1);
        fm6_lfo_key(&patch_lfo[i]);
    }
}

void fm6_note_on_mask(uint8_t note, uint8_t velocity, uint8_t patch_mask)
{
    fm6_note_on_tuned_mask(note, velocity, patch_mask, 0);
}

void fm6_note_on(uint8_t note, uint8_t velocity)
{
    fm6_note_on_mask(note, velocity, (uint8_t)((1u << PATCHES) - 1u));
}

void fm6_note_off_mask(uint8_t note, uint8_t patch_mask)
{
    uint32_t i;
    for (i = 0; i < VOICES; i++) {
        fm1lab_voice_t *v = &voices[i];
        if (v->used && v->note == note && (patch_mask & (1u << v->patch))) {
            fm1lab_patch_t *p = &instrument[v->patch];
            fm6_note_key(&v->state, p->effective, 0);
        }
    }
}

void fm6_note_off(uint8_t note)
{
    fm6_note_off_mask(note, (uint8_t)((1u << PATCHES) - 1u));
}

void fm6_pitch_bend(uint16_t value)
{
    if (value > 16383u) value = 16383u;
    pitch_bend_16 = (int16_t)(((int32_t)value - 8192) * 32 / 8192);
}

void fm6_render(int32_t *stereo, uint32_t frames, uint32_t master_q12)
{
    uint32_t base;
    for (base = 0; base < frames; base += FM6_N) {
        int64_t mix[FM6_N] = {0};
        int32_t patch_gain_q15[PATCHES] = {0};
        uint32_t active_layers = 0;
        uint32_t p, v, i;
        uint32_t count = frames - base < FM6_N ? frames - base : FM6_N;

        {
            uint8_t patch_has_voice[PATCHES] = {0};
            for (v = 0; v < VOICES; ++v)
                if (voices[v].used && voices[v].patch < PATCHES)
                    patch_has_voice[voices[v].patch] = 1u;
            for (p = 0; p < PATCHES; ++p)
                active_layers += patch_has_voice[p] && instrument[p].enabled && instrument[p].level != 0u;
            if (active_layers) {
                for (p = 0; p < PATCHES; ++p)
                    if (patch_has_voice[p] && instrument[p].enabled && instrument[p].level)
                        patch_gain_q15[p] = (int32_t)((uint32_t)instrument[p].level * 32768u /
                                                      (127u * active_layers));
            }
        }
        for (p = 0; p < PATCHES; p++) {
            patch_lfo_value[p] = fm6_lfo_sample(&patch_lfo[p]);
            patch_lfo_delay[p] = fm6_lfo_delay(&patch_lfo[p]);
        }

        for (v = 0; v < VOICES; v++) {
            fm1lab_voice_t *voice = &voices[v];
            fm1lab_patch_t *patch;
            int32_t bus[FM6_N];
            int32_t pitch, fb;
            if (!voice->used)
                continue;
            patch = &instrument[voice->patch];
            pitch = fm6_note_logfreq(((int32_t)voice->note + patch->effective[FP_TRNSP] - 24) * 16 + voice->tune16 + pitch_bend_16, 0);
            fb = patch->feedback;
            if (fm6_note_compute(&voice->state, patch->effective, bus,
                                 patch_lfo_value[voice->patch], patch_lfo_delay[voice->patch],
                                 pitch, patch->effective[FP_ALG], fb, patch->detune, patch->mod_level)) {
                int32_t dc = voice->state.dc;
                for (i = 0; i < FM6_N; i++) {
                    int32_t x = bus[i] - dc;
                    dc += x >> 10; /* ~7 Hz high-pass; FM sidebands can otherwise leave DC. */
                    if (voice->fade_in) {
                        uint32_t gain = 32u - voice->fade_in;
                        x = (int32_t)(((int64_t)x * gain) >> 5);
                        voice->fade_in--;
                    }
                    mix[i] += ((int64_t)x * patch_gain_q15[voice->patch]) >> 15;
                }
                voice->state.dc = dc;
            }
            if (fm6_note_done(&voice->state, patch->effective, patch->effective[FP_ALG])) {
                voice->state.live = 0;
                voice->used = 0;
            }
        }

        for (i = 0; i < count; i++) {
            int32_t delta = (int32_t)master_q12 - output_gain_q12;
            if (delta)
                output_gain_q12 += delta / 64 + (delta > 0 ? 1 : -1);
            stereo[(base + i) * 2u] = fm6_soft_clip((mix[i] * output_gain_q12) >> 15);
            stereo[(base + i) * 2u + 1u] = stereo[(base + i) * 2u];
        }
    }
}

uint32_t fm6_patch_count(void) { return PATCHES; }
const char *fm6_patch_name(uint32_t patch) { return patch < PATCHES ? (const char *)&instrument[patch].base[FP_NAME] : ""; }
uint8_t fm6_patch_enabled(uint32_t patch) { return patch < PATCHES ? instrument[patch].enabled : 0; }
void fm6_patch_set_enabled(uint32_t patch, uint8_t enabled)
{
    uint32_t i;
    if (patch >= PATCHES)
        return;
    instrument[patch].enabled = enabled != 0;
    if (!enabled)
        for (i = 0; i < VOICES; i++)
            if (voices[i].used && voices[i].patch == patch)
                voices[i].used = 0;
}
uint8_t fm6_patch_algorithm(uint32_t patch) { return patch < PATCHES ? instrument[patch].effective[FP_ALG] : 0; }
uint8_t fm6_operator_is_carrier(uint32_t patch, uint32_t op_1_to_6)
{
    uint32_t bit;
    if (patch >= PATCHES || op_1_to_6 < 1u || op_1_to_6 > 6u)
        return 0;
    bit = 5u - (op_1_to_6 - 1u);
    return (uint8_t)((fm6_carriers(instrument[patch].effective[FP_ALG]) >> bit) & 1u);
}
const uint8_t *fm6_patch_data(uint32_t patch) { return patch < PATCHES ? instrument[patch].base : 0; }

int fm6_patch_set_voice(uint32_t patch, const uint8_t *voice)
{
    if (patch >= PATCHES || !voice) return -1;
    for (uint32_t i = 0; i < FP_SIZE; ++i)
        if (voice[i] > 127u) return -1;
    fm6_copy_bytes(instrument[patch].base, voice, FP_SIZE);
    fm6_sanitize(instrument[patch].base);
    for (uint32_t i = 0; i < VOICES; ++i)
        if (voices[i].used && voices[i].patch == patch)
            voices[i].used = 0;
    fm6_rebuild_patch(patch);
    return 0;
}

void fm6_patch_set_level(uint32_t patch, uint8_t level)
{
    if (patch >= PATCHES) return;
    if (level > 127u) level = 127u;
    instrument[patch].level = level;
    if (!level)
        for (uint32_t i = 0; i < VOICES; ++i)
            if (voices[i].used && voices[i].patch == patch)
                voices[i].used = 0;
}
void fm6_patch_set_byte(uint32_t patch, uint32_t byte_index, uint8_t value)
{
    if (patch >= PATCHES || byte_index >= FP_SIZE)
        return;
    instrument[patch].base[byte_index] = value;
    fm6_sanitize(instrument[patch].base);
    fm6_rebuild_patch(patch);
}
void fm6_macro_set(uint32_t patch, uint32_t macro, uint8_t value)
{
    if (patch >= PATCHES || macro >= FM1LAB_MACRO_COUNT)
        return;
    instrument[patch].macro[macro] = value > 100u ? 100u : value;
    fm6_rebuild_patch(patch);
}
uint8_t fm6_macro_get(uint32_t patch, uint32_t macro)
{
    return patch < PATCHES && macro < FM1LAB_MACRO_COUNT ? instrument[patch].macro[macro] : 50u;
}
uint32_t fm6_active_voices(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < VOICES; i++)
        n += voices[i].used != 0;
    return n;
}
