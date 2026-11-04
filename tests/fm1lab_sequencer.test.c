#include "fm1lab_sequencer.h"
#include "instrument.h"
#include <assert.h>
#include <string.h>

static unsigned roles[8], sources[8], lengths[8], tempo = 120, swing, enabled_mask = 0xffU, playing;
static struct groove_step steps[8][64];
static unsigned settings[8][64][7];

unsigned groove_track_role(unsigned t) { return t < 8 ? roles[t] : GROOVE_TRACK_OFF; }
unsigned groove_source_type(unsigned t) { return t < 8 ? sources[t] : GROOVE_SOURCES; }
int groove_sequence_track_enable(unsigned t, unsigned on) { if (t >= 8) return -1; if (on) enabled_mask |= 1U << t; else enabled_mask &= ~(1U << t); return 0; }
void groove_sequence_transport(uint8_t status) { playing = status == 0xfaU; }
void groove_sequence_view(unsigned t, unsigned s, struct groove_view *v) { (void)t; (void)s; memset(v, 0, sizeof *v); v->playing = (uint8_t)playing; }
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
    assert(fm1lab_sequencer_bind(2, 5) == 0);
    assert(enabled_mask == ((1U << 2) | (1U << 5)));
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
    assert(fm1lab_sequencer_transport(1) == 0 && playing == 1);
    assert(fm1lab_sequencer_read(&state) == 0 && state.playing == 1);
    assert(fm1lab_sequencer_transport(0) == 0 && playing == 0);
    assert(state.lanes[0].length == 64 && state.tempo == 120);

    assert(fm1lab_sequencer_set_tempo(240) == 0 && tempo == 240);
    assert(fm1lab_sequencer_set_tempo(301) == -1 && tempo == 240);
    assert(fm1lab_sequencer_set_swing(35) == 0 && swing == 35);
    assert(fm1lab_sequencer_set_swing(51) == -1 && swing == 35);
    assert(fm1lab_sequencer_set_length(0, 32) == 0 && lengths[2] == 32);
    assert(fm1lab_sequencer_set_length(1, 24) == -1 && lengths[5] == 64);

    fm1lab_sequence_step_t step = {1, 69, 101, 12, 1, 1};
    assert(fm1lab_sequencer_set_step(0, 7, &step) == 0);
    assert(steps[2][7].note == 69 && steps[2][7].velocity == 101);
    assert(settings[2][7][1] == 12 && settings[2][7][4] == 1);
    assert((steps[2][7].drums & GROOVE_STEP_ALT) != 0);
    step.active = 0;
    assert(fm1lab_sequencer_set_step(0, 7, &step) == 0);
    assert(steps[2][7].velocity == 0);
    assert(fm1lab_sequencer_select_lane(1) == 0);
    assert(fm1lab_sequencer_select_lane(2) == -1);

    /* Unbound project tracks remain byte-for-byte untouched. */
    assert(steps[0][7].note == 0 && steps[1][7].note == 0);
    assert(steps[3][7].note == 0 && steps[4][7].note == 0);
    assert(steps[6][7].note == 0 && steps[7][7].note == 0);
    assert(fm1lab_sequencer_unbind() == 0 && enabled_mask == 0xffU);
    assert(fm1lab_sequencer_transport(1) == -1);
    return 0;
}
