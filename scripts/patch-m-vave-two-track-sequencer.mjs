import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const app = path.join(root, "vendor/m-vave-fm1/app");
const sequencerPath = path.join(root, "vendor/m-vave-fm1/web/engine/sequencer.c");
const instrumentPath = path.join(app, "instrument.h");

function replaceOnce(source, before, after, label) {
  const first = source.indexOf(before);
  if (first < 0 || source.indexOf(before, first + before.length) >= 0) {
    throw new Error("FM1-Lab sequencer patch: expected one exact match for " + label);
  }
  return source.slice(0, first) + after + source.slice(first + before.length);
}

let sequencer = fs.readFileSync(sequencerPath, "utf8");
if (!sequencer.includes("static uint8_t sequence_enabled")) {
  sequencer = replaceOnce(sequencer,
    "static uint8_t lengths[GROOVE_TRACKS];",
    "static uint8_t lengths[GROOVE_TRACKS];\n/* Runtime-only mask: FM1-Lab can audition two lanes without mutating other project rows. */\nstatic uint8_t sequence_enabled = 0xffU;",
    "track enable mask");
  sequencer = replaceOnce(sequencer,
    "        c->next[i] = (uint8_t)((s + 1U) % c->length[i]);\n        if (groove_track_role(track) != GROOVE_TRACK_NORMAL) continue;",
    "        c->next[i] = (uint8_t)((s + 1U) % c->length[i]);\n        if (!(sequence_enabled & (1U << track))) continue;\n        if (groove_track_role(track) != GROOVE_TRACK_NORMAL) continue;",
    "playback enable gate");
  sequencer = replaceOnce(sequencer,
    "    memset(lengths, 64, sizeof lengths);",
    "    memset(lengths, 64, sizeof lengths);\n    sequence_enabled = 0xffU;",
    "default enabled tracks");
  sequencer = replaceOnce(sequencer,
    "__attribute__((cold, minsize)) void groove_tempo_edit(int delta)",
    "int groove_sequence_track_enable(unsigned track, unsigned enabled)\n{\n    if (track >= GROOVE_TRACKS || enabled > 1U) return -1;\n    unsigned bit = 1U << track;\n    if (!!(sequence_enabled & bit) == !!enabled) return 0;\n    fm1_web_pause();\n    if (enabled) sequence_enabled |= (uint8_t)bit;\n    else {\n        release_track(&sequence_clock, track);\n        groove_lock_restore(track);\n        sequence_enabled &= (uint8_t)~bit;\n    }\n    fm1_web_resume();\n    return 0;\n}\n\n__attribute__((cold, minsize)) void groove_tempo_edit(int delta)",
    "runtime track enable API");
  fs.writeFileSync(sequencerPath, sequencer);
}

let instrument = fs.readFileSync(instrumentPath, "utf8");
if (!instrument.includes("groove_sequence_track_enable")) {
  instrument = replaceOnce(instrument,
    "void groove_sequence_length_set(unsigned track, unsigned steps);",
    "void groove_sequence_length_set(unsigned track, unsigned steps);\n/* Runtime-only playback mask; does not alter project rows or save data. */\nint groove_sequence_track_enable(unsigned track, unsigned enabled);",
    "track enable declaration");
  fs.writeFileSync(instrumentPath, instrument);
}

console.log("Applied non-destructive native sequencer track isolation.");
