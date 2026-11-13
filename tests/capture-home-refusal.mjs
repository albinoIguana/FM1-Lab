import { chromium } from "playwright";

const browser = await chromium.launch({ headless: true, args: ["--autoplay-policy=no-user-gesture-required"] });
try {
  const page = await browser.newPage({ viewport: { width: 1200, height: 900 }, deviceScaleFactor: 1 });
  page.on("pageerror", error => { throw error; });
  await page.goto("http://127.0.0.1:4173/", { waitUntil: "networkidle" });
  await page.evaluate(() => {
    window.emuTestHomeRefusalResult = null;
    window.emuWorkletPong = false;
    window.emuRuntimeError = null;
    window.emuProcessorError = null;
  });
  await page.locator("#go").click();
  await page.waitForFunction(() => document.querySelector("#status")?.textContent.startsWith("On"), null, { timeout: 15000 });
  // Let the power-on ident finish so the HOME page and PRE: INST cue are visible.
  await page.waitForTimeout(4700);
  await page.evaluate(() => {
    if (typeof node === "undefined" || !node) throw new Error("FM1 AudioWorklet is not ready");
    window.emuTestHomeRefusalResult = null;
    window.emuWorkletPong = false;
    // Do not reset runtime/processor errors here: a worklet may have failed
    // during boot, before this post-ident diagnostic runs.
    node.port.postMessage({ type: "ping" });
  });
  await page.waitForFunction(() => window.emuWorkletPong === true ||
    window.emuRuntimeError || window.emuProcessorError, null, { timeout: 5000 });
  const bridge = await page.evaluate(() => ({
    pong: window.emuWorkletPong,
    runtimeError: window.emuRuntimeError,
    processorError: window.emuProcessorError
  }));
  if (bridge.runtimeError || bridge.processorError || !bridge.pong) {
    throw new Error(`FM1 AudioWorklet bridge failed: ${bridge.runtimeError || bridge.processorError || "no pong"}`);
  }
  await page.evaluate(() => node.port.postMessage({ type: "test-home-refusal" }));
  await page.waitForFunction(() => window.emuTestHomeRefusalResult !== null &&
    window.emuTestHomeRefusalResult !== undefined || window.emuRuntimeError || window.emuProcessorError,
    null, { timeout: 5000 });
  const framebuffer = await page.evaluate(() => {
    if (window.emuRuntimeError || window.emuProcessorError)
      throw new Error(window.emuRuntimeError || window.emuProcessorError);
    return window.emuTestHomeRefusalResult;
  });
  if (!framebuffer.ok) {
    throw new Error(`HOME refusal fixture failed in AudioWorklet: ${framebuffer.error || "unknown error"}`);
  }
  if (framebuffer.framebufferLit < 500) {
    throw new Error(`HOME refusal draw produced a blank framebuffer (blits=${framebuffer.blits}, lit=${framebuffer.framebufferLit})`);
  }
  // Do not accept a successful browser run with a blank LCD. Wait for a
  // real framebuffer update and verify that the visible display has ink.
  await page.waitForFunction(() => {
    const canvas = document.querySelector("#screen");
    if (!canvas) return false;
    const ctx = canvas.getContext("2d");
    const { data } = ctx.getImageData(0, 0, canvas.width, canvas.height);
    let lit = 0;
    for (let i = 0; i < data.length; i += 4) {
      if (data[i] > 12 || data[i + 1] > 12 || data[i + 2] > 12) lit++;
    }
    return lit > 500;
  }, null, { timeout: 5000 });
  await page.locator("#device").screenshot({ path: "app/build/emu/home-refusal.png" });
  console.log("Captured non-blank HOME refusal overlay with PRE: INST cue.");
} finally {
  await browser.close();
}
