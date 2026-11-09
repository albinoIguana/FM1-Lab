/**
 * Event-based sequencer domain model.
 *
 * Time is expressed in sixteenth-note steps (one tick = one visible step).
 * Notes are independent events; no continuation/tie cells are stored.
 * All event starts and durations are quantized at the model boundary.
 */
export const SEQUENCER_TRACKS = 2;
export const MAX_SEQUENCE_STEPS = 64;
export const SEQUENCE_LENGTHS = Object.freeze([16, 32, 48, 64]);
export const MAX_EVENTS_PER_TRACK = 128;

const clamp = (value, min, max) => Math.max(min, Math.min(max, Math.round(Number(value) || 0)));
const quantize = (value, grid) => Math.max(0, Math.round((Number(value) || 0) / grid) * grid);
const validTrack = track => {
  if (!Number.isInteger(track) || track < 0 || track >= SEQUENCER_TRACKS) {
    throw new RangeError("Sequencer track must be 1 or 2.");
  }
};
const validId = (track, id) => {
  const event = track.events.find(item => item.id === id);
  if (!event) throw new RangeError("Selected note no longer exists.");
  return event;
};

export function createSequenceTrack(name, patchIndex = 0) {
  return {
    name: String(name || "TRACK").slice(0, 12),
    patchIndex: clamp(patchIndex, 0, 3),
    length: 16,
    events: []
  };
}

export function createTwoTrackSequence() {
  return {
    format: "fm1lab-note-event-sequence",
    version: 3,
    tempo: 120,
    swing: 0,
    quantize: 1,
    selectedTrack: 0,
    tracks: [createSequenceTrack("TRACK 1", 0), createSequenceTrack("TRACK 2", 1)]
  };
}

function migrateStepTrack(candidate, trackIndex) {
  const track = createSequenceTrack(candidate.name || "TRACK " + (trackIndex + 1), candidate.patchIndex ?? trackIndex);
  const requestedLength = clamp(candidate.length ?? 16, 16, 64);
  track.length = SEQUENCE_LENGTHS.reduce((best, option) =>
    Math.abs(option - requestedLength) < Math.abs(best - requestedLength) ? option : best, 16);
  if (!Array.isArray(candidate.steps) || candidate.steps.length !== 64) {
    throw new TypeError("Legacy sequencer tracks must contain 64 steps.");
  }
  let previous = null;
  for (let index = 0; index < candidate.steps.length; index++) {
    const step = candidate.steps[index] || {};
    if (!step.active || !step.velocity) { previous = null; continue; }
    const pitch = clamp(step.note ?? 60, 0, 127);
    if (step.tie && previous && previous.pitch === pitch && previous.start + previous.duration === index) {
      previous.duration++;
      continue;
    }
    const event = {
      id: "t" + (trackIndex + 1) + "n" + (track.events.length + 1),
      start: index,
      duration: 1,
      pitch,
      velocity: clamp(step.velocity ?? 100, 1, 127),
      accent: Boolean(step.accent)
    };
    track.events.push(event);
    previous = event;
  }
  return track;
}

function normalizeEvent(event, index, trackLength, grid) {
  const start = clamp(quantize(event?.start ?? 0, grid), 0, trackLength - 1);
  const duration = clamp(quantize(event?.duration ?? 1, grid), grid, trackLength);
  return {
    id: String(event?.id || "n" + (index + 1)),
    start,
    duration,
    pitch: clamp(event?.pitch ?? 60, 0, 127),
    velocity: clamp(event?.velocity ?? 100, 1, 127),
    accent: Boolean(event?.accent)
  };
}

export function normalizeSequence(sequence) {
  if (!sequence || !Array.isArray(sequence.tracks) || sequence.tracks.length !== SEQUENCER_TRACKS) {
    throw new TypeError("The FM1-Lab sequencer must contain exactly two tracks.");
  }
  const base = createTwoTrackSequence();
  const grid = clamp(sequence.quantize ?? 1, 1, 16);
  const tracks = sequence.tracks.map((candidate, trackIndex) => {
    if (!candidate) throw new TypeError("Each sequencer track must be an object.");
    if (Array.isArray(candidate.events)) {
      const requestedLength = clamp(candidate.length ?? 16, 16, 64);
      const length = SEQUENCE_LENGTHS.reduce((best, option) =>
        Math.abs(option - requestedLength) < Math.abs(best - requestedLength) ? option : best, 16);
      if (candidate.events.length > MAX_EVENTS_PER_TRACK) throw new RangeError("A track cannot contain more than 128 notes.");
      return {
        name: String(candidate.name || "TRACK " + (trackIndex + 1)).slice(0, 12),
        patchIndex: clamp(candidate.patchIndex ?? trackIndex, 0, 3),
        length,
        events: candidate.events.map((event, index) => normalizeEvent(event, index, length, grid))
      };
    }
    return migrateStepTrack(candidate, trackIndex);
  });
  return {
    ...base,
    tempo: clamp(sequence.tempo ?? 120, 30, 300),
    swing: clamp(sequence.swing ?? 0, 0, 50),
    quantize: grid,
    selectedTrack: clamp(sequence.selectedTrack ?? 0, 0, 1),
    tracks
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
  validTrack(trackIndex);
  const next = normalizeSequence(sequence);
  next.tracks[trackIndex].patchIndex = clamp(patchIndex, 0, 3);
  return next;
}

export function setTrackLength(sequence, trackIndex, length) {
  validTrack(trackIndex);
  const next = normalizeSequence(sequence);
  const requested = clamp(length, 16, 64);
  const selected = SEQUENCE_LENGTHS.reduce((best, option) =>
    Math.abs(option - requested) < Math.abs(best - requested) ? option : best, 16);
  next.tracks[trackIndex].length = selected;
  next.tracks[trackIndex].events = next.tracks[trackIndex].events.filter(event => event.start < selected)
    .map(event => ({ ...event, duration: Math.min(event.duration, selected - event.start) }));
  return next;
}

export function addNote(sequence, trackIndex, note, grid = sequence?.quantize ?? 1) {
  validTrack(trackIndex);
  const next = normalizeSequence(sequence);
  const track = next.tracks[trackIndex];
  if (track.events.length >= MAX_EVENTS_PER_TRACK) throw new RangeError("This track already contains 128 notes.");
  const step = clamp(quantize(note?.start ?? 0, clamp(grid, 1, 16)), 0, track.length - 1);
  const duration = clamp(quantize(note?.duration ?? 1, clamp(grid, 1, 16)), clamp(grid, 1, 16), track.length);
  const id = String(note?.id || "t" + (trackIndex + 1) + "n" + (track.events.length + 1));
  if (track.events.some(event => event.id === id)) throw new RangeError("Note IDs must be unique within a track.");
  track.events.push({
    id, start: step, duration,
    pitch: clamp(note?.pitch ?? 60, 0, 127),
    velocity: clamp(note?.velocity ?? 100, 1, 127),
    accent: Boolean(note?.accent)
  });
  return next;
}

export function updateNote(sequence, trackIndex, id, changes, grid = sequence?.quantize ?? 1) {
  validTrack(trackIndex);
  const next = normalizeSequence(sequence);
  const track = next.tracks[trackIndex];
  const event = validId(track, id);
  const q = clamp(grid, 1, 16);
  if ("start" in changes) event.start = clamp(quantize(changes.start, q), 0, track.length - 1);
  if ("duration" in changes) event.duration = clamp(quantize(changes.duration, q), q, track.length);
  if ("pitch" in changes) event.pitch = clamp(changes.pitch, 0, 127);
  if ("velocity" in changes) event.velocity = clamp(changes.velocity, 1, 127);
  if ("accent" in changes) event.accent = Boolean(changes.accent);
  return next;
}

export function deleteNote(sequence, trackIndex, id) {
  validTrack(trackIndex);
  const next = normalizeSequence(sequence);
  const events = next.tracks[trackIndex].events;
  const index = events.findIndex(event => event.id === id);
  if (index < 0) throw new RangeError("Selected note no longer exists.");
  events.splice(index, 1);
  return next;
}

/** Visual navigation: high pitches first (top-to-bottom), then earlier starts (left-to-right). */
export function orderedNotes(sequence, trackIndex = sequence?.selectedTrack ?? 0) {
  validTrack(trackIndex);
  return normalizeSequence(sequence).tracks[trackIndex].events.slice()
    .sort((a, b) => b.pitch - a.pitch || a.start - b.start || a.id.localeCompare(b.id));
}

export function selectNextNote(sequence, trackIndex, selectedId = null) {
  const notes = orderedNotes(sequence, trackIndex);
  if (!notes.length) return null;
  const index = notes.findIndex(note => note.id === selectedId);
  return notes[(index + 1) % notes.length].id;
}

export function notesStartingAt(sequence, trackIndex, tick) {
  validTrack(trackIndex);
  const position = clamp(tick, 0, 63);
  return normalizeSequence(sequence).tracks[trackIndex].events.filter(note => note.start === position);
}

export function notesEndingAt(sequence, trackIndex, tick) {
  validTrack(trackIndex);
  const position = clamp(tick, 0, 64);
  return normalizeSequence(sequence).tracks[trackIndex].events.filter(note => note.start + note.duration === position);
}

export function selectSequenceTrack(sequence, trackIndex) {
  validTrack(trackIndex);
  const next = normalizeSequence(sequence);
  next.selectedTrack = trackIndex;
  return next;
}
