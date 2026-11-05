/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* FM1-Lab native foundation bring-up app: every control on the screen as it arrives, by the name the app knows it by (plat.h B_*, EN_*,
 * the keys), and a few of them doing something: the keys play a voice and go out over USB-MIDI (MIDI in plays it
 * too), OCT- / OCT+ set the octave, PRESET the wave, MASTER the volume, SAVE stores the settings; the keys are
 * played with velocity and pressure (lib/touch.h), shown on them. One of each thing the platform offers, kept short. Part of the unity build (device/unity.c, host/plat_host.c), after the platform:
 * gfx.c and announce.c are in scope. */
#include "fm6.h"

#define SET_MAGIC 0x4C314D46u            /* "FM1L": native Instrument settings */
#define INST_STORE_MAGIC 0x31494D46u      /* "FM1I" */
#define INST_STORE_VERSION 1u
#define INST_CHUNKS 3u
#define INST_HEADER_BYTES 8u
#define INST_CHUNK_MAX_RECORDS 6u
typedef struct {
    uint32_t magic;
    int8_t octave;                       /* -2 .. +2 */
    uint8_t instrument;                  /* selected user Instrument, 0..15 */
    uint8_t patch;                       /* selected patch inside the Instrument, 0..3 */
    uint8_t ble_off;                     /* Bluetooth switched off (REC held); 0, the default: on */
    uint8_t rsv[3];
} settings_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t first;
    uint8_t count;
} instrument_chunk_header_t;

static fm1lab_instrument_record_t instrument_cache[FM1LAB_INSTRUMENT_COUNT];
static fm1lab_instrument_record_t save_snapshot;
static volatile uint8_t snapshot_ready;

static void app_copy_bytes(void *dst, const void *src, uint32_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--)
        *d++ = *s++;
}

static void instrument_store_load(void)
{
    static const uint8_t first[INST_CHUNKS] = {0u, 6u, 11u};
    static const uint8_t count[INST_CHUNKS] = {6u, 5u, 5u};
    uint8_t buf[INST_HEADER_BYTES + INST_CHUNK_MAX_RECORDS * sizeof(fm1lab_instrument_record_t)];
    uint32_t chunk, i;
    app_copy_bytes(instrument_cache, instrument_cache, 0u);
    for (chunk = 0; chunk < INST_CHUNKS; chunk++) {
        int n = plat_store_load(OBJ_INSTRUMENTS0 + chunk, buf, sizeof(buf));
        instrument_chunk_header_t h;
        if (n != (int)(INST_HEADER_BYTES + count[chunk] * sizeof(fm1lab_instrument_record_t)))
            continue;
        app_copy_bytes(&h, buf, sizeof(h));
        if (h.magic != INST_STORE_MAGIC || h.version != INST_STORE_VERSION
            || h.first != first[chunk] || h.count != count[chunk])
            continue;
        for (i = 0; i < count[chunk]; i++) {
            uint32_t slot = first[chunk] + i;
            app_copy_bytes(&instrument_cache[slot],
                           buf + INST_HEADER_BYTES + i * sizeof(fm1lab_instrument_record_t),
                           sizeof(fm1lab_instrument_record_t));
            if (instrument_cache[slot].valid)
                fm6_instrument_load(slot, &instrument_cache[slot]);
        }
    }
}

static int instrument_store_save(uint32_t slot, const fm1lab_instrument_record_t *record)
{
    static const uint8_t first[INST_CHUNKS] = {0u, 6u, 11u};
    static const uint8_t count[INST_CHUNKS] = {6u, 5u, 5u};
    uint8_t buf[INST_HEADER_BYTES + INST_CHUNK_MAX_RECORDS * sizeof(fm1lab_instrument_record_t)];
    instrument_chunk_header_t h;
    uint32_t chunk, i;
    if (slot >= FM1LAB_INSTRUMENT_COUNT || !record)
        return 0;
    chunk = slot < 6u ? 0u : slot < 11u ? 1u : 2u;
    app_copy_bytes(&instrument_cache[slot], record, sizeof(*record));
    instrument_cache[slot].valid = 1;
    h.magic = INST_STORE_MAGIC;
    h.version = INST_STORE_VERSION;
    h.first = first[chunk];
    h.count = count[chunk];
    app_copy_bytes(buf, &h, sizeof(h));
    for (i = 0; i < count[chunk]; i++)
        app_copy_bytes(buf + INST_HEADER_BYTES + i * sizeof(fm1lab_instrument_record_t),
                       &instrument_cache[first[chunk] + i], sizeof(fm1lab_instrument_record_t));
    return plat_store_save(OBJ_INSTRUMENTS0 + chunk, buf,
                           INST_HEADER_BYTES + count[chunk] * sizeof(fm1lab_instrument_record_t)) == 0;
}
static settings_t set;
static uint8_t set_dirty;                /* changed since the last save: SAVE is lit */

/* the controls by their names in plat.h, in its order */
static const char *const BTN_NAME[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                         "PLAY", "REC", "OCT-", "OCT+"};
static const char *const ENC_NAME[NE] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};

/* ------------------------------------------------------------------ voice --- */
/* one voice, the last note wins; the main loop and MIDI in set it, the audio interrupt plays it */
static volatile int32_t voice_note = -1;     /* last MIDI/key note shown in the header */
enum {
    EV_NOTE_ON = 1, EV_NOTE_OFF, EV_PANIC, EV_PATCH_ENABLE,
    EV_SELECT_INSTRUMENT, EV_MACRO, EV_ALGORITHM, EV_NAME_CHAR,
    EV_SNAPSHOT, EV_MARK_SAVED
};
typedef struct { volatile uint8_t type, a, b; } ui_event_t;
static volatile ui_event_t ui_events[64];
static volatile uint8_t ui_event_head, ui_event_tail, ui_event_overflow;

/* Main-loop controls never write FM voice state directly. They publish compact events;
 * the audio interrupt is the sole runtime writer of the synthesizer state. */
static void ui_event_push(uint8_t type, uint8_t a, uint8_t b)
{
    uint8_t head = ui_event_head, next = (uint8_t)((head + 1u) & 63u);
    if (next == ui_event_tail) {
        ui_event_overflow = 1;
        return;
    }
    ui_events[head].type = type;
    ui_events[head].a = a;
    ui_events[head].b = b;
    ui_event_head = next;
}

static void ui_events_drain(void)
{
    uint8_t tail;
    if (ui_event_overflow) {
        ui_event_tail = ui_event_head;
        ui_event_overflow = 0;
        fm6_panic();
        return;
    }
    while ((tail = ui_event_tail) != ui_event_head) {
        uint8_t type = ui_events[tail].type, a = ui_events[tail].a, b = ui_events[tail].b;
        ui_event_tail = (uint8_t)((tail + 1u) & 63u);
        if (type == EV_NOTE_ON)
            fm6_note_on(a, b);
        else if (type == EV_NOTE_OFF)
            fm6_note_off(a);
        else if (type == EV_PANIC)
            fm6_panic();
        else if (type == EV_PATCH_ENABLE)
            fm6_patch_set_enabled(a, b);
        else if (type == EV_SELECT_INSTRUMENT)
            fm6_select_instrument(a);
        else if (type == EV_MACRO)
            fm6_macro_set(a >> 2, a & 3u, b);
        else if (type == EV_ALGORITHM)
            fm6_patch_set_algorithm(a, b);
        else if (type == EV_NAME_CHAR)
            fm6_instrument_set_name_char(a, b);
        else if (type == EV_SNAPSHOT) {
            fm6_instrument_export_current(&save_snapshot);
            snapshot_ready = 1;
        } else if (type == EV_MARK_SAVED)
            fm6_instrument_mark_saved(a);
    }
}

static void note_on(int32_t note, uint32_t vel)
{
    voice_note = note;
    ui_event_push(EV_NOTE_ON, (uint8_t)note, (uint8_t)vel);
}
static void note_off(int32_t note)
{
    ui_event_push(EV_NOTE_OFF, (uint8_t)note, 0);
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

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    midi_in();
    ui_events_drain();
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
    set.magic = SET_MAGIC;
    set.octave = 0;
    set.patch = 0;
    set.enabled_mask = 1u;
    set.ble_off = 0;
}

void app_init(void)
{
    uint32_t i;
    fm6_init();
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC
        || set.octave < -2 || set.octave > 2 || set.patch >= FM1LAB_PATCH_COUNT
        || (set.enabled_mask & 0xF0u) || set.ble_off > 1u)
        settings_defaults();
    for (i = 0; i < FM1LAB_PATCH_COUNT; i++)
        fm6_patch_set_enabled(i, (set.enabled_mask >> i) & 1u);
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
        text_cat(t, ". PATCH ");
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
    ui_event_push(EV_PANIC, 0, 0);
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
        else if (k == B_SEL) {
            set.enabled_mask ^= (uint8_t)(1u << set.patch);
            ui_event_push(EV_PATCH_ENABLE, set.patch, (set.enabled_mask >> set.patch) & 1u);
            set_dirty = 1;
            app_say(fm6_patch_name(set.patch), fm6_patch_enabled(set.patch) ? "ON" : "OFF");
        } else if (k == B_SAVE) {
            if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) {
                set_dirty = 0;
                app_say("SAVED", "");
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
         } else if (k == EN_PRESET) {
            int32_t next = ((int32_t)set.patch + e) % (int32_t)FM1LAB_PATCH_COUNT;
            if (next < 0)
                next += (int32_t)FM1LAB_PATCH_COUNT;
            set.patch = (uint8_t)next;
            set_dirty = 1;
            app_say("PATCH", fm6_patch_name(set.patch));
        } else {
            int_text(ui.turns[k], t);
            app_say(ENC_NAME[k], t);
        }
    }
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

static void draw_patch_card(uint32_t patch, int32_t x, int32_t y)
{
    uint32_t op;
    int selected = patch == set.patch;
    int enabled = fm6_patch_enabled(patch);
    uint16_t edge = selected ? C_HI : C_LINE;
    char alg[12] = "ALG ";
    int_text(fm6_patch_algorithm(patch), alg + 4);

    cv_rect(x, y, 112, 52, selected ? C_LINE : C_BLACK);
    cv_line(x, y, x + 111, y, edge);
    cv_line(x, y + 51, x + 111, y + 51, edge);
    cv_line(x, y, x, y + 51, edge);
    cv_line(x + 111, y, x + 111, y + 51, edge);
    if (selected)
        cv_rect(x + 3, y + 4, 2, 43, C_HI);

    cv_text(x + 8, y + 5, &FONT_B, fm6_patch_name(patch), enabled ? C_WHITE : C_DIM);
    cv_text(x + 8, y + 23, &FONT_XS, alg, C_GRAY);
    cv_rect(x + 82, y + 5, 24, 13, enabled ? C_HI : C_DIM);
    cv_text(x + 86, y + 6, &FONT_XS, enabled ? "ON" : "OFF", C_BLACK);

    /* Six small operator tiles: bright = carrier, muted = modulator. */
    for (op = 1; op <= 6; op++) {
        uint16_t col = fm6_operator_is_carrier(patch, op) ? C_HI : C_DIM;
        cv_rect(x + 8 + (int32_t)(op - 1u) * 15, y + 39, 9, 7, col);
    }
}

static void draw(void)
{
    char t[24];
    int32_t note = voice_note;
    uint32_t now = plat_ms(), pct = plat_master() * 100u / 4096u, i;
    static const char *const MACRO_NAME[4] = {"BRIGHT", "BODY", "MOTION", "DIRT"};

    /* Header and the 2x2 patch matrix. */
    cv_begin(240, 144, C_BLACK);
    cv_text(7, 5, &FONT_B, APP_NAME, C_WHITE);
    cv_text(104, 8, &FONT_XS, "FOUR-PATCH FM", C_HI);
    if (FM1B_BLE)
        cv_text(194, 8, &FONT_XS, "BLE", plat_ble_connected() ? C_HI : C_DIM);
    if (now < ui.msg_until) {
        cv_rect(4, 23, 232, 17, C_LINE);
        cv_text(9, 24, &FONT_XS, ui.msg[0], C_WHITE);
        cv_text(120, 24, &FONT_XS, ui.msg[1], C_HI);
    } else {
        cv_line(4, 22, 236, 22, C_LINE);
        if (note >= 0) {
            note_name(note, t);
            cv_text(196, 5, &FONT_XS, t, C_HI);
        }
    }
    draw_patch_card(0, 6, 29);
    draw_patch_card(1, 122, 29);
    draw_patch_card(2, 6, 86);
    draw_patch_card(3, 122, 86);
    cv_blit(0, 0);

    /* Four topology-aware macros and the two-lane step display. */
    cv_begin(240, 96, C_BLACK);
    cv_text(6, 3, &FONT_XS, "MACROS", C_GRAY);
    int_text((int32_t)pct, t);
    text_cat(t, "%");
    cv_text(204, 3, &FONT_XS, t, C_HI);
    for (i = 0; i < 4; i++) {
        int32_t x = 6 + (int32_t)i * 59;
        uint32_t value = fm6_macro_get(set.patch, i);
        cv_text(x, 17, &FONT_XS, MACRO_NAME[i], C_GRAY);
        int_text((int32_t)value, t);
        cv_text(x, 30, &FONT_B, t, C_WHITE);
        cv_rect(x, 43, 48, 4, C_LINE);
        cv_rect(x, 43, (int32_t)value * 48 / 100, 4, C_HI);
    }
    cv_line(4, 54, 236, 54, C_LINE);
    cv_text(6, 58, &FONT_XS, "SEQUENCER", C_GRAY);
    cv_text(86, 58, &FONT_XS, "2 LANES", C_HI);
    cv_text(190, 58, &FONT_XS, "STOPPED", C_DIM);
    for (i = 0; i < 16; i++) {
        int32_t x = 37 + (int32_t)i * 12;
        cv_rect(x, 72, 8, 7, C_LINE);
        cv_rect(x, 85, 8, 7, C_LINE);
    }
    cv_text(6, 70, &FONT_XS, "A", C_HI);
    cv_text(6, 83, &FONT_XS, "B", C_HI);
    cv_blit(0, 144);
}

void app_ui_init(void)
{
    touch_on(1);                             /* velocity and pressure: the keys' readings on */
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
}

void app_frame(void)
{
    static uint32_t last_pct, lit_until, last_ble, last_midi;
    uint32_t i, now = plat_ms(), pct = plat_master() * 100u / 4096u;
    input();
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
