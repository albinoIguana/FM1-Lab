/* SPDX-License-Identifier: GPL-3.0-only
 * FM1-Lab FM6 voice layer.
 *
 * FM6 core: Felucca's integer-only MSFA/Dexed-derived core, Apache-2.0.
 * The core is included in this translation unit because its compact API is
 * intentionally static; this wrapper supplies the two four-patch Instrument banks, eight independent eight-voice
 * pools, and per-patch note allocation.
 */
#include <stdint.h>
#include "fm6.h"
#include "felucca_tables.h"
#include "felucca_fm6.h"

/* FM6's two operator buses must be private to each render call when CPU2 is
 * active. The upstream FELUCCA_CPU2 branch moves them from shared static
 * storage to the render stack, preventing cross-core bus races. */
#if (defined(FM1B_CPU2) && FM1B_CPU2) || defined(FM6_TEST)
#ifndef FELUCCA_CPU2
#define FELUCCA_CPU2 1
#endif
#endif
#ifndef FM6_TEST
#include "plat.h"
#endif

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
    /* The device's small standalone DSP unit has no string.h include path.
     * Clear aligned words first, then any byte tail, to avoid a byte-at-a-time
     * loop on the render path without relying on the C library. */
    uint8_t *d = (uint8_t *)dst;
    while (n && ((uintptr_t)d & 3u)) {
        *d++ = 0u;
        --n;
    }
    uint32_t *w = (uint32_t *)d;
    while (n >= sizeof(*w)) {
        *w++ = 0u;
        n -= sizeof(*w);
    }
    d = (uint8_t *)w;
    while (n--) *d++ = 0u;
}

#define PATCHES FM6_ENGINE_PATCH_COUNT
#define VOICES (PATCHES * FM1LAB_VOICE_LIMIT)
#define Q24_ONE (1 << 24)
#define Q24_DB6 (1 << 24)
#define OUTPUT_LIMIT 8388607
/* Base per-patch calibration against the pinned FM6 core. The curated
 * four-layer bank applies a per-Instrument mix level before the master stage. */
#define FM6_PATCH_CALIBRATION_Q15 10536
#define FM6_MASTER_TRIM_Q15 32768 /* unity: the patch calibration already reserves 12 dB per layer */
#define FM6_FX_HEADROOM_SHIFT 5u
#define FM6_FX_HEADROOM_SCALE (1u << FM6_FX_HEADROOM_SHIFT)
#define FM6_LIMITER_CEILING_Q23 7470000
#ifndef PLAT_POOL
#define PLAT_POOL __attribute__((section(".pool")))
#endif

typedef struct {
    uint8_t base[FP_SIZE + 1u];
    uint8_t effective[FP_SIZE + 1u];
    uint8_t macro[FM1LAB_MACRO_COUNT];
    uint8_t enabled;
    uint8_t level;
    uint8_t pan;
    int32_t pan_l_current_q15;
    int32_t pan_r_current_q15;
    int32_t mod_level;
    int32_t detune[6];
    uint8_t feedback;
    int8_t octave;
    int8_t fine_cents;
    uint8_t ready;
} fm1lab_patch_t;

typedef struct {
    fm6_note_t state;
    uint32_t age;
    uint8_t patch;
    uint8_t note;
    uint8_t used;
    uint16_t source;
    uint8_t fade_in;
    uint8_t fade_out;
    int16_t tune16;
} fm1lab_voice_t;

static fm1lab_patch_t instrument[PATCHES];
static fm1lab_voice_t voices[VOICES];
static int32_t fm6_patch_gain_q15[PATCHES];

/* Dual-core render scratch lives in .pool, not either core's small stack.
 * The active-voice index list is rebuilt each 32-sample quantum, so a single
 * patch's notes are split too (rather than splitting fixed patch-slot ranges). */
static uint8_t fm6_voice_work[VOICES] PLAT_POOL;
static int64_t fm6_patch_mix[2][PATCHES][FM6_N] PLAT_POOL;
static int32_t fm6_voice_bus[2][FM6_N] PLAT_POOL;
#if defined(FM1_CPU1) || defined(FM6_TEST)
static fm1lab_voice_t fm6_cpu2_voice_backup[VOICES / 2u] PLAT_POOL;
#endif
static uint32_t fm6_cpu2_job_start, fm6_cpu2_job_count;
static volatile uint32_t fm6_cpu2_job_us, fm6_cpu2_wait_us;
#if defined(PLAT_WEB)
static volatile uint32_t fm6_cpu2_total_job_ticks_accum;
#endif

static fm1lab_insert_state_t fm6_master_inserts;
static uint32_t fm6_amp_drive_gain_q15, fm6_amp_output_gain_q15;
static uint16_t fm6_tone_alpha_q15;
static int32_t fm6_amp_tone_state[2];
static int32_t fm6_eq_x1[2][3],fm6_eq_x2[2][3],fm6_eq_y1[2][3],fm6_eq_y2[2][3];
static int32_t fm6_eq_coeff[3][5];
static fm6_lfo_t patch_lfo[PATCHES];
static int32_t patch_lfo_value[PATCHES];
static int32_t patch_lfo_delay[PATCHES];
static uint32_t age_counter;
static uint32_t overload_shed_count;
static int32_t output_gain_q12;
static int32_t limiter_gain_q15;
static int16_t pitch_bend_16;
static uint8_t fm6_comp_enabled, fm6_comp_threshold, fm6_comp_ratio;
static uint8_t fm6_comp_attack, fm6_comp_release, fm6_comp_makeup;
static int32_t fm6_comp_envelope;
static uint32_t fm6_comp_threshold_q24, fm6_comp_ratio_recip_q15;
static uint32_t fm6_comp_attack_alpha_q15, fm6_comp_release_alpha_q15;
static uint32_t fm6_comp_makeup_gain_q15;
static uint8_t fm6_chaos_amount, fm6_chaos_rate, fm6_chaos_tone, fm6_chaos_shape;
static uint32_t fm6_chaos_phase, fm6_chaos_phase_inc, fm6_chaos_random=0x6d2b79f5u;
static int32_t fm6_chaos_random_hold;


#ifdef FM6_TEST
static uint8_t fm6_test_cpu2_mode;
void fm6_test_set_cpu2_mode(uint8_t mode)
{
    fm6_test_cpu2_mode = mode <= 2u ? mode : 0u;
}
static uint64_t fm6_test_patch_peak_q24[PATCHES];
static uint64_t fm6_test_dry_peak_q24;
static uint64_t fm6_test_send_peak[4][2];
static uint32_t fm6_test_fx_saturation_count;
static uint32_t fm6_test_limiter_event_count;
static uint64_t fm6_test_output_peak_q23;
static uint64_t fm6_test_abs_i64(int64_t value)
{
    return value < 0 ? (uint64_t)(-(value + 1)) + 1u : (uint64_t)value;
}
uint64_t fm6_test_patch_peak(uint32_t patch)
{
    return patch < PATCHES ? fm6_test_patch_peak_q24[patch] : 0u;
}
uint64_t fm6_test_dry_peak(void)
{
    return fm6_test_dry_peak_q24;
}
uint64_t fm6_test_fx_send_peak(uint32_t effect, uint32_t channel)
{
    return effect < 4u && channel < 2u ? fm6_test_send_peak[effect][channel] : 0u;
}
uint32_t fm6_test_fx_saturations(void) { return fm6_test_fx_saturation_count; }
uint32_t fm6_test_limiter_events(void) { return fm6_test_limiter_event_count; }
uint64_t fm6_test_output_peak(void) { return fm6_test_output_peak_q23; }
int32_t fm6_test_macro_mod_level(uint32_t patch) { return patch < PATCHES ? instrument[patch].mod_level : 0; }
uint8_t fm6_test_macro_feedback(uint32_t patch) { return patch < PATCHES ? instrument[patch].feedback : 0u; }
int32_t fm6_test_macro_detune(uint32_t patch, uint32_t operator_index) {
    return patch < PATCHES && operator_index < 6u ? instrument[patch].detune[operator_index] : 0;
}
uint8_t fm6_test_effective_byte(uint32_t patch, uint32_t byte_index) {
    return patch < PATCHES && byte_index < FP_SIZE ? instrument[patch].effective[byte_index] : 0u;
}


#endif


#define FM6_SAMPLE_RATE FM6_SAMPLE_RATE_HZ
#define FM6_SEQ_LANES 2u
#define FM6_SEQ_STEPS 64u
#define FM6_DELAY_SIZE 8192u
#define FM6_CHORUS_SIZE 2048u
typedef struct { uint8_t enabled, note, velocity, gate; } fm6_step_t;
static volatile fm6_step_t seq_pattern[2][64];
#define FM6_SEQ_EVENTS_PER_LANE 128u
static fm6_seq_event_t seq_events[2][FM6_SEQ_EVENTS_PER_LANE];
static volatile uint8_t seq_event_count[2], seq_event_active[2][FM6_SEQ_EVENTS_PER_LANE];
static volatile uint8_t seq_event_mode;
#ifdef FM6_TEST
uint32_t fm6_test_sequencer_active_events(uint8_t lane)
{
    uint32_t count=0u;
    if(lane>=2u)return 0u;
    for(uint32_t i=0;i<seq_event_count[lane];i++)count+=seq_event_active[lane][i]!=0u;
    return count;
}
#endif
static volatile uint8_t seq_lane_base[2] = {0u,4u}, seq_lane_instrument[2]={0u,0u}, seq_length=16u, seq_position, seq_running;
static volatile uint8_t seq_start_pending, seq_stop_pending, arp_clear_pending, arp_reset_pending, seq_clear_pending[2];
static volatile uint8_t seq_play_position, arp_play_position;
static volatile uint8_t seq_tick_position;
static volatile uint32_t seq_tick_counter;
static volatile uint8_t arp_capture_head, arp_capture_tail;
static fm6_arp_capture_event_t arp_capture_queue[64];
static uint8_t seq_live_note[2], seq_live_mask[2];
static uint32_t seq_gate_left[2];
static volatile uint32_t transport_bpm=120u;
static volatile uint8_t transport_swing=50u;
static uint32_t transport_phase;
static volatile uint8_t arp_mode, arp_latch, arp_notes[16], arp_velocities[16], arp_count, arp_index, arp_direction=1u;
static volatile uint8_t arp_division=3u, arp_gate=75u, arp_octaves=1u, arp_length=16u;
static volatile uint8_t arp_step_gain[16], arp_step_accent[16], arp_step_gate[16];
static volatile uint8_t arp_output, arp_random, arp_macro_dest, arp_depth=50u;
static volatile uint8_t arp_step_position;
static volatile uint8_t arp_patch_mask=0x0fu;
static uint32_t arp_random_state=0x6D2B79F5u, arp_phase, arp_gate_left;
static int32_t arp_amp_gain_q15=32768, arp_amp_target_q15=32768;
static int32_t arp_macro_offset;
static uint8_t arp_live_note, arp_live, arp_live_patch_mask=0x0fu;
static void fm6_rebuild_patch(uint32_t index);
static int32_t fx_delay[FM6_DELAY_SIZE] PLAT_POOL, fx_delay_r[FM6_DELAY_SIZE] PLAT_POOL;
static int32_t fx_chorus[FM6_CHORUS_SIZE] PLAT_POOL, fx_chorus_r[FM6_CHORUS_SIZE] PLAT_POOL;
static int32_t fx_comb_a[2048] PLAT_POOL, fx_comb_a_r[2048] PLAT_POOL;
static int32_t fx_comb_b[4096] PLAT_POOL, fx_comb_b_r[4096] PLAT_POOL;
static int32_t fx_comb_c[8192] PLAT_POOL, fx_comb_c_r[8192] PLAT_POOL;
static int32_t fx_phaser_state_l[4], fx_phaser_state_r[4];
static int32_t fx_phaser_feedback_l, fx_phaser_feedback_r;
static uint16_t fx_delay_pos, fx_chorus_pos, fx_comb_pos_a, fx_comb_pos_b, fx_comb_pos_c;
static volatile uint8_t fx_enabled[4], fx_mix[4]={18u,24u,20u,35u},
    fx_time[4]={48u,52u,45u,35u}, fx_feedback[4]={40u,42u,60u,20u},
    fx_depth[4]={50u,50u,50u,65u};
static volatile uint8_t fx_send[PATCHES][4];
static uint8_t fx_mix_effective[4];
static uint32_t fx_lfo_phase, fx_phaser_phase;

/* Multiple parallel wet returns share one 100% wet budget. This prevents four
 * independently-maxed effects from summing to a clipped, harsh output. */
static void fm6_fx_mix_budget_update(void)
{
    uint32_t total=0u;
    for(uint32_t fx=0u;fx<4u;fx++)
        if(fx_enabled[fx])total+=fx_mix[fx];
    for(uint32_t fx=0u;fx<4u;fx++) {
        if(!fx_enabled[fx])fx_mix_effective[fx]=0u;
        else if(total<=100u)fx_mix_effective[fx]=fx_mix[fx];
        else fx_mix_effective[fx]=(uint8_t)((uint32_t)fx_mix[fx]*100u/total);
    }
}
static void fm6_pan_gains(uint8_t pan, int32_t *left_q15, int32_t *right_q15)
{
    /* Constant-voltage balance pan: center preserves the existing mono level. */
    if (pan <= 64u) {
        *left_q15 = 32768;
        *right_q15 = (int32_t)((uint32_t)pan * 32768u / 64u);
    } else {
        *left_q15 = (int32_t)((uint32_t)(127u - pan) * 32768u / 63u);
        *right_q15 = 32768;
    }
}


static const int32_t fm6_eq_sin_q15[101] = {93,100,107,115,123,132,141,151,162,174,186,200,214,229,246,263,282,302,324,347,372,398,427,457,490,525,563,603,646,692,742,795,851,912,978,1047,1122,1203,1289,1381,1479,1585,1698,1820,1950,2089,2238,2398,2569,2752,2949,3159,3384,3625,3883,4160,4455,4772,5110,5472,5860,6274,6716,7189,7694,8233,8807,9420,10072,10767,11504,12288,13118,13997,14925,15903,16930,18007,19130,20297,21502,22739,23999,25268,26531,27767,28952,30053,31032,31844,32435,32741,32690,32202,31187,29555,27214,24079,20085,15197,9435};
static const int32_t fm6_eq_cos_q15[101] = {32768,32768,32768,32768,32768,32768,32768,32768,32768,32768,32767,32767,32767,32767,32767,32767,32767,32767,32766,32766,32766,32766,32765,32765,32764,32764,32763,32762,32762,32761,32760,32758,32757,32755,32753,32751,32749,32746,32743,32739,32735,32730,32724,32717,32710,32701,32691,32680,32667,32652,32635,32615,32593,32567,32537,32503,32464,32419,32367,32308,32240,32162,32072,31970,31852,31717,31562,31385,31182,30949,30682,30377,30028,29628,29172,28650,28055,27377,26604,25725,24726,23594,22312,20863,19232,17399,15347,13060,10523,7725,4659,1327,-2257,-6066,-10054,-14150,-18252,-22225,-25891,-29031,-31380};
static const uint32_t fm6_eq_q_q15[32] = {16384,20084,23783,27483,31182,34882,38582,42281,45981,49681,53380,57080,60779,64479,68179,71878,75578,79277,82977,86677,90376,94076,97775,101475,105175,108874,112574,116274,119973,123673,127372,131072};
static const uint32_t fm6_eq_a_q15[25] = {16423,17396,18427,19519,20675,21900,23198,24573,26029,27571,29205,30935,32768,34710,36766,38945,41252,43697,46286,49029,51934,55011,58271,61723,65381};

static uint64_t fm6_abs_i64(int64_t value);
static int32_t fm6_div_q15(int32_t numerator, uint32_t denominator)
{
    uint32_t magnitude = numerator < 0 ? (uint32_t)(-(int64_t)numerator) : (uint32_t)numerator;
    uint32_t result = denominator ? (magnitude * 32768u) / denominator : 0u;
    return numerator < 0 ? -(int32_t)result : (int32_t)result;
}
static void fm6_eq_coeff_update(uint32_t band)
{
    uint32_t frequency = fm6_master_inserts.eq[band].frequency > 100u ? 100u : fm6_master_inserts.eq[band].frequency;
    uint32_t gain = fm6_master_inserts.eq[band].gain > 24u ? 24u : fm6_master_inserts.eq[band].gain;
    uint32_t q = fm6_master_inserts.eq[band].q > 31u ? 31u : fm6_master_inserts.eq[band].q;
    uint32_t sine = (uint32_t)(fm6_eq_sin_q15[frequency] < 0 ? -fm6_eq_sin_q15[frequency] : fm6_eq_sin_q15[frequency]);
    int32_t cosine = fm6_eq_cos_q15[frequency];
    uint32_t alpha = (sine * 32768u) / (2u * fm6_eq_q_q15[q]);
    uint32_t alpha_a = (alpha * fm6_eq_a_q15[gain]) >> 15;
    uint32_t alpha_over_a = (alpha << 15) / fm6_eq_a_q15[gain];
    uint32_t a0 = 32768u + alpha_over_a;
    int32_t b0 = (int32_t)(32768u + alpha_a);
    int32_t b1 = -2 * cosine;
    int32_t b2 = (int32_t)(32768u - alpha_a);
    int32_t a1 = b1;
    int32_t a2 = (int32_t)(32768u - alpha_over_a);
    fm6_eq_coeff[band][0] = fm6_div_q15(b0, a0);
    fm6_eq_coeff[band][1] = fm6_div_q15(b1, a0);
    fm6_eq_coeff[band][2] = fm6_div_q15(b2, a0);
    fm6_eq_coeff[band][3] = fm6_div_q15(a1, a0);
    fm6_eq_coeff[band][4] = fm6_div_q15(a2, a0);
}
static int32_t fm6_clamp_sample(int64_t x)
{
    return x > INT32_MAX ? INT32_MAX : x < INT32_MIN ? INT32_MIN : (int32_t)x;
}
static int64_t fm6_amp_soft_clip(int64_t x)
{
    const int64_t knee = 1LL << 23; /* half-scale soft knee in the Q24 patch domain */
    uint64_t magnitude = x < 0 ? (uint64_t)(-(x + 1)) + 1u : (uint64_t)x;
    if (magnitude <= (uint64_t)knee) return x;
    uint64_t excess = magnitude - (uint64_t)knee;
    const uint64_t full_knee_span = (uint64_t)knee * 2u;
    uint64_t shaped;
    if (excess >= full_knee_span) shaped = (uint64_t)knee * 2u;
    else shaped = (uint64_t)knee + excess - ((excess * excess) >> 25);
    return x < 0 ? -(int64_t)shaped : (int64_t)shaped;
}
static uint32_t fm6_amp_output_gain(uint8_t output)
{
    if (output <= 50u) return 8192u + (uint32_t)output * 491u;
    return 32768u + (uint32_t)(output - 50u) * 655u;
}
static uint32_t fm6_amp_drive_gain(uint8_t drive)
{ return 32768u + (uint32_t)drive * 229376u / 100u; }
static uint16_t fm6_tone_alpha(uint8_t tone)
{
    uint32_t cutoff=300u+(uint32_t)tone*197u;
    return (uint16_t)((cutoff*32768u)/(cutoff+7016u));
}
static void fm6_master_insert_recalculate(uint32_t field)
{
    if(field==FM6_INSERT_DRIVE)fm6_amp_drive_gain_q15=fm6_amp_drive_gain(fm6_master_inserts.drive);
    else if(field==FM6_INSERT_TONE)fm6_tone_alpha_q15=fm6_tone_alpha(fm6_master_inserts.tone);
    else if(field==FM6_INSERT_OUTPUT)fm6_amp_output_gain_q15=fm6_amp_output_gain(fm6_master_inserts.output);
    else if(field>=FM6_INSERT_EQ_LOW_FREQUENCY && field<FM6_INSERT_FIELD_COUNT){
        uint32_t offset=field-FM6_INSERT_EQ_LOW_FREQUENCY,band=offset/4u;
        if(band<3u && (offset%4u)<3u)fm6_eq_coeff_update(band);
    }
}
static int64_t fm6_master_insert_channel(uint32_t channel,int64_t input)
{
    int64_t amp=input;
    if(fm6_master_inserts.amp_enabled){
        amp=(input*fm6_amp_drive_gain_q15)>>15;
        if(fm6_master_inserts.drive)amp=fm6_amp_soft_clip(amp);
        int32_t tone_input=fm6_clamp_sample(amp);
        int32_t difference=tone_input-fm6_amp_tone_state[channel];
        fm6_amp_tone_state[channel]+=(int32_t)(((int64_t)difference*fm6_tone_alpha_q15)>>15);
        amp=((int64_t)fm6_amp_tone_state[channel]*fm6_amp_output_gain_q15)>>15;
    } else {
        fm6_amp_tone_state[channel]=fm6_clamp_sample(input);
    }
    if(!(fm6_master_inserts.eq_enabled&1u))return amp;
    int64_t eq_output=amp;
    for(uint32_t band=0;band<3u;band++){
        int32_t x=fm6_clamp_sample(eq_output);
        uint8_t enabled=(uint8_t)(fm6_master_inserts.eq_enabled&(1u<<(band+1u)));
        if(!enabled || fm6_master_inserts.eq[band].gain==12u){
            fm6_eq_x2[channel][band]=fm6_eq_x1[channel][band];
            fm6_eq_x1[channel][band]=x;
            fm6_eq_y2[channel][band]=fm6_eq_y1[channel][band];
            fm6_eq_y1[channel][band]=x;
            continue;
        }
        int64_t acc=(int64_t)fm6_eq_coeff[band][0]*x+
            (int64_t)fm6_eq_coeff[band][1]*fm6_eq_x1[channel][band]+
            (int64_t)fm6_eq_coeff[band][2]*fm6_eq_x2[channel][band]-
            (int64_t)fm6_eq_coeff[band][3]*fm6_eq_y1[channel][band]-
            (int64_t)fm6_eq_coeff[band][4]*fm6_eq_y2[channel][band];
        int32_t y=fm6_clamp_sample(acc>>15);
        fm6_eq_x2[channel][band]=fm6_eq_x1[channel][band];
        fm6_eq_x1[channel][band]=x;
        fm6_eq_y2[channel][band]=fm6_eq_y1[channel][band];
        fm6_eq_y1[channel][band]=y;
        eq_output=y;
    }
    return eq_output;
}
static void fm6_master_insert_process(int64_t *left,int64_t *right)
{
    *left=fm6_master_insert_channel(0u,*left);
    *right=fm6_master_insert_channel(1u,*right);
}
static uint32_t fm6_chaos_next_random(void)
{
    uint32_t x=fm6_chaos_random;
    x^=x<<13;x^=x>>17;x^=x<<5;
    fm6_chaos_random=x?x:0x6d2b79f5u;
    return fm6_chaos_random;
}
static void fm6_chaos_rate_update(void)
{
    /* Bar, half, quarter, eighth, sixteenth, thirty-second and triplet clocks. */
    static const uint16_t scale[8]={406u,812u,1624u,3248u,6496u,12992u,4872u,9744u};
    uint32_t slot=(uint32_t)fm6_chaos_rate*7u/100u;
    uint32_t bpm=transport_bpm<30u?30u:transport_bpm>300u?300u:transport_bpm;
    fm6_chaos_phase_inc=bpm*scale[slot];
}
static int32_t fm6_chaos_fold(int64_t sample)
{
    /* PI32V2 has no 64-bit modulo helper. The drive stage is bounded to a
     * small multiple of Q24, so fold safely in signed 32-bit. */
    int32_t bounded=sample>INT32_MAX?INT32_MAX:sample<INT32_MIN?INT32_MIN:(int32_t)sample;
    const int32_t period=Q24_ONE*2;
    int32_t folded=bounded%period;
    if(folded>Q24_ONE)folded=period-folded;
    else if(folded< -Q24_ONE)folded=-period-folded;
    return folded;
}
static int32_t fm6_chaos_wave(uint32_t phase)
{
    uint32_t p=phase>>16;
    int32_t tri=p<32768u?(int32_t)p*2-32768:98303-(int32_t)p*2;
    if(fm6_chaos_shape==0u) {
        int32_t abs_tri=tri<0?-tri:tri;
        return (int32_t)(((int64_t)tri*41722>>15)-((int64_t)tri*abs_tri*13280>>30));
    }
    if(fm6_chaos_shape==1u)return tri;
    if(fm6_chaos_shape==2u)return (phase&0x80000000u)?-32768:32767;
    if(fm6_chaos_shape==3u)return (tri/8192)*8192;
    if(fm6_chaos_shape==4u)return fm6_chaos_random_hold;
    if(fm6_chaos_shape==5u)return (fm6_chaos_random_hold/4096)*4096;
    return tri;
}
static void fm6_chaos_process_channel(int64_t *sample,int32_t modulation)
{
    int64_t dry=*sample;
    int64_t ring=(dry*modulation)>>15;
    int64_t shaped=dry+((ring-dry)*fm6_chaos_amount)/100;
    int64_t drive=100+(int64_t)fm6_chaos_amount*5;
    shaped=fm6_chaos_fold((shaped*drive)/100);
    /* Q24 signal: the control spans 16-bit down to deliberately brutal 2-bit.
     * Keep eight fractional bits at the lightest setting, up to twenty-two
     * at maximum reduction. */
    uint32_t crush_bits=8u+(uint32_t)fm6_chaos_tone*14u/100u;
    if(crush_bits) {
        int32_t q=shaped>INT32_MAX?INT32_MAX:shaped<INT32_MIN?INT32_MIN:(int32_t)shaped;
        uint32_t mask=~(((uint32_t)1u<<crush_bits)-1u);
        if(q>=0)q=(int32_t)((uint32_t)q&mask);
        else q=-(int32_t)(((uint32_t)(-(int64_t)q))&mask);
        shaped=q;
    }
    *sample=dry+((shaped-dry)*fm6_chaos_amount)/100;
}
static void fm6_chaos_process(int64_t *left,int64_t *right)
{
    if(!fm6_chaos_amount)return; /* true transparent bypass */
    uint32_t old_phase=fm6_chaos_phase;
    fm6_chaos_phase+=fm6_chaos_phase_inc;
    if(fm6_chaos_phase<old_phase)
        fm6_chaos_random_hold=(int32_t)(fm6_chaos_next_random()>>16)-32768;
    int32_t wave=fm6_chaos_wave(fm6_chaos_phase);
    fm6_chaos_process_channel(left,wave);
    fm6_chaos_process_channel(right,wave);
}
static uint32_t fm6_comp_makeup_q15(uint8_t value)
{
    static const uint32_t gain[13] = {32768u,34716u,36781u,38968u,41285u,43739u,46341u,
        49099u,52028u,55146u,58470u,61999u,65536u};
    uint32_t scaled = (uint32_t)value * 12u;
    uint32_t index = scaled / 100u, remainder = scaled % 100u;
    if (index >= 12u) return gain[12];
    return gain[index] + ((gain[index + 1u] - gain[index]) * remainder) / 100u;
}
static void fm6_comp_params_update(void)
{
    uint32_t attack_ms=1u+(uint32_t)fm6_comp_attack*49u/100u;
    uint32_t release_ms=20u+(uint32_t)fm6_comp_release*480u/100u;
    uint32_t ratio=1u+(uint32_t)fm6_comp_ratio*11u/100u;
    fm6_comp_threshold_q24=500000u+(uint32_t)fm6_comp_threshold*154000u;
    fm6_comp_attack_alpha_q15=32768u/(attack_ms*44u);
    fm6_comp_release_alpha_q15=32768u/(release_ms*44u);
    fm6_comp_ratio_recip_q15=32768u/ratio;
    fm6_comp_makeup_gain_q15=fm6_comp_makeup_q15(fm6_comp_makeup);
}
static void fm6_master_comp_process(int64_t *left, int64_t *right)
{
    uint64_t al=fm6_abs_i64(*left),ar=fm6_abs_i64(*right);
    uint64_t peak=al>ar?al:ar;
    int32_t detector=peak>INT32_MAX?INT32_MAX:(int32_t)peak;
    int32_t difference=detector-fm6_comp_envelope;
    uint32_t coefficient=difference>0?fm6_comp_attack_alpha_q15:fm6_comp_release_alpha_q15;
    int64_t step=((int64_t)difference*coefficient)>>15;
    if(difference&&!step)step=difference>0?1:-1;
    fm6_comp_envelope=fm6_clamp_sample((int64_t)fm6_comp_envelope+step);
    if(!fm6_comp_enabled)return;
    uint32_t gain_q15=32768u;
    if((uint32_t)fm6_comp_envelope>fm6_comp_threshold_q24){
        uint32_t over=(uint32_t)fm6_comp_envelope-fm6_comp_threshold_q24;
        uint32_t compressed=fm6_comp_threshold_q24+(uint32_t)(((uint64_t)over*fm6_comp_ratio_recip_q15)>>15);
        uint32_t den=(uint32_t)fm6_comp_envelope>>15;
        uint32_t num=(compressed>>15)*32768u;
        gain_q15=den?num/den:32768u;
        if(gain_q15>32768u)gain_q15=32768u;
    }
    gain_q15=(uint32_t)(((uint64_t)gain_q15*fm6_comp_makeup_gain_q15)>>15);
    *left=(*left*gain_q15)>>15;
    *right=(*right*gain_q15)>>15;
}

static int32_t fm6_sat_q23(int64_t x)
{
#ifdef FM6_TEST
    if (x > OUTPUT_LIMIT || x < -OUTPUT_LIMIT) ++fm6_test_fx_saturation_count;
#endif
    return x > OUTPUT_LIMIT ? OUTPUT_LIMIT : x < -OUTPUT_LIMIT ? -OUTPUT_LIMIT : (int32_t)x;
}
static uint32_t fm6_step_period(void) { return (FM6_SAMPLE_RATE*60u)/((uint32_t)transport_bpm*4u); }
static uint32_t fm6_transport_tick_threshold(void)
{
    uint32_t percent=(seq_tick_position&1u)?(100u-transport_swing)*2u:(uint32_t)transport_swing*2u;
    return (FM6_SAMPLE_RATE*60u*percent)/100u;
}
static void fm6_note_start_tuned_mask(uint8_t note,uint8_t velocity,uint8_t patch_mask,int16_t tune16,uint16_t source);
static void fm6_note_off_source(uint8_t note,uint8_t patch_mask,uint16_t source);
static void fm6_seq_event_release_lane(uint32_t lane)
{
    for(uint32_t i=0;i<FM6_SEQ_EVENTS_PER_LANE;i++) {
        if(seq_event_active[lane][i]) {
            const fm6_seq_event_t *event=&seq_events[lane][i];
            uint16_t token=(uint16_t)(4u+lane*FM6_SEQ_EVENTS_PER_LANE+i);
            fm6_note_off_source(event->pitch,(uint8_t)(0x0fu<<seq_lane_base[lane]),token);
            seq_event_active[lane][i]=0u;
        }
    }
}
static void fm6_seq_event_tick(void)
{
    uint32_t loop_ticks=(uint32_t)(seq_length<2u?2u:seq_length)*2u;
    for(uint32_t lane=0;lane<2u;lane++) {
        for(uint32_t i=0;i<seq_event_count[lane];i++) {
            const fm6_seq_event_t *event=&seq_events[lane][i];
            uint32_t end=((uint32_t)event->start+event->duration)%loop_ticks;
            if(seq_event_active[lane][i] && end==seq_tick_position) {
                uint16_t token=(uint16_t)(4u+lane*FM6_SEQ_EVENTS_PER_LANE+i);
                fm6_note_off_source(event->pitch,(uint8_t)(0x0fu<<seq_lane_base[lane]),token);
                seq_event_active[lane][i]=0u;
            }
        }
    }
    for(uint32_t lane=0;lane<2u;lane++) {
        uint8_t mask=(uint8_t)(0x0fu<<seq_lane_base[lane]);
        for(uint32_t i=0;i<seq_event_count[lane];i++) {
            const fm6_seq_event_t *event=&seq_events[lane][i];
            if(event->start==seq_tick_position && event->velocity && !seq_event_active[lane][i]) {
                uint16_t token=(uint16_t)(4u+lane*FM6_SEQ_EVENTS_PER_LANE+i);
                uint32_t accented=(uint32_t)event->velocity+
                    ((127u-(uint32_t)event->velocity)*event->accent)/100u;
                if(accented>127u)accented=127u;
                fm6_note_start_tuned_mask(event->pitch,(uint8_t)accented,mask,0,token);
                seq_event_active[lane][i]=1u;
            }
        }
    }
}
static void fm6_transport_step(void)
{
    seq_play_position=seq_position;
    for(uint32_t lane=0;lane<2u;lane++) {
        if(seq_running) {
            volatile fm6_step_t *s=&seq_pattern[lane][seq_position];
            uint8_t continuation=(uint8_t)(s->enabled && s->velocity &&
                (s->gate & 0x80u) && seq_live_mask[lane] &&
                seq_live_note[lane]==s->note);
            if(seq_live_mask[lane] && !continuation) {
                fm6_note_off_source(seq_live_note[lane],seq_live_mask[lane],(uint8_t)(lane+1u));
                seq_live_mask[lane]=0u;
            }
            if(s->enabled && s->velocity) {
                uint8_t mask=(uint8_t)(0x0fu<<seq_lane_base[lane]);
                if(!continuation) {
                    fm6_note_start_tuned_mask(s->note,s->velocity,mask,0,(uint8_t)(lane+1u));
                    seq_live_note[lane]=s->note; seq_live_mask[lane]=mask;
                }
                uint8_t gate=(uint8_t)(s->gate & 0x7fu);
                seq_gate_left[lane]=fm6_step_period()*(gate?gate:50u)/100u;
                if(!seq_gate_left[lane]) seq_gate_left[lane]=1u;
            } else seq_gate_left[lane]=0u;
        }
    }
}
static void fm6_transport_tick(void)
{
    if(!seq_running)return;
    seq_play_position=(uint8_t)(seq_tick_position/2u);
    if(seq_event_mode)fm6_seq_event_tick();
    else if((seq_tick_position&1u)==0u)fm6_transport_step();
    seq_tick_position=(uint8_t)(seq_tick_position+1u);
    if(seq_tick_position>=(uint8_t)(seq_length*2u))seq_tick_position=0u;
    ++seq_tick_counter;
    seq_position=(uint8_t)(seq_tick_position/2u);
}
static uint32_t fm6_arp_step_period(void)
{
    uint32_t denominator=1u<<(arp_division>5u?5u:arp_division);
    uint32_t bpm=transport_bpm<30u?30u:transport_bpm;
    uint32_t period=(FM6_SAMPLE_RATE*60u*4u)/(bpm*denominator);
    uint32_t percent=(arp_step_position&1u)?(100u-transport_swing)*2u:(uint32_t)transport_swing*2u;
    return (period*percent)/100u;
}
static void fm6_arp_capture_push(uint8_t on,uint8_t note,uint8_t velocity,uint8_t patch_mask)
{
    uint8_t head=arp_capture_head;
    uint8_t next=(uint8_t)((head+1u)&63u);
    uint8_t lane=(uint8_t)((patch_mask&0xf0u)?1u:0u);
    if(next==arp_capture_tail)return; /* drop newest; keep the SPSC indices single-writer */
    uint32_t tick=seq_tick_counter;
    uint32_t threshold=fm6_transport_tick_threshold();
    if(transport_phase*2u>=threshold)++tick; /* nearest swung 32nd-note grid position */
    arp_capture_queue[head]=(fm6_arp_capture_event_t){tick,lane,note,velocity,on};
    arp_capture_head=next;
}
static uint32_t fm6_arp_random(void)
{
    arp_random_state^=arp_random_state<<13;
    arp_random_state^=arp_random_state>>17;
    arp_random_state^=arp_random_state<<5;
    return arp_random_state;
}
static void fm6_arp_macro_apply(int32_t offset)
{
    if(offset==arp_macro_offset)return;
    arp_macro_offset=offset;
    for(uint32_t p=0;p<PATCHES;p++)fm6_rebuild_patch(p);
}
static void fm6_arpeggiator_step(void)
{
    uint32_t count=(uint32_t)arp_count*(arp_octaves?arp_octaves:1u);
    uint8_t step=(uint8_t)(arp_step_position%(arp_length?arp_length:16u));
    uint32_t index;
    uint8_t note,velocity;
    uint32_t gain=arp_step_gain[step];
    uint32_t tied=arp_step_gate[step]?1u:0u;
    uint32_t effective_gate=tied?100u:(uint32_t)(arp_gate<10u?10u:arp_gate>100u?100u:arp_gate);
    if(effective_gate<1u)effective_gate=1u;
    if(arp_live){fm6_arp_capture_push(0u,arp_live_note,0u,arp_live_patch_mask);fm6_note_off_source(arp_live_note,arp_live_patch_mask,3u);arp_live=0u;}
    arp_gate_left=0u;
    if(!arp_mode||!count){
        arp_gate_left=0u;arp_amp_target_q15=32768;
        fm6_arp_macro_apply(0);
        return;
    }
    {
        /* Accent is an audible transient across all four macros. */
        int32_t boost=gain?(int32_t)arp_step_accent[step]*35/100:0;
        if(arp_output==2u)boost+=(int32_t)arp_depth-50;
        fm6_arp_macro_apply(boost);
    }
    if(arp_output==1u){
        uint32_t level=gain>127u?127u:gain;
        arp_amp_target_q15=(int32_t)(level*32768u/127u);
        arp_gate_left=(uint32_t)((uint64_t)fm6_arp_step_period()*effective_gate/100u);
        if(!arp_gate_left)arp_gate_left=1u;
    } else if(arp_output==2u) {
        arp_gate_left=(uint32_t)((uint64_t)fm6_arp_step_period()*effective_gate/100u);
        if(!arp_gate_left)arp_gate_left=1u;
    } else {
        arp_amp_target_q15=32768;
        if(arp_index>=count)arp_index=0u;
        if(arp_mode==1u||arp_mode==2u) arp_index=(uint8_t)(arp_index%count);
        else if(arp_mode==4u) arp_index=(uint8_t)(fm6_arp_random()%count);
        else if(arp_mode==3u && count<2u){arp_index=0u;arp_direction=1u;}
        if(arp_random && arp_mode!=4u && (fm6_arp_random()%100u)<arp_random)
            index=fm6_arp_random()%count;
        else index=arp_mode==2u?(count-1u-arp_index):arp_index;
        uint32_t octave=index/arp_count, note_index=index%arp_count;
        note=(uint8_t)((uint32_t)arp_notes[note_index]+octave*12u);
        if(note>127u)note=127u;
        velocity=arp_velocities[note_index];
        velocity=(uint8_t)((uint32_t)velocity*gain/127u);
        /* Gain controls velocity; accent boosts all four macros for the gate. */
        if(velocity) {
            arp_live_note=note;
            fm6_note_start_tuned_mask(note,velocity,arp_patch_mask,0,3u);
            arp_live_patch_mask=arp_patch_mask;
            fm6_arp_capture_push(1u,note,velocity,arp_live_patch_mask);
            arp_live=1u;
            arp_gate_left=(uint32_t)((uint64_t)fm6_arp_step_period()*effective_gate/100u);
            if(!arp_gate_left)arp_gate_left=1u;
        }
        if(arp_mode==3u) {
            if(count<2u)arp_index=0u;
            else if(arp_direction) {
                if((uint32_t)arp_index+1u>=count){arp_direction=0u;arp_index=(uint8_t)(count-2u);}
                else ++arp_index;
            } else {
                if(arp_index==0u){arp_direction=1u;arp_index=1u;}
                else --arp_index;
            }
        } else if(arp_mode!=4u)++arp_index;
    }
    arp_play_position=step;
    arp_step_position=(uint8_t)((step+1u)%(arp_length?arp_length:16u));
}
static int32_t fm6_allpass_stage(int32_t input, int32_t *state, int32_t coefficient_q15)
{
    int64_t output = -(((int64_t)coefficient_q15 * input) >> 15) + *state;
    int64_t next = (int64_t)input + ((int64_t)coefficient_q15 * output >> 15);
    *state = fm6_sat_q23(next);
    return fm6_sat_q23(output);
}
static int32_t fm6_phaser_channel(int32_t input, int32_t state[4], uint32_t phase)
{
    uint32_t tri = phase < 32768u ? phase : 65535u - phase;
    uint32_t span = (uint32_t)fx_depth[3] * 230u;
    int32_t coefficient = (int32_t)(16500u - span/2u + (span * tri) / 32768u);
    for (uint32_t stage=0; stage<4u; ++stage)
        input = fm6_allpass_stage(input, &state[stage], coefficient);
    return input;
}
static void fm6_fx_process(int64_t input_l,int64_t input_r,const int32_t sends[4][2],int64_t *left,int64_t *right)
{
    /* True bypass: the old path still clocked every delay/comb buffer and ran
     * both four-stage phasers while all effects were OFF. On the FM1 this
     * consumed a large share of the 5.8 ms audio budget before a note played. */
    if(!fx_enabled[0] && !fx_enabled[1] && !fx_enabled[2] && !fx_enabled[3]) {
        *left=input_l;
        *right=input_r;
        return;
    }
    int32_t chorus=0,chorus_r=0,reverb=0,reverb_r=0,phaser_l=0,phaser_r=0;
    int32_t delay_feedback_l=0,delay_feedback_r=0;
    /* Bypassed effects do not touch delay memory, divide feedback, or advance pointers. */
    if(fx_enabled[0]) {
        uint32_t cp=fx_chorus_pos&(FM6_CHORUS_SIZE-1u);
        uint32_t tri=(fx_lfo_phase>>16)&1023u, depth=tri<512u?tri:1023u-tri;
        uint32_t base_delay=120u+(uint32_t)fx_time[0]*3u;
        uint32_t chorus_delay=base_delay+depth*base_delay/511u;
        uint32_t chorus_l_index=(cp-chorus_delay)&(FM6_CHORUS_SIZE-1u);
        uint32_t chorus_r_index=(cp-(chorus_delay+160u))&(FM6_CHORUS_SIZE-1u);
        int32_t chorus_l_feedback=fx_chorus[chorus_l_index];
        int32_t chorus_r_feedback=fx_chorus_r[chorus_r_index];
        if(fx_mix_effective[0]) { chorus=chorus_l_feedback; chorus_r=chorus_r_feedback; }
        fx_chorus[cp]=fm6_sat_q23((int64_t)sends[0][0]+(int64_t)chorus_l_feedback*fx_feedback[0]/100u);
        fx_chorus_r[cp]=fm6_sat_q23((int64_t)sends[0][1]+(int64_t)chorus_r_feedback*fx_feedback[0]/100u);
        fx_chorus_pos=(uint16_t)((cp+1u)&(FM6_CHORUS_SIZE-1u));
        fx_lfo_phase+=200u+(uint32_t)fx_time[0]*10u;
    }
    if(fx_enabled[1]) {
        uint32_t dp=fx_delay_pos&(FM6_DELAY_SIZE-1u);
        uint32_t delay_length=256u+(uint32_t)fx_time[1]*78u;
        delay_feedback_l=fx_delay[(dp-delay_length)&(FM6_DELAY_SIZE-1u)];
        delay_feedback_r=fx_delay_r[(dp-delay_length)&(FM6_DELAY_SIZE-1u)];
        fx_delay[dp]=fm6_sat_q23((int64_t)sends[1][0]+(int64_t)delay_feedback_l*fx_feedback[1]/100u);
        fx_delay_r[dp]=fm6_sat_q23((int64_t)sends[1][1]+(int64_t)delay_feedback_r*fx_feedback[1]/100u);
        fx_delay_pos=(uint16_t)((dp+1u)&(FM6_DELAY_SIZE-1u));
    }
    if(fx_enabled[2]) {
        int32_t a_l=fx_comb_a[fx_comb_pos_a],b_l=fx_comb_b[fx_comb_pos_b],c_l=fx_comb_c[fx_comb_pos_c];
        int32_t a_r=fx_comb_a_r[fx_comb_pos_a],b_r=fx_comb_b_r[fx_comb_pos_b],c_r=fx_comb_c_r[fx_comb_pos_c];
        uint8_t reverb_type=fx_depth[2]<=2u?fx_depth[2]:1u;
        if(fx_mix_effective[2]) {
            if(reverb_type==2u) { reverb=(a_l*2+b_l*2+c_l)/5; reverb_r=(a_r*2+b_r*2+c_r)/5; }
            else if(reverb_type==0u) { reverb=(a_l*3+b_l+c_l)/5; reverb_r=(a_r*3+b_r+c_r)/5; }
            else { reverb=(a_l+b_l+c_l)/3; reverb_r=(a_r+b_r+c_r)/3; }
        }
        uint32_t decay=reverb_type==0u
            ? 30u+(uint32_t)fx_time[2]*20u/100u+(uint32_t)fx_feedback[2]*8u/100u
            : reverb_type==2u
                ? 60u+(uint32_t)fx_time[2]*25u/100u+(uint32_t)fx_feedback[2]*10u/100u
                : 40u+(uint32_t)fx_time[2]*25u/100u+(uint32_t)fx_feedback[2]*15u/100u;
        fx_comb_a[fx_comb_pos_a]=fm6_sat_q23((int64_t)sends[2][0]+(int64_t)a_l*decay/100);
        fx_comb_b[fx_comb_pos_b]=fm6_sat_q23((int64_t)sends[2][0]+(int64_t)b_l*(decay+3u)/100);
        fx_comb_c[fx_comb_pos_c]=fm6_sat_q23((int64_t)sends[2][0]+(int64_t)c_l*(decay+6u)/100);
        fx_comb_a_r[fx_comb_pos_a]=fm6_sat_q23((int64_t)sends[2][1]+(int64_t)a_r*decay/100);
        fx_comb_b_r[fx_comb_pos_b]=fm6_sat_q23((int64_t)sends[2][1]+(int64_t)b_r*(decay+3u)/100);
        fx_comb_c_r[fx_comb_pos_c]=fm6_sat_q23((int64_t)sends[2][1]+(int64_t)c_r*(decay+6u)/100);
        fx_comb_pos_a=(uint16_t)((fx_comb_pos_a+1u)&2047u);
        fx_comb_pos_b=(uint16_t)((fx_comb_pos_b+1u)&4095u);
        fx_comb_pos_c=(uint16_t)((fx_comb_pos_c+1u)&8191u);
    }
    if(fx_enabled[3]) {
        int32_t in_l=fm6_sat_q23((int64_t)sends[3][0]+(int64_t)fx_phaser_feedback_l*fx_feedback[3]/100u);
        int32_t in_r=fm6_sat_q23((int64_t)sends[3][1]+(int64_t)fx_phaser_feedback_r*fx_feedback[3]/100u);
        phaser_l=fm6_phaser_channel(in_l,fx_phaser_state_l,(fx_phaser_phase>>16)&0xFFFFu);
        phaser_r=fm6_phaser_channel(in_r,fx_phaser_state_r,((fx_phaser_phase+0x40000000u)>>16)&0xFFFFu);
        fx_phaser_feedback_l=phaser_l; fx_phaser_feedback_r=phaser_r;
        fx_phaser_phase+=4870u+(uint32_t)fx_time[3]*3896u;
    }
    int64_t wet_l=input_l,wet_r=input_r;
    /* FX state runs 32x below the Q24 dry bus. This preserves the int32
     * delay/comb memory footprint while reserving 30 dB for feedback growth. */
    if(fx_enabled[0]) { wet_l+=(int64_t)chorus*FM6_FX_HEADROOM_SCALE*fx_mix_effective[0]/100; wet_r+=(int64_t)chorus_r*FM6_FX_HEADROOM_SCALE*fx_mix_effective[0]/100; }
    if(fx_enabled[1]) { wet_l+=(int64_t)delay_feedback_l*FM6_FX_HEADROOM_SCALE*fx_mix_effective[1]/100; wet_r+=(int64_t)delay_feedback_r*FM6_FX_HEADROOM_SCALE*fx_mix_effective[1]/100; }
    if(fx_enabled[2]) { wet_l+=(int64_t)reverb*FM6_FX_HEADROOM_SCALE*fx_mix_effective[2]/100; wet_r+=(int64_t)reverb_r*FM6_FX_HEADROOM_SCALE*fx_mix_effective[2]/100; }
    if(fx_enabled[3]) { wet_l+=(int64_t)phaser_l*FM6_FX_HEADROOM_SCALE*fx_mix_effective[3]/100; wet_r+=(int64_t)phaser_r*FM6_FX_HEADROOM_SCALE*fx_mix_effective[3]/100; }
    *left=wet_l; *right=wet_r;
}

static uint64_t fm6_abs_i64(int64_t value)
{
    return value < 0 ? (uint64_t)(-(value + 1)) + 1u : (uint64_t)value;
}

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
    int32_t macro_values[FM1LAB_MACRO_COUNT];
    for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++) macro_values[m]=p->macro[m];
    for(uint32_t m=0;m<FM1LAB_MACRO_COUNT;m++)
        macro_values[m]=clamp_i32(macro_values[m]+arp_macro_offset,0,100);
    int32_t body = macro_values[FM1LAB_MACRO_BODY] - 50;
    int32_t motion = macro_values[FM1LAB_MACRO_MOTION];
    int32_t dirt = macro_values[FM1LAB_MACRO_EDGE] - 50;
    int32_t ratio = body * 12 / 50;
    int32_t meg = body * 60 / 50;
    int32_t motion_gain = motion <= 50 ? 25 + motion * 75 / 50
                                        : 100 + (motion - 50) * 200 / 50;
    uint32_t carriers, k, j, carrier_n = 0;
    static const int8_t spread[6] = {0, 1, -1, 2, -2, 3};
    int32_t cents_step = dirt * 1398101 / 50; /* +/-100 cents per carrier spread step */
    fm6_copy_bytes(e, b, FP_SIZE + 1u);
    carriers = fm6_carriers(e[FP_ALG]);

    /* BRIGHT is a full-depth FM-index sweep: the core clamps modulator levels
     * to 0..17 Q24 log2 units. Use 64-bit arithmetic for the full-range offset. */
    p->mod_level = (int32_t)(((int64_t)(macro_values[FM1LAB_MACRO_BRIGHTNESS] - 50) * (18LL << 24)) / 50);
    p->feedback = (uint8_t)clamp_i32((int32_t)b[FP_FB] + dirt * 7 / 50, 0, 7);

    /* MOTION keeps 50 neutral, spanning a quarter-speed crawl through 3x speed/depth. */
    e[FP_LFS] = (uint8_t)clamp_i32((int32_t)b[FP_LFS] * motion_gain / 100, 0, 99);
    e[FP_LPMD] = (uint8_t)clamp_i32((int32_t)b[FP_LPMD] * motion_gain / 100, 0, 99);
    e[FP_LAMD] = (uint8_t)clamp_i32((int32_t)b[FP_LAMD] * motion_gain / 100, 0, 99);

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
    overload_shed_count = 0u;
#if defined(PLAT_WEB)
    fm6_cpu2_total_job_ticks_accum = 0u;
#endif
    output_gain_q12 = 0;
    limiter_gain_q15 = 32768;
#ifdef FM6_TEST
    fm6_test_fx_saturation_count = 0u;
    fm6_test_limiter_event_count = 0u;
    fm6_test_output_peak_q23 = 0u;
#endif
    pitch_bend_16 = 0;
    fm6_comp_enabled=0u; fm6_comp_threshold=65u; fm6_comp_ratio=40u;
    fm6_comp_attack=20u; fm6_comp_release=40u; fm6_comp_makeup=0u; fm6_comp_envelope=0;
    fm6_comp_params_update();
    fm1lab_insert_state_init(&fm6_master_inserts);
    fm6_chaos_amount=0u;fm6_chaos_rate=42u;fm6_chaos_tone=55u;fm6_chaos_shape=0u;
    fm6_chaos_phase=0u;fm6_chaos_random=0x6d2b79f5u;fm6_chaos_random_hold=(int32_t)(fm6_chaos_random>>16)-32768;fm6_chaos_rate_update();
    fm6_amp_tone_state[0]=fm6_amp_tone_state[1]=0;
    fm6_zero_bytes(fm6_eq_x1,sizeof(fm6_eq_x1));fm6_zero_bytes(fm6_eq_x2,sizeof(fm6_eq_x2));
    fm6_zero_bytes(fm6_eq_y1,sizeof(fm6_eq_y1));fm6_zero_bytes(fm6_eq_y2,sizeof(fm6_eq_y2));
    fm6_amp_drive_gain_q15=fm6_amp_drive_gain(fm6_master_inserts.drive);
    fm6_amp_output_gain_q15=fm6_amp_output_gain(fm6_master_inserts.output);
    fm6_tone_alpha_q15=fm6_tone_alpha(fm6_master_inserts.tone);
    for(uint32_t band=0;band<3u;band++)fm6_eq_coeff_update(band);
    for(uint32_t lane=0;lane<2u;lane++) for(uint32_t step=0;step<64u;step++) {
        seq_pattern[lane][step].enabled=0u; seq_pattern[lane][step].note=0u;
        seq_pattern[lane][step].velocity=0u; seq_pattern[lane][step].gate=0u;
    }
    fm6_zero_bytes(seq_gate_left,sizeof(seq_gate_left));
    fm6_zero_bytes(seq_live_note,sizeof(seq_live_note)); fm6_zero_bytes(seq_live_mask,sizeof(seq_live_mask));
    for(uint32_t n=0;n<16u;n++) { arp_notes[n]=0u; arp_velocities[n]=0u; arp_step_gain[n]=127u; arp_step_accent[n]=0u; arp_step_gate[n]=0u; }
    fm6_zero_bytes(fx_delay,sizeof(fx_delay)); fm6_zero_bytes(fx_delay_r,sizeof(fx_delay_r));
    fm6_zero_bytes(fx_chorus,sizeof(fx_chorus)); fm6_zero_bytes(fx_chorus_r,sizeof(fx_chorus_r));
    fm6_zero_bytes(fx_comb_a,sizeof(fx_comb_a)); fm6_zero_bytes(fx_comb_a_r,sizeof(fx_comb_a_r));
    fm6_zero_bytes(fx_comb_b,sizeof(fx_comb_b)); fm6_zero_bytes(fx_comb_b_r,sizeof(fx_comb_b_r));
    fm6_zero_bytes(fx_comb_c,sizeof(fx_comb_c)); fm6_zero_bytes(fx_comb_c_r,sizeof(fx_comb_c_r));
    fm6_zero_bytes(fx_phaser_state_l,sizeof(fx_phaser_state_l));
    fm6_zero_bytes(fx_phaser_state_r,sizeof(fx_phaser_state_r));
    fx_phaser_feedback_l=fx_phaser_feedback_r=0;
    for(uint32_t fx=0;fx<4u;fx++) fx_enabled[fx]=0u;
    for(uint32_t p=0;p<PATCHES;p++) for(uint32_t fx=0;fx<4u;fx++) fx_send[p][fx]=fx==3u?0u:100u;
    fx_mix[0]=18u; fx_mix[1]=24u; fx_mix[2]=20u; fx_mix[3]=35u; fm6_fx_mix_budget_update(); fx_time[0]=48u; fx_time[1]=52u; fx_time[2]=45u; fx_time[3]=35u; fx_feedback[0]=40u; fx_feedback[1]=42u; fx_feedback[2]=60u; fx_feedback[3]=20u; fx_depth[0]=50u; fx_depth[1]=50u; fx_depth[2]=50u; fx_depth[3]=65u;
    seq_lane_base[0]=0u; seq_lane_base[1]=4u; seq_lane_instrument[0]=seq_lane_instrument[1]=0u; seq_length=16u; seq_position=0u; seq_play_position=0u; seq_tick_position=0u;seq_tick_counter=0u;seq_running=0u;
    seq_start_pending=seq_stop_pending=arp_clear_pending=arp_reset_pending=0u;seq_clear_pending[0]=seq_clear_pending[1]=0u;
    arp_capture_head=arp_capture_tail=0u;
    arp_mode=0u; arp_latch=0u; arp_count=0u; arp_index=0u; arp_play_position=0u; arp_direction=1u; arp_live=0u;arp_live_patch_mask=0x0fu;
    arp_division=3u;arp_gate=75u;arp_octaves=1u;arp_length=16u;arp_output=0u;arp_random=0u;arp_macro_dest=0u;arp_depth=50u;
    arp_step_position=0u;arp_phase=0u;arp_gate_left=0u;arp_amp_gain_q15=arp_amp_target_q15=32768;arp_macro_offset=0;
    transport_bpm=120u;transport_swing=50u; transport_phase=0u; seq_gate_left[0]=seq_gate_left[1]=0u;
    seq_event_mode=0u;seq_event_count[0]=seq_event_count[1]=0u;
    for(uint32_t lane=0;lane<2u;lane++)for(uint32_t i=0;i<FM6_SEQ_EVENTS_PER_LANE;i++)seq_event_active[lane][i]=0u;
    fx_delay_pos=fx_chorus_pos=fx_comb_pos_a=fx_comb_pos_b=fx_comb_pos_c=0u; fx_lfo_phase=fx_phaser_phase=0u;
    for (i = 0; i < PATCHES; i++) {
        fm6_unpack(FM6_FACTORY[i % FM1LAB_PATCH_COUNT], instrument[i].base);
        instrument[i].macro[0] = 50;
        instrument[i].macro[1] = 50;
        instrument[i].macro[2] = 50;
        instrument[i].macro[3] = 50;
        instrument[i].enabled = 1u; /* factory instrument starts as a four-layer stack */
        instrument[i].level = 31u; /* 24.4% linear: safe factory starting level */
        instrument[i].pan = 64u; /* centered; constant-voltage pan preserves the reference level */
        instrument[i].pan_l_current_q15 = 32768;
        instrument[i].pan_r_current_q15 = 32768;
        fm6_rebuild_patch(i);
    }
}

void fm6_overload(void)
{
    fm1lab_voice_t *oldest = 0;
    for (uint32_t i = 0; i < VOICES; ++i) {
        fm1lab_voice_t *voice = &voices[i];
        if (!voice->used || voice->fade_out) continue;
        if (!oldest || (int32_t)(voice->age - oldest->age) < 0)
            oldest = voice;
    }
    /* Fade the globally oldest active voice over 32 samples. This does not
     * alter any patch's allocator ownership; the slot becomes free after the
     * next FM6 block has rendered its short fade. */
    if (oldest) {
        oldest->fade_out = 32u;
        ++overload_shed_count;
    }
}

uint32_t fm6_overload_shed_count(void) { return overload_shed_count; }

void fm6_panic(void)
{
    fm6_zero_bytes(voices, sizeof(voices));
    pitch_bend_16 = 0;
    seq_running=0u; seq_start_pending=seq_stop_pending=arp_clear_pending=arp_reset_pending=0u;seq_clear_pending[0]=seq_clear_pending[1]=0u; seq_gate_left[0]=seq_gate_left[1]=0u;
    arp_count=0u; arp_live=0u; arp_index=0u;arp_gate_left=0u;arp_patch_mask=0x0fu;
    arp_amp_gain_q15=arp_amp_target_q15=32768;fm6_arp_macro_apply(0);
}

static void fm6_note_start_tuned_mask(uint8_t note,uint8_t velocity,uint8_t patch_mask,int16_t tune16,uint16_t source)
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
        v->source = source;
        v->age = ++age_counter;
        v->fade_in = 32;
        v->fade_out = 0u;
        v->tune16 = tune16;
        fm6_note_init(&v->state, p->effective, (int32_t)note + p->effective[FP_TRNSP] - 24, velocity, 1);
        fm6_lfo_key(&patch_lfo[i]);
    }
}

void fm6_note_on_tuned_mask(uint8_t note,uint8_t velocity,uint8_t patch_mask,int16_t tune16)
{
    if(note>127u || !velocity) return;
    if(arp_mode) {
        uint32_t pos=0u;
        arp_patch_mask=patch_mask;
        if(!arp_count){arp_index=0u;arp_direction=1u;arp_step_position=0u;arp_phase=0u;}
        while(pos<arp_count && arp_notes[pos]<note) ++pos;
        if(pos<arp_count && arp_notes[pos]==note) return;
        if(arp_count<16u) {
            for(uint32_t k=arp_count;k>pos;k--) {
                arp_notes[k]=arp_notes[k-1u];
                arp_velocities[k]=arp_velocities[k-1u];
            }
            arp_notes[pos]=note;
            arp_velocities[pos]=velocity;
            ++arp_count;
        }
        if(arp_output==1u)fm6_note_start_tuned_mask(note,velocity,patch_mask,tune16,0u);
        return;
    }
    fm6_note_start_tuned_mask(note,velocity,patch_mask,tune16,0u);
}

void fm6_note_on_mask(uint8_t note, uint8_t velocity, uint8_t patch_mask)
{
    fm6_note_on_tuned_mask(note, velocity, patch_mask, 0);
}

void fm6_sequencer_note_on(uint8_t note,uint8_t velocity,uint8_t patch_mask,uint16_t event_token)
{
    if(event_token<=3u)return;
    fm6_note_start_tuned_mask(note,velocity,patch_mask,0,event_token);
}

void fm6_sequencer_note_off(uint8_t note,uint8_t patch_mask,uint16_t event_token)
{
    if(event_token<=3u)return;
    fm6_note_off_source(note,patch_mask,event_token);
}

void fm6_note_on(uint8_t note, uint8_t velocity)
{
    fm6_note_on_mask(note, velocity, (uint8_t)((1u << PATCHES) - 1u));
}

static void fm6_note_off_source(uint8_t note,uint8_t patch_mask,uint16_t source)
{
    for(uint32_t i=0;i<VOICES;i++) {
        fm1lab_voice_t *v=&voices[i];
        if(v->used && v->note==note && v->source==source && (patch_mask&(1u<<v->patch))) {
            fm1lab_patch_t *p=&instrument[v->patch];
            fm6_note_key(&v->state,p->effective,0);
        }
    }
}
void fm6_note_off_mask(uint8_t note,uint8_t patch_mask)
{
    if(arp_mode && !arp_latch) {
        for(uint32_t k=0;k<arp_count;k++) if(arp_notes[k]==note) {
            for(uint32_t j=k+1u;j<arp_count;j++){arp_notes[j-1u]=arp_notes[j];arp_velocities[j-1u]=arp_velocities[j];}
            --arp_count;if(arp_index>k&&arp_index)--arp_index;
            if(arp_output==1u)fm6_note_off_source(note,patch_mask,0u);
            break;
        }
        if(!arp_count){
            if(arp_live){fm6_arp_capture_push(0u,arp_live_note,0u,arp_live_patch_mask);fm6_note_off_source(arp_live_note,arp_live_patch_mask,3u);}
            arp_live=0u;arp_gate_left=0u;arp_amp_target_q15=32768;fm6_arp_macro_apply(0);
        }
    }
    for(uint32_t i=0;i<VOICES;i++) {
        fm1lab_voice_t *v=&voices[i];
        if(v->used && v->note==note && v->source==0u && (patch_mask&(1u<<v->patch))) {
            fm1lab_patch_t *p=&instrument[v->patch];
            fm6_note_key(&v->state,p->effective,0);
        }
    }
}

void fm6_note_off(uint8_t note)
{
    if(arp_mode) {
        for(uint32_t k=0;k<arp_count;k++) if(arp_notes[k]==note) {
            for(uint32_t j=k+1u;j<arp_count;j++) {
                arp_notes[j-1u]=arp_notes[j];
                arp_velocities[j-1u]=arp_velocities[j];
            }
            --arp_count; if(arp_index>k && arp_index) --arp_index;
            if(arp_output==1u)fm6_note_off_source(note,(uint8_t)((1u<<PATCHES)-1u),0u);
            break;
        }
        if(!arp_count && arp_live) { fm6_arp_capture_push(0u,arp_live_note,0u,arp_live_patch_mask);fm6_note_off_source(arp_live_note,arp_live_patch_mask,3u); arp_live=0u; }
        if(!arp_count){arp_gate_left=0u;arp_amp_target_q15=32768;fm6_arp_macro_apply(0);}
        return;
    }
    fm6_note_off_source(note,(uint8_t)((1u<<PATCHES)-1u),0u);
}

void fm6_pitch_bend(uint16_t value)
{
    if (value > 16383u) value = 16383u;
    pitch_bend_16 = (int16_t)(((int32_t)value - 8192) * 32 / 8192);
}

/* Render a contiguous slice of the dynamically packed active-voice list.
 * Each worker owns its voices, output accumulator and FM6 operator bus. */
static void fm6_render_voice_range(uint32_t first, uint32_t end,
                                   int64_t patch_mix[PATCHES][FM6_N],
                                   int32_t bus[FM6_N])
{
    for (uint32_t k = first; k < end; ++k) {
        uint32_t i;
        fm1lab_voice_t *voice = &voices[fm6_voice_work[k]];
        fm1lab_patch_t *patch = &instrument[voice->patch];
        int32_t pitch, fb;
        if (!voice->used)
            continue;
        pitch = fm6_note_logfreq(((int32_t)voice->note + patch->effective[FP_TRNSP] - 24) * 16 + voice->tune16 + pitch_bend_16, 0);
        /* Q24 log2-frequency offset: octave is exact; cents retain 1-cent resolution. */
        pitch += (int32_t)patch->octave * (1 << 24);
        pitch += (int32_t)(((int64_t)patch->fine_cents * (1LL << 24)) / 1200LL);
        fb = patch->feedback;
        if (fm6_note_compute(&voice->state, patch->effective, bus,
                             patch_lfo_value[voice->patch], patch_lfo_delay[voice->patch],
                             pitch, patch->effective[FP_ALG], fb, patch->detune, patch->mod_level)) {
            int32_t dc = voice->state.dc;
            uint8_t shedding = voice->fade_out != 0u;
            for (i = 0; i < FM6_N; i++) {
                int32_t x = bus[i] - dc;
                dc += x >> 10; /* ~7 Hz high-pass; FM sidebands can otherwise leave DC. */
                if (voice->fade_in) {
                    uint32_t gain = 32u - voice->fade_in;
                    x = (int32_t)(((int64_t)x * gain) >> 5);
                    voice->fade_in--;
                }
                if (voice->fade_out) {
                    x = (int32_t)(((int64_t)x * voice->fade_out) >> 5);
                    --voice->fade_out;
                }
                patch_mix[voice->patch][i] += (((int64_t)x * fm6_patch_gain_q15[voice->patch]) >> 15) * FM6_PATCH_CALIBRATION_Q15 >> 15;
            }
            voice->state.dc = dc;
            if (shedding && !voice->fade_out) {
                voice->state.live = 0;
                voice->used = 0;
            }
        }
        if (voice->used && fm6_note_done(&voice->state, patch->effective, patch->effective[FP_ALG])) {
            voice->state.live = 0;
            voice->used = 0;
        }
    }
}

static void fm6_cpu2_voice_job(uint32_t unused)
{
    (void)unused;
#ifndef FM6_TEST
    uint32_t t0 = plat_ticks24();
#endif
    fm6_render_voice_range(fm6_cpu2_job_start,
                           fm6_cpu2_job_start + fm6_cpu2_job_count,
                           fm6_patch_mix[1], fm6_voice_bus[1]);
#ifndef FM6_TEST
    {
        uint32_t elapsed = plat_ticks24() - t0;
        fm6_cpu2_job_us = elapsed / PLAT_TICKS_PER_US;
#if defined(PLAT_WEB)
        fm6_cpu2_total_job_ticks_accum += elapsed;
#endif
    }
#endif
}

uint32_t fm6_cpu2_last_job_us(void) { return fm6_cpu2_job_us; }
uint32_t fm6_cpu2_last_wait_us(void) { return fm6_cpu2_wait_us; }
uint32_t fm6_cpu2_total_job_ticks(void)
{
#if defined(PLAT_WEB)
    return fm6_cpu2_total_job_ticks_accum;
#else
    return 0u;
#endif
}

void fm6_render(int32_t *stereo, uint32_t frames, uint32_t master_gain_q12)
{
#ifdef FM6_TEST
    fm6_zero_bytes(fm6_test_patch_peak_q24, sizeof fm6_test_patch_peak_q24);
    fm6_test_dry_peak_q24 = 0u;
    fm6_zero_bytes(fm6_test_send_peak, sizeof fm6_test_send_peak);
#endif
    uint32_t base;
    for (base = 0; base < frames; base += FM6_N) {
        int32_t patch_pan_l_q15[PATCHES], patch_pan_r_q15[PATCHES];
        uint32_t p, v, i;
        uint32_t count = frames - base < FM6_N ? frames - base : FM6_N;

        fm6_zero_bytes(fm6_patch_mix[0], sizeof fm6_patch_mix[0]);
        fm6_zero_bytes(fm6_patch_gain_q15, sizeof fm6_patch_gain_q15);
        /* Patch level is an independent linear-amplitude control. Never
         * renormalize it when other patches become active or inactive. */
        for (p = 0; p < PATCHES; ++p) {
            if (instrument[p].enabled && instrument[p].level)
                fm6_patch_gain_q15[p] = (int32_t)((uint32_t)instrument[p].level * 32768u / 127u);
            fm6_pan_gains(instrument[p].pan, &patch_pan_l_q15[p], &patch_pan_r_q15[p]);
        }
        for (p = 0; p < PATCHES; p++) {
            patch_lfo_value[p] = fm6_lfo_sample(&patch_lfo[p]);
            patch_lfo_delay[p] = fm6_lfo_delay(&patch_lfo[p]);
        }

        /* Pack only sounding voices, then divide that list. This is important:
         * a fixed split by patch index would leave the second core idle when a
         * single patch is being played. */
        uint32_t active_voice_count = 0u;
        for (v = 0; v < VOICES; ++v)
            if (voices[v].used)
                fm6_voice_work[active_voice_count++] = (uint8_t)v;

        if (active_voice_count > 1u) {
            uint32_t split = (active_voice_count + 1u) / 2u;
            fm6_cpu2_job_start = split;
            fm6_cpu2_job_count = active_voice_count - split;
#ifdef FM6_TEST
            if (fm6_test_cpu2_mode == 1u || fm6_test_cpu2_mode == 2u) {
                fm6_zero_bytes(fm6_patch_mix[1], sizeof fm6_patch_mix[1]);
                for (uint32_t k = 0; k < fm6_cpu2_job_count; ++k)
                    fm6_cpu2_voice_backup[k] = voices[fm6_voice_work[split + k]];
                if (fm6_test_cpu2_mode == 1u) {
                    fm6_cpu2_voice_job(0u);
                    fm6_render_voice_range(0u, split, fm6_patch_mix[0], fm6_voice_bus[0]);
                    for (p = 0; p < PATCHES; ++p)
                        for (i = 0; i < FM6_N; ++i)
                            fm6_patch_mix[0][p][i] += fm6_patch_mix[1][p][i];
                } else {
                    uint32_t partial = fm6_cpu2_job_count / 2u;
                    if (!partial) partial = 1u;
                    fm6_render_voice_range(split, split + partial,
                                           fm6_patch_mix[1], fm6_voice_bus[1]);
                    fm6_render_voice_range(0u, split, fm6_patch_mix[0], fm6_voice_bus[0]);
                    for (uint32_t k = 0; k < fm6_cpu2_job_count; ++k)
                        voices[fm6_voice_work[split + k]] = fm6_cpu2_voice_backup[k];
                    fm6_render_voice_range(split, active_voice_count,
                                           fm6_patch_mix[0], fm6_voice_bus[0]);
                }
            } else {
                fm6_render_voice_range(0u, active_voice_count,
                                       fm6_patch_mix[0], fm6_voice_bus[0]);
            }
#elif defined(FM1_CPU1)
            if (plat_cpu2_on()) {
                fm6_zero_bytes(fm6_patch_mix[1], sizeof fm6_patch_mix[1]);
                for (uint32_t k = 0; k < fm6_cpu2_job_count; ++k)
                    fm6_cpu2_voice_backup[k] = voices[fm6_voice_work[split + k]];
                if (plat_cpu2_run(fm6_cpu2_voice_job, 0u)) {
                    fm6_render_voice_range(0u, split,
                                           fm6_patch_mix[0], fm6_voice_bus[0]);
                    uint32_t wait_t0 = plat_ticks24();
                    int wait_rc = plat_cpu2_wait();
                    fm6_cpu2_wait_us = (plat_ticks24() - wait_t0) / PLAT_TICKS_PER_US;
                    if (wait_rc == 0) {
                        for (p = 0; p < PATCHES; ++p)
                            for (i = 0; i < FM6_N; ++i)
                                fm6_patch_mix[0][p][i] += fm6_patch_mix[1][p][i];
                    } else {
                        /* If core 1 was late or faulted mid-job, roll back its
                         * voice state before rendering that half on core 0. */
                        for (uint32_t k = 0; k < fm6_cpu2_job_count; ++k)
                            voices[fm6_voice_work[split + k]] = fm6_cpu2_voice_backup[k];
                        fm6_render_voice_range(split, active_voice_count,
                                               fm6_patch_mix[0], fm6_voice_bus[0]);
                    }
                } else {
                    fm6_render_voice_range(0u, active_voice_count,
                                           fm6_patch_mix[0], fm6_voice_bus[0]);
                }
            } else {
                fm6_render_voice_range(0u, active_voice_count,
                                       fm6_patch_mix[0], fm6_voice_bus[0]);
            }
#else
            fm6_render_voice_range(0u, active_voice_count,
                                   fm6_patch_mix[0], fm6_voice_bus[0]);
#endif
        } else {
            fm6_render_voice_range(0u, active_voice_count,
                                   fm6_patch_mix[0], fm6_voice_bus[0]);
        }

        for (i = 0; i < count; i++) {
            int32_t delta=(int32_t)master_gain_q12-output_gain_q12;
            if(delta) output_gain_q12+=delta/64+(delta>0?1:-1);
            /* Stop first: a rapid stop/start must release the old lane notes
             * before start resets their gate counters. */
            if(seq_stop_pending) {
                for(uint32_t lane=0;lane<2u;lane++) {
                    if(seq_gate_left[lane]) {
                        fm6_note_off_source(seq_live_note[lane],seq_live_mask[lane],(uint8_t)(lane+1u));
                        seq_gate_left[lane]=0u;
                    }
                    fm6_seq_event_release_lane(lane);
                }
                seq_stop_pending=0u;
            }
            if(seq_start_pending) {
                for(uint32_t lane=0;lane<2u;lane++) {
                    if(seq_gate_left[lane]) {
                        fm6_note_off_source(seq_live_note[lane],seq_live_mask[lane],(uint8_t)(lane+1u));
                        seq_gate_left[lane]=0u;
                    }
                    fm6_seq_event_release_lane(lane);
                }
                seq_position=0u;seq_tick_position=0u;seq_tick_counter=0u; transport_phase=0u; seq_start_pending=0u;
            }
            if(arp_clear_pending) {
                if(arp_live){fm6_arp_capture_push(0u,arp_live_note,0u,arp_live_patch_mask);fm6_note_off_source(arp_live_note,arp_live_patch_mask,3u);}
                arp_live=0u; arp_count=0u; arp_index=0u; arp_gate_left=0u;
                arp_amp_target_q15=32768;fm6_arp_macro_apply(0);arp_clear_pending=0u;
            }
            if(arp_reset_pending) {
                if(arp_live){fm6_arp_capture_push(0u,arp_live_note,0u,arp_live_patch_mask);fm6_note_off_source(arp_live_note,arp_live_patch_mask,3u);}
                arp_live=0u;arp_index=0u;arp_direction=1u;arp_step_position=0u;
                arp_play_position=0u;arp_phase=0u;arp_gate_left=0u;
                arp_amp_target_q15=32768;fm6_arp_macro_apply(0);arp_reset_pending=0u;
            }
            for(uint32_t lane=0;lane<2u;lane++)if(seq_clear_pending[lane]){
                if(seq_gate_left[lane])
                    fm6_note_off_source(seq_live_note[lane],seq_live_mask[lane],(uint8_t)(lane+1u));
                seq_gate_left[lane]=0u;
                fm6_seq_event_release_lane(lane);
                seq_event_count[lane]=0u;
                for(uint32_t step=0;step<64u;step++)seq_pattern[lane][step].enabled=0u;
                seq_clear_pending[lane]=0u;
            }
            transport_phase+=(uint32_t)transport_bpm*8u;
            {uint32_t threshold=fm6_transport_tick_threshold();
             if(transport_phase>=threshold) { transport_phase-=threshold; fm6_transport_tick(); }}
            arp_phase+=(uint32_t)transport_bpm*(1u<<(arp_division>5u?5u:arp_division));
            {uint32_t percent=(arp_step_position&1u)?(100u-transport_swing)*2u:(uint32_t)transport_swing*2u;
             uint32_t threshold=(FM6_SAMPLE_RATE*60u*4u*percent)/100u;
             if(arp_phase>=threshold){arp_phase-=threshold;fm6_arpeggiator_step();}}
            if(arp_gate_left && --arp_gate_left==0u) {
                if(arp_live){fm6_arp_capture_push(0u,arp_live_note,0u,arp_live_patch_mask);fm6_note_off_source(arp_live_note,arp_live_patch_mask,3u);arp_live=0u;}
                if(arp_output==1u)arp_amp_target_q15=0;
                fm6_arp_macro_apply(0);
            }
            {int32_t delta=arp_amp_target_q15-arp_amp_gain_q15;if(delta)arp_amp_gain_q15+=delta/32+(delta>0?1:-1);}
            for(uint32_t lane=0;lane<2u;lane++) if(seq_gate_left[lane] && --seq_gate_left[lane]==0u) {
                uint8_t next=(uint8_t)(seq_position%seq_length);
                volatile fm6_step_t *following=&seq_pattern[lane][next];
                uint8_t tied=(uint8_t)(seq_running && following->enabled && following->velocity &&
                    (following->gate & 0x80u) && following->note==seq_live_note[lane]);
                if(!tied && seq_live_mask[lane]) {
                    fm6_note_off_source(seq_live_note[lane],seq_live_mask[lane],(uint8_t)(lane+1u));
                    seq_live_mask[lane]=0u;
                }
            }
            int64_t dry=0, dry_l=0, dry_r=0, send_accum[4][2]={{0,0},{0,0},{0,0},{0,0}};
            for(uint32_t p=0;p<PATCHES;p++) {
                int64_t sample=fm6_patch_mix[0][p][i];
                if(arp_output==1u)sample=(sample*arp_amp_gain_q15)>>15;
#ifdef FM6_TEST
                uint64_t patch_abs=fm6_test_abs_i64(sample);
                if(patch_abs>fm6_test_patch_peak_q24[p]) fm6_test_patch_peak_q24[p]=patch_abs;
#endif
                dry+=sample; /* mono diagnostic reference, before pan */
                int32_t pan_delta_l=patch_pan_l_q15[p]-instrument[p].pan_l_current_q15;
                int32_t pan_delta_r=patch_pan_r_q15[p]-instrument[p].pan_r_current_q15;
                int32_t pan_step_l=pan_delta_l/64, pan_step_r=pan_delta_r/64;
                if(pan_delta_l && !pan_step_l) pan_step_l=pan_delta_l>0?1:-1;
                if(pan_delta_r && !pan_step_r) pan_step_r=pan_delta_r>0?1:-1;
                instrument[p].pan_l_current_q15+=pan_step_l;
                instrument[p].pan_r_current_q15+=pan_step_r;
                dry_l+=(sample*instrument[p].pan_l_current_q15)>>15;
                dry_r+=(sample*instrument[p].pan_r_current_q15)>>15;
                /* Apply the same smoothed per-patch pan to each send before
                 * the shared stereo FX buses. This keeps a hard-panned layer
                 * out of the opposite return channel. */
                int64_t send_l=(sample*instrument[p].pan_l_current_q15)>>15;
                int64_t send_r=(sample*instrument[p].pan_r_current_q15)>>15;
                for(uint32_t fx=0;fx<4u;fx++) if(fx_enabled[fx]) {
                    send_accum[fx][0]+=send_l*fx_send[p][fx]/100;
                    send_accum[fx][1]+=send_r*fx_send[p][fx]/100;
                }
            }
#ifdef FM6_TEST
            for(uint32_t fx=0;fx<4u;fx++) for(uint32_t channel=0;channel<2u;channel++) {
                uint64_t send_abs=fm6_test_abs_i64(send_accum[fx][channel]);
                if(send_abs>fm6_test_send_peak[fx][channel]) fm6_test_send_peak[fx][channel]=send_abs;
            }
#endif
#ifdef FM6_TEST
            {
                uint64_t dry_abs=fm6_test_abs_i64(dry);
                if(dry_abs>fm6_test_dry_peak_q24) fm6_test_dry_peak_q24=dry_abs;
            }
#endif
            /* Master trim is part of the fixed gain budget, before FX sends
             * and returns. FX buses then enter their Q19 internal domain. */
            int64_t trimmed_dry_l = (dry_l * FM6_MASTER_TRIM_Q15) >> 15;
            int64_t trimmed_dry_r = (dry_r * FM6_MASTER_TRIM_Q15) >> 15;
            int32_t sends[4][2];
            for (uint32_t fx=0; fx<4u; ++fx) {
                for (uint32_t channel=0; channel<2u; ++channel) {
                    int64_t trimmed_send = (send_accum[fx][channel] * FM6_MASTER_TRIM_Q15) >> 15;
                    sends[fx][channel] = fm6_sat_q23(trimmed_send >> FM6_FX_HEADROOM_SHIFT);
                }
            }
            int64_t left=0,right=0;
            fm6_fx_process(trimmed_dry_l,trimmed_dry_r,sends,&left,&right);
            /* One Instrument-wide post-mix chain: Amp/Drive -> parametric EQ -> stereo-linked compressor. */
            fm6_chaos_process(&left,&right);
            fm6_master_insert_process(&left,&right);
            fm6_master_comp_process(&left,&right);
            /* fm1-base passes APP_GAIN(master): Q12 amplitude, 0..4096.
             * The output stage adds the requested 6 dB; calibrated Instrument
             * levels preserve headroom, with the stereo-linked limiter as backup. */
            int64_t out_l = ((left * output_gain_q12) >> 12);
            int64_t out_r = ((right * output_gain_q12) >> 12);
            uint64_t abs_l = fm6_abs_i64(out_l);
            uint64_t abs_r = fm6_abs_i64(out_r);
            uint64_t peak = abs_l > abs_r ? abs_l : abs_r;
            uint32_t target_gain = 32768u;
            if (peak > FM6_LIMITER_CEILING_Q23) {
                /* AC79's PI32V2 runtime has no __udivdi3. Scale the peak
                 * into Q15 first, then use a native 32-bit divide. The final
                 * hard ceiling below covers the less-than-one-LSB ratio error. */
                uint64_t scaled_peak = peak >> 15;
                uint32_t peak_q15 = scaled_peak > UINT32_MAX ? UINT32_MAX : (uint32_t)scaled_peak;
                target_gain = scaled_peak > UINT32_MAX ? 1u :
                    (peak_q15 ? (uint32_t)FM6_LIMITER_CEILING_Q23 / peak_q15 : 0u);
            }
            if (target_gain < (uint32_t)limiter_gain_q15) {
                limiter_gain_q15 = (int32_t)target_gain; /* instantaneous attack */
#ifdef FM6_TEST
                ++fm6_test_limiter_event_count;
#endif
            } else if (limiter_gain_q15 < 32768) {
                uint32_t release = (uint32_t)(32768 - limiter_gain_q15 + 255) / 256u;
                limiter_gain_q15 += (int32_t)(release ? release : 1u); /* ~5.8 ms release */
                if (limiter_gain_q15 > 32768) limiter_gain_q15 = 32768;
            }
            out_l = (out_l * limiter_gain_q15) >> 15;
            out_r = (out_r * limiter_gain_q15) >> 15;
            if (out_l > FM6_LIMITER_CEILING_Q23) out_l = FM6_LIMITER_CEILING_Q23;
            if (out_l < -FM6_LIMITER_CEILING_Q23) out_l = -FM6_LIMITER_CEILING_Q23;
            if (out_r > FM6_LIMITER_CEILING_Q23) out_r = FM6_LIMITER_CEILING_Q23;
            if (out_r < -FM6_LIMITER_CEILING_Q23) out_r = -FM6_LIMITER_CEILING_Q23;
#ifdef FM6_TEST
            abs_l = fm6_abs_i64(out_l);
            abs_r = fm6_abs_i64(out_r);
            peak = abs_l > abs_r ? abs_l : abs_r;
            if (peak > fm6_test_output_peak_q23) fm6_test_output_peak_q23 = peak;
#endif
            stereo[(base+i)*2u] = (int32_t)out_l;
            stereo[(base+i)*2u+1u] = (int32_t)out_r;
        }
    }
}


void fm6_set_tempo(uint16_t bpm) { transport_bpm=bpm<30u?30u:bpm>300u?300u:bpm; }
void fm6_set_swing(uint8_t percent) { transport_swing=percent<50u?50u:percent>75u?75u:percent; }
uint8_t fm6_get_swing(void) { return transport_swing; }
void fm6_sequencer_set_running(uint8_t running) {
    uint8_t next=(uint8_t)(running!=0u);
    if(next&&!seq_running) seq_start_pending=1u;
    if(!next&&seq_running) seq_stop_pending=1u;
    seq_running=next;
}
static void fm6_seq_event_trim(uint8_t length)
{
    if(!seq_event_mode)return;
    uint32_t loop_ticks=(uint32_t)length*2u;
    for(uint32_t lane=0;lane<2u;lane++) {
        fm6_seq_event_release_lane(lane);
        uint32_t write=0u;
        for(uint32_t i=0;i<seq_event_count[lane];i++) {
            fm6_seq_event_t event=seq_events[lane][i];
            if(event.start>=loop_ticks)continue;
            uint32_t max_duration=loop_ticks-event.start;
            if(event.duration>max_duration)event.duration=(uint8_t)max_duration;
            if(event.duration)seq_events[lane][write++]=event;
        }
        for(uint32_t i=write;i<FM6_SEQ_EVENTS_PER_LANE;i++)seq_event_active[lane][i]=0u;
        seq_event_count[lane]=(uint8_t)write;
    }
}
void fm6_sequencer_set_length(uint8_t steps) {
    uint8_t next=steps<2u?2u:steps>64u?64u:steps;
    if(next!=seq_length) {
        seq_length=next;
        fm6_seq_event_trim(seq_length);
    }
    uint16_t loop_ticks=(uint16_t)seq_length*2u;
    if(seq_tick_position>=loop_ticks)seq_tick_position=0u;
    seq_position=(uint8_t)(seq_tick_position/2u);
}
void fm6_sequencer_set_lane(uint8_t lane,uint8_t patch_base) { if(lane<2u&&(patch_base==0u||patch_base==4u)) seq_lane_base[lane]=patch_base; }
void fm6_sequencer_set_step(uint8_t lane,uint8_t step,uint8_t enabled,uint8_t note,uint8_t velocity,uint8_t gate) {
    if(lane>=2u||step>=64u||note>127u||velocity>127u) return;
    seq_pattern[lane][step].enabled=0u;
    seq_pattern[lane][step].note=note;
    seq_pattern[lane][step].velocity=velocity;
    seq_pattern[lane][step].gate=gate<10u?10u:gate>100u?100u:gate;
    seq_pattern[lane][step].enabled=enabled!=0u;
}
int fm6_sequencer_set_events(uint8_t lane,const fm6_seq_event_t *events,uint16_t count)
{
    if(lane>=2u || count>FM6_SEQ_EVENTS_PER_LANE || (count && !events)) return -1;
    /* Validate the entire replacement before touching the currently sounding pattern. */
    for(uint32_t i=0;i<count;i++) {
        const fm6_seq_event_t *event=&events[i];
        if(event->start>=(uint32_t)seq_length*2u || !event->duration || event->pitch>127u ||
           !event->velocity || event->velocity>127u || event->accent>100u) return -1;
    }
    fm6_seq_event_release_lane(lane);
    if(seq_live_mask[lane]) {
        fm6_note_off_source(seq_live_note[lane],seq_live_mask[lane],(uint16_t)(lane+1u));
        seq_live_mask[lane]=0u;
    }
    seq_gate_left[lane]=0u;
    for(uint32_t i=0;i<FM6_SEQ_EVENTS_PER_LANE;i++) {
        seq_event_active[lane][i]=0u;
        if(i<count) {
            fm6_seq_event_t event=events[i];
            uint32_t max_duration=(uint32_t)seq_length*2u-event.start;
            if(event.duration>max_duration) event.duration=(uint8_t)max_duration;
            seq_events[lane][i]=event;
        }
    }
    seq_event_count[lane]=(uint8_t)count;
    seq_event_mode=1u;
    return 0;
}
uint8_t fm6_sequencer_step(void) { return seq_position; }
uint8_t fm6_sequencer_playing_step(void) { return seq_play_position; }
uint8_t fm6_sequencer_tick_position(void)
{
    uint8_t position=seq_tick_position;
    if(transport_phase*2u>=fm6_transport_tick_threshold()) {
        uint16_t next=(uint16_t)position+1u,loop_ticks=(uint16_t)seq_length*2u;
        position=(uint8_t)(next>=loop_ticks?0u:next);
    }
    return position;
}
uint8_t fm6_sequencer_running(void) { return seq_running; }
uint8_t fm6_arp_capture_pop(fm6_arp_capture_event_t *event)
{
    uint8_t tail=arp_capture_tail;
    if(!event || tail==arp_capture_head)return 0u;
    *event=arp_capture_queue[tail];
    arp_capture_tail=(uint8_t)((tail+1u)&63u);
    return 1u;
}
uint8_t fm6_arpeggiator_playing_step(void) { return arp_play_position; }
uint8_t fm6_arpeggiator_active(void) { return (uint8_t)(arp_mode && arp_count); }
void fm6_arpeggiator_set_mode(uint8_t mode) {
    if(mode>4u) mode=4u;
    /* Enabling starts with an empty held-note set naturally (keys played
     * while bypassed are not captured). Only disabling needs an audio-owner
     * release/clear event for the currently generated arp note. */
    if(arp_mode!=0u && mode==0u) arp_clear_pending=1u;
    if(arp_mode==0u && mode!=0u) { arp_index=0u; arp_direction=1u;arp_step_position=0u;arp_phase=0u; }
    arp_mode=mode;
}
uint8_t fm6_arpeggiator_mode(void) { return arp_mode; }
void fm6_arpeggiator_reset_steps(void) { arp_reset_pending=1u; }
void fm6_arpeggiator_set_latch(uint8_t enabled) { arp_latch=(uint8_t)(enabled!=0u); }
void fm6_sequencer_clear_lane(uint8_t lane) { if(lane<2u)seq_clear_pending[lane]=1u; }
#ifdef FM6_TEST
uint8_t fm6_test_arpeggiator_amp_level(void)
{
    uint32_t level=(uint32_t)(arp_amp_gain_q15<0?0:arp_amp_gain_q15);
    return (uint8_t)(level>=32768u?127u:level*127u/32768u);
}
#endif
void fm6_fx_set_enabled(uint8_t effect,uint8_t enabled)
{
    if(effect>=4u)return;
    uint8_t next=(uint8_t)(enabled!=0u);
    if(!fx_enabled[effect] && next) {
        /* Never resurrect a tail from before a bypass interval. */
        if(effect==0u) {
            fm6_zero_bytes(fx_chorus,sizeof(fx_chorus));
            fm6_zero_bytes(fx_chorus_r,sizeof(fx_chorus_r));
            fx_chorus_pos=0u;
        } else if(effect==1u) {
            fm6_zero_bytes(fx_delay,sizeof(fx_delay));
            fm6_zero_bytes(fx_delay_r,sizeof(fx_delay_r));
            fx_delay_pos=0u;
        } else if(effect==2u) {
            fm6_zero_bytes(fx_comb_a,sizeof(fx_comb_a));
            fm6_zero_bytes(fx_comb_a_r,sizeof(fx_comb_a_r));
            fm6_zero_bytes(fx_comb_b,sizeof(fx_comb_b));
            fm6_zero_bytes(fx_comb_b_r,sizeof(fx_comb_b_r));
            fm6_zero_bytes(fx_comb_c,sizeof(fx_comb_c));
            fm6_zero_bytes(fx_comb_c_r,sizeof(fx_comb_c_r));
            fx_comb_pos_a=fx_comb_pos_b=fx_comb_pos_c=0u;
        } else {
            fm6_zero_bytes(fx_phaser_state_l,sizeof(fx_phaser_state_l));
            fm6_zero_bytes(fx_phaser_state_r,sizeof(fx_phaser_state_r));
            fx_phaser_feedback_l=fx_phaser_feedback_r=0;
        }
    }
    fx_enabled[effect]=next;
    fm6_fx_mix_budget_update();
}
void fm6_fx_set_mix(uint8_t effect,uint8_t mix) {
    if(effect<4u) { fx_mix[effect]=mix>100u?100u:mix; fm6_fx_mix_budget_update(); }
}
void fm6_fx_set_time(uint8_t effect,uint8_t value) { if(effect<4u) fx_time[effect]=value>100u?100u:value; }
void fm6_fx_set_feedback(uint8_t effect,uint8_t value) { if(effect<4u) fx_feedback[effect]=value>(effect==0u?70u:90u)?(effect==0u?70u:90u):value; }
void fm6_fx_set_depth(uint8_t effect,uint8_t value) { if(effect<4u) fx_depth[effect]=value>100u?100u:value; }
uint8_t fm6_fx_enabled(uint8_t effect) { return effect<4u?fx_enabled[effect]:0u; }
uint8_t fm6_fx_mix(uint8_t effect) { return effect<4u?fx_mix[effect]:0u; }
uint8_t fm6_fx_time(uint8_t effect) { return effect<4u?fx_time[effect]:0u; }
uint8_t fm6_fx_feedback(uint8_t effect) { return effect<4u?fx_feedback[effect]:0u; }
uint8_t fm6_fx_depth(uint8_t effect) { return effect<4u?fx_depth[effect]:0u; }
void fm6_fx_set_send(uint8_t patch,uint8_t effect,uint8_t send) {
    if(patch<FM1LAB_PATCH_COUNT&&effect<4u) {
        uint8_t value=send>100u?100u:send;
        fx_send[patch][effect]=value;
        fx_send[patch+FM1LAB_PATCH_COUNT][effect]=value;
    }
}
uint8_t fm6_fx_get_send(uint8_t patch,uint8_t effect) {
    return patch<PATCHES&&effect<4u?fx_send[patch%FM1LAB_PATCH_COUNT][effect]:0u;
}

int fm6_performance_state_valid(const fm6_performance_state_t *s)
{
    if (!s || s->tempo_bpm < 30u || s->tempo_bpm > 300u ||
        s->seq_length < 1u || s->seq_length > 64u || s->arp_mode > 4u ||
        s->arp_division>5u || s->arp_gate<10u || s->arp_gate>100u ||
        s->arp_octaves<1u || s->arp_octaves>4u || (s->arp_length!=8u&&s->arp_length!=16u) ||
        s->arp_output>2u || s->arp_random>100u || s->arp_macro_dest>3u || s->arp_depth>100u)
        return 0;
    for(uint32_t step=0;step<16u;step++)
        if(s->arp_step_gain[step]>127u||s->arp_step_accent[step]>100u||s->arp_step_gate[step]>1u)return 0;
    for (uint32_t lane = 0; lane < 2u; ++lane) {
        if (s->lane_instrument[lane] >= 32u) return 0;
        for (uint32_t step = 0; step < 64u; ++step) {
            if (s->seq_enabled[lane][step] > 1u ||
                s->seq_note[lane][step] > 127u ||
                s->seq_velocity[lane][step] > 127u ||
                (s->seq_gate[lane][step] & 0x7fu) > 100u)
                return 0;
        }
    }
    for(uint32_t lane=0;lane<2u;lane++) {
        if(s->seq_event_count[lane]>FM6_SEQ_EVENTS_PER_LANE)return 0;
        for(uint32_t i=0;i<s->seq_event_count[lane];i++) {
            const fm6_seq_event_t *event=&s->seq_events[lane][i];
            if(event->start>=(uint32_t)s->seq_length*2u || !event->duration ||
               event->duration>(uint32_t)s->seq_length*2u-event->start ||
               event->pitch>127u || !event->velocity || event->velocity>127u ||
               event->accent>100u)return 0;
        }
    }
    for (uint32_t fx = 0; fx < 4u; ++fx) {
        if (s->fx_enabled[fx] > 1u || s->fx_mix[fx] > 100u ||
            s->fx_time[fx] > 100u || s->fx_depth[fx] > 100u ||
            s->fx_feedback[fx] > (fx == 0u ? 70u : 90u))
            return 0;
        for (uint32_t patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch)
            if (s->fx_send[patch][fx] > 100u) return 0;
    }
    if (s->chaos_amount>100u || s->chaos_rate>100u || s->chaos_tone>100u || s->chaos_shape>5u) return 0;
    if (s->compressor_enabled > 1u || s->compressor_threshold > 100u ||
        s->compressor_ratio > 100u || s->compressor_attack > 100u ||
        s->compressor_release > 100u || s->compressor_makeup > 100u) return 0;
    return 1;
}

void fm6_performance_state_get(fm6_performance_state_t *s)
{
    if (!s) return;
    s->tempo_bpm = (uint16_t)transport_bpm;
    s->seq_length = seq_length;
    for(uint32_t lane=0;lane<2u;lane++) {
        s->seq_event_count[lane]=seq_event_mode?seq_event_count[lane]:0u;
        for(uint32_t i=0;i<FM6_SEQ_EVENTS_PER_LANE;i++) {
            if(seq_event_mode && i<seq_event_count[lane])s->seq_events[lane][i]=seq_events[lane][i];
            else s->seq_events[lane][i]=(fm6_seq_event_t){0u,0u,0u,0u,0u};
        }
    }
    for (uint32_t lane = 0; lane < 2u; ++lane) {
        s->lane_instrument[lane]=seq_lane_instrument[lane];
        for (uint32_t step = 0; step < 64u; ++step) {
            s->seq_enabled[lane][step] = seq_pattern[lane][step].enabled;
            s->seq_note[lane][step] = seq_pattern[lane][step].note;
            s->seq_velocity[lane][step] = seq_pattern[lane][step].velocity;
            s->seq_gate[lane][step] = seq_pattern[lane][step].gate;
        }
    }
    s->arp_mode = arp_mode;s->arp_division=arp_division;s->arp_gate=arp_gate;s->arp_octaves=arp_octaves;s->arp_length=arp_length;
    s->arp_output=arp_output;s->arp_random=arp_random;s->arp_macro_dest=arp_macro_dest;s->arp_depth=arp_depth;
    for(uint32_t step=0;step<16u;step++){s->arp_step_gain[step]=arp_step_gain[step];s->arp_step_accent[step]=arp_step_accent[step];s->arp_step_gate[step]=arp_step_gate[step];}
    s->compressor_enabled=fm6_comp_enabled; s->compressor_threshold=fm6_comp_threshold;
    s->compressor_ratio=fm6_comp_ratio; s->compressor_attack=fm6_comp_attack;
    s->compressor_release=fm6_comp_release; s->compressor_makeup=fm6_comp_makeup;
    s->chaos_amount=fm6_chaos_amount;s->chaos_rate=fm6_chaos_rate;
    s->chaos_tone=fm6_chaos_tone;s->chaos_shape=fm6_chaos_shape;
    for (uint32_t fx = 0; fx < 4u; ++fx) {
        s->fx_enabled[fx] = fx_enabled[fx];
        s->fx_mix[fx] = fx_mix[fx];
        s->fx_time[fx] = fx_time[fx];
        s->fx_feedback[fx] = fx_feedback[fx];
        s->fx_depth[fx] = fx_depth[fx];
        for (uint32_t patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch)
            s->fx_send[patch][fx] = fx_send[patch][fx];
    }
}

/* Runtime edits use update: change musical controls without flushing held
 * keys, arp position, or a running sequencer. set() is the cold-restore path. */
int fm6_performance_state_update(const fm6_performance_state_t *s)
{
    if(!fm6_performance_state_valid(s))return -1;
    /* A performance/UI refresh must not silently discard the event sequencer.
     * Cold boot starts in legacy mode; event mode is entered explicitly by
     * fm6_sequencer_set_events after migration or recording. */
    transport_bpm=s->tempo_bpm;
    fm6_chaos_rate_update();
    uint8_t next_length=s->seq_length<2u?2u:s->seq_length>64u?64u:s->seq_length;
    if(next_length!=seq_length) {
        seq_length=next_length;
        fm6_seq_event_trim(seq_length);
    }
    if(seq_position>=seq_length)seq_position=0u;
    for(uint32_t lane=0;lane<2u;lane++){
        seq_lane_instrument[lane]=s->lane_instrument[lane];
        for(uint32_t step=0;step<64u;step++){
            seq_pattern[lane][step].enabled=0u;
            seq_pattern[lane][step].note=s->seq_note[lane][step];
            seq_pattern[lane][step].velocity=s->seq_velocity[lane][step];
            seq_pattern[lane][step].gate=s->seq_gate[lane][step];
            seq_pattern[lane][step].enabled=s->seq_enabled[lane][step];
        }
    }
    uint8_t any_events=(uint8_t)(s->seq_event_count[0]||s->seq_event_count[1]);
    if(any_events) {
        for(uint32_t lane=0;lane<2u;lane++) {
            uint8_t changed=(uint8_t)(!seq_event_mode || seq_event_count[lane]!=s->seq_event_count[lane]);
            if(!changed)for(uint32_t i=0;i<seq_event_count[lane];i++) {
                const fm6_seq_event_t *a=&seq_events[lane][i],*b=&s->seq_events[lane][i];
                if(a->start!=b->start||a->duration!=b->duration||a->pitch!=b->pitch||
                   a->velocity!=b->velocity||a->accent!=b->accent){changed=1u;break;}
            }
            if(changed && fm6_sequencer_set_events((uint8_t)lane,s->seq_events[lane],s->seq_event_count[lane])!=0)return -1;
        }
    } else if(seq_event_mode) {
        for(uint32_t lane=0;lane<2u;lane++) {
            fm6_seq_event_release_lane(lane);
            seq_event_count[lane]=0u;
        }
        seq_event_mode=0u;
    }
    if(arp_mode && !s->arp_mode)arp_clear_pending=1u;
    if(!arp_mode && s->arp_mode){arp_index=0u;arp_direction=1u;arp_step_position=0u;arp_phase=0u;}
    if(arp_mode && arp_output!=s->arp_output){
        for(uint32_t n=0;n<arp_count;n++){
            if(arp_output==1u && s->arp_output!=1u)
                fm6_note_off_source(arp_notes[n],arp_patch_mask,0u);
            else if(arp_output!=1u && s->arp_output==1u)
                fm6_note_start_tuned_mask(arp_notes[n],arp_velocities[n],arp_patch_mask,0,0u);
        }
        arp_gate_left=0u;arp_amp_target_q15=32768;
    }
    arp_mode=s->arp_mode;
    arp_division=s->arp_division;arp_gate=s->arp_gate;arp_octaves=s->arp_octaves;arp_length=s->arp_length;
    arp_output=s->arp_output;arp_random=s->arp_random;arp_macro_dest=s->arp_macro_dest;arp_depth=s->arp_depth;
    if(arp_output!=2u)fm6_arp_macro_apply(0);
    for(uint32_t step=0;step<16u;step++){
        arp_step_gain[step]=s->arp_step_gain[step];
        arp_step_accent[step]=s->arp_step_accent[step];
        arp_step_gate[step]=s->arp_step_gate[step];
    }
    if(arp_step_position>=arp_length)arp_step_position=0u;
    fm6_comp_enabled=s->compressor_enabled;fm6_comp_threshold=s->compressor_threshold;
    fm6_comp_ratio=s->compressor_ratio;fm6_comp_attack=s->compressor_attack;
    fm6_comp_release=s->compressor_release;fm6_comp_makeup=s->compressor_makeup;
    fm6_comp_params_update();
    fm6_chaos_set(s->chaos_amount,s->chaos_rate,s->chaos_tone,s->chaos_shape);
    for(uint32_t fx=0;fx<4u;fx++){
        fm6_fx_set_enabled((uint8_t)fx,s->fx_enabled[fx]);fx_mix[fx]=s->fx_mix[fx];
        fx_time[fx]=s->fx_time[fx];fx_feedback[fx]=s->fx_feedback[fx];
        fx_depth[fx]=s->fx_depth[fx];
        for(uint32_t patch=0;patch<FM1LAB_PATCH_COUNT;patch++){fx_send[patch][fx]=s->fx_send[patch][fx];fx_send[patch+FM1LAB_PATCH_COUNT][fx]=s->fx_send[patch][fx];}
    }
    fm6_fx_mix_budget_update();
    return 0;
}
/* Call during initialization, before audio rendering starts. Runtime transport
 * phase, held keys, live notes, and delay/reverb history are never restored. */
int fm6_performance_state_set(const fm6_performance_state_t *s)
{
    if(fm6_performance_state_update(s)!=0)return -1;
    seq_position=0u;seq_tick_position=0u;seq_tick_counter=0u;seq_running=0u;
    seq_start_pending=seq_stop_pending=0u;
    transport_phase=0u;
    seq_gate_left[0]=seq_gate_left[1]=0u;
    seq_live_note[0]=seq_live_note[1]=0u;
    seq_live_mask[0]=seq_live_mask[1]=0u;
    arp_count=arp_index=arp_live=0u;
    arp_direction=1u;arp_step_position=arp_play_position=0u;
    arp_phase=arp_gate_left=0u;arp_amp_gain_q15=arp_amp_target_q15=32768;
    fm6_arp_macro_apply(0);arp_clear_pending=0u;
    return 0;
}


void fm6_chaos_set(uint8_t amount,uint8_t rate,uint8_t tone,uint8_t shape)
{
    uint8_t next_amount=amount>100u?100u:amount;
    fm6_chaos_amount=next_amount;
    fm6_chaos_rate=rate>100u?100u:rate;
    fm6_chaos_tone=tone>100u?100u:tone;
    fm6_chaos_shape=shape>5u?5u:shape;
    fm6_chaos_rate_update();
}
void fm6_chaos_get(uint8_t *amount,uint8_t *rate,uint8_t *tone,uint8_t *shape)
{
    if(amount)*amount=fm6_chaos_amount;
    if(rate)*rate=fm6_chaos_rate;
    if(tone)*tone=fm6_chaos_tone;
    if(shape)*shape=fm6_chaos_shape;
}
void fm6_master_insert_reset_state(void)
{
    fm6_amp_tone_state[0]=fm6_amp_tone_state[1]=0;
    fm6_zero_bytes(fm6_eq_x1,sizeof(fm6_eq_x1));
    fm6_zero_bytes(fm6_eq_x2,sizeof(fm6_eq_x2));
    fm6_zero_bytes(fm6_eq_y1,sizeof(fm6_eq_y1));
    fm6_zero_bytes(fm6_eq_y2,sizeof(fm6_eq_y2));
}
void fm6_master_insert_set(uint32_t field,uint8_t value)
{
    if(field>=FM6_INSERT_FIELD_COUNT)return;
    uint8_t before=fm1lab_insert_state_get(&fm6_master_inserts,field);
    fm1lab_insert_state_set(&fm6_master_inserts,field,value);
    uint8_t after=fm1lab_insert_state_get(&fm6_master_inserts,field);
    if(before!=after)fm6_master_insert_recalculate(field);
}
uint8_t fm6_master_insert_get(uint32_t field)
{
    return fm1lab_insert_state_get(&fm6_master_inserts,field);
}
void fm6_master_comp_set(uint32_t field,uint8_t value)
{
    if(field>=FM6_COMP_FIELD_COUNT)return;
    if(field==FM6_COMP_ENABLED)fm6_comp_enabled=value?1u:0u;
    else if(field==FM6_COMP_THRESHOLD)fm6_comp_threshold=value>100u?100u:value;
    else if(field==FM6_COMP_RATIO)fm6_comp_ratio=value>100u?100u:value;
    else if(field==FM6_COMP_ATTACK)fm6_comp_attack=value>100u?100u:value;
    else if(field==FM6_COMP_RELEASE)fm6_comp_release=value>100u?100u:value;
    else fm6_comp_makeup=value>100u?100u:value;
    fm6_comp_params_update();
}
uint8_t fm6_master_comp_get(uint32_t field)
{
    if(field>=FM6_COMP_FIELD_COUNT)return 0u;
    if(field==FM6_COMP_ENABLED)return fm6_comp_enabled;
    if(field==FM6_COMP_THRESHOLD)return fm6_comp_threshold;
    if(field==FM6_COMP_RATIO)return fm6_comp_ratio;
    if(field==FM6_COMP_ATTACK)return fm6_comp_attack;
    if(field==FM6_COMP_RELEASE)return fm6_comp_release;
    return fm6_comp_makeup;
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
uint8_t fm6_patch_level(uint32_t patch)
{
    return patch < PATCHES ? instrument[patch].level : 0u;
}
void fm6_patch_set_pan(uint32_t patch, uint8_t pan)
{
    if (patch < PATCHES) instrument[patch].pan = pan > 127u ? 127u : pan;
}
uint8_t fm6_patch_pan(uint32_t patch)
{
    return patch < PATCHES ? instrument[patch].pan : 64u;
}
int fm6_patch_set_pitch(uint32_t patch, int octave, int fine_cents)
{
    if (patch >= PATCHES || octave < -4 || octave > 4 ||
        fine_cents < -100 || fine_cents > 100) return -1;
    fm1lab_patch_t *part = &instrument[patch];
    if (part->octave == octave && part->fine_cents == fine_cents) return 0;
    part->octave = (int8_t)octave;
    part->fine_cents = (int8_t)fine_cents;
    /* Pitch is part of the live patch state, so held notes follow the control. */
    return 0;
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

/* Diagnostic/test accessor: count only one patch's fixed voice pool. */
uint32_t fm6_active_voices_patch(uint32_t patch)
{
    uint32_t i, n = 0;
    if (patch >= PATCHES)
        return 0;
    for (i = patch * FM1LAB_VOICE_LIMIT;
         i < (patch + 1u) * FM1LAB_VOICE_LIMIT; ++i)
        n += voices[i].used != 0;
    return n;
}
