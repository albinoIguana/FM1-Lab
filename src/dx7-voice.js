import { operatorByteOffset, DX7_GLOBAL } from "./dx7-macros.js";

const clamp = (value, min, max) => Math.max(min, Math.min(max, Math.round(Number(value) || 0)));

export function patchName(bytes) {
  return String.fromCharCode(...bytes.slice(145, 155)).replace(/[\0\x20]+$/g, "").trim() || "INIT VOICE";
}

export function setPatchName(bytes, name) {
  const out = Uint8Array.from(bytes);
  const text = String(name || "INIT VOICE").toUpperCase().slice(0, 10).padEnd(10, " ");
  for (let i = 0; i < 10; i++) out[145 + i] = text.charCodeAt(i) & 0x7f;
  return out;
}

export function readOperator(bytes, number) {
  const at = operatorByteOffset(number, 0);
  return {
    rates: Array.from(bytes.slice(at, at + 4)),
    levels: Array.from(bytes.slice(at + 4, at + 8)),
    output: bytes[at + 16],
    mode: bytes[at + 17],
    coarse: bytes[at + 18],
    fine: bytes[at + 19],
    detune: bytes[at + 20]
  };
}

export function writeOperator(bytes, number, field, value) {
  const out = Uint8Array.from(bytes);
  const at = operatorByteOffset(number, 0);
  const limits = {
    output: [at + 16, 99],
    mode: [at + 17, 1],
    coarse: [at + 18, 31],
    fine: [at + 19, 99],
    detune: [at + 20, 14]
  };
  if (field.startsWith("rate")) {
    const index = Number(field.slice(4)) - 1;
    if (index < 0 || index > 3) throw new RangeError("Envelope rate index must be 1–4.");
    out[at + index] = clamp(value, 0, 99);
  } else if (field.startsWith("level")) {
    const index = Number(field.slice(5)) - 1;
    if (index < 0 || index > 3) throw new RangeError("Envelope level index must be 1–4.");
    out[at + 4 + index] = clamp(value, 0, 99);
  } else {
    const entry = limits[field];
    if (!entry) throw new RangeError("Unknown DX7 operator field: " + field);
    out[entry[0]] = clamp(value, 0, entry[1]);
  }
  return out;
}

export function readGlobal(bytes) {
  return {
    algorithm: (bytes[DX7_GLOBAL.algorithm] & 31) + 1,
    feedback: bytes[DX7_GLOBAL.feedback],
    lfoSpeed: bytes[DX7_GLOBAL.lfoSpeed],
    lfoDelay: bytes[DX7_GLOBAL.lfoDelay],
    lfoPitchDepth: bytes[DX7_GLOBAL.lfoPitchDepth],
    lfoAmpDepth: bytes[DX7_GLOBAL.lfoAmpDepth]
  };
}

export function writeGlobal(bytes, field, value) {
  const out = Uint8Array.from(bytes);
  const fields = {
    algorithm: [DX7_GLOBAL.algorithm, 31],
    feedback: [DX7_GLOBAL.feedback, 7],
    lfoSpeed: [DX7_GLOBAL.lfoSpeed, 99],
    lfoDelay: [DX7_GLOBAL.lfoDelay, 99],
    lfoPitchDepth: [DX7_GLOBAL.lfoPitchDepth, 99],
    lfoAmpDepth: [DX7_GLOBAL.lfoAmpDepth, 99]
  };
  const entry = fields[field];
  if (!entry) throw new RangeError("Unknown DX7 global field: " + field);
  out[entry[0]] = clamp(value, 0, entry[1]);
  return out;
}
