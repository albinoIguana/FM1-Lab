const PATCH_SIZE = 155;
const OP_SIZE = 21;
const clamp = (value, min, max) => Math.max(min, Math.min(max, Number(value)));
const roundByte = value => Math.round(clamp(value, 0, 99));

export function nativeFixedFrequency(coarse, fine) {
  const logQ24 = (4458616 * (clamp(coarse, 0, 3) * 100 + clamp(fine, 0, 99))) >> 3;
  return Number(Math.pow(2, logQ24 / 16777216).toFixed(2));
}

export function nativePatchName(bytes, fallback = "FM Patch") {
  const raw = String.fromCharCode(...bytes.slice(145, 155)).replace(/[\0-\x1f\x7f]/g, "").trim();
  return raw || fallback;
}

export function decodeNativePatch(patch, source) {
  const bytes = source instanceof Uint8Array ? source : new Uint8Array(source);
  if (bytes.length !== PATCH_SIZE) throw new Error("Expected a 155-byte Felucca FM6 patch.");
  const feedback = bytes[135] & 7;
  const operators = patch.operators.map((previous, index) => {
    // Felucca's native voice format stores OP6 first and OP1 last.
    const offset = (5 - index) * OP_SIZE;
    const mode = bytes[offset + 17] & 1;
    const coarse = bytes[offset + 18] & 31;
    const fine = bytes[offset + 19] % 100;
    let ratio;
    if (mode === 0) {
      ratio = (coarse === 0 ? 0.5 : coarse) * (1 + fine / 100);
    } else {
      const hz = nativeFixedFrequency(coarse, fine);
      ratio = hz / 261.625565;
    }
    return {
      ...previous,
      ratio: Number(clamp(ratio, 0.5, 61.69).toFixed(2)),
      level: Math.round(bytes[offset + 16] / 99 * 100),
      rates: Array.from({ length: 4 }, (_, stage) => bytes[offset + stage]),
      levels: Array.from({ length: 4 }, (_, stage) => bytes[offset + 4 + stage]),
      feedback: index + 1 === (patch.feedbackOperator || 1) ? feedback : 0,
      nativeMode: mode,
      nativeCoarse: coarse,
      nativeFine: fine,
      fixedHz: mode === 1 ? nativeFixedFrequency(coarse, fine) : null
    };
  });
  return {
    ...patch,
    name: nativePatchName(bytes, patch.name),
    algorithm: (bytes[134] & 31) + 1,
    feedback,
    nativePatch: Array.from(bytes),
    operators
  };
}

export function writeNativeField(source, patch, field, operatorNumber = 1) {
  const bytes = source instanceof Uint8Array ? source.slice() : new Uint8Array(source);
  if (bytes.length !== PATCH_SIZE) throw new Error("Expected a 155-byte Felucca FM6 patch.");
  if (field === "algorithm") {
    bytes[134] = clamp(patch.algorithm - 1, 0, 31);
    return bytes;
  }
  if (field === "feedback") {
    bytes[135] = clamp(patch.feedback, 0, 7);
    return bytes;
  }
  if (field === "name") {
    const name = String(patch.name || "FM PATCH").toUpperCase().replace(/[^A-Z0-9 _-]/g, " ").slice(0, 10);
    bytes.fill(32, 145, 155);
    for (let i = 0; i < name.length; i++) bytes[145 + i] = name.charCodeAt(i);
    return bytes;
  }
  const operator = patch.operators[operatorNumber - 1];
  if (!operator) throw new Error("FM6 operator must be 1–6.");
  const offset = (6 - operatorNumber) * OP_SIZE;
  switch (field) {
    case "mode":
      bytes[offset + 17] = clamp(operator.nativeMode, 0, 1);
      break;
    case "coarse":
      bytes[offset + 18] = clamp(operator.nativeCoarse, 0, operator.nativeMode ? 3 : 31);
      break;
    case "fine":
      bytes[offset + 19] = clamp(operator.nativeFine, 0, 99);
      break;
    case "ratio": {
      const ratio = clamp(operator.ratio, 0.5, 61.69);
      const coarse = ratio < 1 ? 0 : Math.min(31, Math.floor(ratio));
      const base = coarse === 0 ? 0.5 : coarse;
      const fine = Math.round((ratio / base - 1) * 100);
      operator.nativeMode = 0;
      operator.nativeCoarse = clamp(coarse, 0, 31);
      operator.nativeFine = clamp(fine, 0, 99);
      bytes[offset + 17] = 0; // ratio mode
      bytes[offset + 18] = operator.nativeCoarse;
      bytes[offset + 19] = operator.nativeFine;
      break;
    }
    case "level": bytes[offset + 16] = roundByte(operator.level / 100 * 99); break;
    case "rate1": case "rate2": case "rate3": case "rate4": {
      const stage = Number(field.slice(4)) - 1;
      bytes[offset + stage] = roundByte(operator.rates[stage]);
      break;
    }
    case "level1": case "level2": case "level3": case "level4": {
      const stage = Number(field.slice(5)) - 1;
      bytes[offset + 4 + stage] = roundByte(operator.levels[stage]);
      break;
    }
    default: throw new Error("Unsupported native FM6 field: " + field);
  }
  return bytes;
}

export function nativeTopologyCaption(topology) {
  const carriers = Array.from({ length: 6 }, (_, i) => (topology.carriers & (1 << i)) ? "OP" + (i + 1) : null).filter(Boolean);
  return "ALGORITHM " + String(topology.algorithm).padStart(2, "0") +
    " · CARRIERS " + carriers.join(" / ") +
    " · FEEDBACK OP" + topology.feedback;
}

export function topologyEdges(topology) {
  const edges = [];
  topology.modulators.forEach((mask, destination) => {
    for (let source = 0; source < 6; source++) {
      if (mask & (1 << source)) edges.push([source + 1, destination + 1]);
    }
  });
  return edges;
}
