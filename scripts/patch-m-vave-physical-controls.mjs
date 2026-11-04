import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const app = path.join(root, "vendor/m-vave-fm1/app");
const screenPath = path.join(app, "screen.c");
let source = fs.readFileSync(screenPath, "utf8");

if (!source.includes('#include "fm1lab_physical_controls.h"')) {
  const anchor = '#include "instrument.h"\n';
  if (source.indexOf(anchor) < 0) throw new Error("FM1-Lab control patch: screen include anchor missing");
  source = source.replace(anchor, anchor + '#include "fm1lab_physical_controls.h"\n');
}

const names = [
  "SELECT", "ALGORITHM", "KNOB_1", "KNOB_2", "KNOB_3", "KNOB_4", "PRESETS"
];
for (let index = 0; index < names.length; index++) {
  const token = String(index);
  const replacement = "FM1LAB_ENCODER_" + names[index];
  for (const prefix of ["controls->encoders", "controls.encoders"]) {
    const before = prefix + "[" + token + "]";
    source = source.split(before).join(prefix + "[" + replacement + "]");
  }
}
fs.writeFileSync(screenPath, source);
console.log("Mapped native screen encoder references to printed FM1 panel names.");
