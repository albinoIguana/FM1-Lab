#include "fm1lab_sequencer.h"
#include "instrument.h"
#include <assert.h>
#include <string.h>

static unsigned roles[8], sources[8], lengths[8], tempo = 120, swing, playing;
static unsigned start_posts, stop_posts;
static int fail_role_track = -1;
static unsigned fail_role_count;
static unsigned track_volumes[8], muted[8];
static uint8_t imported_voices[8][FM1LAB_PATCH_BYTES];
static struct groove_step steps[8][64];
static unsigned settings[8][64][7];

unsigned groove_track_role(unsigned t) { return t < 8 ? roles[t] : GROOVE_TRACK_OFF; }
unsigned groove_source_type(unsigned t) { return t < 8 ? sources[t] : GROOVE_SOURCES; }
int groove_track_role_set(unsigned t, unsigned role)
{
    if (t >= 8 || role > GROOVE_TRACK_OFF) return -1;
    if ((int)t == fail_role_track && fail_role_count) { --fail_role_count; return -1; }
    roles[t] = role;
    return 0;
}
void groove_screen_mmc(unsigned command)
{
    if (command == 1 || command == 9) { ++stop_posts; playing = 0; }
    else if (command == 2 || command == 3 || command == 6) playing = 1;
}
int fm1_midi_post(const uint8_t *message, size_t bytes)
{
    if (!message || bytes != 1) return -1;
    if (message[0] == 0xfaU) { ++start_posts; playing = 1; return 0; }
    if (message[0] == 0xfcU) { ++stop_posts; playing = 0; return 0; }
    return -1;
}
void groove_sequence_view(unsigned t, unsigned s, struct groove_view *v) { (void)t; (void)s; memset(v, 0, sizeof *v); v->playing = (uint8_t)playing; }
int fm1lab_instrument_valid(const fm1lab_instrument_t *i) { return i && i->magic == FM1LAB_INSTRUMENT_MAGIC && i->version == FM1LAB_INSTRUMENT_VERSION; }
int fm1lab_render_patch(const fm1lab_instrument_t *i, unsigned p, uint8_t out[FM1LAB_PATCH_BYTES]) { if (!fm1lab_instrument_valid(i) || p >= FM1LAB_PATCH_COUNT) return -1; memcpy(out, i->patches[p].voice, FM1LAB_PATCH_BYTES); return 0; }
int groove_fm_import(unsigned t, const uint8_t voice[FM1LAB_PATCH_BYTES]) { if (t >= 8) return -1; memcpy(imported_voices[t], voice, FM1LAB_PATCH_BYTES); return 0; }
void groove_get_view(unsigned t, unsigned s, struct groove_view *v) { (void)s; memset(v, 0, sizeof *v); v->volume = (uint8_t)track_volumes[t]; v->muted = (uint8_t)muted[t]; }
void groove_control_edit(unsigned t, int program, int volume, int cutoff) { (void)program; (void)cutoff; track_volumes[t] = (unsigned)((int)track_volumes[t] + volume); }
void groove_mix_toggle(unsigned t, int solo) { (void)solo; muted[t] ^= 1U; }
unsigned groove_tempo_saved(void) { return tempo; }
void groove_tempo_edit(int delta) { tempo = (unsigned)((int)tempo + delta); }
unsigned groove_sequence_setting(unsigned s) { return s == 0 ? swing : 0; }
void groove_sequence_setting_edit(unsigned s, int delta) { if (!s) swing = (unsigned)((int)swing + delta); }
unsigned groove_sequence_length(unsigned t) { return t < 8 ? lengths[t] : 0; }
void groove_sequence_length_set(unsigned t, unsigned n) { if (t < 8) lengths[t] = n; }
void groove_sequence_read(unsigned t, unsigned first, struct groove_step *out, unsigned count)
{ memcpy(out, &steps[t][first], count * sizeof *out); }
void groove_sequence_edit(unsigned t, unsigned s, unsigned note, unsigned velocity)
{ steps[t][s].note = (uint8_t)note; steps[t][s].velocity = (uint8_t)velocity; }
unsigned groove_step_setting(unsigned t, unsigned s, unsigned setting)
{ return settings[t][s][setting]; }
int groove_step_setting_edit(unsigned t, unsigned s, unsigned setting, int delta)
{ settings[t][s][setting] = (unsigned)((int)settings[t][s][setting] + delta); return 0; }
void groove_sequence_replace(unsigned t, unsigned first, const struct groove_step *in, unsigned count)
{ memcpy(&steps[t][first], in, count * sizeof *in); }

int main(void)
{
    for (unsigned i = 0; i < 8; ++i) lengths[i] = 64;
    roles[1] = GROOVE_TRACK_PLAY;
    roles[7] = GROOVE_TRACK_OFF;
    steps[0][3].note = 44;
    steps[1][3].note = 55;
    assert(fm1lab_sequencer_bind(2, 5) == 0);
    assert(roles[0] == GROOVE_TRACK_OFF && roles[1] == GROOVE_TRACK_PLAY);
    assert(roles[2] == GROOVE_TRACK_NORMAL && roles[3] == GROOVE_TRACK_OFF);
    assert(roles[4] == GROOVE_TRACK_OFF && roles[5] == GROOVE_TRACK_NORMAL);
    assert(roles[6] == GROOVE_TRACK_OFF && roles[7] == GROOVE_TRACK_OFF);
    assert(steps[0][3].note == 44 && steps[1][3].note == 55);
    assert(fm1lab_sequencer_bind(2, 2) == -1);
    roles[5] = GROOVE_TRACK_PLAY;
    assert(fm1lab_sequencer_bind(2, 5) == -2);
    roles[5] = GROOVE_TRACK_NORMAL;
    sources[5] = GROOVE_303;
    assert(fm1lab_sequencer_bind(2, 5) == -2);
    sources[5] = GROOVE_DX7;
    assert(fm1lab_sequencer_bind(2, 5) == 0);

    fm1lab_sequence_state_t state;
    assert(fm1lab_sequencer_read(&state) == 0);
    assert(state.lanes[0].backend_track == 2 && state.lanes[1].backend_track == 5);
    unsigned starts_before = start_posts;
    assert(fm1lab_sequencer_transport(1) == 0 && playing == 1);
    assert(start_posts == starts_before + 1);
    assert(fm1lab_sequencer_read(&state) == 0 && state.playing == 1);
    assert(fm1lab_sequencer_transport(0) == 0 && playing == 0);
    assert(state.lanes[0].length == 64 && state.tempo == 120);

    assert(fm1lab_sequencer_set_tempo(240) == 0 && tempo == 240);
    assert(fm1lab_sequencer_set_tempo(301) == -1 && tempo == 240);
    assert(fm1lab_sequencer_set_swing(35) == 0 && swing == 35);
    assert(fm1lab_sequencer_set_swing(51) == -1 && swing == 35);
    assert(fm1lab_sequencer_set_length(0, 32) == 0 && lengths[2] == 32);
    assert(fm1lab_sequencer_set_length(1, 24) == -1 && lengths[5] == 64);

    fm1lab_instrument_t instrument = {0};
    instrument.magic = FM1LAB_INSTRUMENT_MAGIC;
    instrument.version = FM1LAB_INSTRUMENT_VERSION;
    instrument.patches[2].voice[0] = 0x5a;
    instrument.patches[2].enabled = 0;
    instrument.patches[2].level = 77;
    instrument.patches[1].voice[0] = 0x2b;
    instrument.patches[1].enabled = 1;
    instrument.patches[1].level = 99;
    track_volumes[2] = 40;
    track_volumes[5] = 60;
    muted[5] = 1;
    assert(fm1lab_sequencer_set_patch(0, &instrument, 2) == 0);
    assert(imported_voices[2][0] == 0x5a && track_volumes[2] == 77 && muted[2] == 1);
    assert(fm1lab_sequencer_set_patch(1, &instrument, 1) == 0);
    assert(imported_voices[5][0] == 0x2b && track_volumes[5] == 99 && muted[5] == 0);
    assert(fm1lab_sequencer_read(&state) == 0);
    assert(state.lanes[0].patch_index == 2 && state.lanes[1].patch_index == 1);

    fm1lab_sequence_step_t step = {1, 69, 101, 12, 1, 1};
    assert(fm1lab_sequencer_set_step(0, 7, &step) == 0);
    assert(steps[2][7].note == 69 && steps[2][7].velocity == 101);
    assert(settings[2][7][1] == 12 && settings[2][7][4] == 1);
    assert((steps[2][7].drums & GROOVE_STEP_ALT) != 0);
    step.active = 0;
    assert(fm1lab_sequencer_set_step(0, 7, &step) == 0);
    assert(steps[2][7].velocity == 0);
    assert(fm1lab_sequencer_select_lane(1) == 0);
    assert(fm1lab_sequencer_read(&state) == 0 && state.selected_lane == 1);
    assert(fm1lab_sequencer_select_lane(2) == -1);

    /* Unbound project tracks remain byte-for-byte untouched. */
    assert(steps[0][7].note == 0 && steps[1][7].note == 0);
    assert(steps[3][7].note == 0 && steps[4][7].note == 0);
    assert(steps[6][7].note == 0 && steps[7][7].note == 0);
    assert(fm1lab_sequencer_unbind() == 0);
    assert(roles[0] == GROOVE_TRACK_NORMAL && roles[1] == GROOVE_TRACK_PLAY);
    assert(roles[2] == GROOVE_TRACK_NORMAL && roles[3] == GROOVE_TRACK_NORMAL);
    assert(roles[4] == GROOVE_TRACK_NORMAL && roles[5] == GROOVE_TRACK_NORMAL);
    assert(roles[6] == GROOVE_TRACK_NORMAL && roles[7] == GROOVE_TRACK_OFF);
    assert(steps[0][3].note == 44 && steps[1][3].note == 55);
    assert(fm1lab_sequencer_transport(1) == -1);

    /* Official role-set failures must roll back, and failed unbind must retry. */
    fail_role_track = 4; fail_role_count = 1;
    assert(fm1lab_sequencer_bind(2, 5) == -3);
    for (unsigned i = 0; i < 8; ++i)
        assert(roles[i] == (i == 1 ? GROOVE_TRACK_PLAY : (i == 7 ? GROOVE_TRACK_OFF : GROOVE_TRACK_NORMAL)));

    assert(fm1lab_sequencer_bind(2, 5) == 0);
    fail_role_track = 3; fail_role_count = 1;
    assert(fm1lab_sequencer_unbind() == -1);
    assert(fm1lab_sequencer_read(&state) == -3);
    assert(fm1lab_sequencer_unbind() == 0);
    for (unsigned i = 0; i < 8; ++i)
        assert(roles[i] == (i == 1 ? GROOVE_TRACK_PLAY : (i == 7 ? GROOVE_TRACK_OFF : GROOVE_TRACK_NORMAL)));
    return 0;
}
