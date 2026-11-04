import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const vendor = path.join(root, "vendor/m-vave-fm1");
const app = path.join(vendor, "app");
const source = path.join(vendor, "web/synth");
const target = path.join(app, "fm1lab_dx7");

const cFiles = [
  "dx7synth.c", "dx7core.c", "dx7note.c", "dx7env.c",
  "dx7lfo.c", "dx7pitchenv.c", "dx7math.c", "fixed.c", "adsr.c"
];
const hFiles = [
  "dx7synth.h", "dx7core.h", "dx7note.h", "dx7env.h",
  "dx7lfo.h", "dx7pitchenv.h", "dx7math.h", "dx7ctrl.h",
  "types.h", "adsr.h"
];

fs.mkdirSync(target, { recursive: true });
for (const file of cFiles) {
  const input = path.join(source, file);
  if (!fs.existsSync(input)) throw new Error("Missing upstream FM6 source: " + input);
  const original = fs.readFileSync(input, "utf8");
  let staged = original.includes('#include "fm1lab_dx7_rename.h"')
    ? original
    : '#include "fm1lab_dx7_rename.h"\n' + original;
  for (const [symbol, renamed] of Object.entries({
    sim_fm_carriers: "fm1lab_sim_fm_carriers",
    sim_fm_operator_depth: "fm1lab_sim_fm_operator_depth",
    dx7_sin_log_table: "fm1lab_dx7_sin_log_table",
    dx7_sin_exp_table: "fm1lab_dx7_sin_exp_table"
  })) staged = staged.replaceAll(symbol, renamed);
  fs.writeFileSync(path.join(target, file), staged);
}
for (const file of hFiles) {
  const input = path.join(source, file);
  if (!fs.existsSync(input)) throw new Error("Missing upstream FM6 header: " + input);
  fs.copyFileSync(input, path.join(target, file));
}
fs.copyFileSync(path.join(vendor, "third_party/synth/fixed.h"),
                 path.join(target, "fixed.h"));
fs.copyFileSync(path.join(root, "firmware/fm1lab_dx7_rename.h"),
                 path.join(app, "fm1lab_dx7_rename.h"));
fs.copyFileSync(path.join(root, "firmware/fm1lab_dx7_api.h"),
                 path.join(app, "fm1lab_dx7_api.h"));
fs.copyFileSync(path.join(source, "synth_dx7.h"),
                 path.join(app, "synth/synth_dx7.h"));

console.log("Staged the namespaced six-operator DX7 source core in app/fm1lab_dx7.");
