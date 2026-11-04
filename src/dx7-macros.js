// FM1-Lab macro layer for M-VAVE's expanded 156-byte DX7 patch format.
// Operator blocks are stored OP6 first and OP1 last in the official format.
const OPERATOR_COUNT = 6;
const OPERATOR_STRIDE = 21;
const GLOBAL = Object.freeze({ algorithm: 134, feedback: 135, lfoSpeed: 137, lfoDelay: 138, lfoPitchDepth: 139, lfoAmpDepth: 140 });

export const MACRO_NAMES = Object.freeze(["brightness", "body", "motion", "edge"]);
export const DEFAULT_MACROS = Object.freeze({ brightness: 50, body: 50, motion: 50, edge: 50 });

const clamp = (value, min, max) => Math.max(min, Math.min(max, Math.round(Number(value) || 0)));
const operatorOffset = number => (OPERATOR_COUNT - number) * OPERATOR_STRIDE;

function adjusted(base, amount, min = 0, max = 99) {
  return clamp(base + amount, min, max);
}

function writeOperatorMacro(bytes, base, carrierMask, macro, delta) {
  for (let operator = 1; operator <= OPERATOR_COUNT; operator++) {
    const carrier = Boolean(carrierMask & (1 << (operator - 1)));
    const offset = operatorOffset(operator);
    if (macro === "brightness" && carrier) {
      // Carrier output is the direct loudness/harmonic balance control.
      bytes[offset + 16] = adjusted(base[offset + 16], delta * 0.55);
    } else if (macro === "body" && carrier) {
      // DX7 EG level 3 is the sustain body of the note.
      bytes[offset + 6] = adjusted(base[offset + 6], delta * 0.45);
    } else if (macro === "edge" && !carrier) {
      // Modulator output changes modulation index without flattening carriers.
      bytes[offset + 16] = adjusted(base[offset + 16], delta * 0.45);
    }
  }
}

/**
 * Derive a live patch from its saved/base patch and four macro positions.
 * carrierMask uses bit 0 for OP1 through bit 5 for OP6, from M-VAVE's
 * algorithm routing table (sim_fm_carriers).
 */
export function applyMacros(basePatch, values = DEFAULT_MACROS, carrierMask = 0x3f) {
  if (!basePatch || basePatch.length !== 156) throw new Error("M-VAVE DX7 patches must be exactly 156 bytes.");
  const base = Uint8Array.from(basePatch);
  const bytes = base.slice();
  const macros = Object.fromEntries(MACRO_NAMES.map(name => [name, clamp(values[name] ?? 50, 0, 100)]));
  for (const name of MACRO_NAMES) {
    const delta = macros[name] - 50;
    if (name === "brightness" || name === "body" || name === "edge") {
      writeOperatorMacro(bytes, base, carrierMask, name, delta);
    } else if (name === "motion") {
      // A centered macro preserves the source patch; positive motion opens
      // the LFO rate and amplitude depth, negative motion calms both.
      bytes[GLOBAL.lfoSpeed] = adjusted(base[GLOBAL.lfoSpeed], delta * 0.55);
      bytes[GLOBAL.lfoAmpDepth] = adjusted(base[GLOBAL.lfoAmpDepth], delta * 0.45);
    }
  }
  bytes[GLOBAL.feedback] = adjusted(base[GLOBAL.feedback], (macros.edge - 50) * 0.08, 0, 7);
  return bytes;
}

export function macroCarrierMask(algorithm, carrierMasks) {
  const index = clamp(algorithm, 0, 31);
  const mask = carrierMasks?.[index];
  return Number.isInteger(mask) ? mask & 0x3f : 0x3f;
}

export function operatorByteOffset(operator, parameter) {
  if (!Number.isInteger(operator) || operator < 1 || operator > 6) throw new RangeError("Operator must be 1–6.");
  if (!Number.isInteger(parameter) || parameter < 0 || parameter > 20) throw new RangeError("Operator parameter offset must be 0–20.");
  return operatorOffset(operator) + parameter;
}

export const DX7_GLOBAL = GLOBAL;
