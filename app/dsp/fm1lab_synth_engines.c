/* SPDX-License-Identifier: GPL-3.0-only
 * Fixed-point candidate engines for FM1-Lab mixed-synthesis evaluation.
 * Standalone prototype: not yet wired into the live FM6 voice allocator.
 */
#include "fm1lab_synth_engines.h"
#include <string.h>

#define ENV_SUSTAIN_Q15 24576u
#define ENV_ATTACK_STEP 1024u
#define ENV_RELEASE_STEP 192u
#define OUT_LIMIT 32760

/* Q24 semitone ratios relative to C, with C4 phase increment at 44.1 kHz. */
static const uint32_t semitone_ratio_q24[12] = {
    16777216u, 17774841u, 18831813u, 19951693u,
    21138677u, 22396723u, 23730359u, 25143906u,
    26642354u, 28230670u, 29913627u, 31697061u
};
static const uint32_t c4_phase_inc = 25487311u;
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

static int32_t clamp_q15(int64_t x)
{
    if (x > OUT_LIMIT) return OUT_LIMIT;
    if (x < -OUT_LIMIT) return -OUT_LIMIT;
    return (int32_t)x;
}

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

void fm1lab_synth_init(fm1lab_synth_voice_t *v, fm1lab_synth_engine_t engine)
{
    if (!v) return;
    memset(v, 0, sizeof(*v));
    v->engine = (uint8_t)engine;
    v->noise = 0x1ace1u;
    v->filter_alpha_q15 = 8192u;
}

void fm1lab_synth_note_on(fm1lab_synth_voice_t *v, uint8_t note, uint8_t velocity)
{
    if (!v || v->engine >= FM1LAB_SYNTH_ENGINE_COUNT || !velocity) return;
    v->note = note > 127u ? 127u : note;
    v->velocity = velocity;
    v->phase_inc = phase_for_note(v->note);
    v->phase_inc_b = (uint32_t)(((uint64_t)v->phase_inc * 1003u) / 1000u);
    v->phase = 0u; v->phase_b = 0u;
    v->envelope_q15 = 0u;
    v->gate = 1u; v->active = 1u;
    v->filter_state = 0;
    if (!v->noise) v->noise = 0x1ace1u;
}

void fm1lab_synth_note_off(fm1lab_synth_voice_t *v)
{
    if (v) v->gate = 0u;
}

static int32_t envelope_next(fm1lab_synth_voice_t *v)
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

static int32_t render_wavetable(fm1lab_synth_voice_t *v)
{
    int32_t y = sine_lookup(v->phase);
    v->phase += v->phase_inc;
    return y;
}

static int32_t render_psg(fm1lab_synth_voice_t *v)
{
    /* Pulse oscillator plus a deterministic 15-bit LFSR noise source. */
    int32_t pulse = (v->phase & 0x80000000u) ? 24576 : -24576;
    uint32_t bit = ((v->noise >> 0) ^ (v->noise >> 1)) & 1u;
    v->noise = (v->noise >> 1) | (bit << 14);
    v->phase += v->phase_inc;
    return (pulse * 3 + ((v->noise & 1u) ? 16384 : -16384)) / 4;
}

static int32_t render_subtractive(fm1lab_synth_voice_t *v)
{
    /* Two detuned saw oscillators into a one-pole fixed-point low-pass. */
    int32_t saw_a = (int32_t)(v->phase >> 16) - 32768;
    int32_t saw_b = (int32_t)(v->phase_b >> 16) - 32768;
    int32_t x = (saw_a + saw_b) / 2;
    int32_t delta = x - v->filter_state;
    v->filter_state += (int32_t)(((int64_t)delta * v->filter_alpha_q15) >> 15);
    v->phase += v->phase_inc;
    v->phase_b += v->phase_inc_b;
    return v->filter_state;
}

int fm1lab_synth_render(fm1lab_synth_voice_t *v, int16_t *out, uint32_t frames)
{
    if (!v || !out || v->engine >= FM1LAB_SYNTH_ENGINE_COUNT) return -1;
    for (uint32_t i = 0; i < frames; ++i) {
        int32_t sample = 0;
        if (v->active) {
            int32_t env = envelope_next(v);
            if (v->active && env) {
                if (v->engine == FM1LAB_SYNTH_WAVETABLE) sample = render_wavetable(v);
                else if (v->engine == FM1LAB_SYNTH_PSG) sample = render_psg(v);
                else sample = render_subtractive(v);
                sample = (int32_t)(((int64_t)sample * env * v->velocity) / (32768LL * 127LL));
            }
        }
        out[i] = (int16_t)clamp_q15(sample);
    }
    return 0;
}

size_t fm1lab_synth_voice_bytes(void)
{
    return sizeof(fm1lab_synth_voice_t);
}
