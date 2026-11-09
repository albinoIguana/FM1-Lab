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
#include "fm1lab_sequencer_record.h"
#include "fm1lab_instrument.h"
#include "fm1lab_voice_editor.h"
#include "fm1lab_store_library.h"
#include "fm1lab_digitone_data.h"
#include "../firmware/fm1lab_physical_controls.h"
#include <stddef.h>
/* Small freestanding string helpers: the device libc only supplies memcmp. */
static size_t fm1lab_strlen(const char *s) { size_t n=0u; while(s && s[n])++n; return n; }
static int fm1lab_strcmp(const char *a,const char *b) {
    while(*a && *a==*b){++a;++b;} return (unsigned char)*a-(unsigned char)*b;
}
static int fm1lab_strncmp(const char *a,const char *b,size_t n) {
    while(n && *a && *a==*b){++a;++b;--n;} return n?(unsigned char)*a-(unsigned char)*b:0;
}

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
enum { PAGE_HOME = 0, PAGE_MACRO, PAGE_GLOBAL, PAGE_OPERATOR_MAP, PAGE_ENV, PAGE_LFO, PAGE_LFO_MOD, PAGE_SEQ, PAGE_SEQ_OPTIONS, PAGE_SEQ_PIANO, PAGE_ARP, PAGE_ARP_STEPS, PAGE_ARP_LATCH, PAGE_FX_AMP, PAGE_FX_CHORUS, PAGE_FX_DELAY, PAGE_FX_REVERB, PAGE_FX_PHASER, PAGE_FX_COMPRESSOR, PAGE_FX_EQ, PAGE_FX_ROUTING, PAGE_INSTRUMENT_MACRO, PAGE_SAVE, PAGE_COUNT };
static uint8_t page = PAGE_HOME;
/* HOME starts in whole-instrument mode; selecting a layer enters patch focus. */
static uint8_t patch_focus;
static uint8_t eq_ui_band = 1u;
static uint8_t env_operator = 1u, env_mode, operator_detune_mode;
static uint8_t seq_ui_lane, seq_ui_step, seq_ui_instrument[2]={0u,0u}, seq_ui_note[2][64], seq_ui_velocity[2][64],
               seq_ui_gate[2][64], seq_ui_enabled[2][64], seq_ui_tie[2][64], seq_ui_accent[2][64], seq_ui_patch[2]={0u,1u};
static char seq_ui_instrument_name[2][FM1LAB_INSTRUMENT_NAME_BYTES+1u]={{'F','A','C','T','O','R','Y',0},{'F','A','C','T','O','R','Y',0}};
static fm1lab_instrument_t instrument_apply_snapshot;
static fm1lab_insert_state_t inserts_apply_snapshot;
static uint8_t seq_record_armed, seq_record_mode, seq_record_last_step=255u;
static uint8_t save_slot, save_patch, save_name_target, save_name_cursor;
static uint8_t seq_clear_prompt;
static int16_t seq_clear_turn;
static uint8_t seq_record_active[NKEYS+128u], seq_record_step[NKEYS+128u], seq_record_lane[NKEYS+128u], seq_record_note[NKEYS+128u];
static uint32_t seq_record_started[NKEYS+128u];
static uint8_t seq_record_velocity[NKEYS+128u];
static uint8_t seq_record_redraw;
#define APP_SEQ_EVENTS_PER_LANE 128u
static fm6_seq_event_t seq_event_store[2][APP_SEQ_EVENTS_PER_LANE];
static uint8_t seq_event_count[2], seq_event_store_ready;
static uint8_t seq_selected_event[2]={255u,255u};
static uint16_t transport_ui_bpm=120u;
static uint8_t arp_ui_latch, arp_ui_mode, arp_ui_step, arp_ui_length=16u, arp_ui_gain[16], arp_ui_accent[16], arp_ui_step_gate[16];
static uint8_t arp_ui_division=3u, arp_ui_gate=75u, arp_ui_octaves=1u;
static uint8_t arp_ui_output, arp_ui_random, arp_ui_macro_dest, arp_ui_depth=50u;

static const char *patch_display_name(uint32_t patch)
{
    static char display[FM1LAB_PATCH_COUNT][FM1LAB_PATCH_NAME_BYTES + 1u];
    static char fallback[FM1LAB_PATCH_COUNT][12];
    if (patch >= FM1LAB_PATCH_COUNT) patch = 0u;
    /* Use the DX voice-name accessor, never the containing Instrument name. */
    if (fm1lab_voice_name_get(current_instrument.patches[patch].voice,
                              display[patch]) == 0) {
        uint32_t n = fm1lab_strlen(display[patch]);
        const char *instrument_name = instrument_display_name();
        int same_as_instrument = n && fm1lab_strcmp(display[patch], instrument_name) == 0;
        int placeholder = fm1lab_strcmp(display[patch], "INSTRUMENT") == 0 ||
            fm1lab_strcmp(display[patch], "PATCH") == 0 ||
            fm1lab_strcmp(display[patch], "PATCH 1") == 0 ||
            fm1lab_strcmp(display[patch], "FACTORY") == 0;
        if (n && !patch_name_is_factory(display[patch]) &&
            !same_as_instrument && !placeholder) return display[patch];
    }
    fallback[patch][0]='P'; fallback[patch][1]='A'; fallback[patch][2]='T';
    fallback[patch][3]='C'; fallback[patch][4]='H'; fallback[patch][5]=' ';
    int_text((int32_t)patch+1,fallback[patch]+6);
    return fallback[patch];
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
        rounded_panel(x, y, 116, 85, 3, panel_fill(),
                      (!patch_focus || p == set.patch) ? accent : C_LINE);
        cv_rect(x + 5, y + 7, 2, 70, accent);
        t[0] = 'P'; t[1] = 'A'; t[2] = 'T'; t[3] = 'C'; t[4] = 'H'; t[5] = ' ';
        int_text((int32_t)p + 1, t + 6);
        cv_text(x + 12, y + 5, &FONT_XS, t, accent);
        cv_text(x + 65, y + 5, &FONT_XS, enabled ? "ON" : "MUTED",
                enabled ? RGB(75,235,135) : C_GRAY);
        {
            const char *name=patch_display_name(p);
            char short_name[12];
            uint32_t n=0u;
            while(name[n] && n<9u){short_name[n]=name[n];++n;}
            short_name[n]='\0';
            cv_text(x + 12, y + 23, &FONT_B, short_name, C_WHITE);
        }
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
    static const char *const names[4] = {"BRIGHT / FM INDEX", "BODY / HARMONICS", "MOTION / LFO", "EDGE / DETUNE"};
    static const char *const hints[4] = {"DARK <-> BRIGHT", "SOFT <-> METALLIC", "STILL <-> WILD", "CLEAN <-> GRIT"};
    uint32_t now = plat_ms();
    uint16_t accent = patch_color(set.patch);
    char t[24];
    for (uint32_t m = 0; m < 4u; ++m) {
        int32_t x = panel_grid_x(m);
        int32_t y = panel_grid_y(m, UI_GRID_TOP, UI_GRID_ROW_STEP);
        uint32_t value = set.macros[set.patch][m];
        uint16_t border = ui.turned_at[EN_K1 + m] && now - ui.turned_at[EN_K1 + m] < 650u ? accent : C_LINE;
        instrument_card(x, y, UI_GRID_CARD_W, UI_GRID_CARD_H, panel_fill(), border, accent);
        cv_text(x + 9, y + 9, &FONT_XS, names[m], C_WHITE);
        int_text((int32_t)value, t);
        cv_text(x + 88, y + 7, &FONT_B, t, accent);
        cv_rect(x + 9, y + 42, 98, 6, RGB(31, 43, 55));
        cv_rect(x + 9, y + 42, (int32_t)value * 98 / 100, 6, accent);
        cv_rect(x + 9 + (int32_t)value * 98 / 100, y + 39, 2, 12, C_WHITE);
        cv_text(x + 9, y + 60, &FONT_XS, hints[m], C_GRAY);
    }
}
static void draw_instrument_macro_panel(void)
{
    static const char *const names[4] = {"BRIGHT / FM INDEX", "BODY / HARMONICS", "MOTION / LFO", "EDGE / DETUNE"};
    static const char *const hints[4] = {"DARK <-> BRIGHT", "SOFT <-> METALLIC", "STILL <-> WILD", "CLEAN <-> GRIT"};
    const uint16_t accent = RGB(150,120,255);
    const uint32_t now = plat_ms();
    char t[24];
    for (uint32_t m = 0; m < 4u; ++m) {
        int32_t x = panel_grid_x(m);
        int32_t y = panel_grid_y(m, UI_GRID_TOP, UI_GRID_ROW_STEP);
        uint32_t value = current_inserts.instrument_macros[m];
        uint16_t border = ui.turned_at[EN_K1 + m] && now - ui.turned_at[EN_K1 + m] < 650u ? accent : C_LINE;
        instrument_card(x, y, UI_GRID_CARD_W, UI_GRID_CARD_H, panel_fill(), border, accent);
        cv_text(x + 9, y + 9, &FONT_XS, names[m], C_WHITE);
        int_text((int32_t)value, t);
        cv_text(x + 88, y + 7, &FONT_B, t, accent);
        cv_rect(x + 9, y + 42, 98, 6, RGB(31,43,55));
        cv_rect(x + 9 + 49, y + 40, 1, 10, C_GRAY);
        if (value >= 50u) cv_rect(x + 9 + 49, y + 42, (int32_t)(value - 50u) * 49 / 50, 6, accent);
        else cv_rect(x + 9 + (int32_t)value * 49 / 50, y + 42, (int32_t)(50u - value) * 49 / 50, 6, accent);
        cv_text(x + 9, y + 60, &FONT_XS, hints[m], C_GRAY);
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
    uint8_t values[4] = {0u, 0u, 0u, 0u};
    char t[16];
    uint16_t accent = patch_color(set.patch);
    for (uint32_t i = 0; i < 4u; ++i) {
        int32_t x = panel_grid_x(i);
        int32_t y = panel_grid_y(i, UI_GRID_TOP, UI_GRID_ROW_STEP);
        (void)fm1lab_voice_operator_get(voice, env_operator,
            i == 3u && operator_detune_mode ? FM1LAB_OP_DETUNE : fields[i], &values[i]);
        panel_grid_card(i, UI_GRID_TOP, UI_GRID_ROW_STEP, accent);
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
    static const char *const labels[4]={"K1  BYPASS","K2  DRIVE","K3  TONE","K4  OUTPUT"};
    const fm1lab_insert_state_t *v=&current_inserts;
    char t[16];uint16_t accent=RGB(36,215,232);
    uint8_t values[4]={v->amp_enabled,v->drive,v->tone,v->output};
    for(uint32_t i=0;i<4u;i++){
        int32_t x=panel_grid_x(i),y=panel_grid_y(i,UI_GRID_TOP,UI_GRID_ROW_STEP);
        panel_grid_card(i,UI_GRID_TOP,UI_GRID_ROW_STEP,accent);
        cv_text(x+7,y+6,&FONT_XS,labels[i],C_GRAY);
        if(i==0u)cv_text(x+7,y+23,&FONT_B,values[i]?"ON":"OFF",values[i]?RGB(75,235,135):RGB(255,82,101));
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
static void fm1lab_plot_line(int32_t x0,int32_t y0,int32_t x1,int32_t y1,uint16_t color)
{
    int32_t dx=x1>=x0?x1-x0:x0-x1, sx=x0<x1?1:-1;
    int32_t dy=y1>=y0?y0-y1:y1-y0, sy=y0<y1?1:-1;
    int32_t err=dx+dy;
    for(;;) {
        cv_rect(x0,y0,1,1,color);
        if(x0==x1 && y0==y1)break;
        int32_t e2=2*err;
        if(e2>=dy){err+=dy;x0+=sx;}
        if(e2<=dx){err+=dx;y0+=sy;}
    }
}

static void draw_eq_panel(void)
{
    static const uint16_t colors[3]={RGB(242,212,59),RGB(36,215,232),RGB(227,75,210)};
    static const char *const bands[3]={"LOW","MID","HIGH"};
    const fm1lab_insert_state_t *v=&current_inserts;
    const fm1lab_eq_band_state_t *selected=&v->eq[eq_ui_band];
    int32_t response[109]; char t[20];
    int32_t left=12,right=228,top=20,bottom=120,zero=70;
    for(uint32_t i=0;i<109u;i++) {
        int32_t x=left+(int32_t)i*2,db=0;
        for(uint32_t band=0;band<3u;band++) {
            const fm1lab_eq_band_state_t *b=&v->eq[band];
            int32_t center=left+(int32_t)b->frequency*(right-left)/100;
            int32_t width=5+(31-(int32_t)b->q)*2,distance=x-center;
            if(distance<0)distance=-distance;
            if(band==0u && x<center)distance=0;
            if(band==2u && x>center)distance=0;
            if(distance<width)db+=((int32_t)b->gain-12)*(width-distance)/width;
        }
        if(db>12)db=12;
        if(db< -12)db=-12;
        response[i]=zero-db*4;
        if(response[i]<top)response[i]=top;
        if(response[i]>bottom)response[i]=bottom;
    }
    cv_text(9,3,&FONT_XS,(v->eq_enabled&1u)?"3-BAND PARAMETRIC / ON":"3-BAND PARAMETRIC / BYPASS",
            (v->eq_enabled&1u)?RGB(75,235,135):C_GRAY);
    for(int32_t y=zero-48;y<=zero+48;y+=24)cv_rect(left,y,right-left,1,y==zero?C_GRAY:C_DIM);
    for(int32_t x=left;x<=right;x+=36)cv_rect(x,top,1,bottom-top,C_DIM);
    for(uint32_t i=1u;i<109u;i++)
        fm1lab_plot_line(left+(int32_t)(i-1u)*2,response[i-1u],left+(int32_t)i*2,response[i],RGB(36,215,232));
    {
        int32_t marker=left+(int32_t)selected->frequency*(right-left)/100;
        cv_rect(marker-1,top,2,bottom-top,colors[eq_ui_band]);
    }
    cv_text(12,125,&FONT_XS,"20",C_GRAY); cv_text(65,125,&FONT_XS,"200",C_GRAY);
    cv_text(119,125,&FONT_XS,"2K",C_GRAY); cv_text(174,125,&FONT_XS,"10K",C_GRAY);
    cv_text(211,125,&FONT_XS,"20K",C_GRAY);
    cv_text(10,138,&FONT_XS,"K1 BYPASS",C_GRAY);
    cv_text(10,153,&FONT_XS,bands[eq_ui_band],colors[eq_ui_band]);
    int_text((int32_t)eq_frequency_hz(selected->frequency),t); text_cat(t,"Hz");
    cv_text(66,153,&FONT_XS,t,C_WHITE);
    int_text((int32_t)selected->gain-12,t); text_cat(t,"dB");
    cv_text(143,153,&FONT_XS,t,C_WHITE);
    int_text(selected->q,t); text_cat(t," Q");
    cv_text(187,153,&FONT_XS,t,C_WHITE);
    cv_text(10,171,&FONT_XS,"K2 BAND  K3 FREQ  K4 GAIN  /  SELECT Q",C_GRAY);
}

static void draw_compressor_panel(void)
{
    uint8_t enabled=compressor_value(FM6_COMP_ENABLED);
    uint8_t values[4]={enabled,compressor_value(FM6_COMP_RATIO),
        compressor_value(FM6_COMP_THRESHOLD),compressor_value(FM6_COMP_ATTACK)};
    char t[16];uint16_t accent=RGB(36,215,232);
    for(uint32_t i=0;i<4u;i++){
        int32_t x=panel_grid_x(i),y=panel_grid_y(i,UI_GRID_TOP,UI_GRID_ROW_STEP);
        panel_grid_card(i,UI_GRID_TOP,UI_GRID_ROW_STEP,accent);
        cv_text(x+7,y+6,&FONT_XS,i==0u?"K1  BYPASS":i==1u?"K2  RATIO":i==2u?"K3  THRESHOLD":"K4  ATTACK",C_GRAY);
        if(i==0u) cv_text(x+7,y+23,&FONT_B,enabled?"ON":"OFF",enabled?RGB(75,235,135):RGB(255,82,101));
        else {
            if(i==1u){uint32_t ratio=1u+values[i]*11u/100u;int_text((int32_t)ratio,t);text_cat(t,":1");}
            else if(i==3u){uint32_t ms=1u+(uint32_t)values[i]*49u/100u;int_text((int32_t)ms,t);text_cat(t,"ms");}
            else int_text(values[i],t);
            cv_text(x+7,y+23,&FONT_B,t,accent);
        }
    }
}

static void draw_global_panel(uint32_t pct)
{
    char t[24];
    const char *labels[4] = {"MASTER", "OCTAVE", "TEMPO", "MIDI / BLE"};
    uint16_t colors[4] = {RGB(36,215,232), RGB(227,75,210), RGB(242,212,59), RGB(255,82,101)};
    for (uint32_t i = 0; i < 4u; ++i) {
        int32_t x = panel_grid_x(i);
        int32_t y = panel_grid_y(i, UI_GRID_TOP, UI_GRID_ROW_STEP);
        instrument_card(x, y, UI_GRID_CARD_W, UI_GRID_CARD_H, panel_fill(), colors[i], colors[i]);
        cv_text(x + 9, y + 8, &FONT_XS, labels[i], C_GRAY);
        if (i == 0) {
            int_text((int32_t)pct, t); text_cat(t, "%");
            cv_text(x + 9, y + 27, &FONT_B, t, colors[i]);
        } else if (i == 1) {
            octave_text(t);
            cv_text(x + 9, y + 27, &FONT_B, t, colors[i]);
        } else if (i == 2) {
            int_text((int32_t)transport_ui_bpm, t); text_cat(t, " BPM");
            cv_text(x + 9, y + 27, &FONT_B, t, colors[i]);
        } else {
            int_text((int32_t)midi_count, t);
            cv_text(x + 9, y + 27, &FONT_B, t, colors[i]);
        }
    }
}

static void draw_envelope_panel(void)
{
    static const char *const rates[4] = {"RATE 1", "RATE 2", "RATE 3", "RATE 4"};
    static const char *const levels[4] = {"LEVEL 1", "LEVEL 2", "LEVEL 3", "LEVEL 4"};
    static const char *const adsr_names[4] = {"K1  ATTACK", "K2  DECAY", "K3  SUSTAIN", "K4  RELEASE"};
    char t[16];
    const uint8_t *voice = current_instrument.patches[set.patch].voice;
    uint16_t accent = patch_color(set.patch);
    uint8_t adsr[4] = {0u,0u,0u,0u};
    if (env_mode == 0u) {
        int32_t left=12,right=228,top=18,baseline=124;
        int32_t aw,dw,rw,sw=42,total,scale,x1,x2,x3,x4,sy;
        (void)fm1lab_voice_adsr_get(voice,adsr);
        aw=8+(99-(int32_t)adsr[0])*5/9;
        dw=10+(99-(int32_t)adsr[1])*4/9;
        rw=10+(99-(int32_t)adsr[3])*4/9;
        total=aw+dw+rw+sw; scale=(right-left)*100/total;
        aw=aw*scale/100; dw=dw*scale/100; sw=sw*scale/100;
        rw=right-left-aw-dw-sw;
        x1=left+aw; x2=x1+dw; x3=x2+sw; x4=x3+rw;
        sy=baseline-(int32_t)adsr[2]*96/99;
        cv_text(10,3,&FONT_XS,"AMPLITUDE / ALL CARRIERS",C_GRAY);
        cv_rect(left,top,right-left,1,C_DIM); cv_rect(left,71,right-left,1,C_DIM);
        cv_rect(left,baseline,right-left,1,C_GRAY);
        cv_rect(left,top,1,baseline-top,C_DIM); cv_rect(right,top,1,baseline-top,C_DIM);
        fm1lab_plot_line(left,baseline,x1,top,accent);
        fm1lab_plot_line(x1,top,x2,sy,accent);
        fm1lab_plot_line(x2,sy,x3,sy,accent);
        fm1lab_plot_line(x3,sy,x4,baseline,accent);
        cv_rect(x1-2,top-2,5,5,C_WHITE); cv_rect(x2-2,sy-2,5,5,accent);
        cv_text(12,132,&FONT_XS,"ATTACK",C_GRAY); cv_text(67,132,&FONT_XS,"DECAY",C_GRAY);
        cv_text(119,132,&FONT_XS,"SUSTAIN",C_GRAY); cv_text(180,132,&FONT_XS,"RELEASE",C_GRAY);
        int_text(adsr[0],t); cv_text(12,151,&FONT_B,t,accent);
        int_text(adsr[1],t); cv_text(67,151,&FONT_B,t,accent);
        int_text(adsr[2],t); cv_text(129,151,&FONT_B,t,accent);
        int_text(adsr[3],t); cv_text(190,151,&FONT_B,t,accent);
        cv_text(12,169,&FONT_XS,"K1 ATTACK  K2 DECAY  K3 SUSTAIN  K4 RELEASE",C_GRAY);
        return;
    }
    for (uint32_t i = 0; i < 4u; ++i) {
        int32_t x = 4 + (int32_t)(i & 1u) * 118;
        int32_t y = panel_grid_y(i, UI_GRID_TOP, UI_GRID_ROW_STEP);
        uint8_t value = 0;
        (void)fm1lab_voice_operator_get(voice, env_operator,
            (fm1lab_operator_field_t)((env_mode == 2u ? FM1LAB_OP_LEVEL_1 : FM1LAB_OP_RATE_1) + i), &value);
        instrument_card(x, y, UI_GRID_CARD_W, UI_GRID_CARD_H, panel_fill(), accent, accent);
        cv_text(x + 9, y + 10, &FONT_XS, env_mode == 2u ? levels[i] : rates[i], C_WHITE);
        int_text(value, t); cv_text(x + 88, y + 10, &FONT_XS, t, accent);
        cv_rect(x + 9, y + 39, 98, 6, C_LINE);
        cv_rect(x + 9, y + 39, (int32_t)value * 98 / 99, 6, accent);
        cv_text(x + 9, y + 64, &FONT_XS, env_mode == 2u ? "0=OFF  99=MAX" : "0=SLOW  99=FAST", C_GRAY);
    }
    cv_text(8, 170, &FONT_XS, env_operator <= 6u ? "SELECT OP  /  EDIT INDIVIDUAL EG" : "OPERATOR ENVELOPE", C_GRAY);
}

static void draw_lfo_panel(uint32_t modulation)
{
    static const char *const labels[2][4] = {
        {"K1  SHAPE", "K2  RATE", "K3  PITCH", "K4  VOLUME"},
        {"K1  DELAY", "K2  PITCH SENS", "K3  LFO RESET", "K4  PHASE RESET"}
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
    for (uint32_t i = 0; i < 4u; ++i) {
        int32_t x = panel_grid_x(i);
        int32_t y = panel_grid_y(i, UI_GRID_TOP, UI_GRID_ROW_STEP);
        uint8_t value = 0;
        (void)fm1lab_voice_global_get(voice, fields[modulation][i], &value);
        instrument_card(x, y, UI_GRID_CARD_W, UI_GRID_CARD_H, panel_fill(), accent, accent);
        cv_text(x + 9, y + 8, &FONT_XS, labels[modulation][i], C_WHITE);
        if (!modulation && i == 0u) {
            cv_text(x + 9, y + 26, &FONT_B, waves[value % 6u], accent);
            cv_text(x + 9, y + 64, &FONT_XS, "PER-PATCH WAVE", C_GRAY);
        } else {
            if (modulation && i >= 2u) cv_text(x + 9, y + 26, &FONT_B, value ? "ON" : "OFF", accent);
            else {
                int_text(value, t); cv_text(x + 9, y + 26, &FONT_B, t, accent);
                cv_rect(x + 9, y + 46, 94, 5, C_LINE);
                cv_rect(x + 9, y + 46, (int32_t)value * 94 / maxima[modulation][i], 5, accent);
            }
            if (!modulation) cv_text(x + 9, y + 64, &FONT_XS,
                i == 1u ? "SLOW  >  FAST" : i == 2u ? "PITCH WOBBLE" : "VOLUME TREMOLO", C_GRAY);
            else cv_text(x + 9, y + 64, &FONT_XS,
                i == 0u ? "FADE-IN TIME" : i == 1u ? "KEY RESPONSE" : i == 2u ? "RESET LFO" : "RESET OSC", C_GRAY);
        }
    }
}

static void draw_step_columns(uint8_t lane, uint8_t first, uint8_t count,
                               int32_t top, int32_t height, uint8_t selected,
                               uint8_t playing, const uint8_t *velocity,
                               const uint8_t *enabled, const uint8_t *accent_values,
                               uint16_t accent)
{
    int32_t gap=count<=8u?2:1;
    int32_t width=(224-(int32_t)(count-1u)*gap)/(int32_t)count;
    for(uint32_t i=0;i<count;i++) {
        uint8_t step=(uint8_t)(first+i), active=enabled?enabled[step]:1u;
        uint8_t value=velocity?velocity[step]:100u;
        uint8_t accent_value=accent_values?accent_values[step]:50u;
        int32_t x=8+(int32_t)i*(width+gap);
        int32_t bar_h=1+(int32_t)value*(height-5)/127;
        int32_t bar_w=active?(width/2+(int32_t)accent_value*(width/2-1)/100):width/2;
        int32_t bx=x+(width-bar_w)/2;
        cv_rect(x,top,width,height,C_LINE);
        if(active && bar_h>0)cv_rect(bx,top+height-2-bar_h,bar_w,bar_h,accent);
        if(step==selected)rounded_panel(x-1,top-1,width+2,height+2,2,0u,RGB(255,255,255));
        if(step==playing)rounded_panel(x-1,top-1,width+2,height+2,2,0u,RGB(242,212,59));
    }
    (void)lane;
}
static void draw_sequencer_panel(void)
{
    uint8_t playing=fm6_sequencer_playing_step();
    uint8_t count=set.performance.seq_length;
    uint8_t rows;
    if(count<2u)count=2u;
    if(count>64u)count=64u;
    rows=(uint8_t)((count+15u)/16u);

    /* Each track owns a fixed 46px timeline area. Use up to four rows,
     * but expand row height so short patterns fill the same vertical space. */
    int32_t row_gap=2;
    int32_t row_height=(46-(int32_t)(rows-1u)*row_gap)/(int32_t)rows;
    cv_text(6,0,&FONT_XS,"TRACK 1",patch_color(seq_ui_patch[0]));
    cv_text(78,0,&FONT_XS,seq_ui_instrument_name[0],C_WHITE);
    for(uint32_t row=0;row<rows;row++) {
        uint8_t first=(uint8_t)(row*16u);
        uint8_t n=(uint8_t)(count-first);
        if(n>16u)n=16u;
        draw_step_columns(0u,first,n,15+(int32_t)row*(row_height+row_gap),row_height,
                          seq_ui_lane==0u?seq_ui_step:255u,
                          fm6_sequencer_running()?playing:255u,
                          seq_ui_velocity[0],seq_ui_enabled[0],
                          seq_ui_accent[0],patch_color(seq_ui_patch[0]));
    }

    cv_text(6,66,&FONT_XS,"TRACK 2",patch_color(seq_ui_patch[1]));
    cv_text(78,66,&FONT_XS,seq_ui_instrument_name[1],C_WHITE);
    for(uint32_t row=0;row<rows;row++) {
        uint8_t first=(uint8_t)(row*16u);
        uint8_t n=(uint8_t)(count-first);
        if(n>16u)n=16u;
        draw_step_columns(1u,first,n,81+(int32_t)row*(row_height+row_gap),row_height,
                          seq_ui_lane==1u?seq_ui_step:255u,
                          fm6_sequencer_running()?playing:255u,
                          seq_ui_velocity[1],seq_ui_enabled[1],
                          seq_ui_accent[1],patch_color(seq_ui_patch[1]));
    }

    cv_text(6,139,&FONT_XS,fm6_sequencer_running()?(seq_record_armed?"LIVE RECORD":"PLAYING"):(seq_record_armed?"REC ARMED":"STOPPED"),
            fm6_sequencer_running()?RGB(75,235,135):(seq_record_armed?RGB(255,80,90):C_GRAY));
    cv_text(6,166,&FONT_XS,"K1 STEP",C_GRAY);
    cv_text(66,166,&FONT_XS,"K2 CLEAR",C_GRAY);
    cv_text(126,166,&FONT_XS,"K3 VEL",C_GRAY);
    cv_text(186,166,&FONT_XS,"K4 GATE",C_GRAY);
}
static void draw_sequencer_piano_panel(void)
{
    char value[20];
    const uint8_t lane=seq_ui_lane, first=(uint8_t)((seq_ui_step/16u)*16u);
    const uint16_t accent=patch_color(seq_ui_patch[lane]);
    const int32_t gx=24, gy=28, gw=208, gh=72, cw=13, rh=3;
    const fm6_seq_event_t *selected=sequencer_selected_event();
    int32_t focus_note=selected?selected->pitch:60;
    int32_t low=focus_note-12;
    if(low<0)low=0;
    if(low>103)low=103;
    const int32_t high=low+23;
    instrument_card(4,2,232,154,panel_fill(),accent,accent);
    cv_text(12,8,&FONT_XS,lane?"TRACK 2  /  NOTES":"TRACK 1  /  NOTES",accent);
    cv_text(148,8,&FONT_XS,seq_ui_instrument_name[lane],C_WHITE);
    for(uint32_t row=0;row<24u;row++) {
        int32_t note=high-(int32_t)row, y=gy+(int32_t)row*rh;
        uint8_t pc=(uint8_t)(note%12);
        uint8_t black=(uint8_t)(pc==1u||pc==3u||pc==6u||pc==8u||pc==10u);
        cv_rect(gx,y,gw,rh-1,black?RGB(20,29,42):RGB(34,45,57));
        if(pc==0u||pc==5u)cv_rect(gx,y,gw,1,RGB(75,86,101));
    }
    for(uint32_t col=0;col<=16u;col++) {
        int32_t x=gx+(int32_t)col*cw;
        cv_rect(x,gy,1,gh,((col%4u)==0u)?RGB(86,98,112):RGB(49,61,75));
    }
    for(uint32_t col=0;col<16u;col++) {
        uint8_t step=(uint8_t)(first+col);
        int32_t x=gx+(int32_t)col*cw;
        if(step==seq_ui_step)rounded_panel(x,gy,cw,gh,1u,0u,C_WHITE);
        if(fm6_sequencer_running()&&step==fm6_sequencer_playing_step())
            cv_rect(x+5,gy,2,gh,RGB(242,212,59));
        if((col%4u)==0u) {
            int_text((int32_t)step+1,value);
            cv_text(x+1,104,&FONT_XS,value,C_GRAY);
        }
    }
    for(uint32_t i=0;i<seq_event_count[lane];i++) {
        const fm6_seq_event_t *event=&seq_event_store[lane][i];
        int32_t event_start=event->start, event_end=event_start+event->duration;
        int32_t visible_start=event_start>(int32_t)first?event_start:first;
        int32_t visible_end=event_end<(int32_t)first+16?event_end:(int32_t)first+16;
        if(visible_end<=visible_start || event->pitch<low || event->pitch>high)continue;
        int32_t x=gx+(visible_start-(int32_t)first)*cw+1;
        int32_t y=gy+(high-event->pitch)*rh+1;
        int32_t barw=(visible_end-visible_start)*cw-2;
        int32_t barh=2+(int32_t)event->velocity*2/127;
        if(barw<2)barw=2;
        if(barh>3)barh=3;
        cv_rect(x,y,barw,barh,accent);
        if(i==seq_selected_event[lane]) {
            cv_rect(x,y,barw,1,C_WHITE);
            cv_rect(x,y+barh-1,barw,1,C_WHITE);
            cv_rect(x,y,1,barh,C_WHITE);
            if(barw>1)cv_rect(x+barw-1,y,1,barh,C_WHITE);
        }
    }
    cv_text(10,116,&FONT_XS,"NOTE",C_GRAY);
    if(selected)int_text(selected->pitch,value);else {value[0]='-';value[1]='-';value[2]='\0';}
    cv_text(36,116,&FONT_B,value,accent);
    cv_text(68,116,&FONT_XS,"LEN",C_GRAY);
    int_text(selected?selected->duration:0,value);
    cv_text(91,116,&FONT_B,value,C_WHITE);
    cv_text(122,116,&FONT_XS,"ACC",C_GRAY);
    int_text(selected?selected->accent:0,value);
    cv_text(145,116,&FONT_B,value,C_WHITE);
    cv_text(177,116,&FONT_XS,"VEL",C_GRAY);
    int_text(selected?selected->velocity:0,value);
    cv_text(200,116,&FONT_B,value,C_WHITE);
    cv_text(8,137,&FONT_XS,"K1 LENGTH   K2 PITCH   K3 ACCENT   K4 VELOCITY",C_GRAY);
    cv_text(8,151,&FONT_XS,"SELECT NEXT NOTE   EDIT DELETE",C_WHITE);
}

static void draw_save_panel(void)
{
    char t[32];
    const uint16_t accent=RGB(36,215,232);
    instrument_card(5,4,230,170,panel_fill(),accent,accent);
    cv_text(14,12,&FONT_XS,"SAVE SLOT  /  PRESET KNOB",C_GRAY);
    int_text((int32_t)save_slot+1,t);text_cat(t," / 32");
    cv_text(14,30,&FONT_B,t,accent);
    cv_text(14,55,&FONT_XS,"PATCH  /  SELECT KNOB",C_GRAY);
    int_text((int32_t)save_patch+1,t);text_cat(t," / 4");
    cv_text(14,73,&FONT_B,t,patch_color(save_patch));
    cv_text(14,99,&FONT_XS,save_name_target?"PATCH NAME  /  10 CHARS":"INSTRUMENT NAME  /  24 CHARS",C_GRAY);
    if(save_name_target) {
        for(uint32_t i=0;i<FM1LAB_PATCH_NAME_BYTES;i++)t[i]=(char)current_instrument.patches[save_patch].voice[145u+i];
        t[FM1LAB_PATCH_NAME_BYTES]='\0';
    } else {
        for(uint32_t i=0;i<FM1LAB_INSTRUMENT_NAME_BYTES;i++)t[i]=current_instrument.name[i];
        t[FM1LAB_INSTRUMENT_NAME_BYTES]='\0';
    }
    cv_text(14,117,&FONT_B,t,accent);
    int32_t cursor_x=14+(int32_t)save_name_cursor*(save_name_target?8:7);
    cv_rect(cursor_x,137,save_name_target?7:6,2,C_WHITE);
    cv_text(14,151,&FONT_XS,"K1 CURSOR  K2 CHAR  K3 NAME TYPE",C_WHITE);
    cv_text(14,165,&FONT_XS,"SAVE AGAIN TO WRITE   HOME TO EXIT",C_GRAY);
}
static void draw_sequencer_options_card(uint32_t index,const char *label,const char *value,
        const char *hint,uint16_t accent)
{
    int32_t x=panel_grid_x(index),y=panel_grid_y(index,UI_GRID_TOP,UI_GRID_ROW_STEP);
    instrument_card(x,y,UI_GRID_CARD_W,UI_GRID_CARD_H,panel_fill(),accent,accent);
    cv_text(x+9,y+9,&FONT_XS,label,C_GRAY);
    cv_text(x+9,y+28,&FONT_B,value,accent);
    cv_text(x+9,y+67,&FONT_XS,hint,C_WHITE);
}
static void draw_sequencer_options_panel(void)
{
    char value[16];
    const uint16_t yellow=RGB(242,212,59);
    const uint16_t cyan=RGB(36,215,232);
    const uint16_t red=RGB(255,105,115);
    draw_sequencer_options_card(0u,"RECORD MODE",
        "ADDITIVE",
        "EXISTING NOTES ARE PRESERVED",yellow);
    int_text(set.performance.seq_length,value);text_cat(value," STEPS");
    draw_sequencer_options_card(1u,"K2  LOOP LENGTH",value,"TURN 2-64  /  DEFAULT 16",cyan);
    draw_sequencer_options_card(2u,"RECORD STATUS",
        seq_record_armed?"ARMED":"DISARMED",
        "REC BUTTON TO ARM",seq_record_armed?red:RGB(110,125,140));
    draw_sequencer_options_card(3u,"LOOP SCOPE","BOTH TRACKS",
        "ONE SHARED LOOP",RGB(150,120,255));
}
static void draw_arp_card(uint32_t index, const char *label, const char *value,
                          const char *hint, uint16_t accent)
{
    int32_t x=panel_grid_x(index), y=panel_grid_y(index,UI_GRID_TOP,UI_GRID_ROW_STEP);
    instrument_card(x,y,UI_GRID_CARD_W,UI_GRID_CARD_H,panel_fill(),accent,accent);
    cv_text(x+9,y+9,&FONT_XS,label,C_GRAY);
    cv_text(x+9,y+28,&FONT_B,value,accent);
    cv_text(x+9,y+67,&FONT_XS,hint,C_WHITE);
}
static void draw_arp_panel(void)
{
    static const char *const modes[5]={"OFF","UP","DOWN","UP/DOWN","RANDOM"};
    static const char *const divisions[6]={"1/1","1/2","1/4","1/8","1/16","1/32"};
    char t[16];
    uint16_t accent=RGB(227,75,210);
    draw_arp_card(0u,"K1  MODE",modes[arp_ui_mode<5u?arp_ui_mode:0u],"ORDER / ENABLE",accent);
    draw_arp_card(1u,"K2  DIVISION",divisions[arp_ui_division<6u?arp_ui_division:3u],"NOTE RATE",RGB(36,215,232));
    int_text(arp_ui_gate,t); text_cat(t,"%");
    draw_arp_card(2u,"K3  GATE",t,"NOTE LENGTH",RGB(242,212,59));
    int_text(arp_ui_octaves,t); text_cat(t," OCT");
    draw_arp_card(3u,"K4  OCTAVES",t,"RANGE",RGB(255,130,100));
}
static uint16_t arp_step_gradient_color(uint8_t accent, uint32_t band, uint32_t bands, uint8_t playing)
{
    /* Accent is visual-only: it controls a bottom-up color ramp. */
    uint32_t progress=bands>1u?band*100u/(bands-1u):100u;
    uint32_t level=18u+((uint32_t)accent*progress*82u)/10000u;
    uint32_t r=20u+(207u*level)/100u, g=12u+(63u*level)/100u, b=34u+(176u*level)/100u;
    if(playing){r+=r/2u;g+=g/2u;b+=b/2u;}
    if(r>255u)r=255u;if(g>255u)g=255u;if(b>255u)b=255u;
    return RGB(r,g,b);
}
static void draw_arp_latch_panel(void)
{
    uint16_t accent=RGB(227,75,210);
    instrument_card(8,22,224,126,panel_fill(),accent,accent);
    cv_text(20,34,&FONT_XS,"NOTE HOLD BEHAVIOR",C_GRAY);
    cv_text(20,62,&FONT_B,arp_ui_latch?"LATCH ON":"LATCH OFF",arp_ui_latch?RGB(75,235,135):accent);
    cv_text(20,96,&FONT_XS,arp_ui_latch?"RELEASED KEYS STAY IN THE ARP":"RELEASED KEYS LEAVE THE ARP",C_WHITE);
    cv_text(20,126,&FONT_XS,"K1  TOGGLE LATCH",C_GRAY);
}
static void draw_arp_steps_panel(void)
{
    char t[16];
    uint8_t playing=fm6_arpeggiator_playing_step(), active=fm6_arpeggiator_active();
    int32_t gap=2, width=(224-(int32_t)(arp_ui_length-1u)*gap)/(int32_t)arp_ui_length;
    int32_t top=29, height=91, bottom=top+height;
    uint16_t gain_color=RGB(36,215,232), accent_color=RGB(227,75,210), tie_color=RGB(242,212,59);
    cv_text(5,2,&FONT_XS,arp_ui_length==8u?"8 STEP LOOP":"16 STEP LOOP",C_GRAY);
    cv_text(151,2,&FONT_XS,active?"PLAYING":"STANDBY",active?RGB(75,235,135):C_GRAY);
    cv_rect(5,18,214,1,RGB(35,51,64));
    for(uint32_t i=0;i<arp_ui_length;i++) {
        int32_t x=8+(int32_t)i*(width+gap);
        uint8_t gain=arp_ui_gain[i], emphasis=arp_ui_accent[i];
        uint8_t tied=arp_ui_step_gate[i]?1u:0u;
        int32_t bar_h=(int32_t)gain*(height-10)/127;
        uint8_t is_playing=(uint8_t)(active && i==(uint32_t)(playing%arp_ui_length));
        uint8_t is_selected=(uint8_t)(i==arp_ui_step);
        uint32_t bands=7u;
        int_text((int32_t)i+1,t);
        cv_text(x+(width-text_w(&FONT_XS,t))/2,21,&FONT_XS,t,is_selected?C_WHITE:C_GRAY);
        cv_rect(x,top,width,height,is_selected?C_WHITE:RGB(43,59,72));
        cv_rect(x+1,top+1,width-2,height-2,RGB(12,20,29));
        cv_rect(x+2,top+2,width-4,2,emphasis?accent_color:RGB(34,49,61));
        if(bar_h<2&&gain)bar_h=2;
        if(gain) {
            int32_t fill_x=x+2, fill_y=bottom-4-bar_h;
            int32_t bar_w=width-4;
            int32_t band_h=(bar_h+(int32_t)bands-1)/(int32_t)bands;
            for(uint32_t band=0;band<bands;band++) {
                int32_t yy=fill_y+(int32_t)band*band_h, hh=band_h;
                if(yy+hh>bottom-4)hh=bottom-4-yy;
                if(hh>0)cv_rect(fill_x,yy,bar_w,hh,arp_step_gradient_color(emphasis,band,bands,is_playing));
            }
        }
        if(tied && i+1u<arp_ui_length) {
            int32_t bridge_y=bottom-1;
            cv_rect(x+width-1,bridge_y,gap+2,2,tie_color);
            cv_rect(x+width-1,bridge_y-2,1,4,tie_color);
            cv_rect(x+width+gap,bridge_y-2,1,4,tie_color);
        }
        if(is_playing) {
            cv_rect(x+1,top+1,width-2,2,RGB(255,150,255));
            cv_rect(x+1,bottom-3,width-2,1,RGB(255,150,255));
        }
        if(is_selected) {
            cv_rect(x,top,width,1,C_WHITE);cv_rect(x,bottom-1,width,1,C_WHITE);
            cv_rect(x,top,1,height,C_WHITE);cv_rect(x+width-1,top,1,height,C_WHITE);
        }
    }
    cv_text(5,128,&FONT_XS,"STEP",C_GRAY);
    int_text((int32_t)arp_ui_step+1,t);cv_text(31,128,&FONT_XS,t,C_WHITE);
    cv_text(55,128,&FONT_XS,"GAIN",gain_color);
    int_text(arp_ui_gain[arp_ui_step],t);cv_text(83,128,&FONT_XS,t,C_WHITE);
    cv_text(112,128,&FONT_XS,"ACC",accent_color);
    int_text(arp_ui_accent[arp_ui_step],t);cv_text(135,128,&FONT_XS,t,C_WHITE);
    cv_text(163,128,&FONT_XS,"TIE",tie_color);
    cv_text(193,128,&FONT_XS,arp_ui_step_gate[arp_ui_step]?"ON":"OFF",arp_ui_step_gate[arp_ui_step]?tie_color:C_GRAY);
    cv_text(5,151,&FONT_XS,"K1 STEP  K2 GAIN  K3 ACC  K4 TIE",C_GRAY);
}

static void draw_delete_overlay(void)
{
    uint32_t phase=(plat_ms()/180u)&3u;
    uint16_t accent=ui.delete_kind==1u?RGB(227,75,210):RGB(242,212,59);
    const char *label=ui.delete_kind==1u?"CLEARING ARP":
        (ui.delete_index==2u?"CLEARING SEQUENCE 2":"CLEARING SEQUENCE 1");
    uint32_t remaining=(uint32_t)(ui.delete_until-plat_ms());
    uint32_t elapsed=remaining<2100u?2100u-remaining:2100u;
    int32_t progress=(int32_t)(elapsed*176u/2100u);
    rounded_panel(18,48,204,91,3,RGB(5,9,16),accent);
    rounded_panel(21,51,198,85,3,RGB(12,18,29),RGB(31,47,59));
    cv_rect(96,62,32,2,RGB(43,59,72));
    cv_rect(96,62+(int32_t)(phase&1u)*2,8,2,accent);
    cv_rect(108,70,24,16,RGB(20,31,43));
    cv_rect(111,73,4,10,accent);
    cv_rect(120,73,4,10,phase&1u?C_WHITE:accent);
    cv_rect(129,73,4,10,accent);
    cv_text(120-text_w(&FONT_B,label)/2,96,&FONT_B,label,C_WHITE);
    cv_rect(34,119,176,3,RGB(31,47,59));
    if(progress>176)progress=176;
    if(progress>0)cv_rect(34,119,progress,3,accent);
}
static void draw_seq_clear_prompt(void)
{
    char t[20];
    rounded_panel(17,55,206,78,4,RGB(5,9,16),RGB(242,212,59));
    rounded_panel(20,58,200,72,4,RGB(12,18,29),RGB(57,66,72));
    cv_text(120-text_w(&FONT_B,"CLEAR STEP?")/2,72,&FONT_B,"CLEAR STEP?",RGB(255,226,95));
    cv_text(120-text_w(&FONT_XS,"TURN K2 TO CONFIRM")/2,96,&FONT_XS,"TURN K2 TO CONFIRM",C_WHITE);
    t[0]='T';t[1]=(char)('1'+seq_ui_lane);t[2]=' ';t[3]='S';t[4]='T';t[5]='E';t[6]='P';t[7]=' ';
    int_text((int32_t)seq_ui_step+1,t+8);
    cv_text(120-text_w(&FONT_XS,t)/2,115,&FONT_XS,t,C_GRAY);
}

static void draw_fx_panel(uint8_t fx)
{
    static const char *const names[4]={"CHORUS","DELAY","REVERB","PHASER"};
    static const char *const types[3]={"ROOM","HALL","PLATE"};
    char t[16];
    uint16_t accent=fx==0u?RGB(36,215,232):fx==1u?RGB(227,75,210):fx==2u?RGB(242,212,59):RGB(85,190,255);
    uint8_t enabled=fm6_fx_enabled(fx);
    uint8_t values[4]={enabled,fx==FM6_FX_REVERB?set.performance.fx_depth[2]:fm6_fx_time(fx),
        fm6_fx_mix(fx),fx==FM6_FX_PHASER?fm6_fx_feedback(fx):fm6_fx_feedback(fx)};
    const char *labels[4]={"K1  BYPASS",fx==FM6_FX_REVERB?"K2  TYPE":
        fx==FM6_FX_CHORUS?"K2  RATE":fx==FM6_FX_DELAY?"K2  TIME":"K2  RATE",
        "K3  WET","K4  FEEDBACK"};
    for(uint32_t k=0;k<4u;++k) {
        int32_t x=panel_grid_x(k),y=panel_grid_y(k,UI_GRID_TOP,UI_GRID_ROW_STEP);
        panel_grid_card(k,UI_GRID_TOP,UI_GRID_ROW_STEP,accent);
        cv_text(x+7,y+6,&FONT_XS,labels[k],C_GRAY);
        if(k==0u) cv_text(x+7,y+23,&FONT_B,enabled?"ON":"OFF",enabled?RGB(75,235,135):RGB(255,82,101));
        else if(k==1u && fx==FM6_FX_REVERB) {
            uint8_t type=values[k]<=2u?values[k]:1u;
            cv_text(x+7,y+23,&FONT_B,types[type],accent);
            cv_text(x+7,y+43,&FONT_XS,"ROOM / HALL / PLATE",C_GRAY);
        } else {
            int_text(values[k],t);
            cv_text(x+7,y+23,&FONT_B,t,accent);
            cv_rect(x+48,y+29,55,5,C_LINE);
            cv_rect(x+48,y+29,(int32_t)values[k]*55/100,5,accent);
        }
    }
}

static void draw_fx_routing_panel(void)
{
    static const char *const names[FM6_FX_COUNT]={"CHORUS SEND","DELAY SEND","REVERB SEND","PHASER SEND"};
    char t[16];
    uint16_t accent=patch_color(set.patch);
    for(uint32_t fx=0;fx<FM6_FX_COUNT;++fx) {
        uint8_t value=fm6_fx_get_send(set.patch,(uint8_t)fx);
        int32_t x=panel_grid_x(fx);
        int32_t y=panel_grid_y(fx,UI_GRID_TOP,UI_GRID_ROW_STEP);
        uint16_t color=fx==0u?RGB(36,215,232):fx==1u?RGB(227,75,210):fx==2u?RGB(242,212,59):RGB(85,190,255);
        panel_grid_card(fx, UI_GRID_TOP, UI_GRID_ROW_STEP, color);
        cv_text(x+7,y+6,&FONT_XS,names[fx],C_GRAY);
        int_text(value,t);
        cv_text(x+7,y+23,&FONT_B,t,color);
        cv_rect(x+48,y+29,55,5,C_LINE);
        cv_rect(x+48,y+29,(int32_t)value*55/100,5,color);
    }
}

/* The top identity line is the current page title, not the instrument's
 * generic FACTORY storage name. Page accents carry through to the footer. */
static const char *page_title(uint8_t current)
{
    switch (current) {
    case PAGE_HOME: return "HOME";
    case PAGE_MACRO: return patch_focus ? "PATCH MACROS" : "MACROS";
    case PAGE_GLOBAL: return "GLOBAL";
    case PAGE_OPERATOR_MAP: return "OPERATOR MAP";
    case PAGE_ENV: return env_mode == 0u ? "PATCH ADSR" : env_mode == 1u ? "OP EG RATES" : "OP EG LEVELS";
    case PAGE_LFO: return "LFO / MOTION";
    case PAGE_LFO_MOD: return "LFO SYNC";
    case PAGE_SEQ: return "SEQUENCER";
    case PAGE_SEQ_OPTIONS: return "SEQ OPTIONS";
    case PAGE_SEQ_PIANO: return "PIANO ROLL";
    case PAGE_ARP: return "ARP ENGINE";
    case PAGE_ARP_STEPS: return "ARP STEP EDIT";
    case PAGE_ARP_LATCH: return "ARP LATCH";
    case PAGE_FX_AMP: return "AMP / DRIVE";
    case PAGE_FX_EQ: return "PARAMETRIC EQ";
    case PAGE_FX_COMPRESSOR: return "COMPRESSOR";
    case PAGE_FX_CHORUS: return "CHORUS";
    case PAGE_FX_DELAY: return "DELAY";
    case PAGE_FX_REVERB: return "REVERB";
    case PAGE_FX_PHASER: return "PHASER";
    case PAGE_FX_ROUTING: return "PATCH SENDS";
    case PAGE_INSTRUMENT_MACRO: return "INSTRUMENT MACROS";
    case PAGE_SAVE: return "SAVE INSTRUMENT";
    default: return "FM1-LAB";
    }
}

static uint16_t page_accent(uint8_t current)
{
    switch (current) {
    case PAGE_HOME: return patch_color(set.patch);
    case PAGE_MACRO: return patch_color(set.patch);
    case PAGE_INSTRUMENT_MACRO: return RGB(150,120,255);
    case PAGE_GLOBAL: return RGB(85,190,255);
    case PAGE_OPERATOR_MAP: return RGB(36,215,232);
    case PAGE_ENV: return RGB(255,170,75);
    case PAGE_LFO: return RGB(75,220,150);
    case PAGE_LFO_MOD: return RGB(75,190,180);
    case PAGE_SEQ: case PAGE_SEQ_OPTIONS: case PAGE_SEQ_PIANO: return RGB(242,212,59);
    case PAGE_ARP: case PAGE_ARP_STEPS: case PAGE_ARP_LATCH: return RGB(227,75,210);
    case PAGE_FX_AMP: return RGB(255,140,70);
    case PAGE_FX_EQ: return RGB(85,190,255);
    case PAGE_FX_COMPRESSOR: return RGB(255,170,75);
    case PAGE_FX_CHORUS: return RGB(36,215,232);
    case PAGE_FX_DELAY: return RGB(85,160,255);
    case PAGE_FX_REVERB: return RGB(150,120,255);
    case PAGE_FX_PHASER: return RGB(227,75,210);
    case PAGE_FX_ROUTING: return patch_color(set.patch);
    default: return C_GRAY;
    }
}

/* Counters describe the actual navigation path, not enum adjacency.
 * Standalone destinations remain 1/1; multi-page paths restart at 1. */
static void page_counter(uint8_t current, char *out, uint32_t out_size)
{
    uint32_t index = 1u, total = 1u;
    if (current >= PAGE_FX_AMP && current <= PAGE_FX_ROUTING) {
        index = (uint32_t)(current - PAGE_FX_AMP) + 1u;
        total = (uint32_t)(PAGE_FX_ROUTING - PAGE_FX_AMP) + 1u;
    } else if (current == PAGE_LFO || current == PAGE_LFO_MOD) {
        index = current == PAGE_LFO ? 1u : 2u;
        total = 2u;
    } else if (current == PAGE_SEQ || current == PAGE_SEQ_OPTIONS) {
        /* The sequencer and its options are separate destinations. The
         * main two-lane pattern view is a single page, regardless of loop length. */
        index = 1u;
        total = 1u;
    } else if (current == PAGE_ENV) {
        index = (uint32_t)env_mode + 1u;
        total = 3u;
    } else if (current == PAGE_OPERATOR_MAP || current == PAGE_MACRO) {
        index = current == PAGE_OPERATOR_MAP ? 1u : 2u;
        total = 2u;
    } else if (current >= PAGE_ARP && current <= PAGE_ARP_LATCH) {
        index = (uint32_t)(current - PAGE_ARP) + 1u;
        total = 3u;
    }
    int_text((int32_t)index, out);
    text_cat(out, "/");
    char total_text[8];
    int_text((int32_t)total, total_text);
    text_cat(out, total_text);
    (void)out_size;
}

static void draw(void)
{
    char t[24];
    uint32_t now = plat_ms(), pct = plat_master() * 100u / 4096u;
    uint16_t accent = page_accent(page);
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

    /* Page identity replaces the generic FACTORY instrument label. */
    cv_text(8, 15, &FONT_B, page == PAGE_HOME ? instrument_display_name() : page_title(page), accent);
    cv_text(165,16,&FONT_XS,fm6_sequencer_running()?"PLAY":"STOP",fm6_sequencer_running()?RGB(75,235,135):C_GRAY);
    if(seq_record_armed)cv_text(205,16,&FONT_XS,"REC",RGB(255,80,90));
    if (page == PAGE_HOME) {
        cv_text(151, 17, &FONT_XS, seq_ui_lane ? "T2" : "T1", patch_color(seq_ui_patch[seq_ui_lane]));
    }
    cv_rect(0, 34, 240, 3, accent);
    cv_blit(0, 0);
    cv_begin(240, 180, screen_bg());
    if (page == PAGE_HOME) draw_home_panel();
    else if (page == PAGE_MACRO) draw_macro_panel();
    else if (page == PAGE_INSTRUMENT_MACRO) draw_instrument_macro_panel();
    else if (page == PAGE_FX_AMP) draw_amp_panel();
    else if (page == PAGE_FX_EQ) draw_eq_panel();
    else if (page == PAGE_FX_COMPRESSOR) draw_compressor_panel();
    else if (page == PAGE_OPERATOR_MAP) draw_operator_map();
    else if (page == PAGE_ENV) draw_envelope_panel();
    else if (page == PAGE_LFO) draw_lfo_panel(0);
    else if (page == PAGE_LFO_MOD) draw_lfo_panel(1);
    else if (page == PAGE_SEQ) draw_sequencer_panel();
    else if (page == PAGE_SEQ_OPTIONS) draw_sequencer_options_panel();
    else if (page == PAGE_SEQ_PIANO) draw_sequencer_piano_panel();
    else if (page == PAGE_SAVE) draw_save_panel();
    else if (page == PAGE_ARP) draw_arp_panel();
    else if (page == PAGE_ARP_STEPS) draw_arp_steps_panel();
    else if (page == PAGE_ARP_LATCH) draw_arp_latch_panel();
    else if (page == PAGE_FX_CHORUS) draw_fx_panel(0u);
    else if (page == PAGE_FX_DELAY) draw_fx_panel(1u);
    else if (page == PAGE_FX_REVERB) draw_fx_panel(2u);
    else if (page == PAGE_FX_PHASER) draw_fx_panel(3u);
    else if (page == PAGE_FX_ROUTING) draw_fx_routing_panel();
    else draw_global_panel(pct);
    if(ui.delete_until && (int32_t)(ui.delete_until-plat_ms())>0)draw_delete_overlay();
    if(page==PAGE_SEQ && seq_clear_prompt)draw_seq_clear_prompt();
    cv_blit(0, 41);

    /* Compact control strip. HOME and ENV already show these values in
     * their main panels, so avoid duplicating them in a second bar. */
    cv_begin(240, 19, screen_bg());
    if (page == PAGE_HOME) {
        int_text((int32_t)instrument_index + 1, t);
        text_cat(t, "/32");
        cv_text(6, 4, &FONT_XS, t, C_GRAY);
    }
    page_counter(page, t, sizeof(t));
    cv_text(240 - text_w(&FONT_XS, t) - 6, 4, &FONT_XS, t, accent);
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
    static uint32_t last_pct, lit_until, last_ble, last_midi, last_arp_redraw;
    uint32_t i, now = plat_ms(), pct = plat_master() * 100u / 4096u;
    input();
    if(seq_record_redraw){ui.redraw=1u;seq_record_redraw=0u;}
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
    /* Transport playheads advance in the audio callback, independently of
     * UI input. Refresh sequencer and ARP views at a steady cadence so their
     * highlights follow the actual audio-owned step position. */
    if (page == PAGE_SEQ || page == PAGE_SEQ_PIANO ||
        (page == PAGE_ARP_STEPS && fm6_arpeggiator_active())) {
        if ((uint32_t)(now - last_arp_redraw) >= 30u) {
            last_arp_redraw = now;
            ui.redraw = 1u;
        }
    }
    if(ui.delete_until && (int32_t)(ui.delete_until-now)>0 &&
       (uint32_t)(now-last_arp_redraw)>=70u){last_arp_redraw=now;ui.redraw=1u;}
    if(ui.delete_until && (int32_t)(now-ui.delete_until)>=0){ui.delete_until=0u;ui.redraw=1u;}
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
    if (!fm1lab_strcmp(what, "octave"))
        *got = set.octave;
    else if (!fm1lab_strcmp(what, "patch") || !fm1lab_strcmp(what, "wave"))
        *got = set.patch;
    else if (!fm1lab_strcmp(what, "note"))
        *got = voice_note;
    else if (!fm1lab_strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!fm1lab_strcmp(what, "page"))
        *got = page;
    else if (!fm1lab_strcmp(what, "cpu"))
        *got = (int)app_audio_cpu_pct();
    else if (!fm1lab_strcmp(what, "xruns"))
        *got = (int)app_audio_xruns();
    else if (!fm1lab_strcmp(what, "shed"))
        *got = (int)app_audio_overload_shed_count();
    else if (!fm1lab_strcmp(what, "vel"))           /* the last key's velocity and pressure */
        *got = ui.vel[ui.last_key];
    else if (!fm1lab_strcmp(what, "prs"))
        *got = ui.prs[ui.last_key];
    else if (!fm1lab_strncmp(what, "turns", 5))    /* turnsN: encoder N's count (plat.h EN_*) */
        *got = ui.turns[atoi(what + 5) % NE];
    else
        return 0;
    return 1;
}
#endif