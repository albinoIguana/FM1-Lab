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
  assert.match(html, /FELUCCA FM6/);
  assert.match(html, /id="algorithm"/);
  assert.match(html, /id="import-file"/);
  assert.match(html, /id="preset-range-labels"/);
  assert.match(html, /native-status/);
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
  assert.match(patch, /fh_fm6_factory_count/);
  assert.match(patch, /fh_fm6_factory_load/);
  assert.match(patch, /fh_fm6_patch_buffer/);
  assert.match(smoke, /fm6_patch_write/);
  assert.match(smoke, /native write\/read round trip/);
  assert.match(smoke, /factory voice/);
  assert.match(smoke, /factory_count/);
  assert.match(smoke, /algorithm = 1; algorithm <= 32/);
});

test("FM1-Lab uses Felucca's real AudioWorklet and native patch adapter", async () => {
  const main = await read("src/main.js");
  const engine = await read("src/felucca-engine.js");
  const worklet = await read("src/felucca-worklet.js");
  assert.match(main, /new FeluccaEngine/);
  assert.match(main, /engine\.loadFactoryPreset/);
  assert.match(main, /normalizeInstrument\(parsed\)/);
  assert.match(main, /factoryInstruments\.push\(imported\)/);
  assert.match(main, /#import-file/);
  assert.match(main, /engine\.writePatch/);
  assert.match(main, /engine\.getTopology/);
  assert.match(main, /f-mode/);
  assert.match(main, /f-coarse/);
  assert.match(main, /f-fine/);
  assert.match(engine, /AudioWorkletNode/);
  assert.match(engine, /44100/);
  assert.match(engine, /felucca\.wasm/);
  assert.match(worklet, /fh_fm6_patch_read/);
  assert.match(worklet, /fh_fm6_patch_write/);
  assert.match(worklet, /fh_fm6_modulator_mask/);
  assert.match(worklet, /registerProcessor\("fm1lab-felucca"/);
  assert.match(worklet, /fh_render\(count\)/);
  assert.match(worklet, /renderedFrames >= 44100/);
  assert.match(engine, /DSP /);
  assert.doesNotMatch(worklet, /fh_screen/);
});

test("firmware test console opens the compatibility route and retains its Felucca MIDI smoke script", async () => {
  const html = await read("testing.html");
  assert.match(html, /src="\.\/compatibility\//);
  assert.match(html, /href="\.\/compatibility\//);
  assert.match(html, /run-midi-smoke/);
  assert.match(html, /run-native-smoke/);
  assert.match(html, /src="\.\/src\/browser-smoke\.js"/);
  assert.match(html, /class="console emulator-console"/);
  assert.match(html, /querySelector\("\.emulator-console iframe"\)/);
  assert.match(html, /non-silent AudioWorklet samples/);
  assert.match(html, /same[- ]origin/i);
  assert.match(html, /felucca-midi-smoke\.fm1/);
  const browserSmoke = await read("src/browser-smoke.js");
  assert.match(browserSmoke, /loadFactoryPreset/);
  assert.match(browserSmoke, /writePatch/);
  assert.match(browserSmoke, /getFloatTimeDomainData/);
  assert.match(browserSmoke, /LIVE AUDIO RENDER/);
  const script = await read("scripts/felucca-midi-smoke.fm1");
  assert.match(script, /500:m903c64,1300:m803c00/);
});


test("Pages build verifies real browser audio and product startup in Chromium", async () => {
  const workflow = await read(".github/workflows/deploy-pages.yml");
  const browserTest = await read("scripts/browser-e2e.mjs");
  assert.match(workflow, /group: pages-\$\{\{ github\.ref \}\}/);
  assert.match(workflow, /npm install --no-save --no-package-lock playwright/);
  assert.match(workflow, /playwright install --with-deps chromium/);
  assert.match(workflow, /node scripts\/browser-e2e\.mjs/);
  assert.match(browserTest, /chromium\.launch/);
  assert.match(browserTest, /run-native-smoke/);
  assert.match(browserTest, /LIVE AUDIO RENDER/);
  assert.match(browserTest, /NATIVE VOICES LOADED/);
  assert.match(browserTest, /\.edit-patch/);
});
