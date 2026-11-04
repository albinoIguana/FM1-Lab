import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

const path = process.argv[2];
if (!path) throw new Error("Pass the built M-VAVE simulator.wasm path.");
const module = new WebAssembly.Module(readFileSync(path));
const names = new Set(WebAssembly.Module.exports(module).map(item => item.name));
for (const name of ["sim_boot", "sim_render", "sim_select", "sim_patch_export", "sim_patch_import", "sim_fm_factory", "sim_fm_carriers"]) {
  assert.ok(names.has(name), "M-VAVE simulator is missing required FM1-Lab API: " + name);
}
console.log("M-VAVE simulator API verified: official engine, patch import/export, factory voices, algorithm carriers.");
