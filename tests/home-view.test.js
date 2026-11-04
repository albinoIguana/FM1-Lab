import test from "node:test";
import assert from "node:assert/strict";
import { createPatch } from "../src/instrument-model.js";
import { renderPatchGrid } from "../src/ui/home-view.js";

test("HOME view renders exactly four patch tiles and marks the selected patch", () => {
  const patches = Array.from({ length: 4 }, (_, index) => createPatch("PATCH " + (index + 1), new Uint8Array(156)));
  const html = renderPatchGrid(patches, 2);
  assert.equal((html.match(/<article class="patch-card/g) || []).length, 4);
  assert.match(html, /data-patch-card="2" aria-current="true"/);
  assert.match(html, /PATCH 04/);
});

test("HOME view escapes patch names before inserting markup", () => {
  const patches = Array.from({ length: 4 }, (_, index) => createPatch(index === 0 ? '<script>alert("x")</script>' : "PATCH"));
  const html = renderPatchGrid(patches);
  assert.ok(!html.includes("<script>"));
  assert.ok(html.includes("&lt;script&gt;"));
});

test("HOME view rejects a model that violates the four-patch contract", () => {
  assert.throws(() => renderPatchGrid([createPatch()]), TypeError);
});