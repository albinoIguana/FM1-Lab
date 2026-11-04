import { SEQUENCE_LENGTHS } from "../sequencer-model.js";

const escapeHtml = value => String(value).replace(/[&<>"']/g, character => ({
  "&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"
}[character]));
const gatePercent = gate => gate === 0 ? 50 : gate === 15 ? 100 : Math.round(gate * 100 / 16);

function renderTrack(track, trackIndex, selectedTrack, selectedStep, patchNames) {
  const selected = trackIndex === selectedTrack;
  const patchOptions = patchNames.map((name, index) =>
    '<option value="' + index + '" ' + (track.patchIndex === index ? "selected" : "") + '>' + escapeHtml(name) + '</option>'
  ).join("");
  const steps = track.steps.map((step, index) => {
    const inLength = index < track.length;
    const classes = ["seq-step",step.active && inLength ? "active" : "",selected && index === selectedStep ? "chosen" : "",!inLength ? "beyond-length" : ""].filter(Boolean).join(" ");
    return '<button class="' + classes + '" data-seq-step="' + index + '" data-seq-track="' + trackIndex +
      '" aria-label="Track ' + (trackIndex + 1) + ', step ' + (index + 1) + (step.active ? ', active' : ', inactive') +
      '" aria-pressed="' + (step.active && inLength) + '" ' + (!inLength ? "disabled" : "") + '><span>' + String(index + 1).padStart(2, "0") + '</span></button>';
  }).join("");
  return '<section class="seq-lane ' + (selected ? "selected" : "") + '" data-seq-lane="' + trackIndex + '"><header>' +
    '<button data-seq-select="' + trackIndex + '" class="seq-track-select ' + (selected ? "selected" : "") + '">TRACK ' + (trackIndex + 1) + '</button>' +
    '<strong>' + escapeHtml(track.name) + '</strong><label class="seq-patch-select">PATCH <select data-seq-patch="' + trackIndex + '">' + patchOptions + '</select></label>' +
    '<span>' + track.length + ' STEPS</span></header>' +
    '<div class="seq-timeline" role="group" aria-label="Track ' + (trackIndex + 1) + ' 64-step timeline">' + steps + '</div></section>';
}

export function renderSequencer(sequence, selectedStep = 0, pianoRoll = false, advanced = false, patchNames = ["PATCH 1","PATCH 2","PATCH 3","PATCH 4"]) {
  if (!sequence || !Array.isArray(sequence.tracks) || sequence.tracks.length !== 2) throw new TypeError("The SEQ view requires exactly two tracks.");
  const track = sequence.tracks[sequence.selectedTrack];
  const step = track.steps[selectedStep];
  const lengthOptions = SEQUENCE_LENGTHS.map(length => '<option value="' + length + '" ' + (track.length === length ? "selected" : "") + '>' + length + ' STEPS</option>').join("");
  const keyboard = Array.from({length:12},(_,index) => {
    const note=71-index;
    return '<button class="seq-key ' + (step.note===note ? "active" : "") + '" data-seq-note="' + note + '">' + ["B","A#","A","G#","G","F#","F","E","D#","D","C#","C"][index] + '</button>';
  }).join("");
  const pianoGrid = Array.from({length:24},(_,row) => {
    const note=71-row;
    const cells=track.steps.map((item,stepIndex) => '<button class="seq-piano-cell ' + (item.active && item.note===note ? "active" : "") + (stepIndex===selectedStep ? " selected-step" : "") + '" data-seq-piano-step="' + stepIndex + '" data-seq-piano-note="' + note + '" aria-label="Step ' + (stepIndex+1) + ', note ' + note + '" ' + (stepIndex>=track.length ? "disabled" : "") + '></button>').join("");
    return '<div class="seq-piano-row"><span>' + ["B","A#","A","G#","G","F#","F","E","D#","D","C#","C"][row%12] + (Math.floor(note/12)-1) + '</span>' + cells + '</div>';
  }).join("");
  const noteEditor = pianoRoll && advanced ? '<section class="seq-note-editor"><header><strong>NOTE EDIT · STEP ' + (selectedStep+1) + '</strong><span>' + (step.active ? "ACTIVE" : "INACTIVE") + '</span></header><div class="seq-note-grid"><div class="seq-keyboard">' + keyboard + '</div><div class="seq-note-params"><label>VELOCITY <output>' + step.velocity + '</output><input data-seq-velocity type="range" min="0" max="127" value="' + step.velocity + '"></label><label>GATE <output>' + gatePercent(step.gate) + '%</output><input data-seq-gate type="range" min="0" max="15" value="' + step.gate + '"></label><label class="seq-check"><input data-seq-tie type="checkbox" ' + (step.tie ? "checked" : "") + '> TIE</label><label class="seq-check"><input data-seq-alt type="checkbox" ' + (step.alt ? "checked" : "") + '> ALT TUNING</label></div></div></section>' : "";
  return '<div class="seq-function-bar"><div class="seq-tempo"><small>SEQUENCER TEMPO</small><button data-seq-tempo="-1" aria-label="Decrease tempo">−</button><strong>' + sequence.tempo + ' BPM</strong><button data-seq-tempo="1" aria-label="Increase tempo">+</button></div>' +
    '<label class="seq-global-swing">GLOBAL SWING <output>' + sequence.swing + '</output><input data-seq-swing type="range" min="0" max="50" value="' + sequence.swing + '"></label>' +
    '<button data-seq-piano-roll>' + (pianoRoll ? "← TRACK LANES" : "SEQ · PIANO ROLL") + '</button>' + (pianoRoll ? '<button data-seq-advanced>' + (advanced ? "CLOSE NOTE EDIT" : "NOTE EDIT") + '</button>' : "") + '</div>' +
    '<div class="seq-selected-controls"><div><small>SELECTED TRACK</small><strong>TRACK ' + (sequence.selectedTrack+1) + '</strong></div><label>LENGTH<select data-seq-length>' + lengthOptions + '</select></label><span class="seq-knob-hint">NATIVE 16-STEP GRID · SHARED SWING</span></div>' +
    (pianoRoll ? '<section class="seq-piano-roll"><header><strong>TRACK ' + (sequence.selectedTrack+1) + ' · PIANO ROLL</strong><span>64 STEPS · 24 NOTE ROWS</span></header><div class="seq-piano-grid">' + pianoGrid + '</div></section>' : '<div class="seq-lanes">' + sequence.tracks.map((item,index)=>renderTrack(item,index,sequence.selectedTrack,selectedStep,patchNames)).join("") + '</div>') + noteEditor;
}
