import { patchAlgorithm } from "../instrument-model.js";

function escapeHtml(value) {
  return String(value).replace(/[&<>"\']/g, character => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"' : "&quot;",
    "'": "&#39;"
  })[character]);
}

/**
 * Pure HOME patch-grid view. It receives model data and selection only;
 * event binding and engine/session operations stay in the application layer.
 */
export function renderPatchGrid(patches, selectedIndex = 0) {
  if (!Array.isArray(patches) || patches.length !== 4) {
    throw new TypeError("HOME requires exactly four Instrument patches.");
  }
  return patches.map((patch, index) => {
    const selected = index === selectedIndex;
    const algorithm = patchAlgorithm(patch) + 1;
    return '<article class="patch-card ' + (selected ? "selected" : "") + '" data-patch-card="' + index + '" ' + (selected ? 'aria-current="true"' : "") + '>' +
      '<div class="patch-card-top"><span>PATCH 0' + (index + 1) + '</span><span>ALG ' + String(algorithm).padStart(2, "0") + '</span></div>' +
      '<div class="patch-card-main"><b class="patch-number">0' + (index + 1) + '</b><div><h3>' + escapeHtml(patch.name) + '</h3><p>6 OPERATOR DX7 VOICE</p></div></div>' +
      '<button class="open-patch" data-patch="' + index + '">OPEN PATCH ↗</button></article>';
  }).join("");
}