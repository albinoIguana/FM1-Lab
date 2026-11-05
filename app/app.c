/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* FM1-Lab native foundation bring-up app: every control on the screen as it arrives, by the name the app knows it by (plat.h B_*, EN_*,
 * the keys), and a few of them doing something: the keys play a voice and go out over USB-MIDI (MIDI in plays it
 * too), OCT- / OCT+ set the octave, PRESET the wave, MASTER the volume, SAVE stores the settings; the keys are
 * played with velocity and pressure (lib/touch.h), shown on them. One of each thing the platform offers, kept short. Part of the unity build (device/unity.c, host/plat_host.c), after the platform:
 * gfx.c and announce.c are in scope. */
#include "fm6.h"

#define SET_MAGIC_V1 0x4C314D46u
#define SET_MAGIC    0x4C314D47u
typedef struct {
    uint32_t magic;
    int8_t octave;
    uint8_t patch, enabled_mask, ble_off, rsv[1];
} settings_v1_t;
typedef struct {
    uint32_t magic;
    int8_t octave;
    uint8_t patch, enabled_mask, ble_off;
    uint8_t macros[FM1LAB_PATCH_COUNT][FM1LAB_MACRO_COUNT];
    uint8_t levels[FM1LAB_PATCH_COUNT];
} settings_t;
static settings_t set;
static uint8_t set_dirty;
enum { PAGE_HOME = 0, PAGE_MACRO, PAGE_GLOBAL };
static uint8_t page = PAGE_HOME;

/* the controls by their names in plat.h, in its order */
static const char *const BTN_NAME[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                         "PLAY", "REC", "OCT-", "OCT+"};
static const char *const ENC_NAME[NE] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};

/* ------------------------------------------------------------------ voice --- */
/* one voice, the last note wins; the main loop and MIDI in set it, the audio interrupt plays it */
static volatile int32_t voice_note = -1;     /* last MIDI/key note shown in the header */
enum { EV_NOTE_ON = 1, EV_NOTE_OFF, EV_PANIC, EV_PATCH_ENABLE, EV_MACRO_SET, EV_PATCH_LEVEL };
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

static void ui_events_drain(void)
{
    uint8_t tail;
    if (ui_event_overflow) {
        ui_event_tail = ui_event_head;
        ui_event_overflow = 0;
        fm6_panic();
        for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
            fm6_patch_set_enabled(p, (set.enabled_mask >> p) & 1u);
            fm6_patch_set_level(p, set.levels[p]);
            for (uint32_t m = 0; m < FM1LAB_MACRO_COUNT; ++m)
                fm6_macro_set(p, m, set.macros[p][m]);
        }
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
        else if (type == EV_PATCH_ENABLE)
            fm6_patch_set_enabled(a, b);
        else if (type == EV_MACRO_SET)
            fm6_macro_set(a, b, c);
        else if (type == EV_PATCH_LEVEL)
            fm6_patch_set_level(a, b);
    }
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
static int settings_v1_valid(const settings_v1_t *v)
{
    return v->magic == SET_MAGIC_V1 && v->octave >= -2 && v->octave <= 2 &&
        v->patch < FM1LAB_PATCH_COUNT && !(v->enabled_mask & 0xF0u) && v->ble_off <= 1u;
}

static int settings_valid(const settings_t *v)
{
    if (v->magic != SET_MAGIC || v->octave < -2 || v->octave > 2 ||
        v->patch >= FM1LAB_PATCH_COUNT || (v->enabled_mask & 0xF0u) || v->ble_off > 1u)
        return 0;
    for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        if (v->levels[p] > 127u) return 0;
        for (uint32_t m = 0; m < FM1LAB_MACRO_COUNT; ++m)
            if (v->macros[p][m] > 100u) return 0;
    }
    return 1;
}

static void settings_defaults(void)
{
    set.magic = SET_MAGIC;
    set.octave = 0;
    set.patch = 0;
    set.enabled_mask = 1u;
    set.ble_off = 0;
    for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        set.levels[p] = 127u;
        for (uint32_t m = 0; m < FM1LAB_MACRO_COUNT; ++m)
            set.macros[p][m] = 50u;
    }
}

void app_init(void)
{
    settings_v1_t old;
    int loaded;
    fm6_init();
    loaded = plat_store_load(OBJ_SETTINGS, &set, sizeof set);
    if (loaded != (int)sizeof set || !settings_valid(&set)) {
        if (plat_store_load(OBJ_SETTINGS, &old, sizeof old) == (int)sizeof old &&
            settings_v1_valid(&old)) {
            settings_defaults();
            set.octave = old.octave;
            set.patch = old.patch;
            set.enabled_mask = old.enabled_mask;
            set.ble_off = old.ble_off;
            set_dirty = 1u; /* save once to migrate the previous compact settings record */
        } else {
            settings_defaults();
        }
    }
    page = PAGE_HOME;
    for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        fm6_patch_set_enabled(p, (set.enabled_mask >> p) & 1u);
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
            app_say("GLOBAL", page == PAGE_GLOBAL ? "SETTINGS" : "HOME");
        } else if (k == B_SEL) {
            if (page == PAGE_HOME) {
                page = PAGE_MACRO;
                app_say("MACROS", fm6_patch_name(set.patch));
            }
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
        } else if (k == EN_SELECT) {
            int32_t next = ((int32_t)set.patch + e) % (int32_t)FM1LAB_PATCH_COUNT;
            if (next < 0) next += (int32_t)FM1LAB_PATCH_COUNT;
            set.patch = (uint8_t)next;
            set_dirty = 1;
            app_say("PATCH", fm6_patch_name(set.patch));
        } else if (k == EN_PRESET) {
            app_say("INSTRUMENT", "LIBRARY NOT READY");
        } else if (k >= EN_K1 && k <= EN_K4) {
            uint32_t index = (uint32_t)k - EN_K1;
            if (page == PAGE_MACRO) {
                int32_t value = (int32_t)set.macros[set.patch][index] + e;
                if (value < 0) value = 0;
                if (value > 100) value = 100;
                set.macros[set.patch][index] = (uint8_t)value;
                ui_event_push(EV_MACRO_SET, set.patch, (uint8_t)index, (uint8_t)value);
                set_dirty = 1;
                app_say("MACRO", "UPDATED");
            } else if (page == PAGE_HOME) {
                int32_t value = (int32_t)set.levels[index] + e;
                if (value < 0) value = 0;
                if (value > 127) value = 127;
                set.levels[index] = (uint8_t)value;
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

static void draw_home_panel(void)
{
    char t[24];
    for (uint32_t p = 0; p < FM1LAB_PATCH_COUNT; ++p) {
        int32_t x = 4 + (int32_t)(p & 1u) * 118;
        int32_t y = 4 + (int32_t)(p >> 1) * 68;
        uint16_t accent = p == set.patch ? C_HI : C_LINE;
        int enabled = (set.enabled_mask >> p) & 1u;
        cv_rect(x, y, 112, 62, C_BLACK);
        outline(x, y, 112, 62, accent);
        cv_text(x + 6, y + 6, &FONT_XS, fm6_patch_name(p), C_WHITE);
        cv_text(x + 6, y + 24, &FONT_XS, enabled ? "ACTIVE" : "MUTED", enabled ? C_HI : C_DIM);
        cv_text(x + 58, y + 24, &FONT_XS, "ALG", C_GRAY);
        int_text((int32_t)fm6_patch_algorithm(p) + 1, t);
        cv_text(x + 82, y + 24, &FONT_XS, t, C_WHITE);
        cv_text(x + 6, y + 42, &FONT_XS, "LVL", C_GRAY);
        int_text(set.levels[p], t);
        cv_text(x + 32, y + 42, &FONT_XS, t, C_WHITE);
        cv_rect(x + 54, y + 45, 50, 6, C_LINE);
        cv_rect(x + 54, y + 45, (int32_t)set.levels[p] * 50 / 127, 6, C_HI);
    }
    cv_text(4, 138, &FONT_XS, "SELECT: PATCH   SEL: MACROS   GLO: GLOBAL", C_GRAY);
}

static void draw_macro_panel(void)
{
    static const char *const names[FM1LAB_MACRO_COUNT] = {"BRIGHTNESS", "BODY", "MOTION", "EDGE"};
    char t[24];
    cv_text(6, 2, &FONT_XS, fm6_patch_name(set.patch), C_WHITE);
    cv_text(164, 2, &FONT_XS, "ALGO", C_GRAY);
    int_text((int32_t)fm6_patch_algorithm(set.patch) + 1, t);
    cv_text(204, 2, &FONT_XS, t, C_HI);
    for (uint32_t m = 0; m < FM1LAB_MACRO_COUNT; ++m) {
        int32_t y = 28 + (int32_t)m * 27;
        uint32_t value = set.macros[set.patch][m];
        cv_text(6, y, &FONT_XS, names[m], C_WHITE);
        int_text((int32_t)value, t);
        cv_text(78, y, &FONT_XS, t, C_HI);
        cv_rect(108, y + 2, 124, 8, C_LINE);
        cv_rect(108, y + 2, (int32_t)value * 124 / 100, 8, C_HI);
        cv_rect(169, y, 2, 12, C_WHITE); /* neutral midpoint at 50 */
    }
    cv_text(4, 138, &FONT_XS, "K1 BRIGHT  K2 BODY  K3 MOTION  K4 EDGE", C_GRAY);
}

static void draw_global_panel(uint32_t pct)
{
    char t[24];
    cv_text(8, 8, &FONT_XS, "MASTER", C_GRAY);
    int_text((int32_t)pct, t); text_cat(t, "%");
    cv_text(90, 8, &FONT_XS, t, C_HI);
    cv_text(8, 34, &FONT_XS, "OCTAVE", C_GRAY);
    octave_text(t);
    cv_text(90, 34, &FONT_XS, t, C_WHITE);
    cv_text(8, 60, &FONT_XS, "BLUETOOTH", C_GRAY);
    cv_text(90, 60, &FONT_XS, set.ble_off ? "OFF" : "ON", set.ble_off ? C_DIM : C_HI);
    cv_text(8, 86, &FONT_XS, "BLE MIDI", C_GRAY);
    cv_text(90, 86, &FONT_XS, plat_ble_connected() ? "CONNECTED" : "WAITING", plat_ble_connected() ? C_HI : C_DIM);
    cv_text(8, 112, &FONT_XS, "MIDI IN", C_GRAY);
    int_text((int32_t)midi_count, t);
    cv_text(90, 112, &FONT_XS, t, C_WHITE);
    cv_text(4, 138, &FONT_XS, "REC HOLD: BLUETOOTH   SAVE: STORE SETTINGS", C_GRAY);
}

static void draw(void)
{
    char t[24];
    int32_t note = voice_note;
    uint32_t i, now = plat_ms(), pct = plat_master() * 100u / 4096u;
    static const uint8_t WK[16] = PLAT_WHITE_KEYS, BK[11] = PLAT_BLACK_KEYS;
    /* the header: the name and the note, or a message for a moment */
    cv_begin(240, 32, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], C_HI);
    } else {
        const char *title = page == PAGE_MACRO ? "PATCH MACROS" :
            page == PAGE_GLOBAL ? "GLOBAL SETTINGS" : APP_NAME;
        int32_t x = cv_text(10, 6, &FONT_B, title, C_WHITE);
        if (FM1B_BLE)                        /* Bluetooth: grey while it waits, lit while a computer is connected */
            x = cv_text(x + 8, 10, &FONT_XS, "BLE", plat_ble_connected() ? C_HI : C_DIM);
        if (FM1B_BLE_CENTRAL)                /* a Bluetooth MIDI device: lit while connected */
            cv_text(x + 6, 10, &FONT_XS, "DEV", plat_ble_device() == BLE_DEV_READY ? C_HI : C_DIM);
        if (note >= 0) {                     /* the note, and the last key's velocity and pressure */
            int32_t x;
            note_name(note, t);
            x = 230 - text_w(&FONT_B, t);
            cv_text(x, 6, &FONT_B, t, C_HI);
            if (ui.keys >> ui.last_key & 1u) {
                char n[24] = "V ";
                int_text(ui.vel[ui.last_key], t);
                text_cat(n, t);
                text_cat(n, "  P ");
                int_text(ui.prs[ui.last_key], t);
                text_cat(n, t);
                cv_text(x - 12 - text_w(&FONT_XS, n), 10, &FONT_XS, n, C_GRAY);
            }
        }
    }
    cv_blit(0, 0);
    /* HOME tiles, the selected patch's four performance macros, or global settings. */
    cv_begin(240, 144, C_BLACK);
    if (page == PAGE_HOME) draw_home_panel();
    else if (page == PAGE_MACRO) draw_macro_panel();
    else draw_global_panel(pct);
    cv_blit(0, 36);
    /* the keys, black above white: a held key fills to its velocity (amber), its pressure over that (white) */
    cv_begin(240, 60, C_BLACK);
    for (i = 0; i < 16u; i++)
        key_cell(4 + (int32_t)i * 14, 14, 13, 14, WK[i], C_DIM);
    for (i = 0; i < 11u; i++) {
        static const uint8_t AT[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};   /* the white key each one follows */
        key_cell(4 + (int32_t)AT[i] * 14 + 8, 0, 11, 12, BK[i], C_LINE);
    }
    octave_text(t);
    if (page == PAGE_HOME) {
        cv_text(4, 36, &FONT_XS, "OCT", C_GRAY);
        cv_text(30, 36, &FONT_XS, t, C_HI);
        cv_text(64, 36, &FONT_XS, "PATCH", C_GRAY);
        cv_text(112, 36, &FONT_XS, fm6_patch_name(set.patch), C_HI);
        cv_text(202, 36, &FONT_XS, (set.enabled_mask >> set.patch) & 1u ? "ON" : "OFF",
                (set.enabled_mask >> set.patch) & 1u ? C_HI : C_DIM);
    } else if (page == PAGE_MACRO) {
        cv_text(4, 36, &FONT_XS, "MACROS", C_GRAY);
        cv_text(64, 36, &FONT_XS, fm6_patch_name(set.patch), C_HI);
        cv_text(178, 36, &FONT_XS, "NEUTRAL", C_GRAY);
        cv_text(218, 36, &FONT_XS, "50", C_HI);
    } else {
        cv_text(4, 36, &FONT_XS, "GLOBAL", C_GRAY);
        cv_text(64, 36, &FONT_XS, "SETTINGS", C_HI);
        cv_text(178, 36, &FONT_XS, "VOL", C_GRAY);
        int_text((int32_t)pct, t); text_cat(t, "%");
        cv_text(204, 36, &FONT_XS, t, C_HI);
    }
    if (midi_count) {                        /* the last MIDI message in, and where from */
        static const char *const PORT[NPORTS] = {"USB", "BLE", "TRS"};
        static const char HEX[] = "0123456789ABCDEF";
        uint32_t m = midi_last, b;
        char h[12];
        for (b = 0; b < 3u; b++) {
            uint32_t v = (m >> (8u + 8u * b)) & 0xFFu;
            h[3 * b] = HEX[v >> 4];
            h[3 * b + 1] = HEX[v & 15u];
            h[3 * b + 2] = b < 2u ? ' ' : 0;
        }
        cv_text(4, 48, &FONT_XS, "MIDI IN", C_GRAY);
        cv_text(52, 48, &FONT_XS, midi_last_port < NPORTS ? PORT[midi_last_port] : "", C_HI);
        cv_text(82, 48, &FONT_XS, h, C_WHITE);
    }
    /* Persistent, low-profile page index in the display's bottom-right corner. */
    {
        char page_text[8];
        int_text((int32_t)page + 1, page_text);
        text_cat(page_text, "/3");
        cv_text(218, 52, &FONT_XS, page_text, C_GRAY);
    }
    cv_blit(0, 180);
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
    else if (!strcmp(what, "page"))
        *got = page;
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
