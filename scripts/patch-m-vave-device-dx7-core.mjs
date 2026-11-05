import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const vendor = path.join(root, "vendor/m-vave-fm1");
const app = path.join(vendor, "app");
const source = path.join(vendor, "web/synth");
const synthDir = path.join(app, "synth");
const generatedCore = path.join(app, "fm1lab_dx7");

/* The pinned FM1 SDK archive already contains this six-operator DX7 engine.
 * Remove artifacts from the earlier source-vendoring implementation instead
 * of compiling a duplicate core and duplicate lookup tables into the image. */
fs.rmSync(generatedCore, { recursive: true, force: true });
fs.rmSync(path.join(app, "fm1lab_dx7_rename.h"), { force: true });
fs.mkdirSync(synthDir, { recursive: true });

const engineHeader = fs.readFileSync(path.join(synthDir, "engine.h"), "utf8");
if (!engineHeader.includes("#define FM1_SYNTH_API 8U")) {
  throw new Error("Pinned FM1 SDK synth API is not version 8; refusing an unverified ABI.");
}
const synthHeader = fs.readFileSync(path.join(source, "synth_dx7.h"), "utf8");
if (!synthHeader.includes("SYNTH_DX7_EXPANDED_PATCH_BYTES = 156") ||
    !synthHeader.includes("synth_dx7_render")) {
  throw new Error("Pinned M-VAVE six-operator synth API header did not match the expected ABI.");
}
fs.copyFileSync(path.join(source, "synth_dx7.h"),
                 path.join(synthDir, "synth_dx7.h"));
fs.copyFileSync(path.join(root, "firmware/fm1lab_dx7_api.h"),
                 path.join(app, "fm1lab_dx7_api.h"));

console.log("Staged the pinned FM1 SDK six-operator synth API header; no second DX7 DSP core is compiled.");
