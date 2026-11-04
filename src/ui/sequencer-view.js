import { DIVISIONS } from "../sequencer-model.js";

const escapeHtml = value => String(value).replace(/[&<>"']/g, character => ({
  "&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"
})[character]);

function renderTrack(track, trackIndex, selectedTrack, selectedStep) {
  const selected = trackIndex === selectedTrack;
  const steps = track.steps.map((step, index) => {
    const inLength = index < track.length;
    const classes = [
      "seq-step",
      step.active && inLength ? "active" : "",
      selected && index === selectedStep ? "chosen" : "",
      !inLength ? "beyond-length" : ""
    ].filter(Boolean).join(" ");
    return '<button class="' + classes + '" data-seq-step="' + index + '" data-seq-track="' + trackIndex +
      '" aria-label="Track ' + (trackIndex + 1) + ', step ' + (index + 1) + (step.active ? ', active' : ', inactive') +
      '" aria-pressed="' + (step.active && inLength) + '" ' + (!inLength ? "disabled" : "") + '>' +
      '<span>' + String(index + 1).padStart(2, "0") + '</span></button>';
  }).join("");
  return '<section class="seq-lane ' + (selected ? "selected" : "") + '" data-seq-lane="' + trackIndex + '">' +
    '<header><button data-seq-select="' + trackIndex + '" class="seq-track-select ' + (selected ? "selected" : "") +
    '">TRACK ' + (trackIndex + 1) + '</button><strong>' + escapeHtml(track.name) +
    '</strong><span>' + track.length + ' STEPS</span><span>' + DIVISIONS[track.division].label +
    '</span></header><div class="seq-timeline" role="group" aria-label="Track ' + (trackIndex + 1) + ' 64-step timeline">' +
    steps + '</div></section>';
}

export function renderSequencer(sequence, selectedStep = 0, advanced = false) {
  if (!sequence || !Array.isArray(sequence.tracks) || sequence.tracks.length !== 2) {
    throw new TypeError("The SEQ view requires exactly two tracks.");
  }
  const track = sequence.tracks[sequence.selectedTrack];
  const step = track.steps[selectedStep];
  const divisionOptions = DIVISIONS.map((division, index) =>
    '<option value="' + index + '" ' + (track.division === index ? "selected" : "") + '>' + division.label + '</option>'
  ).join("");
  const piano = Array.from({ length: 12 }, (_, index) => {
    const note = 71 - index;
    return '<button class="seq-key ' + (step.note === note ? "active" : "") + '" data-seq-note="' + note +
      '">' + ["B","A#","A","G#","G","F#","F","E","D#","D","C#","C"][index] + '</button>';
  }).join("");
  return '<div class="seq-function-bar"><div><small>SEQUENCER</small><strong>' + sequence.tempo +
    ' BPM</strong></div><div class="seq-global-shift"><label>GLOBAL KEY SHIFT <output>' + sequence.keyShift +
    '</output></label><input data-seq-key-shift type="range" min="-24" max="24" value="' + sequence.keyShift +
    '"></div><button data-seq-advanced>' + (advanced ? "CLOSE NOTE EDIT" : "HOLD SELECT · NOTE EDIT") + '</button></div>' +
    '<div class="seq-selected-controls"><div><small>SELECTED TRACK</small><strong>TRACK ' + (sequence.selectedTrack + 1) +
    '</strong></div><label>LENGTH <output>' + track.length + '</output><input data-seq-length type="range" min="8" max="64" step="1" value="' +
    track.length + '"></label><label>DIVISION<select data-seq-division>' + divisionOptions + '</select></label>' +
    '<label>SWING <output>' + track.swing + '</output><input data-seq-swing type="range" min="-75" max="75" value="' +
    track.swing + '"></label><span class="seq-knob-hint">KNOB 4 · GLOBAL KEY</span></div>' +
    '<div class="seq-lanes">' + sequence.tracks.map((item, index) =>
      renderTrack(item, index, sequence.selectedTrack, selectedStep)
    ).join("") + '</div>' +
    (advanced ? '<section class="seq-note-editor"><header><strong>NOTE EDIT · STEP ' + (selectedStep + 1) +
      '</strong><span>' + (step.active ? "ACTIVE" : "INACTIVE") + '</span></header><div class="seq-note-grid"><div class="seq-keyboard">' +
      piano + '</div><div class="seq-note-params"><label>VELOCITY <output>' + step.velocity +
      '</output><input data-seq-velocity type="range" min="1" max="127" value="' + step.velocity +
      '"></label><label>GATE / LENGTH <output>' + step.gate +
      '</output><input data-seq-gate type="range" min="1" max="200" value="' + step.gate +
      '"></label><label class="seq-check"><input data-seq-tie type="checkbox" ' + (step.tie ? "checked" : "") +
      '> TIE</label><label class="seq-check"><input data-seq-slide type="checkbox" ' + (step.slide ? "checked" : "") +
      '> SLIDE / LEGATO</label></div></div></section>' : "");
}
