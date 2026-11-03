import { readFile, writeFile } from "node:fs/promises";

const target = process.argv[2] || "_site/index.html";
let html = await readFile(target, "utf8");
const marker = '<a class="gh" href="https://github.com/joakimeriksson/fm1-emu"';
if (!html.includes(marker)) throw new Error("Could not find the upstream emulator header link");

const links = [
  ['href="./felucca/"', '<a class="gh" href="./felucca/" aria-label="Felucca browser runtime"><span>Felucca runtime</span><span class="arrow" aria-hidden="true">↗</span></a>'],
  ['href="./designer/"', '<a class="gh" href="./designer/" aria-label="FM1-Lab Instrument designer"><span>Instrument designer</span><span class="arrow" aria-hidden="true">↗</span></a>']
];
let changed = false;
for (const [needle, link] of links) {
  if (!html.includes(needle)) {
    html = html.replace(marker, link + "\n    " + marker);
    changed = true;
  }
}
if (changed) await writeFile(target, html);
