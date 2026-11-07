import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const app = path.join(root, "vendor/m-vave-fm1/app");

function replaceOnce(source, before, after, label) {
  const first = source.indexOf(before);
  if (first < 0 || source.indexOf(before, first + before.length) >= 0)
    throw new Error("FM1-Lab audio integration: expected one exact match for " + label);
  return source.slice(0, first) + after + source.slice(first + before.length);
}

const audioPath = path.join(app, "board/audio.c");
let audio = fs.readFileSync(audioPath, "utf8");
if (!audio.includes('#include "fm1lab_audio_bridge.h"')) {
  audio = replaceOnce(audio, '#include "audio_output.h"\n',
    '#include "audio_output.h"\n#include "fm1lab_audio_bridge.h"\n',
    "audio bridge include");
}
if (!audio.includes("fm1lab_audio_mix(rendered);")) {
  audio = replaceOnce(audio, "    fm1_output_mix(rendered);",
    "    fm1lab_audio_mix(rendered);\n    fm1_output_mix(rendered);",
    "post-engine audio mix");
}
if (!audio.includes("(void)fm1lab_audio_init();")) {
  audio = replaceOnce(audio, "    if (initialized) return 0;\n    uint32_t clock = fm1_device_clock();",
    "    if (initialized) return 0;\n    (void)fm1lab_audio_init();\n    uint32_t clock = fm1_device_clock();",
    "audio engine initialization");
}
fs.writeFileSync(audioPath, audio);

const midiPath = path.join(app, "midi_router.c");
let midi = fs.readFileSync(midiPath, "utf8");
if (!midi.includes('#include "fm1lab_audio_bridge.h"')) {
  midi = replaceOnce(midi, '#include "fm1_engine_control.h"\n',
    '#include "fm1_engine_control.h"\n#include "fm1lab_audio_bridge.h"\n',
    "MIDI bridge include");
}
if (!midi.includes("fm1lab_audio_route_midi(m, n)")) {
  midi = replaceOnce(midi,
    '    if (!m || !n || n > 3 || m[0] < 0x80 || (n > 1 && m[1] >= 128) || (n > 2 && m[2] >= 128)) return -1;\n',
    '    if (!m || !n || n > 3 || m[0] < 0x80 || (n > 1 && m[1] >= 128) || (n > 2 && m[2] >= 128)) return -1;\n    int instrument_route = fm1lab_audio_route_midi(m, n);\n    if (instrument_route > 0) return 0;\n    if (instrument_route < 0) return -1;\n',
    "local keyboard MIDI route");
}
fs.writeFileSync(midiPath, midi);

const screenPath = path.join(app, "screen.c");
let screen = fs.readFileSync(screenPath, "utf8");
if (!screen.includes('#include "fm1lab_audio_bridge.h"')) {
  screen = replaceOnce(screen, '#include "fm1lab_runtime.h"\n',
    '#include "fm1lab_runtime.h"\n#include "fm1lab_audio_bridge.h"\n',
    "screen audio bridge include");
}
if (!screen.includes("fm1lab_audio_set_keyboard_mode(page == FM1LAB_HOME")) {
  screen = replaceOnce(screen, "controls_done:\n",
    "controls_done:\n    fm1lab_audio_set_keyboard_mode(page == FM1LAB_HOME || page == FM1LAB_PATCH || page == FM1LAB_SEQ);\n    if (fm1lab_home_runtime.loaded)\n        (void)fm1lab_audio_sync_instrument(&fm1lab_home_runtime.instrument);\n",
    "live Instrument mode and patch synchronization");
}
fs.writeFileSync(screenPath, screen);

console.log("Connected the four-patch FM6 engine to local MIDI and the FM1 DMA audio mix.");
