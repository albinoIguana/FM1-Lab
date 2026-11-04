import { FeluccaEngine } from "./felucca-engine.js";

const button = document.querySelector("#run-native-smoke");
const status = document.querySelector("#native-status");
const results = document.querySelector("#native-results");

let activeEngine = null;

function setStatus(message) {
  status.textContent = message;
}

function result(label, detail, passed = true) {
  const item = document.createElement("li");
  item.className = passed ? "pass" : "fail";
  const heading = document.createElement("strong");
  heading.textContent = (passed ? "PASS · " : "FAIL · ") + label;
  const copy = document.createElement("span");
  copy.textContent = detail;
  item.append(heading, copy);
  results.append(item);
}

function wait(milliseconds) {
  return new Promise(resolve => window.setTimeout(resolve, milliseconds));
}

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

async function runBrowserSmokeTest() {
  results.replaceChildren();
  button.disabled = true;
  setStatus("STARTING NATIVE FELUCCA BROWSER TEST…");

  activeEngine = new FeluccaEngine({ onState: setStatus });
  let voice = null;
  let testPassed = false;

  try {
    const AudioContextClass = window.AudioContext || window.webkitAudioContext;
    assert(AudioContextClass, "This browser does not expose Web Audio.");
    await activeEngine.ready();
    assert(activeEngine.context?.state === "running", "AudioContext did not enter the running state.");
    result("AUDIOWORKLET + WASM BOOT", "Felucca initialized in the browser at " + activeEngine.context.sampleRate + " Hz.");

    for (let preset = 0; preset < 8; preset++) {
      const track = preset % 4;
      await activeEngine.loadFactoryPreset(track, preset);
      const patch = await activeEngine.readPatch(track);
      assert(patch.length === 155, "Factory voice " + (preset + 1) + " returned " + patch.length + " bytes, expected 155.");
      const algorithm = (patch[134] & 31) + 1;
      const topology = await activeEngine.getTopology(algorithm);
      assert(topology.algorithm === algorithm, "Factory voice " + (preset + 1) + " returned the wrong algorithm.");
      assert(topology.carriers > 0 && topology.carriers < 64, "Factory voice " + (preset + 1) + " returned an invalid carrier mask.");
      assert(topology.feedback >= 1 && topology.feedback <= 6, "Factory voice " + (preset + 1) + " returned an invalid feedback operator.");
      assert(topology.modulators.length === 6, "Factory voice " + (preset + 1) + " returned incomplete routing data.");
    }
    result("EIGHT FACTORY VOICES", "Loaded and read all eight native factory voices across the four Felucca parts.");
    result("NATIVE ALGORITHM DATA", "Validated Felucca routing and feedback data for all eight factory voices.");

    await activeEngine.loadFactoryPreset(0, 0);
    const originalPatch = await activeEngine.readPatch(0);
    await activeEngine.writePatch(0, originalPatch);
    const roundTrip = await activeEngine.readPatch(0);
    assert(roundTrip.length === originalPatch.length, "Patch read-back length changed.");
    assert(roundTrip.every((value, index) => value === originalPatch[index]), "Native patch bytes changed during a write/read round trip.");
    result("PATCH READ / WRITE", "A native patch survived a byte-for-byte write/read round trip.");

    const analyser = activeEngine.context.createAnalyser();
    analyser.fftSize = 2048;
    analyser.smoothingTimeConstant = 0;
    activeEngine.node.disconnect();
    activeEngine.node.connect(analyser);
    analyser.connect(activeEngine.context.destination);

    voice = activeEngine.noteOn({ note: 60, velocity: 110, track: 0 });
    const samples = new Float32Array(analyser.fftSize);
    let peak = 0;
    let squareSum = 0;
    let sampleCount = 0;
    const started = performance.now();

    while (performance.now() - started < 1100) {
      analyser.getFloatTimeDomainData(samples);
      for (const sample of samples) {
        const magnitude = Math.abs(sample);
        if (magnitude > peak) peak = magnitude;
        squareSum += sample * sample;
        sampleCount++;
      }
      await wait(45);
    }

    activeEngine.noteOff(voice);
    voice = null;
    const rms = Math.sqrt(squareSum / Math.max(1, sampleCount));
    assert(peak > 0.00001 && rms > 0.000001, "The AudioWorklet returned silence (peak " + peak.toExponential(2) + ", RMS " + rms.toExponential(2) + ").");
    result("LIVE AUDIO RENDER", "MIDI C4 produced non-silent AudioWorklet samples · peak " + peak.toFixed(5) + " · RMS " + rms.toFixed(5) + ".");
    testPassed = true;
  } catch (error) {
    if (voice) activeEngine?.noteOff(voice);
    result("BROWSER SELF-TEST", error?.message || String(error), false);
    setStatus("NATIVE BROWSER SELF-TEST FAILED");
  } finally {
    await activeEngine?.close().catch(() => {});
    activeEngine = null;
    setStatus(testPassed
      ? "NATIVE BROWSER SELF-TEST PASSED · AUDIO VERIFIED"
      : "NATIVE BROWSER SELF-TEST FAILED");
    button.disabled = false;
  }
}

button.dataset.nativeSmokeReady = "true";
button.addEventListener("click", runBrowserSmokeTest);
