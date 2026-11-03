const PATCH_SIZE = 155;
const OP_SIZE = 21;
const clamp = (value, min, max) => Math.max(min, Math.min(max, Number(value)));
const roundByte = value => Math.round(clamp(value, 0, 99));

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
      const hz = Math.pow(10, coarse & 3) * Math.pow(1.0233, fine);
      ratio = hz / 261.625565;
    }
    return {
      ...previous,
      ratio: Number(clamp(ratio, 0.5, 16).toFixed(2)),
      level: Math.round(bytes[offset + 16] / 99 * 100),
      attack: bytes[offset],
      decay: bytes[offset + 1],
      sustain: bytes[offset + 6],
      release: bytes[offset + 3],
      feedback: index + 1 === (patch.feedbackOperator || 1) ? feedback : 0,
      nativeMode: mode,
      nativeCoarse: coarse,
      nativeFine: fine
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
    case "ratio": {
      const ratio = clamp(operator.ratio, 0.5, 16);
      const coarse = ratio < 1 ? 0 : Math.floor(ratio);
      const fine = ratio < 1 ? Math.round((ratio / 0.5 - 1) * 100) : Math.round((ratio - coarse) * 100);
      bytes[offset + 17] = 0; // ratio mode
      bytes[offset + 18] = clamp(coarse, 0, 31);
      bytes[offset + 19] = clamp(fine, 0, 99);
      break;
    }
    case "level": bytes[offset + 16] = roundByte(operator.level / 100 * 99); break;
    case "attack": bytes[offset] = roundByte(operator.attack); break;
    case "decay": bytes[offset + 1] = roundByte(operator.decay); break;
    case "sustain": bytes[offset + 6] = roundByte(operator.sustain); break;
    case "release": bytes[offset + 3] = roundByte(operator.release); break;
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
