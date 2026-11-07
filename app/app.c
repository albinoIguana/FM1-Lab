/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* FM1-Lab's four-layer FM instrument interface for the native FM1 platform.
 * Physical controls navigate patch, operator, envelope, LFO, sequencer, arp and
 * master-FX pages. Keybed and incoming MIDI feed the FM6 engine; the audio
 * interrupt owns synthesis state while UI edits are published through events.
 * Part of the unity build (device/unity.c, host/plat_host.c), after the platform:
 * gfx.c and announce.c are in scope. */
#include "fm6.h"
#include "fm1lab_settings_record.h"
#include "fm1lab_instrument.h"
#include "fm1lab_voice_editor.h"
#include "fm1lab_store_library.h"
#include "../firmware/fm1lab_physical_controls.h"
#include <stddef.h>

static settings_t set;
static uint8_t set_dirty;
static fm1lab_instrument_t current_instrument;
static fm1lab_insert_state_t current_inserts;
static fm1lab_instrument_t factory_instrument;
static uint8_t instrument_index, instrument_dirty, instrument_loaded;
static volatile uint8_t instrument_apply_pending;
static uint8_t voice_update_snapshot[FM1LAB_PATCH_BYTES];
static volatile uint8_t voice_update_pending, voice_update_patch, voice_dirty;
enum { USER_INSTRUMENT_COUNT = FM1LAB_STORE_INSTRUMENT_COUNT };
enum { PAGE_HOME = 0, PAGE_MACRO, PAGE_GLOBAL, PAGE_OPERATOR_MAP, PAGE_ENV, PAGE_LFO, PAGE_LFO_MOD, PAGE_SEQ, PAGE_ARP, PAGE_FX_AMP, PAGE_FX_EQ, PAGE_FX_COMPRESSOR, PAGE_FX_CHORUS, PAGE_FX_DELAY, PAGE_FX_REVERB, PAGE_FX_PHASER, PAGE_FX_ROUTING, PAGE_COUNT };
static uint8_t page = PAGE_HOME;
static uint8_t eq_ui_band = 1u;
static uint8_t env_operator = 1u, env_levels, operator_detune_mode;
static uint8_t seq_ui_lane, seq_ui_step, seq_ui_note[2][64], seq_ui_velocity[2][64],
               seq_ui_gate[2][64], seq_ui_enabled[2][64], seq_ui_patch[2]={0u,0u};
static uint16_t transport_ui_bpm=120u;
static uint8_t arp_ui_mode;

/* the controls by their names in plat.h, in its order */
static const char *const BTN_NAME[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                         "PLAY", "REC", "OCT-", "OCT+"};
static const char *const ENC_NAME[NE] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};

/* ------------------------------------------------------------------ voice --- */
/* one voice, the last note wins; the main loop and MIDI in set it, the audio interrupt plays it */
static volatile int32_t voice_note = -1;     /* display-only: last pressed MIDI/key note */
enum { EV_NOTE_ON = 1, EV_NOTE_OFF, EV_PANIC, EV_MACRO_SET, EV_PATCH_LEVEL, EV_INSTRUMENT_LOAD, EV_PATCH_VOICE, EV_MASTER_INSERT, EV_MASTER_COMP, EV_PERFORMANCE_APPLY, EV_SEQ_RUN };
static fm6_performance_state_t performance_update_snapshot;
static volatile uint8_t performance_update_pending, performance_update_dirty;
typedef struct { volatile uint8_t type, a, b, c; } ui_event_t;
static volatile ui_event_t ui_events[64];
static volatile uint8_t ui_event_head, ui_event_tail, ui_event_overflow;

/* Main-loop controls never write FM voice state directly. They publish compact events;
 * the audio interrupt is the sole runtime writer of the synthesizer state. */
static void ui_event_push(uint8_t type, uint8_t a, uint8_t b, uint8_t c)
{
    uint8_t head = ui_event_head, next = (uint8_t)((head + 1u) & 63u);
    if (next == ui_event_tail) {
        ui_event_overflow = 1;
        return;
    }
    ui_events[head].type = type;
    ui_events[head].a = a;
    ui_events[head].b = b;
    ui_events[head].c = c;
    ui_event_head = next;
}

static void performance_publish(void)
{
    if (performance_update_pending) { performance_update_dirty=1u; return; }
    set.performance.tempo_bpm = transport_ui_bpm;
    set.performance.arp_mode = arp_ui_mode;
    for (uint32_t lane=0; lane<2u; ++lane) {
        set.performance.lane_patch[lane] = seq_ui_patch[lane];
        for (uint32_t step=0; step<64u; ++step) {
            set.performance.seq_enabled[lane][step] = seq_ui_enabled[lane][step];
            set.performance.seq_note[lane][step] = seq_ui_note[lane][step];
            set.performance.seq_velocity[lane][step] = seq_ui_velocity[lane][step];
            set.performance.seq_gate[lane][step] = seq_ui_gate[lane][step];
        }
    }
    performance_update_snapshot = set.performance;
    performance_update_dirty = 0u;
    performance_update_pending = 1u;
    ui_event_push(EV_PERFORMANCE_APPLY,0u,0u,0u);
}

static void instrument_apply_engine(void)
{
    for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        const fm1lab_patch_t *part = &current_instrument.patches[p];
        (void)fm6_patch_set_voice(p, part->voice);
        /* Layer activation is controlled exclusively by its HOME volume. */
        fm6_patch_set_enabled(p, 1u);
        fm6_patch_set_level(p, part->level);
        fm6_patch_set_pan(p, fm1lab_patch_pan(&current_instrument, p));
        for (uint32_t m = 0; m < FM1LAB_MACRO_COUNT; ++m)
            fm6_macro_set(p, m, part->macros[m]);
    }
    fm6_master_insert_reset_state();
    for(uint32_t field=0;field<FM6_INSERT_FIELD_COUNT;++field)
        fm6_master_insert_set(field,fm1lab_insert_state_get(&current_inserts,field));
}

static void instrument_factory_capture(void)
{
    fm1lab_instrument_init(&factory_instrument, "FACTORY");
    for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        const uint8_t *voice = fm6_patch_data(p);
        if (voice) (void)fm1lab_patch_set_voice(&factory_instrument, p, voice);
        factory_instrument.patches[p].enabled = 1u;
        factory_instrument.patches[p].level = 31u; /* safe factory level */
    }
}

static void instrument_factory_defaults(void)
{
    current_instrument = factory_instrument;
    fm1lab_insert_state_init(&current_inserts);
}

static void instrument_sync_settings(void)
{
    set.enabled_mask = 0x0fu; /* compatibility field; layer volume is authoritative */
    for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        fm1lab_patch_t *part = &current_instrument.patches[p];
        part->enabled = 1u; /* normalize legacy records to volume-controlled layers */
        set.levels[p] = part->level;
        for (uint32_t m = 0; m < FM1LAB_MACRO_COUNT; ++m)
            set.macros[p][m] = part->macros[m];
    }
}

static void instrument_load(uint32_t index, int from_audio)
{
    fm1lab_instrument_t candidate;
    fm1lab_insert_state_t candidate_inserts;
    int loaded = fm1lab_store_library_load_full(index, &candidate, &candidate_inserts);
    if (!loaded && fm1lab_instrument_valid(&candidate) && fm1lab_insert_state_valid(&candidate_inserts)) {
        current_instrument = candidate;
        current_inserts = candidate_inserts;
    } else {
        instrument_factory_defaults();
    }
    instrument_index = (uint8_t)index;
    instrument_loaded = 1u;
    instrument_dirty = 0u;
    instrument_sync_settings();
    if (from_audio) {
        instrument_apply_pending = 1u;
        ui_event_push(EV_INSTRUMENT_LOAD, 0, 0, 0);
    } else {
        instrument_apply_engine();
    }
}

static int instrument_save(void)
{
    if (!instrument_loaded || !fm1lab_instrument_valid(&current_instrument))
        return -1;
    if (fm1lab_store_library_save_full(instrument_index, &current_instrument, &current_inserts))
        return -1;
    instrument_dirty = 0u;
    return 0;
}

static void ui_events_drain(void)
{
    uint8_t tail;
    if (ui_event_overflow) {
        ui_event_tail = ui_event_head;
        ui_event_overflow = 0;
        fm6_panic();
        fm6_sequencer_set_running(0u);
        if (instrument_loaded) instrument_apply_engine();
        else for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
            fm6_patch_set_enabled(p, 1u);
            fm6_patch_set_level(p, set.levels[p]);
            for (uint32_t m = 0; m < FM1LAB_MACRO_COUNT; ++m)
                fm6_macro_set(p, m, set.macros[p][m]);
        }
        instrument_apply_pending = 0u;
        voice_update_pending = 0u;
        voice_dirty = 0u;
        performance_update_pending = 0u;
        performance_update_dirty = 0u;
        (void)fm6_performance_state_set(&set.performance);
        return;
    }
    while ((tail = ui_event_tail) != ui_event_head) {
        uint8_t type = ui_events[tail].type, a = ui_events[tail].a;
        uint8_t b = ui_events[tail].b, c = ui_events[tail].c;
        ui_event_tail = (uint8_t)((tail + 1u) & 63u);
        if (type == EV_NOTE_ON)
            fm6_note_on(a, b);
        else if (type == EV_NOTE_OFF)
            fm6_note_off(a);
        else if (type == EV_PANIC)
            fm6_panic();
        else if (type == EV_MACRO_SET)
            fm6_macro_set(a, b, c);
        else if (type == EV_PATCH_LEVEL)
            fm6_patch_set_level(a, b);
        else if (type == EV_INSTRUMENT_LOAD) {
            fm6_panic();
            instrument_apply_engine();
            instrument_apply_pending = 0u;
            voice_dirty = 0u;
        } else if (type == EV_MASTER_INSERT)
            fm6_master_insert_set(a,b);
        else if (type == EV_MASTER_COMP)
            fm6_master_comp_set(a,b);
        else if (type == EV_PERFORMANCE_APPLY) {
            (void)fm6_performance_state_set(&performance_update_snapshot);
            performance_update_pending = 0u;
        } else if (type == EV_SEQ_RUN)
            fm6_sequencer_set_running(a);
        else if (type == EV_PATCH_VOICE) {
            (void)fm6_patch_set_voice(a, voice_update_snapshot);
            fm6_patch_set_pan(a, fm1lab_patch_pan(&current_instrument, a));
            voice_update_pending = 0u;
        }
    }
}

static void voice_update_publish(void)
{
    if (!voice_dirty || voice_update_pending || !instrument_loaded) return;
    const uint8_t *voice = current_instrument.patches[set.patch].voice;
    for (uint32_t i = 0; i < FM1LAB_PATCH_BYTES; ++i)
        voice_update_snapshot[i] = voice[i];
    voice_update_patch = set.patch;
    voice_dirty = 0u;
    voice_update_pending = 1u;
    ui_event_push(EV_PATCH_VOICE, voice_update_patch, 0, 0);
}

static void voice_edit_changed(void)
{
    instrument_dirty = 1u;
    set_dirty = 1u;
    voice_dirty = 1u;
}

static void instrument_insert_edit(uint32_t field,uint8_t value)
{
    if(field>=FM6_INSERT_FIELD_COUNT)return;
    fm1lab_insert_state_set(&current_inserts,field,value);
    instrument_dirty=1u;set_dirty=1u;
    ui_event_push(EV_MASTER_INSERT,(uint8_t)field,fm1lab_insert_state_get(&current_inserts,field),0u);
}
static uint8_t compressor_value(uint32_t field)
{
    if(field==FM6_COMP_ENABLED)return set.performance.compressor_enabled;
    if(field==FM6_COMP_THRESHOLD)return set.performance.compressor_threshold;
    if(field==FM6_COMP_RATIO)return set.performance.compressor_ratio;
    if(field==FM6_COMP_ATTACK)return set.performance.compressor_attack;
    if(field==FM6_COMP_RELEASE)return set.performance.compressor_release;
    if(field==FM6_COMP_MAKEUP)return set.performance.compressor_makeup;
    return 0u;
}
static void compressor_edit(uint32_t field,uint8_t value)
{
    if(field>=FM6_COMP_FIELD_COUNT)return;
    if(field==FM6_COMP_ENABLED)set.performance.compressor_enabled=value?1u:0u;
    else if(field==FM6_COMP_THRESHOLD)set.performance.compressor_threshold=value>100u?100u:value;
    else if(field==FM6_COMP_RATIO)set.performance.compressor_ratio=value>100u?100u:value;
    else if(field==FM6_COMP_ATTACK)set.performance.compressor_attack=value>100u?100u:value;
    else if(field==FM6_COMP_RELEASE)set.performance.compressor_release=value>100u?100u:value;
    else set.performance.compressor_makeup=value>100u?100u:value;
    set_dirty=1u;
    ui_event_push(EV_MASTER_COMP,(uint8_t)field,compressor_value(field),0u);
}

static void voice_edit_global(fm1lab_global_field_t field, int delta)
{
    uint8_t *voice = current_instrument.patches[set.patch].voice;
    uint8_t value;
    unsigned maximum = field == FM1LAB_GLOBAL_ALGORITHM ? 32u :
        field == FM1LAB_GLOBAL_LFO_WAVE ? 5u :
        field == FM1LAB_GLOBAL_PITCH_SENS ? 7u :
        (field == FM1LAB_GLOBAL_LFO_SYNC || field == FM1LAB_GLOBAL_OSC_SYNC) ? 1u : 99u;
    unsigned minimum = field == FM1LAB_GLOBAL_ALGORITHM ? 1u : 0u;
    if (!delta || fm1lab_voice_global_get(voice, field, &value)) return;
    int next = (int)value + delta;
    if (next < (int)minimum) next = (int)minimum;
    if ((unsigned)next > maximum) next = (int)maximum;
    if ((uint8_t)next != value &&
        !fm1lab_voice_global_set(voice, field, (unsigned)next))
        voice_edit_changed();
}

static void voice_edit_operator(fm1lab_operator_field_t field, int delta)
{
    uint8_t *voice = current_instrument.patches[set.patch].voice;
    uint8_t value;
    if (!delta || fm1lab_voice_operator_get(voice, env_operator, field, &value)) return;
    unsigned maximum = field == FM1LAB_OP_MODE ? 1u :
        field == FM1LAB_OP_COARSE ? 31u :
        field == FM1LAB_OP_DETUNE ? 14u : 99u;
    int next = (int)value + delta;
    if (next < 0) next = 0;
    if ((unsigned)next > maximum) next = (int)maximum;
    if ((uint8_t)next != value &&
        !fm1lab_voice_operator_set(voice, env_operator, field, (unsigned)next))
        voice_edit_changed();
}

static void note_on(int32_t note, uint32_t vel)
{
    voice_note = note;
    ui_event_push(EV_NOTE_ON, (uint8_t)note, (uint8_t)vel, 0);
}
static void note_off(int32_t note)
{
    ui_event_push(EV_NOTE_OFF, (uint8_t)note, 0, 0);
    if (voice_note == note)
        voice_note = -1;
}

static volatile uint32_t midi_last, midi_last_port, midi_count;   /* the last message in (shown), and how many */
static void midi_in(void)                    /* audio interrupt: the packets that came in */
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24;
        if (st != 0xF8u && st != 0xFEu) {    /* (clock and active sensing would flood the screen) */
            midi_last = pkt;
            midi_last_port = (uint32_t)port;
            midi_count++;
        }
        if ((st & 0xF0u) == 0x90u && d2) {
            voice_note = (int32_t)d1;
            fm6_note_on((uint8_t)d1, (uint8_t)d2); /* MIDI is already in the audio interrupt. */
        } else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u) {
            fm6_note_off((uint8_t)d1);
            if (voice_note == (int32_t)d1)
                voice_note = -1;
        }
    }
}

/* Read-only diagnostics from fm1-base's measured audio ISR counters. These
 * deliberately do not add UI work or logging to the real-time render path. */
uint32_t app_audio_cpu_pct(void) { return plat_cpu_pct(); }
uint32_t app_audio_xruns(void) { return plat_xruns(); }
uint32_t app_audio_overload_shed_count(void) { return fm6_overload_shed_count(); }

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    ui_events_drain();
    midi_in();
    fm6_render(lr, n, master_q12);
}

void app_panic(void)
{
    voice_note = -1;
    fm6_panic();
    ui_event_tail = ui_event_head;
    ui_event_overflow = 0;
}

/* --------------------------------------------------------------- settings --- */
static void settings_defaults(void)
{
    memset(&set, 0, sizeof set);
    set.magic = SET_MAGIC;
    set.octave = 0;
    set.patch = 0;
    set.enabled_mask = 0x0fu; /* all four factory layers active */
    set.ble_off = 0;
    for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        set.levels[p] = 31u; /* safe defaults; saved user levels are loaded below */
        for (uint32_t m = 0; m < FM1LAB_MACRO_COUNT; ++m)
            set.macros[p][m] = 50u;
    }
    fm6_performance_state_get(&set.performance);
    settings_seal(&set);
}

/* Called by fm1-base after a measured audio block exceeds 90% of its
 * 5.8 ms deadline. The DSP fades the globally oldest voice over 32 samples;
 * no allocation, flash access, UI work, or cross-patch allocator mutation. */
void app_overload(void)
{
    fm6_overload();
}

void app_init(void)
{
    settings_v1_t old_v1;
    settings_v2_t old_v2;
    settings_v3_t old_v3;
    settings_v4_t old_v4;
    int performance_loaded = 0;
    int loaded;
    fm6_init();
    settings_defaults();
    loaded = plat_store_load(OBJ_SETTINGS, &set, sizeof set);
    if (loaded == (int)sizeof set && settings_valid(&set)) {
        performance_loaded = 1;
    } else {
        settings_defaults();
        if (plat_store_load(OBJ_SETTINGS, &old_v4, sizeof old_v4) == (int)sizeof old_v4 &&
            settings_import_v4(&set, &old_v4) == 0) {
            set_dirty = 1u; /* migrate v4 settings; compressor starts bypassed */
        } else if (plat_store_load(OBJ_SETTINGS, &old_v3, sizeof old_v3) == (int)sizeof old_v3 &&
            settings_import_v3(&set, &old_v3) == 0) {
            set_dirty = 1u; /* migrate v3 three-bus performance into v4 four-bus state */
        } else if (plat_store_load(OBJ_SETTINGS, &old_v2, sizeof old_v2) == (int)sizeof old_v2 &&
                   settings_import_v2(&set, &old_v2) == 0) {
            set_dirty = 1u; /* migrate v2 settings into the CRC-protected performance record */
        } else if (plat_store_load(OBJ_SETTINGS, &old_v1, sizeof old_v1) == (int)sizeof old_v1 &&
                   settings_import_v1(&set, &old_v1) == 0) {
            set_dirty = 1u; /* migrate the original compact settings record */
        }
        settings_seal(&set);
    }
    page = PAGE_HOME;
    instrument_factory_capture();
    if (performance_loaded && fm6_performance_state_set(&set.performance) == 0) {
        transport_ui_bpm = set.performance.tempo_bpm;
        arp_ui_mode = set.performance.arp_mode;
        for (uint32_t lane = 0; lane < 2u; ++lane) {
            seq_ui_patch[lane] = set.performance.lane_patch[lane];
            for (uint32_t step = 0; step < 64u; ++step) {
                seq_ui_enabled[lane][step] = set.performance.seq_enabled[lane][step];
                seq_ui_note[lane][step] = set.performance.seq_note[lane][step];
                seq_ui_velocity[lane][step] = set.performance.seq_velocity[lane][step];
                seq_ui_gate[lane][step] = set.performance.seq_gate[lane][step];
            }
        }
    } else {
        for (uint32_t lane = 0; lane < 2u; ++lane) {
            seq_ui_patch[lane] = 0u;
            for (uint32_t step = 0; step < 64u; ++step) {
                seq_ui_note[lane][step] = (uint8_t)(lane ? 67u : 48u);
                seq_ui_velocity[lane][step] = 100u;
                seq_ui_gate[lane][step] = 50u;
                seq_ui_enabled[lane][step] = (uint8_t)((step < 16u) && (step % 4u == (lane ? 2u : 0u)));
                /* Performance state is applied below from one coherent snapshot. */
            }
        }
        performance_publish();
    }
    for (uint32_t bank = 0; bank < FM1LAB_STORE_BANK_COUNT; ++bank) {
        int size = plat_store_size(OBJ_INSTRUMENTS0 + bank);
        fm1lab_instrument_t probe;
        /* Only a genuinely absent object may be initialized automatically.
         * A present but malformed bank is user data: leave it untouched so a
         * CRC/read failure cannot silently factory-reset eight Instruments. */
        if (size < 0) {
            (void)fm1lab_store_library_format_bank(bank, &factory_instrument);
            continue;
        }
        if (size == 3840) {
            if (fm1lab_store_library_load(bank * 8u, &probe) != 0)
                app_say("BANK ERROR", "DATA PRESERVED");
            continue;
        }
        if (bank < 3u && size == (int)sizeof(fm1lab_instrument_t)) {
            fm1lab_instrument_t legacy;
            if (plat_store_load(OBJ_INSTRUMENTS0 + bank, &legacy, sizeof legacy) ==
                    (int)sizeof legacy && fm1lab_instrument_valid(&legacy)) {
                /* V1's final voice byte was only a 63 sentinel, not pan. */
                for (uint32_t p=0; p<FM1LAB_PATCH_COUNT; ++p)
                    fm1lab_patch_set_pan(&legacy,p,64u);
                if (!fm1lab_store_library_format_bank(bank, &factory_instrument))
                    (void)fm1lab_store_library_save(bank, &legacy);
                continue;
            }
        }
        /* Unexpected non-empty object sizes are not blank storage. Preserve
         * them for recovery rather than replacing them with factory data. */
        app_say("BANK ERROR", "DATA PRESERVED");
    }
    instrument_load(0u, 0);
    for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        fm6_patch_set_enabled(p, 1u);
        fm6_patch_set_level(p, set.levels[p]);
        for (uint32_t m = 0; m < FM1LAB_MACRO_COUNT; ++m)
            fm6_macro_set(p, m, set.macros[p][m]);
    }
}

/* --------------------------------------------------------------------- UI --- */
static struct {
    char msg[2][24];                     /* a short message over the header */
    uint32_t msg_until;
    uint32_t keys, btn;                  /* held at the last input */
    uint8_t vel[NKEYS], prs[NKEYS];      /* each key's last velocity, and its pressure while held */
    int32_t last_key;                    /* the key pressed last: its numbers in the header */
    uint8_t looking;                     /* Bluetooth MIDI devices: REC pressed, the scan's list up (lib/btdev) */
    uint32_t rec_t;                      /* REC went down (0: not held, or its hold already acted) */
    int32_t turns[NE];                   /* detents since boot, per encoder */
    uint32_t turned_at[NE];              /* when each was last turned: it shows lit for a moment */
    uint8_t redraw;
} ui;

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)     /* signed, no padding */
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw = 1;
    if (ann_on()) {                          /* and to a screen reader (announce.c, fm1-reader) */
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void octave_text(char *t)
{
    t[0] = set.octave < 0 ? '-' : '+';
    t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave));
    t[2] = 0;
}

/* a screen reader that has just started listening (or asks) hears the whole screen */
static void speak(void)
{
    char t[96], o[4];
    if (ann_on() && ann_where()) {
        octave_text(o);
        t[0] = 0;
        text_cat(t, APP_NAME ". OCTAVE ");
        text_cat(t, o);
        text_cat(t, ". WAVE ");
        text_cat(t, fm6_patch_name(set.patch));
        if (midi_count) {                    /* the last MIDI message in, as the screen shows it */
            static const char *const PORT[NPORTS] = {"USB", "BLE", "TRS"};
            static const char HEX[] = "0123456789ABCDEF";
            uint32_t m = midi_last, b;
            char h[10];
            for (b = 0; b < 3u; b++) {
                uint32_t v = (m >> (8u + 8u * b)) & 0xFFu;
                h[3 * b] = HEX[v >> 4];
                h[3 * b + 1] = HEX[v & 15u];
                h[3 * b + 2] = b < 2u ? ' ' : 0;
            }
            text_cat(t, ". MIDI IN ");
            text_cat(t, midi_last_port < NPORTS ? PORT[midi_last_port] : "");
            text_cat(t, " ");
            text_cat(t, h);
        }
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

static void note_name(int32_t note, char *b)
{
    static const char *const N[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    char o[12];
    b[0] = 0;
    text_cat(b, N[note % 12]);
    int_text(note / 12 - 1, o);
    text_cat(b, o);
}

static void octave_step(int32_t d)
{
    int32_t o = clamp(set.octave + d, -2, 2);
    char t[4];
    if (o == set.octave)
        return;
    voice_note = -1;                         /* a key held across the change would not find its note off */
    ui_event_push(EV_PANIC, 0, 0, 0);
    set.octave = (int8_t)o;
    set_dirty = 1;
    octave_text(t);
    app_say("OCTAVE", t);
}

int fm1lab_ble_off(void) { return set.ble_off; }

/* REC held a second: Bluetooth off or on (kept with the settings, so it stays so at the next power-on) */
static void ble_toggle(void)
{
    set.ble_off = (uint8_t)!set.ble_off;
    set_dirty = 1;
    plat_ble_enable(!set.ble_off);
    app_say("BLUETOOTH", set.ble_off ? "OFF" : "ON");
}

/* REC: a Bluetooth MIDI device (FM1B_BLE_CENTRAL, lib/btdev): look for devices, then connect to the one ALGORITHM
 * picks, then let it go */
static void ble_device_say(void)
{
    char name[21];
    if (btdev_found(btdev_pick(), name, 0))
        app_say("DEVICE", name);
    else
        app_say("BLE", "SCANNING");
}
static void ble_device_button(void)
{
    int ev;
    if (!btdev_busy() && !ui.looking) {
        ui.looking = 1;
        app_say("BLE", "SCANNING");
        return;
    }
    ev = btdev_save();
    ui.looking = 0;
    app_say(ev == BTDEV_CONNECTING ? btdev_word(ev) : "BLE", ev == BTDEV_CONNECTING ? btdev_name() : btdev_word(ev));
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k;
    int32_t e;
    touch_poll();                            /* the sensed keys' presses, velocities and pressure */
    keys = touch_keys();
    for (k = 0; k < NKEYS; k++) {            /* key 0 is F3 (MIDI 53) */
        uint32_t bit = 1u << k, v;
        int32_t note = 53 + (int32_t)k + 12 * set.octave;
        if ((keys & bit) && !(ui.keys & bit)) {
            v = touch_vel(k);
            ui.vel[k] = (uint8_t)v;
            ui.prs[k] = 0;
            ui.last_key = (int32_t)k;
            note_on(note, v);
            plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)note << 16 | v << 24, TO_USB | TO_BLE);   /* BLE: if built with it */
        } else if (!(keys & bit) && (ui.keys & bit)) {
            ui.prs[k] = 0;
            note_off(note);
            plat_midi_send(0x08u | 0x80u << 8 | (uint32_t)note << 16, TO_USB | TO_BLE);
        } else if ((keys & bit) && touch_pressure(k) != ui.prs[k]) {   /* pressure: poly aftertouch */
            ui.prs[k] = touch_pressure(k);
            ui.redraw = 1;
            plat_midi_send(0x0Au | 0xA0u << 8 | (uint32_t)note << 16 | (uint32_t)ui.prs[k] << 24, TO_USB | TO_BLE);
        }
    }
    for (k = 0; k < NB; k++) {               /* a button pressed: its name, and what it does here */
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        if (k == B_OCTDN)
            octave_step(-1);
        else if (k == B_OCTUP)
            octave_step(1);
        else if (k == B_REC && FM1B_BLE)
            ui.rec_t = plat_ms();                /* a press: on release (a hold switches Bluetooth) */
        else if (k == B_HOME) {
            page = PAGE_HOME;
            app_say("HOME", fm6_patch_name(set.patch));
        } else if (k == B_GLO) {
            page = page == PAGE_GLOBAL ? PAGE_HOME : PAGE_GLOBAL;
            app_say(page==PAGE_GLOBAL?"GLOBAL":"HOME","SETTINGS");
        } else if (k == B_ENV) {
            if (page == PAGE_ENV) env_levels ^= 1u;
            else page = PAGE_ENV;
            app_say("ENVELOPE", env_levels ? "LEVELS" : "RATES");
        } else if (k == B_LFO) {
            page = page == PAGE_LFO ? PAGE_LFO_MOD :
                page == PAGE_LFO_MOD ? PAGE_LFO : PAGE_LFO;
            app_say("LFO", page == PAGE_LFO_MOD ? "MODULATION" : "OSCILLATOR");
        } else if (k == B_SEL) {
            page = page==PAGE_MACRO?PAGE_HOME:PAGE_MACRO;
            app_say(page==PAGE_MACRO?"PATCH MACROS":"HOME",fm6_patch_name(set.patch));
        } else if (k == B_EDIT && page == PAGE_FX_AMP) {
            instrument_insert_edit(FM6_INSERT_AMP_ENABLED,(uint8_t)!fm1lab_insert_state_get(&current_inserts,FM6_INSERT_AMP_ENABLED));
            app_say("MASTER AMP / DRIVE",fm1lab_insert_state_get(&current_inserts,FM6_INSERT_AMP_ENABLED)?"ACTIVE":"BYPASS");
        } else if (k == B_EDIT && page == PAGE_FX_EQ) {
            instrument_insert_edit(FM6_INSERT_EQ_ENABLED,(uint8_t)!fm1lab_insert_state_get(&current_inserts,FM6_INSERT_EQ_ENABLED));
            app_say("MASTER EQ",fm1lab_insert_state_get(&current_inserts,FM6_INSERT_EQ_ENABLED)?"ACTIVE":"BYPASS");
        } else if (k == B_EDIT && page == PAGE_FX_COMPRESSOR) {
            compressor_edit(FM6_COMP_ENABLED,(uint8_t)!compressor_value(FM6_COMP_ENABLED));
            app_say("MASTER COMPRESSOR",compressor_value(FM6_COMP_ENABLED)?"ACTIVE":"BYPASS");
        } else if (k == B_EDIT && page >= PAGE_FX_CHORUS && page <= PAGE_FX_PHASER) {
            uint8_t fx=(uint8_t)(page-PAGE_FX_CHORUS);
            set.performance.fx_enabled[fx]=(uint8_t)!set.performance.fx_enabled[fx];
            performance_publish();
            set_dirty = 1u;
            app_say("FX",set.performance.fx_enabled[fx]?"ON":"BYPASS");
        } else if (k == B_EDIT) {
            page = page == PAGE_OPERATOR_MAP ? PAGE_MACRO : PAGE_OPERATOR_MAP;
            app_say("OPERATOR ROLES", fm6_patch_name(set.patch));
        } else if (k == B_SEQ) {
            page = PAGE_SEQ;
            app_say("SEQUENCER", fm6_sequencer_running() ? "RUNNING" : "TWO LANES");
        } else if (k == B_ARP) {
            page = PAGE_ARP;
            arp_ui_mode = (uint8_t)((arp_ui_mode + 1u) % 4u);
            performance_publish();
            set_dirty = 1u;
            app_say("ARPEGGIATOR", arp_ui_mode == 0u ? "OFF" : arp_ui_mode == 1u ? "UP" : arp_ui_mode == 2u ? "DOWN" : "UP / DOWN");
        } else if (k == B_FX) {
            if (page < PAGE_FX_AMP || page > PAGE_FX_ROUTING) page = PAGE_FX_AMP;
            else page = page == PAGE_FX_ROUTING ? PAGE_FX_AMP : (uint8_t)(page + 1u);
            app_say("FX SECTION",page==PAGE_FX_AMP?"AMP / DRIVE":page==PAGE_FX_EQ?"PARAMETRIC EQ":page==PAGE_FX_COMPRESSOR?"COMPRESSOR":page==PAGE_FX_CHORUS?"CHORUS":page==PAGE_FX_DELAY?"DELAY":page==PAGE_FX_REVERB?"REVERB":page==PAGE_FX_PHASER?"PHASER":"SEND ROUTING");
        } else if (k == B_PLAY) {
            if (page == PAGE_SEQ) {
                ui_event_push(EV_SEQ_RUN,(uint8_t)!fm6_sequencer_running(),0u,0u);
                app_say("SEQUENCER", fm6_sequencer_running() ? "PLAYING" : "STOPPED");
            } else {
                app_say("PLAY", "SELECT SEQUENCER");
            }
        } else if (k == B_SAVE) {
            int instrument_result = instrument_save();
            /* set.performance is the UI-owned source of truth; queued audio
             * updates must not be read back over edits that have not rendered yet. */
            settings_seal(&set);
            int settings_result = plat_store_save(OBJ_SETTINGS, &set, sizeof set);
            if (instrument_result == 0 && settings_result == 0) {
                set_dirty = 0;
                app_say("INSTRUMENT", "SAVED");
            } else {
                app_say("SAVE", "FAILED");
            }
        } else {
            app_say("BUTTON", BTN_NAME[k]);
        }
    }
    if (FM1B_BLE && ui.rec_t) {             /* REC: held a second, Bluetooth on / off; a press, a device */
        if (btn >> B_REC & 1u) {
            if (plat_ms() - ui.rec_t >= 1000u) {
                ui.rec_t = 0;
                ble_toggle();
            }
        } else {
            ui.rec_t = 0;
            if (FM1B_BLE_CENTRAL && !set.ble_off)
                ble_device_button();
            else
                app_say("BUTTON", "REC");
        }
    }
    for (k = 0; k < NE; k++) {               /* an encoder turned: its count; PRESET also changes the wave */
        char t[12];
        if ((e = plat_enc((int)k)) == 0)
            continue;
        ui.turns[k] += e;
        ui.turned_at[k] = plat_ms();
        ui.redraw = 1;
        if (k == EN_ALGO && FM1B_BLE_CENTRAL && ui.looking) {
            btdev_choose(btdev_pick() + e);
            ble_device_say();
        } else if (k == EN_ALGO && (page == PAGE_HOME || page == PAGE_OPERATOR_MAP)) {
            /* ALGO edits the selected patch's actual FM algorithm. PRESET
             * owns Instrument selection; the two controls stay independent. */
            voice_edit_global(FM1LAB_GLOBAL_ALGORITHM, e);
            uint8_t algorithm = 0u;
            (void)fm1lab_voice_global_get(current_instrument.patches[set.patch].voice,
                FM1LAB_GLOBAL_ALGORITHM, &algorithm);
            int_text(algorithm, t);
            app_say("FM ALGORITHM", t);
        } else if (k == EN_SELECT && page == PAGE_OPERATOR_MAP) {
            int32_t next = ((int32_t)env_operator - 1 + e) % 6;
            if (next < 0) next += 6;
            env_operator = (uint8_t)(next + 1);
            app_say("OPERATOR", fm1lab_voice_operator_is_carrier(
                current_instrument.patches[set.patch].voice, env_operator) ? "CARRIER" : "MODULATOR");
        } else if (k == EN_ALGO && page == PAGE_FX_EQ) {
            int32_t next=((int32_t)eq_ui_band+e)%3;
            if(next<0)next+=3;
            eq_ui_band=(uint8_t)next;
            app_say("EQ BAND",eq_ui_band==0u?"LOW":eq_ui_band==1u?"MID":"HIGH");
        } else if (k == EN_ALGO && page == PAGE_ENV) {
            int32_t next = ((int32_t)env_operator - 1 + e) % 6;
            if (next < 0) next += 6;
            env_operator = (uint8_t)(next + 1);
            app_say("ENVELOPE OPERATOR", fm1lab_voice_operator_is_carrier(
                current_instrument.patches[set.patch].voice, env_operator) ? "CARRIER" : "MODULATOR");
        } else if (k == EN_SELECT && page == PAGE_SEQ) {
            int32_t next = ((int32_t)seq_ui_step + e) % 64;
            if (next < 0) next += 64;
            seq_ui_step = (uint8_t)next;
            app_say("STEP", "SELECTED");
        } else if (k == EN_ALGO && page == PAGE_SEQ) {
            int32_t patch = ((int32_t)seq_ui_patch[seq_ui_lane] + e) % 4;
            if (patch < 0) patch += 4;
            seq_ui_patch[seq_ui_lane] = (uint8_t)patch;
            performance_publish();
            set_dirty = 1u;
            app_say("LANE PATCH", fm6_patch_name((uint32_t)patch));
        } else if (k == EN_ALGO && page == PAGE_FX_ROUTING) {
            int32_t next=((int32_t)set.patch+e)%4;
            if(next<0) next+=4;
            set.patch=(uint8_t)next;
            set_dirty=1u;
            app_say("ROUTING PATCH",fm6_patch_name(set.patch));
        } else if (k == EN_ALGO && page == PAGE_FX_COMPRESSOR) {
            compressor_edit(FM6_COMP_MAKEUP,(uint8_t)clamp((int32_t)compressor_value(FM6_COMP_MAKEUP)+e,0,100));
            app_say("COMP MAKEUP","UPDATED");
        } else if (k == EN_SELECT && page >= PAGE_FX_AMP && page <= PAGE_FX_ROUTING) {
            int32_t next = (int32_t)(page - PAGE_FX_AMP) + e;
            next %= 8;
            if (next < 0) next += 8;
            page = (uint8_t)(PAGE_FX_AMP + next);
            app_say("FX SECTION",page==PAGE_FX_AMP?"AMP / DRIVE":page==PAGE_FX_EQ?"PARAMETRIC EQ":page==PAGE_FX_COMPRESSOR?"COMPRESSOR":page==PAGE_FX_CHORUS?"CHORUS":page==PAGE_FX_DELAY?"DELAY":page==PAGE_FX_REVERB?"REVERB":page==PAGE_FX_PHASER?"PHASER":"SEND ROUTING");
        } else if (k == EN_SELECT) {
            int32_t next = ((int32_t)set.patch + e) % (int32_t)FM1LAB_PATCH_COUNT;
            if (next < 0) next += (int32_t)FM1LAB_PATCH_COUNT;
            set.patch = (uint8_t)next;
            set_dirty = 1;
            app_say("PATCH", fm6_patch_name(set.patch));
        } else if (k == EN_PRESET && page == PAGE_SEQ) {
            /* PRESET selects the visible sequencer lane here; it must not
             * switch Instruments while editing a pattern. */
            seq_ui_lane = (uint8_t)fm1lab_sequencer_lane_advance(seq_ui_lane, e);
            app_say("SEQUENCER LANE", seq_ui_lane ? "B" : "A");
        } else if (k == EN_PRESET && page == PAGE_OPERATOR_MAP) {
            int32_t mode = ((int32_t)operator_detune_mode + e) % 2;
            if (mode < 0) mode += 2;
            operator_detune_mode = (uint8_t)mode;
            app_say("OPERATOR PITCH", operator_detune_mode ? "DETUNE" : "FINE");
        } else if (k == EN_PRESET) {
            if (instrument_apply_pending) {
                app_say("INSTRUMENT", "LOADING");
            } else {
                /* PRESET must never appear inert: preserve edits before
                 * moving to another Instrument slot. */
                if (instrument_dirty && instrument_save() != 0) {
                    app_say("SAVE FAILED", "PRESET NOT CHANGED");
                    continue;
                }
                int32_t next = ((int32_t)instrument_index + e) % (int32_t)FM1LAB_STORE_INSTRUMENT_COUNT;
                if (next < 0) next += (int32_t)FM1LAB_STORE_INSTRUMENT_COUNT;
                if ((uint32_t)next != instrument_index) {
                    instrument_load((uint32_t)next, 1);
                    int_text(next + 1, t);
                    app_say("INSTRUMENT", t);
                }
            }
        } else if (k >= EN_K1 && k <= EN_K4) {
            uint32_t index = (uint32_t)k - EN_K1;
            if (page == PAGE_SEQ) {
                uint8_t lane=seq_ui_lane, step=seq_ui_step;
                if(index==0u) {
                    /* Each encoder detent toggles once; multi-detent reads
                     * must preserve even/odd parity instead of toggling once. */
                    if (e % 2 != 0) seq_ui_enabled[lane][step]^=1u;
                } else if(index==1u) seq_ui_note[lane][step]=(uint8_t)clamp((int32_t)seq_ui_note[lane][step]+e,0,127);
                else if(index==2u) seq_ui_velocity[lane][step]=(uint8_t)clamp((int32_t)seq_ui_velocity[lane][step]+e,0,127);
                else seq_ui_gate[lane][step]=(uint8_t)clamp((int32_t)seq_ui_gate[lane][step]+e,10,100);
                performance_publish();
                set_dirty = 1u;
                app_say("SEQUENCER STEP", "UPDATED");
            } else if (page == PAGE_ARP) {
                if(index==0u) {
                    transport_ui_bpm=(uint16_t)clamp((int32_t)transport_ui_bpm+e,30,300);
                    performance_publish();
                    set_dirty = 1u;
                    app_say("TEMPO", "UPDATED");
                } else if(index==1u) {
                    int32_t mode=((int32_t)arp_ui_mode+e)%4;
                    if(mode<0) mode+=4;
                    arp_ui_mode=(uint8_t)mode;
                    performance_publish();
                    set_dirty = 1u;
                    app_say("ARP MODE",arp_ui_mode==0u?"OFF":arp_ui_mode==1u?"UP":arp_ui_mode==2u?"DOWN":"UP / DOWN");
                }
            } else if (page == PAGE_FX_ROUTING) {
                uint8_t fx=(uint8_t)index;
                set.performance.fx_send[set.patch][fx]=(uint8_t)clamp((int32_t)set.performance.fx_send[set.patch][fx]+e,0,100);
                performance_publish();
                set_dirty=1u;
                app_say("PATCH FX SEND",fx==0u?"CHORUS":fx==1u?"DELAY":fx==2u?"REVERB":"PHASER");
            } else if (page >= PAGE_FX_CHORUS && page <= PAGE_FX_PHASER) {
                uint8_t fx=(uint8_t)(page-PAGE_FX_CHORUS);
                if(index==0u) set.performance.fx_mix[fx]=(uint8_t)clamp((int32_t)set.performance.fx_mix[fx]+e,0,100);
                else if(index==1u) set.performance.fx_time[fx]=(uint8_t)clamp((int32_t)set.performance.fx_time[fx]+e,0,100);
                else if(index==2u) {
                    if(fx==FM6_FX_PHASER) set.performance.fx_depth[fx]=(uint8_t)clamp((int32_t)set.performance.fx_depth[fx]+e,0,100);
                    else set.performance.fx_feedback[fx]=(uint8_t)clamp((int32_t)set.performance.fx_feedback[fx]+e,0,fx==0u?70:90);
                } else if (fx == FM6_FX_PHASER) {
                    set.performance.fx_feedback[fx]=(uint8_t)clamp((int32_t)set.performance.fx_feedback[fx]+e,0,90);
                } else {
                    set.performance.fx_send[set.patch][fx]=(uint8_t)clamp((int32_t)set.performance.fx_send[set.patch][fx]+e,0,100);
                }
                performance_publish();
                set_dirty = 1u;
                app_say(index==0u?"FX WET":index==1u?(fx==0u?"CHORUS RATE":fx==1u?"DELAY TIME":fx==2u?"REVERB DECAY":"PHASER RATE"):index==2u?(fx==FM6_FX_PHASER?"PHASER DEPTH":"FX FEEDBACK"):(fx==FM6_FX_PHASER?"PHASER RESONANCE":"PATCH SEND"), "UPDATED");
            } else if (page == PAGE_FX_AMP) {
                static const uint8_t fields[4]={FM6_INSERT_DRIVE,FM6_INSERT_TONE,FM6_INSERT_OUTPUT,FM6_INSERT_AMP_ENABLED};
                uint8_t field=fields[index],value=fm1lab_insert_state_get(&current_inserts,field);
                if(index==3u)value=(uint8_t)!value;
                else value=(uint8_t)clamp((int32_t)value+e,0,100);
                instrument_insert_edit(field,value);
                app_say(index==0u?"AMP DRIVE":index==1u?"AMP TONE":index==2u?"AMP OUTPUT":"AMP",index==3u?(value?"ON":"BYPASS"):"UPDATED");
            } else if (page == PAGE_FX_EQ) {
                uint8_t field=(uint8_t)(FM6_INSERT_EQ_LOW_FREQUENCY+eq_ui_band*4u+index);
                uint8_t value=fm1lab_insert_state_get(&current_inserts,field);
                if(index==3u)value=(uint8_t)!value;
                else value=(uint8_t)clamp((int32_t)value+e,0,index==1u?24:index==2u?31:100);
                instrument_insert_edit(field,value);
                app_say(index==0u?"EQ FREQUENCY":index==1u?"EQ GAIN":index==2u?"EQ Q":"EQ BAND",index==3u?(value?"ON":"OFF"):"UPDATED");
            } else if (page == PAGE_FX_COMPRESSOR) {
                static const uint8_t fields[4]={FM6_COMP_THRESHOLD,FM6_COMP_RATIO,FM6_COMP_ATTACK,FM6_COMP_RELEASE};
                uint8_t field=fields[index];
                compressor_edit(field,(uint8_t)clamp((int32_t)compressor_value(field)+e,0,100));
                app_say(index==0u?"COMP THRESHOLD":index==1u?"COMP RATIO":index==2u?"COMP ATTACK":"COMP RELEASE","UPDATED");
            } else if (page == PAGE_OPERATOR_MAP) {
                static const fm1lab_operator_field_t fields[4] = {
                    FM1LAB_OP_OUTPUT, FM1LAB_OP_MODE, FM1LAB_OP_COARSE, FM1LAB_OP_FINE
                };
                voice_edit_operator(index == 3u && operator_detune_mode ?
                    FM1LAB_OP_DETUNE : fields[index], e);
                app_say(index == 0u ? "OPERATOR OUTPUT" : index == 1u ? "OSCILLATOR MODE" :
                    index == 2u ? "COARSE FREQUENCY" : operator_detune_mode ? "OPERATOR DETUNE" : "FINE FREQUENCY", "UPDATED");
            } else if (page == PAGE_ENV) {
                voice_edit_operator((fm1lab_operator_field_t)((env_levels ? FM1LAB_OP_LEVEL_1 : FM1LAB_OP_RATE_1) + index), e);
                app_say(env_levels ? "ENVELOPE LEVEL" : "ENVELOPE RATE", "UPDATED");
            } else if (page == PAGE_LFO) {
                static const fm1lab_global_field_t fields[4] = {
                    FM1LAB_GLOBAL_LFO_WAVE, FM1LAB_GLOBAL_LFO_SPEED,
                    FM1LAB_GLOBAL_LFO_PITCH_DEPTH, FM1LAB_GLOBAL_LFO_AMP_DEPTH
                };
                voice_edit_global(fields[index], e);
                app_say("LFO", "UPDATED");
            } else if (page == PAGE_LFO_MOD) {
                static const fm1lab_global_field_t fields[4] = {
                    FM1LAB_GLOBAL_LFO_DELAY, FM1LAB_GLOBAL_PITCH_SENS,
                    FM1LAB_GLOBAL_LFO_SYNC, FM1LAB_GLOBAL_OSC_SYNC
                };
                voice_edit_global(fields[index], e);
                app_say("LFO MODULATION", "UPDATED");
            } else if (page == PAGE_MACRO) {
                int32_t value = (int32_t)set.macros[set.patch][index] + e;
                if (value < 0) value = 0;
                if (value > 100) value = 100;
                set.macros[set.patch][index] = (uint8_t)value;
                current_instrument.patches[set.patch].macros[index] = (uint8_t)value;
                instrument_dirty = 1u;
                ui_event_push(EV_MACRO_SET, set.patch, (uint8_t)index, (uint8_t)value);
                set_dirty = 1;
                app_say("MACRO", "UPDATED");
            } else if (page == PAGE_HOME) {
                /* HOME K1-K4 are volume-only; SELECT is the sole patch selector. */
                int32_t value = (int32_t)set.levels[index] + e;
                if (value < 0) value = 0;
                if (value > 127) value = 127;
                set.levels[index] = (uint8_t)value;
                current_instrument.patches[index].level = (uint8_t)value;
                instrument_dirty = 1u;
                ui_event_push(EV_PATCH_LEVEL, (uint8_t)index, (uint8_t)value, 0);
                set_dirty = 1;
                app_say("PATCH LEVEL", "UPDATED");
            } else {
                int_text(ui.turns[k], t);
                app_say(ENC_NAME[k], t);
            }
        } else {
            int_text(ui.turns[k], t);
            app_say(ENC_NAME[k], t);
        }
    }
    voice_update_publish();
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    plat_glow(1);                            /* the unlit buttons glow, readable in the dark */
    plat_leds(btn | (set_dirty ? 1u << B_SAVE : 0u), keys);
}

void app_input_only(void) { input(); }

static void cell(int32_t x, int32_t y, int32_t w, const char *s, int on)   /* a labelled box, lit when on */
{
    cv_rect(x, y, w, 18, on ? C_HI : C_LINE);
    cv_text(x + (w - text_w(&FONT_XS, s)) / 2, y + 2, &FONT_XS, s, on ? C_BLACK : C_GRAY);
}

static void key_cell(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t k, uint16_t off)
{
    int32_t v, p;
    cv_rect(x, y, w, h, off);
    if (!(ui.keys >> k & 1u))
        return;
    v = (ui.vel[k] * h + 126) / 127;
    p = (ui.prs[k] * h + 126) / 127;
    cv_rect(x, y + h - v, w, v, C_HI);
    if (p)
        cv_rect(x + 2, y + h - p, w - 4, p, C_WHITE);
}

static void outline(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color)
{
    cv_rect(x, y, w, 1, color);
    cv_rect(x, y + h - 1, w, 1, color);
    cv_rect(x, y, 1, h, color);
    cv_rect(x + w - 1, y, 1, h, color);
}

static void rounded_panel(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t fill, uint16_t border)
{
    /* FM1's display is a tiny instrument panel, not a miniature phone UI:
     * clipped corners, a quiet inset, and one precise highlight line. */
    (void)r;
    cv_rect(x + 2, y, w - 4, h, border);
    cv_rect(x, y + 2, 1, h - 4, border);
    cv_rect(x + w - 1, y + 2, 1, h - 4, border);
    cv_rect(x + 1, y + 1, w - 2, h - 2, fill);
    cv_rect(x + 3, y + 2, w - 6, 1, RGB(31, 47, 59));
    cv_rect(x + 2, y + h - 2, w - 4, 1, RGB(5, 9, 14));
}
static void instrument_card(int32_t x, int32_t y, int32_t w, int32_t h,
                            uint16_t fill, uint16_t border, uint16_t accent)
{
    rounded_panel(x, y, w, h, 3, fill, border);
    /* A narrow color rail gives every editor the same instrument-panel identity
     * without spending scarce pixels on extra labels or decorative chrome. */
    cv_rect(x + 3, y + 6, 2, h - 12, accent);
    cv_rect(x + 7, y + 2, w - 14, 1, RGB(31, 47, 59));
}

static uint16_t patch_color(uint32_t p)
{
    static const uint16_t colors[4] = {RGB(36,215,232), RGB(227,75,210), RGB(242,212,59), RGB(255,82,101)};
    return colors[p & 3u];
}
static uint16_t panel_fill(void) { return RGB(17,29,39); }
static uint16_t screen_bg(void) { return RGB(8,13,19); }

static int algorithm_preview_active(void)
{
    return page == PAGE_HOME && plat_ms() < ui.msg_until &&
        ui.msg[0][0]=='F' && ui.msg[0][1]=='M' && ui.msg[0][2]==' ' &&
        ui.msg[0][3]=='A' && ui.msg[0][4]=='L';
}

/* M-VAVE-style six-operator topology preview. Connections come from the
 * selected voice's actual algorithm routing table; carrier nodes are filled. */
static void draw_algorithm_diagram(void)
{
    static const int16_t nx[6]={20,102,184,20,102,184};
    static const int16_t ny[6]={39,39,39,89,89,89};
    const uint8_t *voice=current_instrument.patches[set.patch].voice;
    uint8_t algorithm=1u;
    uint16_t accent=patch_color(set.patch);
    char t[12];
    (void)fm1lab_voice_global_get(voice,FM1LAB_GLOBAL_ALGORITHM,&algorithm);
    cv_text(7,5,&FONT_XS,"OPERATOR ROUTING",C_GRAY);
    cv_text(178,5,&FONT_XS,"ALG",C_GRAY);
    int_text(algorithm,t); cv_text(205,5,&FONT_XS,t,accent);

    /* Draw modulation links behind the operator blocks. */
    for (unsigned source=1;source<=6u;++source) {
        for (unsigned target=1;target<=6u;++target) {
            if (source==target ||
                fm1lab_voice_operator_modulates(voice,source,target)<=0) continue;
            unsigned si=source-1u, ti=target-1u;
            int32_t sx=nx[si]+18, sy=ny[si]+13;
            int32_t tx=nx[ti]+18, ty=ny[ti]+13;
            uint16_t ink=RGB(88,112,132);
            if (sy==ty) {
                int32_t source_edge=sx<tx?sx+18:sx-18;
                int32_t target_edge=sx<tx?tx-18:tx+18;
                int32_t left=source_edge<target_edge?source_edge:target_edge;
                int32_t right=source_edge<target_edge?target_edge:source_edge;
                cv_rect(left,sy-1,right-left+1,2,ink);
                cv_rect(target_edge-2,ty-3,2,6,accent);
            } else {
                int32_t source_edge=sy<ty?sy+13:sy-13;
                int32_t target_edge=sy<ty?ty-13:ty+13;
                int32_t mid_y=(source_edge+target_edge)/2;
                int32_t left=sx<tx?sx:tx, right=sx<tx?tx:sx;
                int32_t top=source_edge<mid_y?source_edge:mid_y;
                int32_t bottom=source_edge<mid_y?mid_y:source_edge;
                cv_rect(sx-1,top,2,bottom-top+1,ink);
                cv_rect(left,mid_y-1,right-left+1,2,ink);
                top=target_edge<mid_y?target_edge:mid_y;
                bottom=target_edge<mid_y?mid_y:target_edge;
                cv_rect(tx-1,top,2,bottom-top+1,ink);
                cv_rect(tx-3,target_edge-2,6,2,accent);
            }
        }
    }
    for (unsigned op=1;op<=6u;++op) {
        unsigned i=op-1u;
        int carrier=fm1lab_voice_operator_is_carrier(voice,op)>0;
        uint16_t edge=carrier?accent:C_LINE;
        uint16_t fill=carrier?RGB(28,57,67):panel_fill();
        rounded_panel(nx[i],ny[i],36,26,3,fill,edge);
        cv_text(nx[i]+7,ny[i]+7,&FONT_B,op==1?"OP1":op==2?"OP2":op==3?"OP3":op==4?"OP4":op==5?"OP5":"OP6",
            carrier?accent:C_WHITE);
        cv_rect(nx[i]+3,ny[i]+22,30,1,carrier?accent:C_DIM);
    }
    cv_text(8,126,&FONT_XS,"CARRIER",accent);
    cv_text(75,126,&FONT_XS,"MODULATOR",C_GRAY);
    cv_text(166,126,&FONT_XS,"ALGO KNOB",C_GRAY);
}

static void draw_home_panel(void)
{
    char t[24];
    if (algorithm_preview_active()) {
        draw_algorithm_diagram();
        return;
    }
    for (uint32_t p = 0; p < 4u; ++p) {
        int32_t x = 2 + (int32_t)(p & 1u) * 120;
        int32_t y = 7 + (int32_t)(p >> 1u) * 85;
        uint16_t accent = patch_color(p);
        int enabled = set.levels[p] != 0u;
        rounded_panel(x, y, 116, 85, 3, panel_fill(), p == set.patch ? accent : C_LINE);
        cv_rect(x + 5, y + 7, 2, 70, accent);
        int_text((int32_t)p + 1, t);
        cv_text(x + 12, y + 5, &FONT_XS, t, accent);
        cv_text(x + 28, y + 5, &FONT_XS, enabled ? "LAYER ACTIVE" : "LAYER MUTED",
                enabled ? RGB(75,235,135) : C_GRAY);
        cv_text(x + 12, y + 23, &FONT_B, fm6_patch_name(p), C_WHITE);
        cv_text(x + 12, y + 49, &FONT_XS, "ALG", C_GRAY);
        int_text((int32_t)fm6_patch_algorithm(p) + 1, t);
        cv_text(x + 34, y + 49, &FONT_XS, t, accent);
        cv_text(x + 59, y + 49, &FONT_XS, "LEVEL", C_GRAY);
        int_text(set.levels[p], t);
        cv_text(x + 91, y + 49, &FONT_XS, t, C_WHITE);
        cv_rect(x + 12, y + 69, 96, 5, RGB(31, 43, 55));
        cv_rect(x + 12, y + 69, (int32_t)set.levels[p] * 96 / 127, 5, accent);
        if (p == set.patch) cv_rect(x + 12, y + 80, 96, 1, accent);
    }
}
static void draw_macro_panel(void)
{
    static const char *const names[4] = {"BRIGHTNESS", "BODY", "MOTION", "EDGE"};
    static const char *const hints[4] = {"FM DEPTH", "RATIO / ENV", "LFO / DEPTH", "FEEDBACK"};
    uint32_t now = plat_ms();
    uint16_t accent = patch_color(set.patch);
    char t[24];
    cv_text(6, 2, &FONT_XS, fm6_patch_name(set.patch), C_WHITE);
    cv_text(164, 2, &FONT_XS, "ALG", C_GRAY);
    int_text((int32_t)fm6_patch_algorithm(set.patch) + 1, t);
    cv_text(204, 2, &FONT_XS, t, accent);
    for (uint32_t m = 0; m < 4u; ++m) {
        int32_t x = 4 + (int32_t)(m & 1u) * 118;
        int32_t y = 18 + (int32_t)(m >> 1u) * 64;
        uint32_t value = set.macros[set.patch][m];
        uint16_t border = ui.turned_at[EN_K1 + m] && now - ui.turned_at[EN_K1 + m] < 650u ? accent : C_LINE;
        rounded_panel(x, y, 112, 56, 3, panel_fill(), border);
        cv_rect(x + 6, y + 7, 2, 41, accent);
        cv_text(x + 13, y + 7, &FONT_XS, names[m], C_WHITE);
        int_text((int32_t)value, t);
        cv_text(x + 82, y + 5, &FONT_B, t, accent);
        cv_rect(x + 13, y + 29, 91, 5, RGB(31, 43, 55));
        cv_rect(x + 13, y + 29, (int32_t)value * 91 / 100, 5, accent);
        cv_rect(x + 13 + (int32_t)value * 91 / 100, y + 26, 2, 11, C_WHITE);
        cv_text(x + 13, y + 43, &FONT_XS, hints[m], C_GRAY);
    }
}
/* Four operator quadrants; roles come from the selected algorithm's actual data. */
static void draw_operator_map(void)
{
    static const char *const labels[4] = {"OUTPUT", "MODE", "COARSE", "FINE"};
    const fm1lab_operator_field_t fields[4] = {
        FM1LAB_OP_OUTPUT, FM1LAB_OP_MODE, FM1LAB_OP_COARSE, FM1LAB_OP_FINE
    };
    uint8_t maxima[4] = {99u, 1u, 31u, operator_detune_mode ? 14u : 99u};
    const uint8_t *voice = current_instrument.patches[set.patch].voice;
    uint8_t values[4] = {0u, 0u, 0u, 0u}, algorithm = 1u;
    char t[16];
    uint16_t accent = patch_color(set.patch);
    int carrier = fm1lab_voice_operator_is_carrier(voice, env_operator);
    (void)fm1lab_voice_global_get(voice, FM1LAB_GLOBAL_ALGORITHM, &algorithm);
    cv_text(6, 2, &FONT_XS, fm6_patch_name(set.patch), C_WHITE);
    cv_text(91, 2, &FONT_XS, "ALG", C_GRAY);
    int_text(algorithm, t); cv_text(113, 2, &FONT_XS, t, accent);
    cv_text(145, 2, &FONT_XS, carrier ? "CARRIER" : "MODULATOR", carrier ? accent : C_GRAY);
    cv_text(202, 2, &FONT_XS, "OP", C_GRAY);
    int_text(env_operator, t); cv_text(226, 2, &FONT_XS, t, accent);
    for (uint32_t i = 0; i < 4u; ++i) {
        int32_t x = 4 + (int32_t)(i & 1u) * 118;
        int32_t y = 18 + (int32_t)(i >> 1u) * 64;
        (void)fm1lab_voice_operator_get(voice, env_operator,
            i == 3u && operator_detune_mode ? FM1LAB_OP_DETUNE : fields[i], &values[i]);
        instrument_card(x, y, 112, 56, panel_fill(), accent, accent);
        cv_text(x + 9, y + 8, &FONT_XS,
            i == 3u && operator_detune_mode ? "DETUNE" : labels[i], C_WHITE);
        if (i == 1u) {
            cv_text(x + 9, y + 27, &FONT_B, values[i] ? "FIXED" : "RATIO", accent);
        } else {
            int_text(values[i], t);
            cv_text(x + 9, y + 27, &FONT_B, t, accent);
            cv_rect(x + 9, y + 46, 94, 5, C_LINE);
            cv_rect(x + 9, y + 46, (int32_t)values[i] * 94 / maxima[i], 5, accent);
        }
    }
}


static void draw_amp_panel(void)
{
    static const char *const labels[4]={"DRIVE","TONE","OUTPUT","AMP STATE"};
    const fm1lab_insert_state_t *v=&current_inserts;
    char t[16];uint16_t accent=RGB(36,215,232);
    uint8_t values[4]={v->drive,v->tone,v->output,v->amp_enabled};
    cv_text(6,2,&FONT_XS,"MASTER BUS",C_WHITE);
    cv_text(172,2,&FONT_XS,v->amp_enabled?"ACTIVE":"BYPASS",v->amp_enabled?RGB(75,235,135):C_GRAY);
    for(uint32_t i=0;i<4u;i++){
        int32_t x=4+(int32_t)(i&1u)*118,y=18+(int32_t)(i>>1u)*54;
        instrument_card(x,y,112,48,panel_fill(),accent,accent);
        cv_text(x+7,y+6,&FONT_XS,labels[i],C_GRAY);
        if(i==3u)cv_text(x+7,y+23,&FONT_B,values[i]?"ON":"OFF",accent);
        else {int_text(values[i],t);cv_text(x+7,y+23,&FONT_B,t,accent);cv_rect(x+48,y+29,55,5,C_LINE);cv_rect(x+48,y+29,(int32_t)values[i]*55/100,5,accent);}
    }
}
static uint32_t eq_frequency_hz(uint8_t index)
{
    static const uint16_t anchors[11]={20,40,80,160,320,630,1260,2520,5000,10000,20000};
    uint32_t step=index/10u,rem=index%10u;
    if(step>=10u)return 20000u;
    return anchors[step]+((uint32_t)(anchors[step+1u]-anchors[step])*rem)/10u;
}
static void draw_eq_panel(void)
{
    static const char *const bands[3]={"LOW","MID","HIGH"};
    const fm1lab_insert_state_t *v=&current_inserts;
    const fm1lab_eq_band_state_t *b=&v->eq[eq_ui_band];
    uint8_t enabled=(uint8_t)((v->eq_enabled>>(eq_ui_band+1u))&1u);
    char t[16],qtext[8];uint16_t accent=RGB(36,215,232);
    cv_text(6,2,&FONT_XS,"MASTER BUS",C_WHITE);
    cv_text(164,2,&FONT_XS,bands[eq_ui_band],accent);
    cv_text(202,2,&FONT_XS,(v->eq_enabled&1u)?"ON":"BYPASS",(v->eq_enabled&1u)?RGB(75,235,135):C_GRAY);
    for(uint32_t i=0;i<4u;i++){
        int32_t x=4+(int32_t)(i&1u)*118,y=18+(int32_t)(i>>1u)*54;
        instrument_card(x,y,112,48,panel_fill(),accent,accent);
        cv_text(x+7,y+6,&FONT_XS,i==0u?"FREQUENCY":i==1u?"GAIN":i==2u?"Q":"BAND",C_GRAY);
        if(i==0u){int_text((int32_t)eq_frequency_hz(b->frequency),t);text_cat(t,"Hz");}
        else if(i==1u){int_text((int32_t)b->gain-12,t);text_cat(t,"dB");}
        else if(i==2u){
            uint32_t q=5u+(uint32_t)b->q*35u/31u;
            int_text((int32_t)(q/10u),qtext);text_cat(qtext,".");
            int_text((int32_t)(q%10u),qtext+2);
            cv_text(x+7,y+23,&FONT_B,qtext,accent);continue;
        } else {cv_text(x+7,y+23,&FONT_B,enabled?"ON":"OFF",enabled?accent:C_GRAY);continue;}
        cv_text(x+7,y+23,&FONT_B,t,accent);
    }
}
static void draw_compressor_panel(void)
{
    static const char *const labels[4]={"THRESHOLD","RATIO","ATTACK","RELEASE"};
    uint8_t values[4]={compressor_value(FM6_COMP_THRESHOLD),compressor_value(FM6_COMP_RATIO),
        compressor_value(FM6_COMP_ATTACK),compressor_value(FM6_COMP_RELEASE)};
    char t[16];uint16_t accent=RGB(36,215,232);
    cv_text(6,2,&FONT_XS,"MASTER BUS",C_WHITE);
    cv_text(164,2,&FONT_XS,compressor_value(FM6_COMP_ENABLED)?"ACTIVE":"BYPASS",
        compressor_value(FM6_COMP_ENABLED)?RGB(75,235,135):C_GRAY);
    for(uint32_t i=0;i<4u;i++){
        int32_t x=4+(int32_t)(i&1u)*118,y=18+(int32_t)(i>>1u)*54;
        instrument_card(x,y,112,48,panel_fill(),accent,accent);
        cv_text(x+7,y+6,&FONT_XS,labels[i],C_GRAY);
        if(i==1u){uint32_t ratio=1u+values[i]*11u/100u;int_text((int32_t)ratio,t);text_cat(t,":1");}
        else if(i==2u){uint32_t ms=1u+(uint32_t)values[i]*49u/100u;int_text((int32_t)ms,t);text_cat(t,"ms");}
        else if(i==3u){uint32_t ms=20u+(uint32_t)values[i]*480u/100u;int_text((int32_t)ms,t);text_cat(t,"ms");}
        else int_text(values[i],t);
        cv_text(x+7,y+23,&FONT_B,t,accent);
        cv_rect(x+48,y+29,55,5,C_LINE);
        cv_rect(x+48,y+29,(int32_t)values[i]*55/100,5,accent);
    }
    int_text((int32_t)compressor_value(FM6_COMP_MAKEUP)*12/100,t);text_cat(t,"dB");
    cv_text(6,132,&FONT_XS,"MAKEUP",C_GRAY);
    cv_text(56,132,&FONT_XS,t,accent);
}

static void draw_global_panel(uint32_t pct)
{
    char t[24];
    const char *labels[4] = {"MASTER", "OCTAVE", "BLUETOOTH", "MIDI IN"};
    uint16_t colors[4] = {RGB(36,215,232), RGB(227,75,210), RGB(242,212,59), RGB(255,82,101)};
    for (uint32_t i = 0; i < 4u; ++i) {
        int32_t x = 4 + (int32_t)(i & 1u) * 118;
        int32_t y = 4 + (int32_t)(i >> 1u) * 68;
        instrument_card(x, y, 112, 62, panel_fill(), colors[i], colors[i]);
        cv_text(x + 9, y + 8, &FONT_XS, labels[i], C_GRAY);
        if (i == 0) {
            int_text((int32_t)pct, t); text_cat(t, "%");
            cv_text(x + 9, y + 27, &FONT_B, t, colors[i]);
            cv_text(x + 9, y + 47, &FONT_XS, "OUTPUT LEVEL", C_GRAY);
        } else if (i == 1) {
            octave_text(t);
            cv_text(x + 9, y + 27, &FONT_B, t, colors[i]);
            cv_text(x + 9, y + 47, &FONT_XS, "TRANSPOSE", C_GRAY);
        } else if (i == 2) {
            cv_text(x + 9, y + 27, &FONT_B, set.ble_off ? "OFF" : "ON", colors[i]);
            cv_text(x + 9, y + 47, &FONT_XS, plat_ble_connected() ? "CONNECTED" : "WAITING", C_GRAY);
        } else {
            int_text((int32_t)midi_count, t);
            cv_text(x + 9, y + 27, &FONT_B, t, colors[i]);
            cv_text(x + 9, y + 47, &FONT_XS, "MESSAGES RECEIVED", C_GRAY);
        }
    }
}

static void draw_envelope_panel(void)
{
    static const char *const rates[4] = {"RATE 1", "RATE 2", "RATE 3", "RATE 4"};
    static const char *const levels[4] = {"LEVEL 1", "LEVEL 2", "LEVEL 3", "LEVEL 4"};
    char t[16];
    const uint8_t *voice = current_instrument.patches[set.patch].voice;
    int carrier = fm1lab_voice_operator_is_carrier(voice, env_operator);
    uint16_t accent = patch_color(set.patch);
    cv_text(6, 2, &FONT_XS, fm6_patch_name(set.patch), C_WHITE);
    cv_text(140, 2, &FONT_XS, "OPERATOR", C_GRAY);
    int_text(env_operator, t); cv_text(204, 2, &FONT_XS, t, accent);
    cv_text(6, 19, &FONT_XS, carrier ? "CARRIER ENVELOPE" : "MODULATOR ENVELOPE", carrier ? accent : C_GRAY);
    for (uint32_t i = 0; i < 4u; ++i) {
        int32_t x = 4 + (int32_t)(i & 1u) * 118;
        int32_t y = 28 + (int32_t)(i >> 1u) * 56;
        uint8_t value = 0;
        (void)fm1lab_voice_operator_get(voice, env_operator,
            (fm1lab_operator_field_t)((env_levels ? FM1LAB_OP_LEVEL_1 : FM1LAB_OP_RATE_1) + i), &value);
        instrument_card(x, y, 112, 52, panel_fill(), accent, accent);
        cv_text(x + 9, y + 8, &FONT_XS, env_levels ? levels[i] : rates[i], C_WHITE);
        int_text(value, t); cv_text(x + 88, y + 8, &FONT_XS, t, accent);
        cv_rect(x + 9, y + 24, 94, 6, C_LINE);
        cv_rect(x + 9, y + 24, (int32_t)value * 94 / 99, 6, accent);
        cv_text(x + 9, y + 39, &FONT_XS, env_levels ? "0=OFF  99=MAX" : "0=SLOW  99=FAST", C_GRAY);
    }
}

static void draw_lfo_panel(uint32_t modulation)
{
    static const char *const labels[2][4] = {
        {"WAVEFORM", "SPEED", "PITCH DEPTH", "AMP DEPTH"},
        {"DELAY", "PITCH SENS", "LFO KEY SYNC", "OSC KEY SYNC"}
    };
    static const char *const waves[6] = {"TRIANGLE", "SAW DOWN", "SAW UP", "SQUARE", "SINE", "S/HOLD"};
    static const fm1lab_global_field_t fields[2][4] = {
        {FM1LAB_GLOBAL_LFO_WAVE, FM1LAB_GLOBAL_LFO_SPEED, FM1LAB_GLOBAL_LFO_PITCH_DEPTH, FM1LAB_GLOBAL_LFO_AMP_DEPTH},
        {FM1LAB_GLOBAL_LFO_DELAY, FM1LAB_GLOBAL_PITCH_SENS, FM1LAB_GLOBAL_LFO_SYNC, FM1LAB_GLOBAL_OSC_SYNC}
    };
    static const uint8_t maxima[2][4] = {{5,99,99,99},{99,7,1,1}};
    char t[20];
    const uint8_t *voice = current_instrument.patches[set.patch].voice;
    uint16_t accent = patch_color(set.patch);
    cv_text(6, 2, &FONT_XS, fm6_patch_name(set.patch), C_WHITE);
    cv_text(164, 2, &FONT_XS, modulation ? "MODULATION" : "OSCILLATOR", accent);
    for (uint32_t i = 0; i < 4u; ++i) {
        int32_t x = 4 + (int32_t)(i & 1u) * 118;
        int32_t y = 18 + (int32_t)(i >> 1u) * 64;
        uint8_t value = 0;
        (void)fm1lab_voice_global_get(voice, fields[modulation][i], &value);
        instrument_card(x, y, 112, 56, panel_fill(), accent, accent);
        cv_text(x + 9, y + 8, &FONT_XS, labels[modulation][i], C_WHITE);
        if (!modulation && i == 0u) {
            cv_text(x + 9, y + 26, &FONT_B, waves[value % 6u], accent);
        } else {
            if (modulation && i >= 2u) cv_text(x + 9, y + 26, &FONT_B, value ? "ON" : "OFF", accent);
            else {
                int_text(value, t); cv_text(x + 9, y + 26, &FONT_B, t, accent);
                cv_rect(x + 9, y + 46, 94, 5, C_LINE);
                cv_rect(x + 9, y + 46, (int32_t)value * 94 / maxima[modulation][i], 5, accent);
            }
        }
    }
}

static void draw_sequencer_panel(void)
{
    char t[16]; uint8_t lane=seq_ui_lane, step=seq_ui_step;
    cv_text(6,2,&FONT_XS,lane?"LANE B":"LANE A",patch_color(seq_ui_patch[lane]));
    cv_text(130,2,&FONT_XS,fm6_patch_name(seq_ui_patch[lane]),C_WHITE);
    for(uint32_t i=0;i<16u;i++) {
        int32_t x=5+(int32_t)(i%8u)*29, y=12+(int32_t)(i/8u)*49;
        uint8_t s=(uint8_t)((step/16u)*16u+i);
        uint16_t color=seq_ui_enabled[lane][s]?patch_color(seq_ui_patch[lane]):C_LINE;
        rounded_panel(x,y,25,38,4,panel_fill(),s==step?C_WHITE:color);
        int_text(s+1u,t); cv_text(x+5,y+4,&FONT_XS,t,C_GRAY);
        cv_rect(x+4,y+23,17,(int32_t)seq_ui_velocity[lane][s]*12/127, color);
    }
    cv_text(6,116, &FONT_XS, fm6_sequencer_running()?"TRANSPORT RUNNING":"TRANSPORT STOPPED",fm6_sequencer_running()?RGB(75,235,135):C_GRAY);
}
static void draw_arp_panel(void)
{
    char t[16];
    const char *mode=arp_ui_mode==0u?"OFF":arp_ui_mode==1u?"UP":arp_ui_mode==2u?"DOWN":"UP / DOWN";
    instrument_card(4,4,112,62,panel_fill(),RGB(36,215,232),RGB(36,215,232));
    cv_text(13,13,&FONT_XS,"ARPEGGIATOR",C_GRAY); cv_text(13,34,&FONT_B,mode,RGB(36,215,232));
    instrument_card(122,4,112,62,panel_fill(),RGB(227,75,210),RGB(227,75,210));
    cv_text(131,13,&FONT_XS,"CLOCK",C_GRAY); int_text(transport_ui_bpm,t); cv_text(131,34,&FONT_B,t,RGB(227,75,210));
    cv_text(6,82,&FONT_XS,"HOLD NOTES ON THE KEYBED",C_WHITE);
    cv_text(6,101,&FONT_XS,"K1 TEMPO   K2 MODE",C_GRAY);
}
static void draw_fx_panel(uint8_t fx)
{
    const char *names[4]={"CHORUS","DELAY","REVERB","PHASER"};
    uint8_t values[4]={fm6_fx_mix(fx),fm6_fx_time(fx),
        fx==FM6_FX_PHASER?fm6_fx_depth(fx):fm6_fx_feedback(fx),
        fx==FM6_FX_PHASER?fm6_fx_feedback(fx):fm6_fx_get_send(set.patch,fx)};
    char t[16]; uint16_t accent=fx==0u?RGB(36,215,232):fx==1u?RGB(227,75,210):fx==2u?RGB(242,212,59):RGB(85,190,255);
    cv_text(6,2,&FONT_XS,names[fx],C_WHITE);
    cv_text(154,2,&FONT_XS,fm6_fx_enabled(fx)?"ACTIVE":"BYPASS",fm6_fx_enabled(fx)?RGB(75,235,135):RGB(255,82,101));
    for(uint32_t k=0;k<4u;++k) {
        int32_t x=4+(int32_t)(k&1u)*118;
        int32_t y=18+(int32_t)(k>>1u)*54;
        instrument_card(x,y,112,48,panel_fill(),accent,accent);
        cv_text(x+7,y+6,&FONT_XS,k==0u?"K1 WET":k==1u?(fx==0u||fx==3u?"K2 RATE":fx==1u?"K2 TIME":"K2 DECAY"):k==2u?(fx==FM6_FX_PHASER?"K3 DEPTH":"K3 FEEDBACK"):fx==FM6_FX_PHASER?"K4 RESONANCE":"K4 PATCH SEND",C_GRAY);
        int_text(values[k],t);
        cv_text(x+7,y+23,&FONT_B,t,accent);
        cv_rect(x+48,y+29,55,5,C_LINE);
        cv_rect(x+48,y+29,(int32_t)values[k]*55/100,5,accent);
    }
}

static void draw_fx_routing_panel(void)
{
    static const char *const names[FM6_FX_COUNT]={"CHORUS SEND","DELAY SEND","REVERB SEND","PHASER SEND"};
    char t[16];
    uint16_t accent=patch_color(set.patch);
    cv_text(6,2,&FONT_XS,fm6_patch_name(set.patch),C_WHITE);
    cv_text(154,2,&FONT_XS,"SEND ROUTING",accent);
    for(uint32_t fx=0;fx<FM6_FX_COUNT;++fx) {
        uint8_t value=fm6_fx_get_send(set.patch,(uint8_t)fx);
        int32_t x=4+(int32_t)(fx&1u)*118;
        int32_t y=18+(int32_t)(fx>>1u)*54;
        uint16_t color=fx==0u?RGB(36,215,232):fx==1u?RGB(227,75,210):fx==2u?RGB(242,212,59):RGB(85,190,255);
        instrument_card(x,y,112,48,panel_fill(),color,color);
        cv_text(x+7,y+6,&FONT_XS,names[fx],C_GRAY);
        int_text(value,t);
        cv_text(x+7,y+23,&FONT_B,t,color);
        cv_rect(x+48,y+29,55,5,C_LINE);
        cv_rect(x+48,y+29,(int32_t)value*55/100,5,color);
    }
}

static void draw(void)
{
    char t[24];
    uint32_t now = plat_ms(), pct = plat_master() * 100u / 4096u;
    uint16_t accent = patch_color(set.patch);
    cv_begin(240, 37, screen_bg());

    /* Global status strip: tempo, physical battery gauge, and active arp. */
    int_text(transport_ui_bpm, t);
    text_cat(t, " BPM");
    cv_text(4, 1, &FONT_XS, t, C_GRAY);

    int battery = plat_battery(); /* 0..3 battery bars, 4 USB powered, -1 unknown */
    cv_rect(183, 2, 23, 7, C_GRAY);
    cv_rect(206, 4, 2, 3, C_GRAY);
    if (battery >= 0) {
        uint32_t bars = battery > 3 ? 4u : (uint32_t)battery;
        if (bars) cv_rect(185, 4, (int32_t)(bars * 5u), 3,
                          battery >= 3 ? RGB(75,235,135) : accent);
    }
    if (arp_ui_mode) cv_text(216, 1, &FONT_XS, "ARP", RGB(75,235,135));

    /* The second line is reserved for the instrument name, on every page. */
    for (uint32_t n = 0; n < FM1LAB_INSTRUMENT_NAME_BYTES; ++n) {
        char ch = current_instrument.name[n];
        t[n] = (ch >= 32 && ch <= 126) ? ch : ' ';
    }
    t[FM1LAB_INSTRUMENT_NAME_BYTES < sizeof t ? FM1LAB_INSTRUMENT_NAME_BYTES : sizeof t - 1u] = 0;
    cv_text(8, 15, &FONT_B, t, C_WHITE);
    cv_rect(0, 34, 240, 3, accent);
    cv_blit(0, 0);
    cv_begin(240, 180, screen_bg());
    if (page == PAGE_HOME) draw_home_panel();
    else if (page == PAGE_MACRO) draw_macro_panel();
    else if (page == PAGE_FX_AMP) draw_amp_panel();
    else if (page == PAGE_FX_EQ) draw_eq_panel();
    else if (page == PAGE_FX_COMPRESSOR) draw_compressor_panel();
    else if (page == PAGE_OPERATOR_MAP) draw_operator_map();
    else if (page == PAGE_ENV) draw_envelope_panel();
    else if (page == PAGE_LFO) draw_lfo_panel(0);
    else if (page == PAGE_LFO_MOD) draw_lfo_panel(1);
    else if (page == PAGE_SEQ) draw_sequencer_panel();
    else if (page == PAGE_ARP) draw_arp_panel();
    else if (page == PAGE_FX_CHORUS) draw_fx_panel(0u);
    else if (page == PAGE_FX_DELAY) draw_fx_panel(1u);
    else if (page == PAGE_FX_REVERB) draw_fx_panel(2u);
    else if (page == PAGE_FX_PHASER) draw_fx_panel(3u);
    else if (page == PAGE_FX_ROUTING) draw_fx_routing_panel();
    else draw_global_panel(pct);
    cv_blit(0, 41);

    /* Compact control strip. HOME and ENV already show these values in
     * their main panels, so avoid duplicating them in a second bar. */
    cv_begin(240, 19, screen_bg());
    int_text((int32_t)page + 1, t);
    text_cat(t, "/");
    int_text(PAGE_COUNT, bank_label);
    text_cat(t, bank_label);
    cv_text(240 - text_w(&FONT_XS, t) - 6, 4, &FONT_XS, t, C_GRAY);
    cv_blit(0, 221);
}

void app_ui_init(void)
{
    touch_on(1);                             /* velocity and pressure: the keys' readings on */
    lcd_fill(0, 0, 240, 240, screen_bg());
    ui.redraw = 1;
}

void app_frame(void)
{
    static uint32_t last_pct, lit_until, last_ble, last_midi;
    uint32_t i, now = plat_ms(), pct = plat_master() * 100u / 4096u;
    input();
    if (performance_update_dirty && !performance_update_pending) performance_publish();
    speak();
    for (i = 0; i < NE; i++)                 /* an encoder's light goes out */
        if (ui.turned_at[i] && now - ui.turned_at[i] < 400u && now > lit_until)
            lit_until = ui.turned_at[i] + 300u;
    if (lit_until && now >= lit_until) {
        lit_until = 0;
        ui.redraw = 1;
    }
    if (FM1B_BLE_CENTRAL) {                  /* looking: a device found is named at once (REC connects to it) */
        static int found;
        int n = ui.looking ? btdev_count() : 0, ev;
        btdev_page(ui.looking && !set.ble_off);
        if (n > found && btdev_pick() < n)
            ble_device_say();
        found = n;
        while ((ev = btdev_event()) != BTDEV_NONE)   /* a device connected, given up or lost */
            if (ev == BTDEV_CONNECTED || ev == BTDEV_NO_ANSWER || ev == BTDEV_LOST)
                app_say("DEVICE", btdev_word(ev));
    }
    if (midi_count != last_midi) {
        last_midi = midi_count;
        ui.redraw = 1;
    }
    if (pct != last_pct || (uint32_t)(plat_ble_connected() | plat_ble_device() << 1) != last_ble ||
        (ui.msg_until && now >= ui.msg_until)) {
        last_pct = pct;
        last_ble = (uint32_t)(plat_ble_connected() | plat_ble_device() << 1);
        if (now >= ui.msg_until)
            ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (ui.redraw) {
        ui.redraw = 0;
        draw();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "octave"))
        *got = set.octave;
    else if (!strcmp(what, "patch") || !strcmp(what, "wave"))
        *got = set.patch;
    else if (!strcmp(what, "note"))
        *got = voice_note;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "page"))
        *got = page;
    else if (!strcmp(what, "cpu"))
        *got = (int)app_audio_cpu_pct();
    else if (!strcmp(what, "xruns"))
        *got = (int)app_audio_xruns();
    else if (!strcmp(what, "shed"))
        *got = (int)app_audio_overload_shed_count();
    else if (!strcmp(what, "vel"))           /* the last key's velocity and pressure */
        *got = ui.vel[ui.last_key];
    else if (!strcmp(what, "prs"))
        *got = ui.prs[ui.last_key];
    else if (!strncmp(what, "turns", 5))    /* turnsN: encoder N's count (plat.h EN_*) */
        *got = ui.turns[atoi(what + 5) % NE];
    else
        return 0;
    return 1;
}
#endif
