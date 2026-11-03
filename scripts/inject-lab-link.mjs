import { readFile, writeFile } from "node:fs/promises";

const target = process.argv[2] || "_site/index.html";
let html = await readFile(target, "utf8");
const marker = '<a class="gh" href="https://github.com/joakimeriksson/fm1-emu"';
if (!html.includes(marker)) throw new Error("Could not find the upstream emulator header link");
if (!html.includes('href="./designer/"')) {
  const link = '<a class="gh" href="./designer/" aria-label="FM1-Lab Instrument designer"><span>Instrument designer</span><span class="arrow" aria-hidden="true">↗</span></a>\n    ';
  html = html.replace(marker, link + marker);
  await writeFile(target, html);
}
