import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const screenPath = path.join(root, "vendor/m-vave-fm1/app/screen.c");
let source = fs.readFileSync(screenPath, "utf8");
function replaceOnce(before, after, label) {
  const first = source.indexOf(before);
  if (first < 0 || source.indexOf(before, first + before.length) >= 0)
    throw new Error("FM1-Lab macro workflow: expected one exact match for " + label);
  source = source.slice(0, first) + after + source.slice(first + before.length);
}
if (!source.includes('#include "fm1lab_runtime.h"'))
  throw new Error("FM1-Lab macro workflow must run after the Instrument home patch.");
if (!source.includes("FM1LAB_MACRO_PAGE"))
  replaceOnce("FM1LAB_HOME, FM1LAB_SEQ, FM1LAB_PATCH };",
    "FM1LAB_HOME, FM1LAB_SEQ, FM1LAB_PATCH, FM1LAB_MACRO_PAGE };", "macro page enum");
if (!source.includes("static unsigned fm1lab_macro_return_page"))
  replaceOnce("static unsigned fm1lab_patch_env_levels;",
    "static unsigned fm1lab_patch_env_levels;\nstatic unsigned fm1lab_macro_return_page = FM1LAB_HOME;", "macro page state");
if (!source.includes("static void fm1lab_macro_draw(void)")) {
const draw = String.raw`
static void fm1lab_macro_draw(void)
{
    paint_fill(0, 0, FM1_SCREEN_WIDTH, FM1_SCREEN_HEIGHT, BACK);
    if (!fm1lab_home_runtime.loaded || fm1lab_home_runtime.selected_patch >= FM1LAB_PATCH_COUNT) {
        text(8, 10, "INSTRUMENT MACROS", 2, ACCENT, BACK);
        text(8, 100, "INSTRUMENT NOT LOADED", 1, AMBER, BACK);
        return;
    }
    const fm1lab_patch_t *patch = &fm1lab_home_runtime.instrument.patches[fm1lab_home_runtime.selected_patch];
    char patch_name[FM1LAB_PATCH_NAME_BYTES + 1];
    (void)fm1lab_voice_name_get(patch->voice, patch_name);
    text(8, 5, "PERFORMANCE MACROS", 1, DIM, BACK);
    text(8, 17, patch_name[0] ? patch_name : "UNTITLED PATCH", 2, ACCENT, BACK);
    text(184, 8, "PATCH", 1, DIM, BACK);
    number(218, 8, (int)fm1lab_home_runtime.selected_patch + 1, 1, 1, TEXT, BACK);
    if (fm1lab_home_runtime.dirty) text(8, 37, "UNSAVED", 1, AMBER, BACK);
    static const char *const names[FM1LAB_MACRO_COUNT] = {"BRIGHTNESS", "BODY", "MOTION", "DIRT"};
    static const char *const descriptions[FM1LAB_MACRO_COUNT] = {
        "MODULATOR ENERGY", "MOD RATIO + ENVELOPE", "LFO SPEED + DEPTH", "FEEDBACK + DETUNE"
    };
    for (unsigned i = 0; i < FM1LAB_MACRO_COUNT; ++i) {
        unsigned y = 49U + i * 43U, value = patch->macros[i];
        uint16_t tint = colors[i];
        frame(8, y, 224, 38, tint);
        text(15, y + 5, names[i], 1, tint, BACK);
        text(15, y + 19, descriptions[i], 1, DIM, BACK);
        number(187, y + 7, (int)value, 3, 1, TEXT, BACK);
        paint_fill(15, y + 32, 196, 3, PANEL);
        paint_fill(15, y + 32, value * 196U / 100U, 3, tint);
        paint_fill(113, y + 29, 1, 9, ACCENT);
    }
    text(8, 225, "KNOB 1-4 ADJUST   50 = NEUTRAL   SAVE = STORE", 1, DIM, BACK);
    text(8, 236, "GLO / HOME RETURNS TO INSTRUMENT", 1, DIM, BACK);
}
`;
replaceOnce("static void draw_page_body(const struct fm1_ble_status *ble)\n{",draw+"\nstatic void draw_page_body(const struct fm1_ble_status *ble)\n{","macro page drawing");
}
if (!source.includes("if (visible == FM1LAB_MACRO_PAGE) { fm1lab_macro_draw(); return; }"))
replaceOnce("    if (visible == FM1LAB_PATCH) { fm1lab_patch_draw(); return; }",
"    if (visible == FM1LAB_PATCH) { fm1lab_patch_draw(); return; }\n    if (visible == FM1LAB_MACRO_PAGE) { fm1lab_macro_draw(); return; }","macro page render dispatch");
if (!source.includes("FM1-LAB performance macro controls")) {
const controls = String.raw`
    if ((page == FM1LAB_HOME || page == FM1LAB_PATCH) && (pressed & BUTTON(GLO)) && !(pressed & BUTTON(HOME))) {
        fm1lab_macro_return_page = (unsigned)page;
        page = FM1LAB_MACRO_PAGE;
        goto controls_done;
    }
    if (page == FM1LAB_MACRO_PAGE) {
        if (pressed & (BUTTON(HOME) | BUTTON(GLO))) {
            page = fm1lab_macro_return_page == FM1LAB_PATCH ? FM1LAB_PATCH : FM1LAB_HOME;
            goto controls_done;
        }
        if (pressed & BUTTON(SAVE)) {
            int saved = fm1lab_runtime_save(&fm1lab_home_runtime);
            notice = saved ? "INSTRUMENT SAVE FAILED" : "INSTRUMENT SAVED";
            notice_until = now + 1800U;
            goto controls_done;
        }
        if (fm1lab_home_runtime.loaded) {
            int patch_delta = controls->encoders[FM1LAB_ENCODER_PRESETS];
            if (patch_delta) {
                int next = ((int)fm1lab_home_runtime.selected_patch + patch_delta) % FM1LAB_PATCH_COUNT;
                if (next < 0) next += FM1LAB_PATCH_COUNT;
                (void)fm1lab_runtime_select_patch(&fm1lab_home_runtime, (unsigned)next);
            }
            for (unsigned macro = 0; macro < FM1LAB_MACRO_COUNT; ++macro) {
                int delta = controls->encoders[FM1LAB_ENCODER_KNOB_1 + macro];
                if (delta) {
                    unsigned current = fm1lab_home_runtime.instrument.patches[
                        fm1lab_home_runtime.selected_patch].macros[macro];
                    int value = clamp((int)current + delta, 0, 100);
                    (void)fm1lab_runtime_set_macro(&fm1lab_home_runtime,
                        fm1lab_home_runtime.selected_patch, macro, (unsigned)value);
                }
            }
        }
        if (encoder_active(controls->encoders)) goto controls_done;
    }
`;
replaceOnce("    /* Panel shortcuts precede modal editors.",
"    /* FM1-LAB performance macro controls */\n"+controls+"\n    /* FM1-LAB native patch editor controls */","macro page controls");
}
fs.writeFileSync(screenPath, source);
console.log("Applied FM1-Lab four-macro performance page and physical controls.");
