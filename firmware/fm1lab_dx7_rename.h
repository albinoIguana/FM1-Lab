#ifndef FM1LAB_DX7_RENAME_H
#define FM1LAB_DX7_RENAME_H

#include "engine.h"

/* Keep FM1-Lab's source-built DX7 core isolated from the proprietary engine
 * archive, which exports several identically named DSP entry points. */
#ifndef FM1_DSP_ENTRY
#define FM1_DSP_ENTRY FM1_DSP_CODE
#endif

#define synth_dx7_create                 fm1lab_synth_dx7_create
#define synth_dx7_destroy                fm1lab_synth_dx7_destroy
#define synth_dx7_reset                  fm1lab_synth_dx7_reset
#define synth_dx7_midi                   fm1lab_synth_dx7_midi
#define synth_dx7_midi_tuned             fm1lab_synth_dx7_midi_tuned
#define synth_dx7_lfo_reset               fm1lab_synth_dx7_lfo_reset
#define synth_dx7_render                 fm1lab_synth_dx7_render
#define synth_dx7_set_poly_mode          fm1lab_synth_dx7_set_poly_mode
#define synth_dx7_is_mono                fm1lab_synth_dx7_is_mono
#define synth_dx7_all_notes_off          fm1lab_synth_dx7_all_notes_off
#define synth_dx7_set_pitch_bend_range   fm1lab_synth_dx7_set_pitch_bend_range
#define synth_dx7_set_glide              fm1lab_synth_dx7_set_glide
#define synth_dx7_set_operator_mask      fm1lab_synth_dx7_set_operator_mask
#define synth_dx7_set_adsr               fm1lab_synth_dx7_set_adsr
#define synth_dx7_limit                  fm1lab_synth_dx7_limit
#define synth_dx7_active                 fm1lab_synth_dx7_active
#define fm1_dx7_panic                    fm1lab_synth_dx7_panic

#define dx7_sin_log_table                fm1lab_dx7_sin_log_table
#define dx7_sin_exp_table                fm1lab_dx7_sin_exp_table
#define dx7core_compute_64               fm1lab_dx7core_compute_64
#define dx7note_init                     fm1lab_dx7note_init
#define dx7note_compute_64               fm1lab_dx7note_compute_64
#define dx7note_keyup                    fm1lab_dx7note_keyup
#define dx7note_oscsync                  fm1lab_dx7note_oscsync
#define dx7note_transfer_signal          fm1lab_dx7note_transfer_signal
#define dx7note_transfer_state           fm1lab_dx7note_transfer_state
#define dx7note_setglide                 fm1lab_dx7note_setglide
#define dx7env_set                       fm1lab_dx7env_set
#define dx7env_getsample_64              fm1lab_dx7env_getsample_64
#define dx7env_keyup                     fm1lab_dx7env_keyup
#define dx7env_transfer                  fm1lab_dx7env_transfer
#define dx7lfo_reset                     fm1lab_dx7lfo_reset
#define dx7lfo_getsample                 fm1lab_dx7lfo_getsample
#define dx7lfo_getdelay                  fm1lab_dx7lfo_getdelay
#define dx7lfo_keydown                  fm1lab_dx7lfo_keydown
#define dx7pitchenv_init                 fm1lab_dx7pitchenv_init
#define dx7pitchenv_set                  fm1lab_dx7pitchenv_set
#define dx7pitchenv_getsample            fm1lab_dx7pitchenv_getsample
#define dx7pitchenv_keyup                fm1lab_dx7pitchenv_keyup
#define k_sin_q24                        fm1lab_k_sin_q24
#define k_pow2_q24                       fm1lab_k_pow2_q24
#define freq_lut_acquire                 fm1lab_freq_lut_acquire
#define freq_lut_release                 fm1lab_freq_lut_release
#define freqlut_qx                       fm1lab_freqlut_qx
#define k_log2_q24                       fm1lab_k_log2_q24
#define mk_sin                           fm1lab_mk_sin
#define dx7_lvllut                       fm1lab_dx7_lvllut
#define adsr_config_set                  fm1lab_adsr_config_set
#define adsr_init                        fm1lab_adsr_init
#define adsr_note_off                    fm1lab_adsr_note_off
#define adsr_update                      fm1lab_adsr_update
#define fm_exp2_fixed                    fm1lab_fm_exp2_fixed

#endif
