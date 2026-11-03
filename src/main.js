import { FeluccaEngine } from "./felucca-engine.js";
import { createFactoryInstruments, migrateLegacyPatch, normalizeInstrument } from "./instrument.js";
import { decodeNativePatch, nativeTopologyCaption, topologyEdges, writeNativeField } from "./native-fm6.js";

const $ = selector => document.querySelector(selector);
const clamp = (value, min, max) => Math.max(min, Math.min(max, Number(value)));
const factoryInstruments = createFactoryInstruments();
const STORAGE_KEY = "fm1lab.instrument-library.v2";
let library = Object.fromEntries(factoryInstruments.map(item => [item.id, item]));
let selectedInstrumentId = factoryInstruments[0].id;
let instrument = library[selectedInstrumentId];
let selectedPatchIndex = 0;
let patch = instrument.patches[selectedPatchIndex];
let engineReady = false;
let activationPromise = null;
const topologyCache = new Map();
const engine = new FeluccaEngine({ onState: state => { $("#engine-status").textContent = state; } });
const heldNotes = new Map();
const heldKeyIntents = new Set();

function setActivePatch(index) {
  selectedPatchIndex = clamp(index, 0, 3);
  patch = instrument.patches[selectedPatchIndex];
}

function persist() {
  try {
    library[instrument.id] = normalizeInstrument(instrument);
    localStorage.setItem(STORAGE_KEY, JSON.stringify({
      format: "fm1-lab-library", version: 2, selectedInstrumentId, instruments: library
    }));
  } catch (error) {
    console.warn("Could not persist Instrument", error);
  }
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, char => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
  }[char]));
}

function renderHome() {
  $("#instrument-name").value = instrument.name;
  const presetIndex = factoryInstruments.findIndex(item => item.id === selectedInstrumentId);
  $("#preset-number").textContent = presetIndex < 0 ? "USR" : String(presetIndex + 1).padStart(2, "0");
  $("#preset-name").textContent = instrument.name.toUpperCase();
  $("#preset-knob").value = Math.max(0, presetIndex);
  $("#patch-slots").innerHTML = instrument.patches.map((item, index) => `
    <article class="patch-slot ${selectedPatchIndex === index ? "active" : ""}">
      <div class="slot-top"><span>FM PATCH 0${index + 1}</span><span class="slot-state">ALG ${String(item.algorithm || 1).padStart(2, "0")}</span></div>
      <div class="slot-main"><button class="patch-knob" data-patch="${index}" aria-label="Select FM Patch ${index + 1}"><span>PATCH</span><b>${index + 1}</b></button><div class="slot-copy"><h3>${escapeHtml(item.name)}</h3><p>FELUCCA FM6 · 6 OPERATORS</p><button class="edit-patch" data-patch="${index}">OPEN PATCH ↗</button></div></div>
    </article>`).join("");
  $("#patch-slots").querySelectorAll("[data-patch]").forEach(button => {
    button.onclick = () => openPatch(+button.dataset.patch);
  });
}

function openPatch(index) {
  setActivePatch(index);
  $("#home-screen").hidden = true;
  $("#editor-screen").hidden = false;
  renderAll();
}

function showHome() {
  stopAllHeldNotes();
  engine.allNotesOff();
  $("#editor-screen").hidden = true;
  $("#home-screen").hidden = false;
  renderHome();
}

async function loadTopology(algorithm) {
  if (!engineReady || topologyCache.has(algorithm)) return topologyCache.get(algorithm);
  try {
    const topology = await engine.getTopology(algorithm);
    topologyCache.set(algorithm, topology);
    if (patch.algorithm === algorithm) {
      patch.feedbackOperator = topology.feedback;
      patch.operators.forEach((op, index) => {
        op.feedback = index + 1 === topology.feedback ? patch.feedback : 0;
      });
      renderMap();
      renderEditor();
    }
    return topology;
  } catch (error) {
    console.error("Could not read native FM6 routing", error);
    return null;
  }
}

function renderMap() {
  const algorithm = patch.algorithm || 1;
  const topology = topologyCache.get(algorithm);
  if (!topology) {
    $("#map").innerHTML = '<div class="map-wait">READING FELUCCA ROUTING TABLE…</div>';
    $("#map-caption").textContent = "Native FM6 topology will appear when the firmware is ready.";
    if (engineReady) loadTopology(algorithm);
    return;
  }
  const positions = [[95, 45], [305, 45], [515, 45], [95, 145], [305, 145], [515, 145]];
  const edges = topologyEdges(topology).map(([source, destination]) => {
    const p = positions[source - 1], q = positions[destination - 1];
    const bend = (source % 2 ? -16 : 16);
    return `<path class="edge ${patch.selected === source || patch.selected === destination ? "hot" : ""}" d="M${p[0]} ${p[1]} Q${(p[0] + q[0]) / 2 + bend} ${(p[1] + q[1]) / 2 - 18} ${q[0]} ${q[1]}"/>`;
  }).join("");
  const nodes = positions.map(([x, y], index) => {
    const number = index + 1;
    const carrier = Boolean(topology.carriers & (1 << index));
    const feedback = topology.feedback === number;
    return `<g class="node ${carrier ? "carrier" : ""} ${patch.selected === number ? "selected" : ""} ${feedback ? "feedback" : ""}" data-op="${number}" tabindex="0" role="button" aria-label="Edit operator ${number}">
      <circle cx="${x}" cy="${y}" r="21"/><text x="${x}" y="${y}">OP ${number}</text>
      <text class="kind" x="${x}" y="${y + 32}">${carrier ? "CARRIER" : "MODULATOR"}${feedback ? " · FB" : ""}</text>
    </g>`;
  }).join("");
  $("#map").innerHTML = `<svg viewBox="0 0 610 190" role="img" aria-label="Native Felucca FM6 algorithm ${algorithm} routing diagram">${edges}${nodes}</svg>`;
  $("#map-caption").textContent = nativeTopologyCaption(topology) + " · Select an operator to edit.";
  $("#map").querySelectorAll("[data-op]").forEach(node => {
    const select = () => selectOp(+node.dataset.op);
    node.onclick = select;
    node.onkeydown = event => {
      if (event.key === "Enter" || event.key === " ") { event.preventDefault(); select(); }
    };
  });
}

function selectOp(number) {
  patch.selected = number;
  renderTabs();
  renderMap();
  renderEditor();
  persist();
}

const fields = [
  ["level", "OUTPUT LEVEL", 0, 99, 1],
  ["rate1", "STAGE 1 RATE · ATTACK", 0, 99, 1],
  ["level1", "STAGE 1 LEVEL", 0, 99, 1],
  ["rate2", "STAGE 2 RATE", 0, 99, 1],
  ["level2", "STAGE 2 LEVEL", 0, 99, 1],
  ["rate3", "STAGE 3 RATE · SUSTAIN", 0, 99, 1],
  ["level3", "STAGE 3 LEVEL", 0, 99, 1],
  ["rate4", "STAGE 4 RATE · RELEASE", 0, 99, 1],
  ["level4", "STAGE 4 LEVEL", 0, 99, 1],
  ["feedback", "ALGORITHM FEEDBACK", 0, 7, 1]
];

async function writeCurrentField(key, operatorNumber = patch.selected) {
  if (!engineReady || !Array.isArray(patch.nativePatch)) return;
  try {
    const bytes = writeNativeField(Uint8Array.from(patch.nativePatch), patch, key, operatorNumber);
    await engine.writePatch(selectedPatchIndex, bytes);
    patch.nativePatch = Array.from(bytes);
    persist();
  } catch (error) {
    console.error("FM6 patch write failed", error);
    $("#engine-status").textContent = "FM6 PATCH WRITE ERROR";
  }
}

function renderTabs() {
  $("#tabs").innerHTML = patch.operators.map((_, index) =>
    `<button data-op="${index + 1}" role="tab" aria-selected="${patch.selected === index + 1}">OPERATOR 0${index + 1}</button>`
  ).join("");
  $("#tabs").querySelectorAll("button").forEach(button => {
    button.onclick = () => selectOp(+button.dataset.op);
  });
}

function renderEditor() {
  const operator = patch.operators[patch.selected - 1];
  const topology = topologyCache.get(patch.algorithm);
  const feedbackActive = topology && topology.feedback === patch.selected;
  const modeControl = `<div class="param"><label for="f-mode">OSCILLATOR MODE</label><output>${operator.nativeMode ? "FIXED" : "RATIO"}</output><select id="f-mode"><option value="0" ${operator.nativeMode ? "" : "selected"}>RATIO</option><option value="1" ${operator.nativeMode ? "selected" : ""}>FIXED</option></select></div>`;
  const frequencyControl = operator.nativeMode
    ? `<div class="param"><label for="f-coarse">FIXED BASE FREQUENCY</label><output>${[1, 10, 100, 1000][operator.nativeCoarse]} HZ</output><select id="f-coarse">${[1, 10, 100, 1000].map((hz, index) => `<option value="${index}" ${operator.nativeCoarse === index ? "selected" : ""}>${hz} HZ</option>`).join("")}</select></div>
       <div class="param"><label for="f-fine">FIXED FINE</label><output id="v-fine">${operator.nativeFine}</output><input id="f-fine" type="range" min="0" max="99" step="1" value="${operator.nativeFine}"></div>`
    : `<div class="param"><label for="f-ratio">FREQUENCY RATIO</label><output id="v-ratio">${operator.ratio}</output><input id="f-ratio" type="range" min="0.5" max="61.69" step="0.01" value="${operator.ratio}"></div>`;
  $("#editor").innerHTML = `<div class="op-title">OPERATOR 0${patch.selected}<span>LIVE FELUCCA FM6 PARAMETERS</span></div>` + modeControl + frequencyControl +
    fields.map(([key, label, min, max, step]) => {
      const disabled = key === "feedback" && !feedbackActive ? "disabled" : "";
      const value = key === "feedback" ? operator.feedback
        : /^rate[1-4]$/.test(key) ? operator.rates[Number(key.slice(4)) - 1]
        : /^level[1-4]$/.test(key) ? operator.levels[Number(key.slice(5)) - 1]
        : operator[key];
      return `<div class="param"><label for="f-${key}">${label}</label><output id="v-${key}">${value}</output><input id="f-${key}" type="range" min="${min}" max="${max}" step="${step}" value="${value}" ${disabled}></div>`;
    }).join("");
  const modeSelect = $("#f-mode");
  modeSelect.onchange = async event => {
    operator.nativeMode = +event.target.value;
    if (operator.nativeMode === 1) operator.nativeCoarse = Math.min(3, operator.nativeCoarse);
    else operator.ratio = Number(((operator.nativeCoarse === 0 ? 0.5 : operator.nativeCoarse) * (1 + operator.nativeFine / 100)).toFixed(2));
    await writeCurrentField("mode");
    if (operator.nativeMode === 1) await writeCurrentField("coarse");
    renderEditor();
  };
  const ratioInput = $("#f-ratio");
  if (ratioInput) ratioInput.oninput = event => {
    operator.ratio = +event.target.value;
    operator.nativeMode = 0;
    operator.nativeCoarse = operator.ratio < 1 ? 0 : Math.floor(operator.ratio);
    operator.nativeFine = operator.ratio < 1
      ? Math.round((operator.ratio / 0.5 - 1) * 100)
      : Math.round((operator.ratio - operator.nativeCoarse) * 100);
    $("#v-ratio").textContent = operator.ratio;
    writeCurrentField("ratio");
    renderSpectrum();
  };
  const coarseSelect = $("#f-coarse");
  if (coarseSelect) coarseSelect.onchange = event => {
    operator.nativeCoarse = +event.target.value;
    writeCurrentField("coarse");
    const output = coarseSelect.parentElement.querySelector("output");
    output.textContent = [1, 10, 100, 1000][operator.nativeCoarse] + " HZ";
  };
  const fineInput = $("#f-fine");
  if (fineInput) fineInput.oninput = event => {
    operator.nativeFine = +event.target.value;
    $("#v-fine").textContent = operator.nativeFine;
    writeCurrentField("fine");
  };
  fields.forEach(([key]) => {
    const input = $("#f-" + key);
    if (!input || input.disabled) return;
    input.oninput = event => {
      const value = +event.target.value;
      const output = $("#v-" + key);
      output.textContent = value;
      if (key === "feedback") {
        patch.feedback = value;
        operator.feedback = value;
      } else if (/^rate[1-4]$/.test(key)) {
        operator.rates[Number(key.slice(4)) - 1] = value;
      } else if (/^level[1-4]$/.test(key)) {
        operator.levels[Number(key.slice(5)) - 1] = value;
      } else {
        operator[key] = value;
      }
      renderSpectrum();
      writeCurrentField(key);
    };
  });
}

function renderSpectrum() {
  // This is a patch-derived harmonic sketch, not a measured FFT of the audio output.
  const bars = Array.from({ length: 40 }, (_, index) => {
    const harmonic = index + 1;
    let height = 0;
    patch.operators.forEach((operator, opIndex) => {
      height += (operator.level / 100) * Math.exp(-Math.abs(harmonic - operator.ratio) * 1.6) / (1 + opIndex * 0.12);
    });
    return `<i class="bar" style="height:${clamp(height * 17 / (1 + Math.log2(harmonic) * 0.5), 2, 98)}%"></i>`;
  }).join("");
  $("#spectrum").innerHTML = bars;
}

function renderAll() {
  $("#patch-name").value = patch.name;
  $("#algorithm").value = String(patch.algorithm || 1);
  $("#editor-instrument-name").textContent = instrument.name.toUpperCase();
  $("#patch-id").textContent = "FM PATCH 0" + (selectedPatchIndex + 1);
  $("#editor-patch-label").textContent = "FM PATCH 0" + (selectedPatchIndex + 1);
  renderMap();
  renderTabs();
  renderEditor();
  renderSpectrum();
}

function load() {
  try {
    const saved = JSON.parse(localStorage.getItem(STORAGE_KEY) || localStorage.getItem("fm1lab.instrument-library.v1"));
    if (saved?.format === "fm1-lab-library" && saved.instruments) {
      for (const [id, candidate] of Object.entries(saved.instruments)) {
        try { library[id] = normalizeInstrument(candidate, id); }
        catch (error) { console.warn("Skipped invalid saved Instrument", id, error); }
      }
      for (const item of factoryInstruments) {
        if (library[item.id] && !library[item.id].factorySlots && !library[item.id].patches.some(p => Array.isArray(p.nativePatch))) {
          library[item.id].factorySlots = [...item.factorySlots];
        }
      }
      if (library[saved.selectedInstrumentId] && factoryInstruments.some(item => item.id === saved.selectedInstrumentId)) selectedInstrumentId = saved.selectedInstrumentId;
    } else {
      const legacy = JSON.parse(localStorage.getItem("fm1lab.patch.v3") || localStorage.getItem("fm1lab.patch.v2"));
      if (legacy?.operators) {
        const migrated = migrateLegacyPatch(legacy);
        migrated.id = factoryInstruments[0].id;
        library[migrated.id] = migrated;
        selectedInstrumentId = migrated.id;
      }
    }
  } catch (error) {
    console.warn("Ignoring invalid saved Instrument library", error);
  }
  instrument = library[selectedInstrumentId] || factoryInstruments[0];
  setActivePatch(0);
}

async function activateInstrument() {
  if (!engineReady) return;
  for (let index = 0; index < 4; index++) {
    let bytes;
    const candidate = instrument.patches[index];
    if (Array.isArray(candidate.nativePatch) && candidate.nativePatch.length === 155) {
      bytes = Uint8Array.from(candidate.nativePatch);
      await engine.writePatch(index, bytes);
    } else if (Array.isArray(candidate.factoryNativePatch) && candidate.factoryNativePatch.length === 155) {
      bytes = Uint8Array.from(candidate.factoryNativePatch);
      await engine.writePatch(index, bytes);
    } else if (instrument.factorySlots) {
      await engine.loadFactoryPreset(index, instrument.factorySlots[index]);
      bytes = await engine.readPatch(index);
      candidate.factoryNativePatch = Array.from(bytes);
    } else {
      bytes = await engine.readPatch(index);
      candidate.factoryNativePatch = Array.from(bytes);
    }
    const algorithm = (bytes[134] & 31) + 1;
    const topology = await loadTopology(algorithm);
    candidate.feedbackOperator = topology?.feedback || 1;
    instrument.patches[index] = decodeNativePatch(candidate, bytes);
    instrument.patches[index].feedbackOperator = candidate.feedbackOperator;
    instrument.patches[index].operators.forEach((operator, opIndex) => {
      operator.feedback = opIndex + 1 === candidate.feedbackOperator
        ? instrument.patches[index].feedback : 0;
    });
  }
  setActivePatch(selectedPatchIndex);
  persist();
  renderHome();
  renderAll();
  $("#native-status").textContent = "4 / 4 NATIVE VOICES LOADED · 8 FELUCCA FACTORY SLOTS";
}

async function ensureEngine() {
  if (engineReady) return;
  if (activationPromise) return activationPromise;
  activationPromise = (async () => {
    await engine.ready();
    engineReady = true;
    await activateInstrument();
  })();
  try { await activationPromise; }
  catch (error) { engineReady = false; throw error; }
  finally { activationPromise = null; }
}

async function selectInstrument(index) {
  const next = factoryInstruments[clamp(index, 0, factoryInstruments.length - 1)];
  selectedInstrumentId = next.id;
  if (!library[selectedInstrumentId]) library[selectedInstrumentId] = structuredClone(next);
  instrument = library[selectedInstrumentId];
  setActivePatch(0);
  renderHome();
  renderAll();
  if (engineReady) {
    $("#native-status").textContent = "LOADING FOUR NATIVE VOICES…";
    try { await activateInstrument(); }
    catch (error) { console.error(error); $("#native-status").textContent = error.message; }
  }
}

async function writeWholePatch() {
  if (!engineReady || !Array.isArray(patch.nativePatch)) return;
  let bytes = Uint8Array.from(patch.nativePatch);
  bytes = writeNativeField(bytes, patch, "algorithm");
  bytes = writeNativeField(bytes, patch, "feedback");
  bytes = writeNativeField(bytes, patch, "name");
  for (let operator = 1; operator <= 6; operator++) {
    bytes = writeNativeField(bytes, patch, "mode", operator);
    if (patch.operators[operator - 1].nativeMode === 1) {
      bytes = writeNativeField(bytes, patch, "coarse", operator);
      bytes = writeNativeField(bytes, patch, "fine", operator);
    } else {
      bytes = writeNativeField(bytes, patch, "ratio", operator);
    }
    for (const key of ["level", "rate1", "level1", "rate2", "level2", "rate3", "level3", "rate4", "level4"]) {
      bytes = writeNativeField(bytes, patch, key, operator);
    }
  }
  await engine.writePatch(selectedPatchIndex, bytes);
  patch.nativePatch = Array.from(bytes);
  persist();
  const topology = await loadTopology(patch.algorithm);
  if (topology) {
    patch.feedbackOperator = topology.feedback;
    patch.operators.forEach((operator, index) => {
      operator.feedback = index + 1 === topology.feedback ? patch.feedback : 0;
    });
  }
  renderAll();
}

async function playNote(note, velocity = 100, duration = 1.15) {
  try {
    await ensureEngine();
    engine.noteOn({ note, velocity, duration, track: selectedPatchIndex });
  } catch (error) {
    console.error(error);
    alert(error.message);
  }
}

function renderKeys() {
  const notes = [60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 77, 79];
  const black = [1, 3, 6, 8, 10];
  $("#keyboard").innerHTML = notes.map((note, index) =>
    `<button class="key ${black.includes(index) ? "black" : ""}" data-note="${note}" aria-label="Play MIDI note ${note}">${["C", "D", "E", "F", "G", "A", "B", "C", "D", "E", "F", "G"][index]}</button>`
  ).join("");
  $("#keyboard").querySelectorAll("button").forEach(button => {
    const note = +button.dataset.note;
    button.addEventListener("pointerdown", event => {
      event.preventDefault();
      button.setPointerCapture?.(event.pointerId);
      startHeldNote(note, +$("#velocity").value);
    });
    button.addEventListener("pointerup", () => stopHeldNote(note));
    button.addEventListener("pointercancel", () => stopHeldNote(note));
    button.addEventListener("lostpointercapture", () => stopHeldNote(note));
    button.addEventListener("keydown", event => {
      if ((event.key === " " || event.key === "Enter") && !event.repeat) {
        event.preventDefault();
        startHeldNote(note, +$("#velocity").value);
      }
    });
    button.addEventListener("keyup", event => {
      if (event.key === " " || event.key === "Enter") {
        event.preventDefault();
        stopHeldNote(note);
      }
    });
  });
}

async function startHeldNote(note, velocity = 100) {
  heldKeyIntents.add(note);
  try {
    await ensureEngine();
    if (!heldKeyIntents.has(note)) return;
    const voice = engine.noteOn({ note, velocity, track: selectedPatchIndex, duration: null });
    heldNotes.set(note, voice);
  } catch (error) {
    heldKeyIntents.delete(note);
    console.error(error);
    alert(error.message);
  }
}

function stopHeldNote(note) {
  heldKeyIntents.delete(note);
  const voice = heldNotes.get(note);
  if (voice) {
    engine.noteOff(voice);
    heldNotes.delete(note);
  }
}

function stopAllHeldNotes() {
  for (const note of new Set([...heldNotes.keys(), ...heldKeyIntents])) stopHeldNote(note);
}

window.addEventListener("blur", stopAllHeldNotes);

for (let algorithm = 1; algorithm <= 32; algorithm++) {
  const option = document.createElement("option");
  option.value = String(algorithm);
  option.textContent = "ALGORITHM " + String(algorithm).padStart(2, "0");
  $("#algorithm").appendChild(option);
}

$("#enter").onclick = async () => {
  $("#boot").remove();
  $("#app").hidden = false;
  try { await ensureEngine(); }
  catch (error) {
    console.error(error);
    $("#native-status").textContent = "ENGINE LOAD FAILED · " + error.message;
  }
};
$("#preset-knob").oninput = event => selectInstrument(+event.target.value);
$("#instrument-name").onchange = event => {
  instrument.name = event.target.value.trim().slice(0, 32) || "Untitled Instrument";
  persist();
  renderHome();
  renderAll();
};
$("#home-button").onclick = showHome;
$("#patch-name").onchange = event => {
  patch.name = event.target.value.trim().toUpperCase().slice(0, 10) || "FM PATCH";
  writeCurrentField("name");
  renderHome();
};
$("#algorithm").onchange = async event => {
  patch.algorithm = +event.target.value;
  await writeCurrentField("algorithm");
  await loadTopology(patch.algorithm);
  const topology = topologyCache.get(patch.algorithm);
  if (topology) {
    patch.feedbackOperator = topology.feedback;
    patch.operators.forEach((operator, index) => {
      operator.feedback = index + 1 === topology.feedback ? patch.feedback : 0;
    });
  }
  renderAll();
  persist();
};
$("#mutate").onclick = async () => {
  patch.algorithm = 1 + Math.floor(Math.random() * 32);
  patch.operators = patch.operators.map(operator => ({
    ...operator,
    ratio: Number((0.5 + Math.random() * 7.5).toFixed(2)),
    level: 20 + Math.round(Math.random() * 80),
    rates: Array.from({ length: 4 }, () => Math.round(Math.random() * 99)),
    levels: Array.from({ length: 4 }, () => Math.round(Math.random() * 99))
  }));
  patch.feedback = Math.floor(Math.random() * 8);
  patch.name = "LAB " + (100 + Math.floor(Math.random() * 900));
  await writeWholePatch();
  renderHome();
};
$("#reset").onclick = async () => {
  if (!confirm("Reset this FM patch to its original Felucca factory voice?")) return;
  try {
    const bytes = Uint8Array.from(patch.factoryNativePatch || patch.nativePatch);
    await engine.writePatch(selectedPatchIndex, bytes);
    instrument.patches[selectedPatchIndex] = decodeNativePatch(patch, bytes);
    const topology = await loadTopology(instrument.patches[selectedPatchIndex].algorithm);
    const resetPatch = instrument.patches[selectedPatchIndex];
    resetPatch.feedbackOperator = topology?.feedback || 1;
    resetPatch.operators.forEach((operator, index) => {
      operator.feedback = index + 1 === resetPatch.feedbackOperator
        ? resetPatch.feedback : 0;
    });
    setActivePatch(selectedPatchIndex);
    renderAll();
    persist();
    renderHome();
  } catch (error) {
    console.error(error);
    alert(error.message);
  }
};
$("#export").onclick = () => {
  const blob = new Blob([JSON.stringify(normalizeInstrument(instrument), null, 2)], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = (instrument.name.replace(/[^a-z0-9-_]/gi, "-") || "instrument") + ".fm1lab.json";
  anchor.click();
  URL.revokeObjectURL(url);
};
$("#play").onclick = () => playNote(+$("#note").value, +$("#velocity").value, 1.5);

load();
renderHome();
renderAll();
renderKeys();
