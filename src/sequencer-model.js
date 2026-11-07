export const SEQUENCER_TRACKS = 2;
export const MIN_SEQUENCE_LENGTH = 16;
export const MAX_SEQUENCE_LENGTH = 64;
export const SEQUENCE_LENGTHS = Object.freeze([16, 32, 48, 64]);

const clamp = (value, min, max) => Math.max(min, Math.min(max, Math.round(Number(value) || 0)));

export function createSequenceTrack(name, patchIndex = 0) {
  return {
    name: String(name || "TRACK").slice(0, 12),
    patchIndex: clamp(patchIndex, 0, 3),
    length: 16,
    steps: Array.from({ length: MAX_SEQUENCE_LENGTH }, (_, index) => ({
      active: false, note: 60, velocity: 100, gate: 0, tie: false, alt: false, index
    }))
  };
}

export function createTwoTrackSequence() {
  return {
    format: "fm1lab-two-track-sequence",
    version: 2,
    tempo: 120,
    swing: 0,
    selectedTrack: 0,
    tracks: [createSequenceTrack("TRACK 1", 0), createSequenceTrack("TRACK 2", 1)]
  };
}

export function normalizeSequence(sequence) {
  if (!sequence || !Array.isArray(sequence.tracks) || sequence.tracks.length !== SEQUENCER_TRACKS) {
    throw new TypeError("The FM1-Lab sequencer must contain exactly two tracks.");
  }
  const base = createTwoTrackSequence();
  return {
    ...base,
    tempo: clamp(sequence.tempo ?? 120, 30, 300),
    swing: clamp(sequence.swing ?? 0, 0, 50),
    selectedTrack: clamp(sequence.selectedTrack ?? 0, 0, 1),
    tracks: sequence.tracks.map((candidate, trackIndex) => {
      if (!candidate || !Array.isArray(candidate.steps) || candidate.steps.length !== MAX_SEQUENCE_LENGTH) {
        throw new TypeError("Each sequencer track must contain 64 steps.");
      }
      const requestedLength = clamp(candidate.length ?? 16, MIN_SEQUENCE_LENGTH, MAX_SEQUENCE_LENGTH);
      const length = SEQUENCE_LENGTHS.reduce((best, option) =>
        Math.abs(option - requestedLength) < Math.abs(best - requestedLength) ? option : best, 16);
      return {
        name: String(candidate.name || "TRACK " + (trackIndex + 1)).slice(0, 12),
        patchIndex: clamp(candidate.patchIndex ?? trackIndex, 0, 3),
        length,
        steps: candidate.steps.map((step, index) => ({
          active: Boolean(step?.active),
          note: clamp(step?.note ?? 60, 0, 127),
          velocity: clamp(step?.velocity ?? 100, 0, 127),
          gate: clamp(step?.gate ?? 0, 0, 15),
          tie: Boolean(step?.tie),
          alt: Boolean(step?.alt),
          index
        }))
      };
    })
  };
}

export function setTempo(sequence, tempo) {
  const next = normalizeSequence(sequence);
  next.tempo = clamp(tempo, 30, 300);
  return next;
}

export function setGlobalSwing(sequence, swing) {
  const next = normalizeSequence(sequence);
  next.swing = clamp(swing, 0, 50);
  return next;
}

export function setTrackPatch(sequence, trackIndex, patchIndex) {
  const next = normalizeSequence(sequence);
  if (!Number.isInteger(trackIndex) || trackIndex < 0 || trackIndex > 1) throw new RangeError("Sequencer track must be 1 or 2.");
  next.tracks[trackIndex].patchIndex = clamp(patchIndex, 0, 3);
  return next;
}

export function setTrackLength(sequence, trackIndex, length) {
  const next = normalizeSequence(sequence);
  if (!Number.isInteger(trackIndex) || trackIndex < 0 || trackIndex > 1) throw new RangeError("Sequencer track must be 1 or 2.");
  const requested = clamp(length, MIN_SEQUENCE_LENGTH, MAX_SEQUENCE_LENGTH);
  next.tracks[trackIndex].length = SEQUENCE_LENGTHS.reduce((best, option) =>
    Math.abs(option - requested) < Math.abs(best - requested) ? option : best, 16);
  return next;
}

export function setStep(sequence, trackIndex, stepIndex, changes = {}) {
  const next = normalizeSequence(sequence);
  if (!Number.isInteger(trackIndex) || trackIndex < 0 || trackIndex > 1) throw new RangeError("Sequencer track must be 1 or 2.");
  if (!Number.isInteger(stepIndex) || stepIndex < 0 || stepIndex >= MAX_SEQUENCE_LENGTH) throw new RangeError("Sequencer step must be 1–64.");
  const step = next.tracks[trackIndex].steps[stepIndex];
  if ("active" in changes) step.active = Boolean(changes.active);
  if ("note" in changes) step.note = clamp(changes.note, 0, 127);
  if ("velocity" in changes) { step.velocity = clamp(changes.velocity, 0, 127); if (step.velocity === 0) step.active = false; }
  if ("active" in changes && step.active && step.velocity === 0) step.velocity = 1;
  if ("gate" in changes) step.gate = clamp(changes.gate, 0, 15);
  if ("tie" in changes) step.tie = Boolean(changes.tie);
  if ("alt" in changes) step.alt = Boolean(changes.alt);
  return next;
}

export function selectSequenceTrack(sequence, trackIndex) {
  const next = normalizeSequence(sequence);
  if (!Number.isInteger(trackIndex) || trackIndex < 0 || trackIndex > 1) throw new RangeError("Sequencer track must be 1 or 2.");
  next.selectedTrack = trackIndex;
  return next;
}
