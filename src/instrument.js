// Instrument model: one saved Instrument contains exactly four independent FM patches.
const BASE_OPERATORS = [
  { ratio: 1, level: 86, attack: 8, decay: 54, sustain: 55, release: 30, rates: [85, 58, 46, 30], levels: [99, 78, 62, 0], feedback: 18 },
  { ratio: 2, level: 58, attack: 14, decay: 42, sustain: 48, release: 24, rates: [78, 54, 46, 32], levels: [99, 70, 56, 0], feedback: 0 },
  { ratio: 3, level: 42, attack: 22, decay: 62, sustain: 62, release: 42, rates: [70, 48, 42, 26], levels: [99, 74, 60, 0], feedback: 0 },
  { ratio: 1.5, level: 65, attack: 4, decay: 30, sustain: 35, release: 18, rates: [90, 62, 50, 38], levels: [99, 80, 64, 0], feedback: 0 },
  { ratio: 0.5, level: 34, attack: 36, decay: 72, sustain: 70, release: 55, rates: [56, 38, 34, 20], levels: [99, 72, 58, 0], feedback: 0 },
  { ratio: 1, level: 72, attack: 10, decay: 48, sustain: 50, release: 32, rates: [82, 56, 48, 28], levels: [99, 76, 60, 0], feedback: 0 }
];

export function createDefaultPatch(name = "Glass Current", seed = 0) {
  const topologies = ["cascade", "pairs", "split"];
  const operators = BASE_OPERATORS.map((op, i) => ({
    ...op,
    ratio: Math.max(.25, Math.min(16, Number((op.ratio * (1 + seed * .035 + (i % 2 ? seed * .012 : 0))).toFixed(2)))),
    level: Math.max(8, Math.min(100, op.level + seed * (i % 2 ? 3 : -2))),
    attack: Math.max(0, Math.min(100, op.attack + seed * (i % 3 === 0 ? 3 : 1))),
    feedback: i === 0 ? Math.max(0, Math.min(80, op.feedback + seed * 4)) : op.feedback
  }));
  return {
    name, topology: topologies[seed % topologies.length], algorithm: 1 + ((seed * 3) % 32), feedback: 0, selected: 1,
    macros: {
      brightness: Math.max(0, Math.min(100, 62 + seed * 5)),
      body: Math.max(0, Math.min(100, 48 + seed * 3)),
      motion: Math.max(0, Math.min(100, 24 + seed * 6)),
      edge: Math.max(0, Math.min(100, 36 + seed * 4))
    },
    operators
  };
}

const FACTORY_DEFINITIONS = [
  { id: "glass-current", name: "Glass Current", factorySlots: [0, 1, 2, 3], patches: ["Factory Voice 01", "Factory Voice 02", "Factory Voice 03", "Factory Voice 04"] },
  { id: "neon-orchard", name: "Neon Orchard", factorySlots: [4, 5, 6, 7], patches: ["Factory Voice 05", "Factory Voice 06", "Factory Voice 07", "Factory Voice 08"] }
];

export function createFactoryInstruments() {
  return FACTORY_DEFINITIONS.map((definition, index) => ({
    format: "fm1-lab-instrument",
    version: 1,
    id: definition.id,
    name: definition.name,
    factorySlots: [...definition.factorySlots],
    patches: definition.patches.map((name, patchIndex) => createDefaultPatch(name, index + patchIndex))
  }));
}

export function normalizePatch(candidate, fallback = createDefaultPatch()) {
  if (!candidate || !Array.isArray(candidate.operators) || candidate.operators.length !== 6) {
    throw new Error("Each FM patch must contain exactly six operators.");
  }
  const base = createDefaultPatch();
  const topology = ["cascade", "pairs", "split"].includes(candidate.topology) ? candidate.topology : "cascade";
  return {
    ...base, ...candidate, name: String(candidate.name || fallback.name || base.name).slice(0, 32),
    topology, algorithm: Math.max(1, Math.min(32, Number(candidate.algorithm) || 1)),
    feedback: Math.round(Math.max(0, Math.min(7, Number(candidate.feedback) || 0))),
    selected: Math.max(1, Math.min(6, Number(candidate.selected) || 1)),
    macros: { ...base.macros, ...(candidate.macros || {}) },
    operators: candidate.operators.map((operator, index) => {
      const baseOperator = base.operators[index];
      return {
        ...baseOperator, ...operator,
        rates: Array.isArray(operator.rates) && operator.rates.length === 4
          ? operator.rates.map(value => Math.round(Math.max(0, Math.min(99, Number(value) || 0))))
          : [...baseOperator.rates],
        levels: Array.isArray(operator.levels) && operator.levels.length === 4
          ? operator.levels.map(value => Math.round(Math.max(0, Math.min(99, Number(value) || 0))))
          : [...baseOperator.levels]
      };
    })
  };
}

export function normalizeInstrument(candidate, fallbackId = "custom-instrument") {
  if (!candidate || !Array.isArray(candidate.patches) || candidate.patches.length !== 4) {
    throw new Error("An Instrument must contain exactly four FM patches.");
  }
  const fallback = createFactoryInstruments()[0];
  return {
    format: "fm1-lab-instrument", version: 1,
    id: String(candidate.id || fallbackId),
    name: String(candidate.name || "Untitled Instrument").slice(0, 32),
    factorySlots: Array.isArray(candidate.factorySlots) && candidate.factorySlots.length === 4 ? candidate.factorySlots.map(value => Math.max(0, Math.min(7, Number(value) || 0))) : null,
    patches: candidate.patches.map((patch, index) => normalizePatch(patch, fallback.patches[index]))
  };
}

export function migrateLegacyPatch(legacyPatch) {
  const base = createFactoryInstruments()[0];
  const first = normalizePatch(legacyPatch, base.patches[0]);
  first.name = String(legacyPatch.name || "Imported Patch").slice(0, 32);
  return {
    format: "fm1-lab-instrument", version: 1,
    id: "migrated-instrument", name: first.name,
    patches: [first, ...base.patches.slice(1).map((patch, index) => normalizePatch({
      ...patch, name: ["Layer Two", "Layer Three", "Layer Four"][index]
    }, patch))]
  };
}
