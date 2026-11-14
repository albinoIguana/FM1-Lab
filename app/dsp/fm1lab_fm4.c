/* SPDX-License-Identifier: GPL-3.0-only
 * Four-operator fixed-point FM candidate for mixed-synthesis evaluation.
 * Standalone prototype; it does not replace the current FM6 render path.
 */
#include "fm1lab_fm4.h"
#include <string.h>

#define ENV_SUSTAIN_Q15 24576u
#define ENV_ATTACK_STEP 1024u
#define ENV_RELEASE_STEP 192u
#define OUT_LIMIT 32760

static const uint32_t semitone_ratio_q24[12] = {
    16777216u, 17774841u, 18831813u, 19951693u,
    21138677u, 22396723u, 23730359u, 25143906u,
    26642354u, 28230670u, 29913627u, 31697061u
};
static const uint32_t c4_phase_inc = 25480119u;
static const int16_t sine64[64] = {
    0, 3212, 6393, 9512, 12539, 15446, 18204, 20787,
    23170, 25329, 27245, 28898, 30273, 31356, 32137, 32609,
    32767, 32609, 32137, 31356, 30273, 28898, 27245, 25329,
    23170, 20787, 18204, 15446, 12539, 9512, 6393, 3212,
    0, -3212, -6393, -9512, -12539, -15446, -18204, -20787,
    -23170, -25329, -27245, -28898, -30273, -31356, -32137, -32609,
    -32767, -32609, -32137, -31356, -30273, -28898, -27245, -25329,
    -23170, -20787, -18204, -15446, -12539, -9512, -6393, -3212
};
static const uint16_t op_ratio_q8[4] = {256u, 512u, 768u, 256u};

static uint32_t phase_for_note(uint8_t note)
{
    int32_t offset = (int32_t)(note > 127u ? 127u : note) - 60;
    int32_t octave = offset / 12;
    int32_t semitone = offset % 12;
    if (semitone < 0) { semitone += 12; --octave; }
    uint64_t increment = ((uint64_t)c4_phase_inc * semitone_ratio_q24[semitone]) >> 24;
    if (octave > 0) increment <<= (uint32_t)octave;
    else if (octave < 0) increment >>= (uint32_t)(-octave);
    return increment > UINT32_MAX ? UINT32_MAX : (uint32_t)increment;
}

static int32_t sine_lookup(uint32_t phase)
{
    uint32_t index = phase >> 26;
    uint32_t frac = (phase >> 10) & 0xffffu;
    int32_t a = sine64[index & 63u];
    int32_t b = sine64[(index + 1u) & 63u];
    return a + (int32_t)(((int64_t)(b - a) * frac) >> 16);
}

static int32_t clamp_q15(int32_t x)
{
    if (x > OUT_LIMIT) return OUT_LIMIT;
    if (x < -OUT_LIMIT) return -OUT_LIMIT;
    return x;
}

void fm1lab_fm4_init(fm1lab_fm4_voice_t *v)
{
    if (v) memset(v, 0, sizeof(*v));
}

void fm1lab_fm4_note_on(fm1lab_fm4_voice_t *v, uint8_t note, uint8_t velocity, uint8_t algorithm)
{
    if (!v || !velocity) return;
    v->note = note > 127u ? 127u : note;
    v->velocity = velocity;
    v->algorithm = algorithm & 3u;
    uint32_t base = phase_for_note(v->note);
    for (uint32_t op = 0; op < 4u; ++op) {
        v->phase[op] = 0u;
        v->phase_inc[op] = (uint32_t)(((uint64_t)base * op_ratio_q8[op]) >> 8);
    }
    v->feedback_sample = 0;
    v->envelope_q15 = 0u;
    v->gate = 1u;
    v->active = 1u;
}

void fm1lab_fm4_note_off(fm1lab_fm4_voice_t *v)
{
    if (v) v->gate = 0u;
}

static int32_t envelope_next(fm1lab_fm4_voice_t *v)
{
    if (v->gate) {
        uint32_t next = (uint32_t)v->envelope_q15 + ENV_ATTACK_STEP;
        v->envelope_q15 = (uint16_t)(next > ENV_SUSTAIN_Q15 ? ENV_SUSTAIN_Q15 : next);
    } else if (v->envelope_q15) {
        v->envelope_q15 = (uint16_t)(v->envelope_q15 > ENV_RELEASE_STEP ?
                                     v->envelope_q15 - ENV_RELEASE_STEP : 0u);
    } else {
        v->active = 0u;
    }
    return v->envelope_q15;
}

static int32_t render_algorithm(fm1lab_fm4_voice_t *v)
{
    int32_t op0, op1, op2, op3;
    uint32_t p0 = v->phase[0] + ((uint32_t)(int32_t)v->feedback_sample << 14);
    uint32_t p1 = v->phase[1], p2 = v->phase[2], p3 = v->phase[3];
    int32_t output;

    switch (v->algorithm) {
    case 0u: /* Four-op serial: 1 -> 2 -> 3 -> 4. */
        op0 = sine_lookup(p0);
        op1 = sine_lookup(p1 + ((uint32_t)op0 << 15));
        op2 = sine_lookup(p2 + ((uint32_t)op1 << 15));
        output = sine_lookup(p3 + ((uint32_t)op2 << 15));
        break;
    case 1u: /* Two independent modulator/carrier pairs. */
        op0 = sine_lookup(p0);
        op1 = sine_lookup(p1 + ((uint32_t)op0 << 15));
        op2 = sine_lookup(p2);
        op3 = sine_lookup(p3 + ((uint32_t)op2 << 15));
        output = (op1 + op3) / 2;
        break;
    case 2u: /* Three-op stack plus one independent carrier. */
        op0 = sine_lookup(p0);
        op1 = sine_lookup(p1 + ((uint32_t)op0 << 15));
        op2 = sine_lookup(p2 + ((uint32_t)op1 << 15));
        op3 = sine_lookup(p3);
        output = (op2 + op3) / 2;
        break;
    default: /* One independent carrier plus a three-op stack. */
        op0 = sine_lookup(p0);
        op1 = sine_lookup(p1);
        op2 = sine_lookup(p2 + ((uint32_t)op1 << 15));
        op3 = sine_lookup(p3 + ((uint32_t)op2 << 15));
        output = (op0 + op3) / 2;
        break;
    }

    v->feedback_sample = (int16_t)(op0 >> 3);
    for (uint32_t i = 0; i < 4u; ++i) v->phase[i] += v->phase_inc[i];
    return output;
}

int fm1lab_fm4_render(fm1lab_fm4_voice_t *v, int16_t *out, uint32_t frames)
{
    if (!v || !out) return -1;
    for (uint32_t i = 0; i < frames; ++i) {
        int32_t sample = 0;
        if (v->active) {
            int32_t env = envelope_next(v);
            if (v->active && env) {
                sample = render_algorithm(v);
                sample = (int32_t)(((int64_t)sample * env * v->velocity) / (32768LL * 127LL));
            }
        }
        out[i] = (int16_t)clamp_q15(sample);
    }
    return 0;
}

size_t fm1lab_fm4_voice_bytes(void)
{
    return sizeof(fm1lab_fm4_voice_t);
}
