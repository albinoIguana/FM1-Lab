import test from "node:test";
import assert from "node:assert/strict";
import { access, readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const read = path => readFile(resolve(root, path), "utf8");

test("FM1-Lab product page owns the root and references local application assets", async () => {
  const html = await read("index.html");
  const stylesheet = html.match(/href="\.\/([^"]+\.css)"/)?.[1];
  const module = html.match(/src="\.\/([^"]+\.js)"/)?.[1];
  assert.equal(stylesheet, "src/styles.css");
  assert.equal(module, "src/main.js");
  await access(resolve(root, stylesheet));
  await access(resolve(root, module));
  assert.match(html, /href="\.\/felucca\//);
  assert.match(html, /href="\.\/compatibility\//);
  assert.match(html, /FOUR PATCHES/);
  assert.match(html, /LOCAL DSP PREVIEW/);
  assert.doesNotMatch(html, /src="\.\.\/?\?embed=1"/);
});

test("deployment publishes FM1-Lab at root and keeps runtimes in supporting routes", async () => {
  const workflow = await read(".github/workflows/deploy-pages.yml");
  assert.match(workflow, /build_site\.sh _compat/);
  assert.match(workflow, /smoke\.mjs _compat/);
  assert.match(workflow, /persistence\.wasm\.mjs _compat/);
  assert.match(workflow, /github\.com\/hugelton\/Felucca/);
  assert.match(workflow, /checkout [0-9a-f]{40}/);
  assert.match(workflow, /setup-emsdk/);
  assert.match(workflow, /\.\/build\.sh gen/);
  assert.match(workflow, /\.\/build\.sh web/);
  assert.match(workflow, /tests\/run_tests\.sh/);
  assert.match(workflow, /felucca-host-fm1lab-api\.patch/);
  assert.match(workflow, /felucca-api-smoke\.mjs/);
  assert.match(workflow, /_site\/compatibility/);
  assert.match(workflow, /_site\/felucca/);
  assert.match(workflow, /cp index\.html _site\/index\.html/);
  assert.ok(workflow.includes("cp -R src/. _site/src/"));
  assert.doesNotMatch(workflow, /build_site\.sh _site/);
});

test("FM1-Lab bridge patch exposes native FM6 state with a round-trip smoke test", async () => {
  const patch = await read("patches/felucca-host-fm1lab-api.patch");
  const smoke = await read("scripts/felucca-api-smoke.mjs");
  assert.match(patch, /fh_fm6_patch_read/);
  assert.match(patch, /fh_fm6_patch_write/);
  assert.match(patch, /fh_fm6_carrier_mask/);
  assert.match(patch, /fh_fm6_modulator_mask/);
  assert.match(patch, /fh_fm6_feedback_operator/);
  assert.match(smoke, /fm6_patch_write/);
  assert.match(smoke, /native write\/read round trip/);
  assert.match(smoke, /four synth parts/);
  assert.match(smoke, /algorithm = 1; algorithm <= 32/);
});

test("firmware test console opens the compatibility route and retains its Felucca MIDI smoke script", async () => {
  const html = await read("testing.html");
  assert.match(html, /src="\.\/compatibility\//);
  assert.match(html, /href="\.\/compatibility\//);
  assert.match(html, /run-midi-smoke/);
  assert.match(html, /same[- ]origin/i);
  assert.match(html, /felucca-midi-smoke\.fm1/);
  const script = await read("scripts/felucca-midi-smoke.fm1");
  assert.match(script, /500:m903c64,1300:m803c00/);
});
