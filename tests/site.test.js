import test from "node:test";
import assert from "node:assert/strict";
import { access, readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const read = path => readFile(resolve(root, path), "utf8");

test("designer page references its local assets and root emulator", async () => {
  const html = await read("index.html");
  const stylesheet = html.match(/href="\.\/([^"]+\.css)"/)?.[1];
  const module = html.match(/src="\.\/([^"]+\.js)"/)?.[1];
  assert.equal(stylesheet, "src/styles.css");
  assert.equal(module, "src/main.js");
  await access(resolve(root, stylesheet));
  await access(resolve(root, module));
  assert.match(html, /href="\.\.\/testing\.html"/);
  assert.match(html, /src="\.\.\/\?embed=1"/);
});

test("deployment makes the actual upstream emulator the primary site", async () => {
  const workflow = await read(".github/workflows/deploy-pages.yml");
  const injector = await read("scripts/inject-lab-link.mjs");
  assert.match(workflow, /build_site\.sh _site/);
  assert.match(workflow, /smoke\.mjs _site/);
  assert.match(workflow, /persistence\.wasm\.mjs _site/);
  assert.match(workflow, /cp index\.html _site\/designer\/index\.html/);
  assert.match(workflow, /inject-lab-link\.mjs _site\/index\.html/);
  assert.match(injector, /Instrument designer/);
  assert.match(injector, /fm1-emu/);
});

test("firmware test console runs the root emulator with a same-origin smoke script", async () => {
  const html = await read("testing.html");
  assert.match(html, /src="\.\//);
  assert.match(html, /run-midi-smoke/);
  assert.match(html, /same[- ]origin/i);
  assert.match(html, /felucca-midi-smoke\.fm1/);
  assert.match(html, /scripts\/felucca-midi-smoke\.fm1/);
  const script = await read("scripts/felucca-midi-smoke.fm1");
  assert.match(script, /500:m903c64,1300:m803c00/);
});
