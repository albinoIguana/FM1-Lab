// FM1-Lab macro layer for M-VAVE's expanded 156-byte DX7 patch format.
// Operator blocks are stored OP6 first and OP1 last in the official format.
const OPERATOR_COUNT = 6;
const OPERATOR_STRIDE = 21;
const GLOBAL = Object.freeze({ algorithm: 134, feedback: 135, lfoSpeed: 137, lfoDelay: 138, lfoPitchDepth: 139, lfoAmpDepth: 140 });

export const MACRO_NAMES = Object.freeze(["brightness", "body", "motion", "dirt"]);
export const DEFAULT_MACROS = Object.freeze({ brightness: 50, body: 50, motion: 50, dirt: 50 });

const clamp = (value, min, max) => Math.max(min, Math.min(max, Math.round(Number(value) || 0)));
const operatorOffset = number => (OPERATOR_COUNT - number) * OPERATOR_STRIDE;
const scaledDelta = (delta, percent) => {
  const value = delta * percent / 100;
  return value >= 0 ? Math.floor(value + 0.5) : Math.ceil(value - 0.5);
};
function adjusted(base, amount, min = 0, max = 99) {
  return clamp(base + amount, min, max);
}

/**
 * Derive a live patch from its saved/base patch and four macro positions.
 * carrierMask uses bit 0 for OP1 through bit 5 for OP6, from M-VAVE's
 * official algorithm routing table. Macro edits never modify the saved patch.
 */
export function applyMacros(basePatch, values = DEFAULT_MACROS, carrierMask = 0x3f, operatorDepths = [1, 1, 1, 1, 1, 1]) {
  if (!basePatch || basePatch.length !== 156) throw new Error("M-VAVE DX7 patches must be exactly 156 bytes.");
  const base = Uint8Array.from(basePatch);
  const bytes = base.slice();
  const macros = Object.fromEntries(MACRO_NAMES.map(name => [name, clamp(values[name] ?? 50, 0, 100)]));
  const delta = Object.fromEntries(MACRO_NAMES.map(name => [name, macros[name] - 50]));

  for (let operator = 1; operator <= OPERATOR_COUNT; operator++) {
    const isCarrier = Boolean(carrierMask & (1 << (operator - 1)));
    const offset = operatorOffset(operator);
    if (isCarrier) {
      // BODY shapes the carrier's sustained presence, not its oscillator ratio.
      if (delta.body) bytes[offset + 6] = adjusted(base[offset + 6], scaledDelta(delta.body, 45));
    } else {
      // BRIGHTNESS changes modulation index by raising/lowering modulator output.
      const depth = Math.max(1, Math.min(6, Number(operatorDepths[operator - 1]) || 1));
      const brightnessScale = Math.trunc(55 / depth);
      const motionLevelScale = Math.trunc(35 / depth);
      const motionReleaseScale = Math.trunc(25 / depth);
      if (delta.brightness) bytes[offset + 16] = adjusted(base[offset + 16], scaledDelta(delta.brightness, brightnessScale));
      // MOTION changes the modulator's evolving spectrum through its EG contour.
      // Level 3 and level 4 move in opposite directions to create a controllable
      // transition; untouched rates preserve the original patch's timing.
      if (delta.motion) bytes[offset + 6] = adjusted(base[offset + 6], scaledDelta(delta.motion, motionLevelScale));
      if (delta.motion) bytes[offset + 7] = adjusted(base[offset + 7], -scaledDelta(delta.motion, motionReleaseScale));
    }
  }

  // DIRT is deliberately a bounded feedback control in this first experiment.
  // Frequency ratios remain untouched until topology-aware ratio mapping exists.
  if (delta.dirt) bytes[GLOBAL.feedback] = adjusted(base[GLOBAL.feedback], scaledDelta(delta.dirt, 8), 0, 7);
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
