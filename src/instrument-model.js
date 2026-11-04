import { applyMacros, DEFAULT_MACROS } from "./dx7-macros.js";

export const PATCH_COUNT = 4;
export const OPERATOR_COUNT = 6;
export const DX7_PATCH_BYTES = 156;

export function createPatch(name = "INIT VOICE", data = new Uint8Array(DX7_PATCH_BYTES)) {
  if (!data || data.length !== DX7_PATCH_BYTES) throw new Error("A DX7 patch must contain exactly 156 bytes.");
  const baseData = Array.from(data, value => Number(value) & 0xff);
  return {
    name: String(name || "INIT VOICE").slice(0, 10),
    baseData,
    macros: { ...DEFAULT_MACROS },
    selectedOperator: 1
  };
}

export function createInstrument(name = "UNTITLED INSTRUMENT", patches = []) {
  if (!Array.isArray(patches) || patches.length !== PATCH_COUNT) {
    throw new Error("An Instrument must contain exactly four patches.");
  }
  return {
    format: "fm1lab-instrument",
    version: 1,
    id: "instrument-" + Math.random().toString(36).slice(2, 10),
    name: String(name || "UNTITLED INSTRUMENT").slice(0, 24),
    patches: patches.map((patch, index) => normalizePatch(patch, index))
  };
}

export function normalizePatch(candidate, index = 0) {
  if (!candidate || !Array.isArray(candidate.baseData) || candidate.baseData.length !== DX7_PATCH_BYTES) {
    throw new Error("Each Instrument patch must contain 156 bytes of M-VAVE DX7 voice data.");
  }
  const data = candidate.baseData.map(value => {
    const n = Number(value);
    if (!Number.isInteger(n) || n < 0 || n > 255) throw new Error("DX7 patch bytes must be integers from 0 to 255.");
    return n;
  });
  const macros = Object.fromEntries(Object.keys(DEFAULT_MACROS).map(key => [
    key, Math.max(0, Math.min(100, Math.round(Number(candidate.macros?.[key] ?? 50))))
  ]));
  return {
    name: String(candidate.name || "PATCH " + (index + 1)).slice(0, 10),
    baseData: data,
    macros,
    selectedOperator: Math.max(1, Math.min(OPERATOR_COUNT, Math.round(Number(candidate.selectedOperator) || 1)))
  };
}

export function normalizeInstrument(candidate) {
  if (!candidate || !Array.isArray(candidate.patches) || candidate.patches.length !== PATCH_COUNT) {
    throw new Error("An Instrument must contain exactly four patches.");
  }
  return {
    format: "fm1lab-instrument",
    version: 1,
    id: String(candidate.id || "custom-instrument"),
    name: String(candidate.name || "UNTITLED INSTRUMENT").slice(0, 24),
    patches: candidate.patches.map(normalizePatch)
  };
}

export function renderPatch(patch, carrierMask) {
  const normalized = normalizePatch(patch);
  return applyMacros(Uint8Array.from(normalized.baseData), normalized.macros, carrierMask);
}

export function patchAlgorithm(patch) {
  return patch.baseData[134] & 31;
}

export function setPatchByte(patch, offset, value) {
  if (!Number.isInteger(offset) || offset < 0 || offset >= DX7_PATCH_BYTES) throw new RangeError("Invalid DX7 patch byte.");
  const normalized = normalizePatch(patch);
  normalized.baseData[offset] = Math.max(0, Math.min(127, Math.round(Number(value) || 0)));
  return normalized;
}
