export const SEQUENCER_TRACKS = 2;
export const MIN_SEQUENCE_LENGTH = 8;
export const MAX_SEQUENCE_LENGTH = 64;
export const DIVISIONS = Object.freeze([
  { label: "1/4", ticks: 96 },
  { label: "1/8", ticks: 48 },
  { label: "1/8T", ticks: 32 },
  { label: "1/16", ticks: 24 },
  { label: "1/16T", ticks: 16 },
  { label: "1/32", ticks: 12 },
  { label: "1/32T", ticks: 8 }
]);

const clamp = (value, min, max) => Math.max(min, Math.min(max, Math.round(Number(value) || 0)));

export function createSequenceTrack(name, patchIndex = 0) {
  return {
    name: String(name || "TRACK").slice(0, 12),
    patchIndex: clamp(patchIndex, 0, 3),
    length: 16,
    division: 3,
    swing: 0,
    steps: Array.from({ length: MAX_SEQUENCE_LENGTH }, (_, index) => ({
      active: false,
      note: 60,
      velocity: 100,
      gate: 75,
      tie: false,
      slide: false,
      index
    }))
  };
}

export function createTwoTrackSequence() {
  return {
    format: "fm1lab-two-track-sequence",
    version: 1,
    tempo: 120,
    keyShift: 0,
    selectedTrack: 0,
    tracks: [
      createSequenceTrack("TRACK 1", 0),
      createSequenceTrack("TRACK 2", 1)
    ]
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
    keyShift: clamp(sequence.keyShift ?? 0, -24, 24),
    selectedTrack: clamp(sequence.selectedTrack ?? 0, 0, 1),
    tracks: sequence.tracks.map((candidate, trackIndex) => {
      if (!candidate || !Array.isArray(candidate.steps) || candidate.steps.length !== MAX_SEQUENCE_LENGTH) {
        throw new TypeError("Each sequencer track must contain 64 steps.");
      }
      return {
        name: String(candidate.name || "TRACK " + (trackIndex + 1)).slice(0, 12),
        patchIndex: clamp(candidate.patchIndex ?? trackIndex, 0, 3),
        length: clamp(candidate.length ?? 16, MIN_SEQUENCE_LENGTH, MAX_SEQUENCE_LENGTH),
        division: clamp(candidate.division ?? 3, 0, DIVISIONS.length - 1),
        swing: clamp(candidate.swing ?? 0, -75, 75),
        steps: candidate.steps.map((step, index) => ({
          active: Boolean(step?.active),
          note: clamp(step?.note ?? 60, 0, 127),
          velocity: clamp(step?.velocity ?? 100, 1, 127),
          gate: clamp(step?.gate ?? 75, 1, 200),
          tie: Boolean(step?.tie),
          slide: Boolean(step?.slide),
          index
        }))
      };
    })
  };
}

export function setTrackLength(sequence, trackIndex, length) {
  const next = normalizeSequence(sequence);
  if (!Number.isInteger(trackIndex) || trackIndex < 0 || trackIndex > 1) throw new RangeError("Sequencer track must be 1 or 2.");
  next.tracks[trackIndex].length = clamp(length, MIN_SEQUENCE_LENGTH, MAX_SEQUENCE_LENGTH);
  return next;
}

export function setTrackDivision(sequence, trackIndex, division) {
  const next = normalizeSequence(sequence);
  if (!Number.isInteger(trackIndex) || trackIndex < 0 || trackIndex > 1) throw new RangeError("Sequencer track must be 1 or 2.");
  next.tracks[trackIndex].division = clamp(division, 0, DIVISIONS.length - 1);
  return next;
}

export function setTrackSwing(sequence, trackIndex, swing) {
  const next = normalizeSequence(sequence);
  if (!Number.isInteger(trackIndex) || trackIndex < 0 || trackIndex > 1) throw new RangeError("Sequencer track must be 1 or 2.");
  next.tracks[trackIndex].swing = clamp(swing, -75, 75);
  return next;
}

export function setStep(sequence, trackIndex, stepIndex, changes = {}) {
  const next = normalizeSequence(sequence);
  if (!Number.isInteger(trackIndex) || trackIndex < 0 || trackIndex > 1) throw new RangeError("Sequencer track must be 1 or 2.");
  if (!Number.isInteger(stepIndex) || stepIndex < 0 || stepIndex >= MAX_SEQUENCE_LENGTH) throw new RangeError("Sequencer step must be 1–64.");
  const step = next.tracks[trackIndex].steps[stepIndex];
  if ("active" in changes) step.active = Boolean(changes.active);
  if ("note" in changes) step.note = clamp(changes.note, 0, 127);
  if ("velocity" in changes) step.velocity = clamp(changes.velocity, 1, 127);
  if ("gate" in changes) step.gate = clamp(changes.gate, 1, 200);
  if ("tie" in changes) step.tie = Boolean(changes.tie);
  if ("slide" in changes) step.slide = Boolean(changes.slide);
  return next;
}

export function selectSequenceTrack(sequence, trackIndex) {
  const next = normalizeSequence(sequence);
  if (!Number.isInteger(trackIndex) || trackIndex < 0 || trackIndex > 1) throw new RangeError("Sequencer track must be 1 or 2.");
  next.selectedTrack = trackIndex;
  return next;
}

export function setGlobalKeyShift(sequence, semitones) {
  const next = normalizeSequence(sequence);
  next.keyShift = clamp(semitones, -24, 24);
  return next;
}
