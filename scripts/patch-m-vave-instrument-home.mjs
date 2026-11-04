import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const screenPath = path.join(root, "vendor/m-vave-fm1/app/screen.c");
let source = fs.readFileSync(screenPath, "utf8");

function replaceOnce(before, after, label) {
  const first = source.indexOf(before);
  if (first < 0 || source.indexOf(before, first + before.length) >= 0) {
    throw new Error("FM1-Lab Instrument home patch: expected one exact match for " + label);
  }
  source = source.slice(0, first) + after + source.slice(first + before.length);
}

if (!source.includes('#include "fm1lab_runtime.h"')) {
  replaceOnce('#include "instrument.h"\n',
    '#include "instrument.h"\n#include "fm1lab_runtime.h"\n',
    "runtime include");
}
if (!source.includes("FM1LAB_HOME")) {
  replaceOnce("ARP_EDIT, STARTUP };", "ARP_EDIT, STARTUP, FM1LAB_HOME };", "page enum");
}
if (!source.includes("static fm1lab_runtime_t fm1lab_home_runtime")) {
  replaceOnce("static uint8_t frame_patch[156];",
    "static uint8_t frame_patch[156];\nstatic fm1lab_runtime_t fm1lab_home_runtime;\nstatic int fm1lab_home_status;
static uint32_t fm1lab_home_signature;",
    "runtime state");
}

if (!source.includes("static void fm1lab_home_draw(void)")) {
  const draw = String.raw`
static void fm1lab_home_draw(void)
{
    if (!paint_rendering) {
        uint32_t state[3] = {fm1lab_home_runtime.instrument_index,
            fm1lab_home_runtime.selected_patch, fm1lab_home_runtime.dirty};
        uint32_t sig = signature(2166136261U, &fm1lab_home_runtime.instrument,
                                 sizeof fm1lab_home_runtime.instrument);
        sig = signature(sig, state, sizeof state);
        if (sig == fm1lab_home_signature) return;
        fm1lab_home_signature = sig;
    }
    paint_fill(0, 0, FM1_SCREEN_WIDTH, FM1_SCREEN_HEIGHT, BACK);
    paint_fill(0, 29, FM1_SCREEN_WIDTH, 1, PANEL);
    if (!fm1lab_home_runtime.loaded) {
        text(8, 8, "INSTRUMENT LIBRARY", 2, ACCENT, BACK);
        text(8, 82, "LIBRARY UNAVAILABLE", 2, AMBER, BACK);
        number(8, 112, fm1lab_home_status, 1, 1, DIM, BACK);
        text(8, 231, "CHECK DX7 BANK STORAGE", 1, DIM, BACK);
        return;
    }

    char name[FM1LAB_INSTRUMENT_NAME_BYTES + 1];
    memcpy(name, fm1lab_home_runtime.instrument.name, FM1LAB_INSTRUMENT_NAME_BYTES);
    name[FM1LAB_INSTRUMENT_NAME_BYTES] = 0;
    text(8, 7, "INSTRUMENT", 1, DIM, BACK);
    text(8, 17, name, 2, ACCENT, BACK);
    char index[12];
    decimal(index, (int)fm1lab_home_runtime.instrument_index + 1, 2);
    text(205, 12, index, 1, DIM, BACK);
    if (fm1lab_home_runtime.dirty) text(8, 33, "UNSAVED", 1, AMBER, BACK);
    else text(8, 33, "FOUR PATCHES", 1, DIM, BACK);

    for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch) {
        unsigned col = patch & 1U, row = patch >> 1;
        unsigned x = 8U + col * 116U, y = 46U + row * 84U;
        const fm1lab_patch_t *part = &fm1lab_home_runtime.instrument.patches[patch];
        uint16_t color = colors[patch];
        paint_fill(x + 1, y + 1, 106, 78, PANEL);
        frame(x, y, 108, 80, patch == fm1lab_home_runtime.selected_patch ? ACCENT : DIM);
        char patch_name[FM1LAB_PATCH_NAME_BYTES + 1];
        memcpy(patch_name, part->voice + 145, FM1LAB_PATCH_NAME_BYTES);
        patch_name[FM1LAB_PATCH_NAME_BYTES] = 0;
        for (int i = FM1LAB_PATCH_NAME_BYTES - 1; i >= 0 && patch_name[i] == ' '; --i)
            patch_name[i] = 0;
        char label[4] = {'P', (char)('1' + patch), 0, 0};
        text(x + 6, y + 6, label, 1, color, PANEL);
        text(x + 26, y + 6, patch_name[0] ? patch_name : "INIT", 1, TEXT, PANEL);
        text(x + 6, y + 27, part->enabled ? "ACTIVE" : "MUTED", 1,
             part->enabled ? GREEN : AMBER, PANEL);
        text(x + 6, y + 43, "ALGO", 1, DIM, PANEL);
        number(x + 38, y + 43, (int)part->voice[134] + 1, 2, 1, TEXT, PANEL);
        text(x + 66, y + 43, "LVL", 1, DIM, PANEL);
        number(x + 86, y + 43, part->level, 3, 1, TEXT, PANEL);
        unsigned bar = (unsigned)part->level * 92U / 127U;
        paint_fill(x + 7, y + 64, 92, 4, BACK);
        if (bar) paint_fill(x + 7, y + 64, bar, 4, color);
    }
    text(8, 222, "SELECT PATCH   ALGORITHM EDIT   PRESETS INSTRUMENT", 1, DIM, BACK);
    text(8, 233, "KNOB 1-4 = PATCH LEVEL     SAVE = STORE INSTRUMENT", 1, DIM, BACK);
}
`;
  replaceOnce("static void draw_page_body(const struct fm1_ble_status *ble)\n{",
    draw + "\nstatic void draw_page_body(const struct fm1_ble_status *ble)\n{",
    "Instrument home drawing");
}

if (!source.includes("if (visible == FM1LAB_HOME) { fm1lab_home_draw(); return; }")) {
  replaceOnce("    if (!paint_rendering) leds(v, visible);\n    if (visible == PRESET_BROWSER) {",
    "    if (!paint_rendering) leds(v, visible);\n    if (visible == FM1LAB_HOME) { fm1lab_home_draw(); return; }\n    if (visible == PRESET_BROWSER) {",
    "Instrument home render dispatch");
}

if (!source.includes("fm1lab_runtime_load(&fm1lab_home_runtime, 0)")) {
  replaceOnce("    track = cursor = drum = writing = page = fm_group = op = 0;",
    "    track = cursor = drum = writing = fm_group = op = 0;\n    groove_fm_library_init();\n    memset(&fm1lab_home_runtime, 0, sizeof fm1lab_home_runtime);\n    fm1lab_home_status = fm1lab_runtime_load(&fm1lab_home_runtime, 0);\n    page = FM1LAB_HOME;",
    "runtime startup load");
  replaceOnce("    music_page = subpage_return = project_return = rec_before_press = OVERVIEW;",
    "    music_page = FM1LAB_HOME;\n    subpage_return = project_return = rec_before_press = OVERVIEW;",
    "music home page");
}

if (!source.includes("FM1LAB_HOME && !(pressed")) {
  const controls = String.raw`
    if (page == FM1LAB_HOME && (pressed & (BUTTON(EDIT) | BUTTON(SCL)))) {
        notice = "PATCH EDITOR NEXT - SAFE BANK MODE";
        notice_until = now + 1800U;
        goto controls_done;
    }
    if (page == FM1LAB_HOME && !(pressed & (BUTTON(HOME) | BUTTON(SEQ) | BUTTON(FX) |
        BUTTON(SCL) | BUTTON(EDIT) | BUTTON(ENV) | BUTTON(LFO) | BUTTON(PLAY)))) {
        if (pressed & BUTTON(SAVE)) {
            int saved = fm1lab_home_runtime.loaded ? fm1lab_runtime_save(&fm1lab_home_runtime) : -1;
            notice = saved ? "INSTRUMENT SAVE FAILED" : "INSTRUMENT SAVED";
            notice_until = now + 1800U;
            goto controls_done;
        }
        if (fm1lab_home_runtime.loaded) {
            int select = controls->encoders[FM1LAB_ENCODER_SELECT];
            if (select) {
                int patch = ((int)fm1lab_home_runtime.selected_patch + select) % FM1LAB_PATCH_COUNT;
                if (patch < 0) patch += FM1LAB_PATCH_COUNT;
                (void)fm1lab_runtime_select_patch(&fm1lab_home_runtime, (unsigned)patch);
            }
            int preset = controls->encoders[FM1LAB_ENCODER_PRESETS];
            if (preset) {
                int result = fm1lab_runtime_select_instrument(&fm1lab_home_runtime, preset);
                notice = result == -2 ? "SAVE INSTRUMENT BEFORE CHANGING"
                    : result ? "INSTRUMENT LOAD FAILED" : "INSTRUMENT LOADED";
                notice_until = now + 1800U;
            }
            for (unsigned patch = 0; patch < FM1LAB_PATCH_COUNT; ++patch) {
                int delta = controls->encoders[FM1LAB_ENCODER_KNOB_1 + patch];
                if (delta) (void)fm1lab_runtime_set_level(&fm1lab_home_runtime, patch, delta);
            }
            int algorithm = controls->encoders[FM1LAB_ENCODER_ALGORITHM];
            if (algorithm) {
                fm1lab_patch_t *part = &fm1lab_home_runtime.instrument.patches[
                    fm1lab_home_runtime.selected_patch];
                int value = clamp((int)part->voice[134] + algorithm, 0, 31);
                if (value != part->voice[134]) {
                    part->voice[134] = (uint8_t)value;
                    fm1lab_home_runtime.dirty = 1;
                }
            }
        }
        if (encoder_active(controls->encoders) || (pressed & BUTTON(SAVE)))
            goto controls_done;
    }
`;
  replaceOnce("    if (pressed & BUTTON(SAVE)) {\n        project_return = page; page = PROJECT;",
    controls + "\n    if (pressed & BUTTON(SAVE)) {\n        project_return = page; page = PROJECT;",
    "Instrument home controls");
}

source = source.replaceAll("page = music_page = OVERVIEW;", "page = music_page = FM1LAB_HOME;");
source = source.replace("            else if (page == OVERVIEW) {",
  "            else if (page == OVERVIEW || page == FM1LAB_HOME) {");
source = source.replace("    if (page == OVERVIEW || page == STEPS) music_page = page;",
  "    if (page == OVERVIEW || page == STEPS || page == FM1LAB_HOME) music_page = page;");
source = source.replace("            } else page = OVERVIEW;\n        } else if (pressed & BUTTON(SEQ)) {",
  "            } else page = FM1LAB_HOME;\n        } else if (pressed & BUTTON(SEQ)) {");
source = source.replace("    page = music_page = OVERVIEW;\n    groove_edit_record(0, track, cursor);",
  "    page = music_page = FM1LAB_HOME;\n    groove_edit_record(0, track, cursor);");
fs.writeFileSync(screenPath, source);
console.log("Applied FM1-Lab four-patch Instrument home screen and panel controls.");
