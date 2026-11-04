import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const screenPath = path.join(root, "vendor/m-vave-fm1/app/screen.c");
let source = fs.readFileSync(screenPath, "utf8");
function replaceOnce(before, after, label) {
  const first = source.indexOf(before);
  if (first < 0 || source.indexOf(before, first + before.length) >= 0)
    throw new Error("FM1-Lab native patch editor: expected one exact match for " + label);
  source = source.slice(0, first) + after + source.slice(first + before.length);
}

if (!source.includes('#include "fm1lab_voice_editor.h"')) {
  replaceOnce('#include "fm1lab_runtime.h"\n',
    '#include "fm1lab_runtime.h"\n#include "fm1lab_voice_editor.h"\n',
    "voice editor include");
}
if (!source.includes("FM1LAB_PATCH")) {
  replaceOnce("FM1LAB_HOME, FM1LAB_SEQ };",
    "FM1LAB_HOME, FM1LAB_SEQ, FM1LAB_PATCH };", "patch editor page");
}
if (!source.includes("static unsigned fm1lab_patch_operator")) {
  replaceOnce("static uint32_t fm1lab_seq_signature;",
    "static uint32_t fm1lab_seq_signature;\nstatic unsigned fm1lab_patch_operator = 1;\nstatic unsigned fm1lab_patch_bank;\nstatic unsigned fm1lab_patch_env_levels;",
    "patch editor state");
}

if (!source.includes("static void fm1lab_patch_draw(void)")) {
  const helpers = String.raw\`
static uint8_t *fm1lab_patch_voice(void)
{
    if (!fm1lab_home_runtime.loaded ||
        fm1lab_home_runtime.selected_patch >= FM1LAB_PATCH_COUNT) return NULL;
    return fm1lab_home_runtime.instrument.patches[
        fm1lab_home_runtime.selected_patch].voice;
}

static void fm1lab_patch_adjust_operator(fm1lab_operator_field_t field, int delta)
{
    uint8_t *voice = fm1lab_patch_voice();
    uint8_t current;
    if (!voice || !delta ||
        fm1lab_voice_operator_get(voice, fm1lab_patch_operator, field, &current)) return;
    int maximum = field == FM1LAB_OP_MODE ? 1 :
        field == FM1LAB_OP_COARSE ? 31 : field == FM1LAB_OP_DETUNE ? 14 : 99;
    int next = clamp((int)current + delta, 0, maximum);
    if (next != current && !fm1lab_voice_operator_set(
            voice, fm1lab_patch_operator, field, (unsigned)next))
        fm1lab_home_runtime.dirty = 1;
}

static void fm1lab_patch_adjust_global(fm1lab_global_field_t field, int delta)
{
    uint8_t *voice = fm1lab_patch_voice();
    uint8_t current;
    if (!voice || !delta || fm1lab_voice_global_get(voice, field, &current)) return;
    int maximum = field == FM1LAB_GLOBAL_FEEDBACK ? 7 :
        field == FM1LAB_GLOBAL_ALGORITHM ? 32 : 99;
    int minimum = field == FM1LAB_GLOBAL_ALGORITHM ? 1 : 0;
    int next = clamp((int)current + delta, minimum, maximum);
    if (next != current && !fm1lab_voice_global_set(voice, field, (unsigned)next))
        fm1lab_home_runtime.dirty = 1;
}

static void fm1lab_patch_draw(void)
{
    uint8_t *voice = fm1lab_patch_voice();
    paint_fill(0, 0, 240, 240, BACK);
    if (!voice) {
        text(8, 12, "PATCH EDITOR", 2, ACCENT, BACK);
        text(8, 100, "INSTRUMENT NOT LOADED", 1, AMBER, BACK);
        return;
    }
    char patch_name[11];
    (void)fm1lab_voice_name_get(voice, patch_name);
    uint8_t algorithm = 0;
    (void)fm1lab_voice_global_get(voice, FM1LAB_GLOBAL_ALGORITHM, &algorithm);
    text(8, 5, "PATCH EDITOR", 1, DIM, BACK);
    text(8, 17, patch_name[0] ? patch_name : "UNTITLED", 2, ACCENT, BACK);
    text(160, 8, "PATCH", 1, DIM, BACK);
    number(202, 8, (int)fm1lab_home_runtime.selected_patch + 1, 1, 1, TEXT, BACK);
    text(160, 20, "ALG", 1, DIM, BACK);
    number(202, 20, algorithm, 2, 1, TEXT, BACK);

    for (unsigned op = 1; op <= 6; ++op) {
        unsigned x = 7U + (op - 1U) * 38U;
        int carrier = fm1lab_voice_operator_is_carrier(voice, op);
        unsigned depth = fm1lab_voice_operator_depth(voice, op);
        frame(x, 48, 34, 43, op == fm1lab_patch_operator ? ACCENT : DIM);
        char label[4] = {'O', 'P', (char)('0' + op), 0};
        text(x + 3, 53, label, 1, TEXT, PANEL);
        text(x + 4, 67, carrier ? "CAR" : "MOD", 1,
             carrier ? GREEN : AMBER, PANEL);
        number(x + 13, 80, (int)depth, 1, 1, DIM, PANEL);
    }
    text(8, 101, "TOPOLOGY", 1, DIM, BACK);
    text(74, 101, "CARRIER / MODULATOR DEPTH", 1, DIM, BACK);
    text(8, 120, fm1lab_patch_bank == 0 ? "ENVELOPE" :
        fm1lab_patch_bank == 1 ? "OSCILLATOR" : "GLOBAL", 2, ACCENT, BACK);

    if (fm1lab_patch_bank == 0) {
        text(8, 145, fm1lab_patch_env_levels ? "LEVELS" : "RATES", 1, DIM, BACK);
        static const char *rate_names[4] = {"R1", "R2", "R3", "R4"};
        static const char *level_names[4] = {"L1", "L2", "L3", "L4"};
        for (unsigned i = 0; i < 4; ++i) {
            uint8_t value = 0;
            (void)fm1lab_voice_operator_get(voice, fm1lab_patch_operator,
                fm1lab_patch_env_levels ? (fm1lab_operator_field_t)(FM1LAB_OP_LEVEL_1 + i)
                                        : (fm1lab_operator_field_t)(FM1LAB_OP_RATE_1 + i),
                &value);
            unsigned x = 8U + i * 57U;
            text(x, 164, fm1lab_patch_env_levels ? level_names[i] : rate_names[i],
                 1, DIM, BACK);
            number(x, 181, value, 3, 1, TEXT, BACK);
            paint_fill(x, 201, 42, 5, PANEL);
            paint_fill(x, 201, (unsigned)value * 42U / 99U, 5, colors[i]);
        }
        text(8, 218, "ENV TOGGLES RATE / LEVEL VIEW", 1, DIM, BACK);
    } else if (fm1lab_patch_bank == 1) {
        static const fm1lab_operator_field_t fields[4] = {
            FM1LAB_OP_OUTPUT, FM1LAB_OP_COARSE, FM1LAB_OP_FINE, FM1LAB_OP_DETUNE
        };
        static const char *labels[4] = {"OUTPUT", "COARSE", "FINE", "DETUNE"};
        for (unsigned i = 0; i < 4; ++i) {
            uint8_t value = 0;
            (void)fm1lab_voice_operator_get(voice, fm1lab_patch_operator, fields[i], &value);
            unsigned x = 8U + i * 57U;
            text(x, 164, labels[i], 1, DIM, BACK);
            number(x, 181, value, 3, 1, TEXT, BACK);
            unsigned maximum = fields[i] == FM1LAB_OP_COARSE ? 31U :
                fields[i] == FM1LAB_OP_DETUNE ? 14U : 99U;
            paint_fill(x, 201, 42, 5, PANEL);
            paint_fill(x, 201, (unsigned)value * 42U / maximum, 5, colors[i]);
        }
        text(8, 218, "RATIO MODE / FREQUENCY VALUES", 1, DIM, BACK);
    } else {
        static const fm1lab_global_field_t fields[4] = {
            FM1LAB_GLOBAL_FEEDBACK, FM1LAB_GLOBAL_LFO_SPEED,
            FM1LAB_GLOBAL_LFO_DELAY, FM1LAB_GLOBAL_LFO_PITCH_DEPTH
        };
        static const char *labels[4] = {"FEEDBACK", "LFO SPEED", "LFO DELAY", "PITCH DEPTH"};
        for (unsigned i = 0; i < 4; ++i) {
            uint8_t value = 0;
            (void)fm1lab_voice_global_get(voice, fields[i], &value);
            unsigned x = 8U + i * 57U;
            text(x, 164, labels[i], 1, DIM, BACK);
            number(x, 181, value, 3, 1, TEXT, BACK);
            unsigned maximum = fields[i] == FM1LAB_GLOBAL_FEEDBACK ? 7U : 99U;
            paint_fill(x, 201, 42, 5, PANEL);
            paint_fill(x, 201, (unsigned)value * 42U / maximum, 5, colors[i]);
        }
        text(8, 218, "ALGORITHM ENCODER CHANGES ROUTING", 1, DIM, BACK);
    }
    text(8, 231, "SELECT OP   PRESETS PATCH   SCL SECTION   SAVE STORE", 1, DIM, BACK);
}
\`;
  replaceOnce("static void draw_page_body(const struct fm1_ble_status *ble)\n{",
    helpers + "\nstatic void draw_page_body(const struct fm1_ble_status *ble)\n{",
    "native patch editor draw helpers");
}

if (!source.includes("if (visible == FM1LAB_PATCH) { fm1lab_patch_draw(); return; }")) {
  replaceOnce("    if (visible == FM1LAB_SEQ) { fm1lab_seq_draw(); return; }",
    "    if (visible == FM1LAB_SEQ) { fm1lab_seq_draw(); return; }\n    if (visible == FM1LAB_PATCH) { fm1lab_patch_draw(); return; }",
    "native patch editor render dispatch");
}

if (!source.includes("FM1-LAB native patch editor controls")) {
  const controls = String.raw\`
    /* All edits target the selected Instrument patch, never project tracks. */
    if (page == FM1LAB_HOME && (pressed & (BUTTON(EDIT) | BUTTON(SCL)))) {
        if (fm1lab_home_runtime.loaded) {
            fm1lab_patch_operator = 1;
            fm1lab_patch_bank = 0;
            fm1lab_patch_env_levels = 0;
            page = FM1LAB_PATCH;
        } else {
            notice = "INSTRUMENT LIBRARY UNAVAILABLE";
            notice_until = now + 1800U;
        }
        goto controls_done;
    }
    if (page == FM1LAB_PATCH) {
        if (pressed & (BUTTON(HOME) | BUTTON(EDIT))) {
            page = FM1LAB_HOME;
            goto controls_done;
        }
        if (pressed & BUTTON(SCL)) {
            fm1lab_patch_bank = (fm1lab_patch_bank + 1U) % 3U;
            goto controls_done;
        }
        if (pressed & BUTTON(ENV)) {
            fm1lab_patch_env_levels ^= 1U;
            goto controls_done;
        }
        if (pressed & BUTTON(SAVE)) {
            int saved = fm1lab_runtime_save(&fm1lab_home_runtime);
            notice = saved ? "INSTRUMENT SAVE FAILED" : "INSTRUMENT SAVED";
            notice_until = now + 1800U;
            goto controls_done;
        }
        if (fm1lab_patch_voice()) {
            int op_delta = controls->encoders[FM1LAB_ENCODER_SELECT];
            if (op_delta) {
                int next = ((int)fm1lab_patch_operator - 1 + op_delta) % 6;
                if (next < 0) next += 6;
                fm1lab_patch_operator = (unsigned)next + 1U;
            }
            int patch_delta = controls->encoders[FM1LAB_ENCODER_PRESETS];
            if (patch_delta) {
                int next = ((int)fm1lab_home_runtime.selected_patch + patch_delta) % 4;
                if (next < 0) next += 4;
                (void)fm1lab_runtime_select_patch(&fm1lab_home_runtime, (unsigned)next);
            }
            fm1lab_patch_adjust_global(FM1LAB_GLOBAL_ALGORITHM,
                controls->encoders[FM1LAB_ENCODER_ALGORITHM]);
            if (fm1lab_patch_bank == 0) {
                for (unsigned i = 0; i < 4; ++i) {
                    fm1lab_operator_field_t field = fm1lab_patch_env_levels
                        ? (fm1lab_operator_field_t)(FM1LAB_OP_LEVEL_1 + i)
                        : (fm1lab_operator_field_t)(FM1LAB_OP_RATE_1 + i);
                    fm1lab_patch_adjust_operator(field,
                        controls->encoders[FM1LAB_ENCODER_KNOB_1 + i]);
                }
            } else if (fm1lab_patch_bank == 1) {
                static const fm1lab_operator_field_t fields[4] = {
                    FM1LAB_OP_OUTPUT, FM1LAB_OP_COARSE, FM1LAB_OP_FINE, FM1LAB_OP_DETUNE
                };
                for (unsigned i = 0; i < 4; ++i)
                    fm1lab_patch_adjust_operator(fields[i],
                        controls->encoders[FM1LAB_ENCODER_KNOB_1 + i]);
            } else {
                static const fm1lab_global_field_t fields[4] = {
                    FM1LAB_GLOBAL_FEEDBACK, FM1LAB_GLOBAL_LFO_SPEED,
                    FM1LAB_GLOBAL_LFO_DELAY, FM1LAB_GLOBAL_LFO_PITCH_DEPTH
                };
                for (unsigned i = 0; i < 4; ++i)
                    fm1lab_patch_adjust_global(fields[i],
                        controls->encoders[FM1LAB_ENCODER_KNOB_1 + i]);
            }
        }
        if (encoder_active(controls->encoders)) goto controls_done;
    }
\`;
  replaceOnce("    if ((controls->buttons & (BUTTON(SEQ) | BUTTON(EDIT))) == (BUTTON(SEQ) | BUTTON(EDIT)) &&",
    "    /* FM1-LAB native patch editor controls */\n" + controls +
    "\n    if ((controls->buttons & (BUTTON(SEQ) | BUTTON(EDIT))) == (BUTTON(SEQ) | BUTTON(EDIT)) &&",
    "native patch editor controls");
}

fs.writeFileSync(screenPath, source);
console.log("Applied FM1-Lab native six-operator patch editor.");
