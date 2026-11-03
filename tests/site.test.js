import test from "node:test";
import assert from "node:assert/strict";
import { access, readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const read = path => readFile(resolve(root, path), "utf8");

test("main page references existing local assets", async () => {
  const html = await read("index.html");
  const stylesheet = html.match(/href="\.\/([^"]+\.css)"/)?.[1];
  const module = html.match(/src="\.\/([^"]+\.js)"/)?.[1];
  assert.equal(stylesheet, "src/styles.css");
  assert.equal(module, "src/main.js");
  await access(resolve(root, stylesheet));
  await access(resolve(root, module));
});

test("main page exposes the Instrument editor and test console", async () => {
  const html = await read("index.html");
  const app = await read("src/main.js");
  assert.match(html, /id="home-screen"/);
  assert.match(html, /id="editor-screen"/);
  assert.match(html, /href="\.\/testing\.html"/);
  assert.match(app, /createFactoryInstruments/);
  assert.match(app, /toggle-emulator/);
});

test("firmware test console embeds the upstream full emulator", async () => {
  const html = await read("testing.html");
  assert.match(html, /\.\/vendor\/fm1-emu\//);
  assert.match(html, /run-midi-smoke/);
  assert.match(html, /title="Full M-VAVE FM-1 firmware emulator and test console"/);
  assert.match(html, /same-origin|cross-origin/i);
  assert.match(html, /felucca-midi-smoke\.fm1/);
  const script = await read("scripts/felucca-midi-smoke.fm1");
  assert.match(script, /500:m903c64,1300:m803c00/);
});
