import { chromium } from "playwright";

const browser = await chromium.launch({ headless: true, args: ["--autoplay-policy=no-user-gesture-required"] });
try {
  const page = await browser.newPage({ viewport: { width: 1200, height: 900 }, deviceScaleFactor: 1 });
  page.on("pageerror", error => { throw error; });
  await page.goto("http://127.0.0.1:4173/", { waitUntil: "networkidle" });
  await page.locator("#go").click();
  await page.waitForFunction(() => document.querySelector("#status")?.textContent.startsWith("On"), null, { timeout: 15000 });
  // Let the power-on ident finish so the HOME page and PRE: INST cue are visible.
  await page.waitForTimeout(4700);
  await page.evaluate(() => {
    if (typeof node === "undefined" || !node) throw new Error("FM1 AudioWorklet is not ready");
    window.emuTestHomeRefusalResult = null;
    node.port.postMessage({ type: "test-home-refusal" });
  });
  await page.waitForFunction(() => window.emuTestHomeRefusalResult === true, null, { timeout: 5000 });
  await page.waitForTimeout(250);
  await page.locator("#device").screenshot({ path: "app/build/emu/home-refusal.png" });
  console.log("Captured HOME refusal overlay with PRE: INST cue.");
} finally {
  await browser.close();
}
