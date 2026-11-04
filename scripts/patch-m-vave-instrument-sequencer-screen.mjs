import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const screenPath = path.join(root, "vendor/m-vave-fm1/app/screen.c");
let source = fs.readFileSync(screenPath, "utf8");
function replaceOnce(before, after, label) {
  const first = source.indexOf(before);
  if (first < 0 || source.indexOf(before, first + before.length) >= 0)
    throw new Error("FM1-Lab sequencer screen: expected one exact match for " + label);
  source = source.slice(0, first) + after + source.slice(first + before.length);
}
if (!source.includes('#include "fm1lab_sequencer.h"')) {
  replaceOnce('#include "instrument.h"\n',
    '#include "instrument.h"\n#include "fm1lab_sequencer.h"\n#include "fm1lab_physical_controls.h"\n',
    "sequencer includes");
}
if (!source.includes("FM1LAB_SEQ"))
  replaceOnce("ARP_EDIT, STARTUP, FM1LAB_HOME };", "ARP_EDIT, STARTUP, FM1LAB_HOME, FM1LAB_SEQ };", "sequencer page enum");
if (!source.includes("static fm1lab_sequence_state_t fm1lab_seq_state"))
  replaceOnce("static uint32_t fm1lab_home_signature;",
    "static uint32_t fm1lab_home_signature;\nstatic fm1lab_sequence_state_t fm1lab_seq_state;\nstatic unsigned fm1lab_seq_cursor;\nstatic int fm1lab_seq_status;\nstatic uint32_t fm1lab_seq_signature;",
    "sequencer state");

if (!source.includes("static void fm1lab_seq_draw(void)")) {
  const helpers = String.raw`
static int fm1lab_seq_bind_tracks(void)
{
    unsigned found[2] = {GROOVE_TRACKS, GROOVE_TRACKS};
    unsigned count = 0;
    for (unsigned t = 0; t < GROOVE_TRACKS && count < 2; ++t) {
        if (groove_track_role(t) == GROOVE_TRACK_NORMAL &&
            groove_source_type(t) == GROOVE_DX7) found[count++] = t;
    }
    if (count != 2) return -1;
    return fm1lab_sequencer_bind(found[0], found[1]);
}

static void fm1lab_seq_draw(void)
{
    fm1lab_seq_status = fm1lab_sequencer_read(&fm1lab_seq_state);
    if (fm1lab_seq_status) {
        paint_fill(0, 0, FM1_SCREEN_WIDTH, FM1_SCREEN_HEIGHT, BACK);
        text(8, 10, "TWO-TRACK SEQUENCER", 2, ACCENT, BACK);
        text(8, 90, "NO TWO FM TRACKS BOUND", 2, AMBER, BACK);
        text(8, 230, "ADD TWO DX7 TRACKS TO PROJECT", 1, DIM, BACK);
        return;
    }
    if (!paint_rendering) {
        uint32_t sig = signature(2166136261U, &fm1lab_seq_state, sizeof fm1lab_seq_state);
        sig = signature(sig, &fm1lab_seq_cursor, sizeof fm1lab_seq_cursor);
        if (sig == fm1lab_seq_signature) return;
        fm1lab_seq_signature = sig;
    }
    paint_fill(0, 0, FM1_SCREEN_WIDTH, FM1_SCREEN_HEIGHT, BACK);
    text(8, 5, "SEQUENCER", 1, DIM, BACK);
    text(8, 17, fm1lab_seq_state.playing ? "PLAYING" : "STOPPED", 2,
         fm1lab_seq_state.playing ? GREEN : AMBER, BACK);
    text(154, 9, "BPM", 1, DIM, BACK);
    number(180, 8, fm1lab_seq_state.tempo, 3, 1, TEXT, BACK);
    for (unsigned lane = 0; lane < 2; ++lane) {
        unsigned y = 48U + lane * 78U;
        uint16_t accent = lane == fm1lab_seq_state.selected_lane ? ACCENT : DIM;
        frame(8, y, 224, 70, accent);
        text(14, y + 5, lane ? "B / TRACK 2" : "A / TRACK 1", 1, accent, PANEL);
        text(142, y + 5, "PATCH", 1, DIM, PANEL);
        number(184, y + 5, fm1lab_seq_state.lanes[lane].patch_index + 1, 1, 1, TEXT, PANEL);
        unsigned first_step = (fm1lab_seq_cursor / 16U) * 16U;
        for (unsigned step = 0; step < 16; ++step) {
            unsigned absolute_step = first_step + step;
            unsigned x = 14U + step * 13U;
            const fm1lab_sequence_step_t *v = &fm1lab_seq_state.lanes[lane].steps[absolute_step];
            uint16_t color = v->active ? colors[lane] : PANEL;
            paint_fill(x, y + 24, 10, 27, color);
            if (lane == fm1lab_seq_state.selected_lane && absolute_step == fm1lab_seq_cursor)
                frame(x - 1, y + 23, 12, 29, ACCENT);
            if (v->active) {
                unsigned h = 4U + (unsigned)v->velocity * 17U / 127U;
                paint_fill(x + 3, y + 48U - h, 4, h, TEXT);
            }
        }
        text(14, y + 55, "LEN", 1, DIM, PANEL);
        number(45, y + 55, fm1lab_seq_state.lanes[lane].length, 2, 1, TEXT, PANEL);
        text(88, y + 55, "SWING", 1, DIM, PANEL);
        number(132, y + 55, fm1lab_seq_state.swing, 2, 1, TEXT, PANEL);
    }
    text(8, 210, "PAGE", 1, DIM, BACK);
    number(45, 210, (int)(fm1lab_seq_cursor / 16U) + 1, 1, 1, TEXT, BACK);
    text(88, 210, "SELECT STEP  PRESETS LANE", 1, DIM, BACK);
    text(8, 226, "KNOB 1 ON/OFF  2 PITCH  3 VELOCITY  4 GATE", 1, DIM, BACK);
}
`;
  replaceOnce("static void draw_page_body(const struct fm1_ble_status *ble)\n{",
    helpers + "\nstatic void draw_page_body(const struct fm1_ble_status *ble)\n{",
    "sequencer draw helpers");
}
if (!source.includes("if (visible == FM1LAB_SEQ) { fm1lab_seq_draw(); return; }"))
  replaceOnce("    if (visible == FM1LAB_HOME) { fm1lab_home_draw(); return; }",
    "    if (visible == FM1LAB_HOME) { fm1lab_home_draw(); return; }\n    if (visible == FM1LAB_SEQ) { fm1lab_seq_draw(); return; }",
    "sequencer render dispatch");
if (!source.includes("fm1lab_seq_status = fm1lab_seq_bind_tracks(); page = FM1LAB_SEQ;"))
  replaceOnce("            page = STEPS; seq_page = 0; step_follow = 1; cache->shown_page = ~0U;",
    "            if (page == FM1LAB_HOME || page == FM1LAB_SEQ) {\n                if (page == FM1LAB_SEQ) { page = FM1LAB_HOME; }\n                else { fm1lab_seq_status = fm1lab_seq_bind_tracks(); page = FM1LAB_SEQ; fm1lab_seq_cursor = 0; }\n            } else { page = STEPS; seq_page = 0; step_follow = 1; }\n            cache->shown_page = ~0U;",
    "SEQ navigation");
if (!source.includes("if (page == FM1LAB_SEQ && (pressed & BUTTON(PLAY)))"))
  replaceOnce("    if ((pressed & BUTTON(PLAY)) && !play_handled) {",
    "    if (page == FM1LAB_SEQ && (pressed & BUTTON(PLAY))) {\n        (void)fm1lab_sequencer_transport(!fm1lab_seq_state.playing);\n        play_handled = 1;\n    }\n    if ((pressed & BUTTON(PLAY)) && !play_handled) {",
    "sequencer transport button");
if (!source.includes("fm1lab_seq_cursor = (unsigned)clamp")) {
  replaceOnce("    if (page == FM1LAB_HOME && (pressed & (BUTTON(EDIT) | BUTTON(SCL)))) {",
    "    if (page == FM1LAB_SEQ) {\n        (void)fm1lab_sequencer_read(&fm1lab_seq_state);\n        int step_delta = controls->encoders[FM1LAB_ENCODER_SELECT];\n        if (step_delta) fm1lab_seq_cursor = (unsigned)clamp((int)fm1lab_seq_cursor + step_delta, 0, 63);\n        int lane_delta = controls->encoders[FM1LAB_ENCODER_PRESETS];\n        if (lane_delta) {\n            unsigned next_lane = lane_delta > 0 ? 1U : 0U;\n            if (!fm1lab_sequencer_select_lane(next_lane))\n                fm1lab_seq_state.selected_lane = (uint8_t)next_lane;\n        }\n        unsigned lane = fm1lab_seq_state.selected_lane;\n        unsigned step = fm1lab_seq_cursor;\n        fm1lab_sequence_step_t value = fm1lab_seq_state.lanes[lane].steps[step];\n        int on = controls->encoders[FM1LAB_ENCODER_KNOB_1];\n        int pitch = controls->encoders[FM1LAB_ENCODER_KNOB_2];\n        int velocity = controls->encoders[FM1LAB_ENCODER_KNOB_3];\n        int gate = controls->encoders[FM1LAB_ENCODER_KNOB_4];\n        if (on) value.active = on > 0 ? 1U : 0U;\n        if (pitch) value.note = (uint8_t)clamp((int)value.note + pitch, 0, 127);\n        if (velocity) value.velocity = (uint8_t)clamp((int)value.velocity + velocity, 1, 127);\n        if (gate) value.gate = (uint8_t)clamp((int)value.gate + gate, 0, 15);\n        if (on || pitch || velocity || gate) (void)fm1lab_sequencer_set_step(lane, step, &value);\n        int patch_delta = controls->encoders[FM1LAB_ENCODER_ALGORITHM];\n        if (patch_delta && fm1lab_home_runtime.loaded) {\n            unsigned patch = (unsigned)clamp((int)fm1lab_seq_state.lanes[lane].patch_index + patch_delta, 0, 3);\n            (void)fm1lab_sequencer_set_patch(lane, &fm1lab_home_runtime.instrument, patch);\n        }\n        if (encoder_active(controls->encoders) || (pressed & BUTTON(PLAY))) goto controls_done;\n    }\n\n    if (page == FM1LAB_HOME && (pressed & (BUTTON(EDIT) | BUTTON(SCL)))) {",
    "sequencer encoder controls");
}
source = source.replace("    if (page == OVERVIEW || page == STEPS) music_page = page;",
  "    if (page == OVERVIEW || page == STEPS || page == FM1LAB_HOME) music_page = page;");
source = source.replace("            } else page = OVERVIEW;\n        } else if (pressed & BUTTON(SEQ)) {",
  "            } else page = FM1LAB_HOME;\n        } else if (pressed & BUTTON(SEQ)) {");
if (!source.includes("old_page == FM1LAB_SEQ && page != FM1LAB_SEQ")) {
  replaceOnce("controls_done:\\n    if (page != old_page) {",
    "controls_done:\\n    /* Leaving the FM1-Lab sequencer stops its clock and restores all eight project tracks. */\\n    if (old_page == FM1LAB_SEQ && page != FM1LAB_SEQ) {\\n        (void)fm1lab_sequencer_transport(0);\\n        (void)fm1lab_sequencer_unbind();\\n        fm1lab_seq_signature = 0;\\n    }\\n    if (page != old_page) {",
    "sequencer exit cleanup");
}
fs.writeFileSync(screenPath, source);
console.log("Applied FM1-Lab two-lane sequencer screen and named panel controls.");
