import assert from "node:assert/strict";
import { chromium } from "playwright";

const baseUrl = "http://127.0.0.1:4173";
const browser = await chromium.launch({
  headless: true,
  args: ["--autoplay-policy=no-user-gesture-required"]
});

const pageErrors = [];
try {
  const testPage = await browser.newPage();
  testPage.on("pageerror", error => pageErrors.push("test console: " + error.message));
  let response;
  for (let attempt = 0; attempt < 40; attempt++) {
    try {
      response = await testPage.goto(baseUrl + "/testing.html", { waitUntil: "domcontentloaded", timeout: 3000 });
      if (response?.ok()) break;
    } catch {}
    await new Promise(resolve => setTimeout(resolve, 250));
  }
  assert(response?.ok(), "The local Pages artifact did not become available.");
  await testPage.locator("#run-native-smoke").click();
  await testPage.waitForFunction(
    () => /NATIVE BROWSER SELF-TEST (PASSED|FAILED)/.test(document.querySelector("#native-status")?.textContent || ""),
    null,
    { timeout: 120000 }
  );
  const status = await testPage.locator("#native-status").innerText();
  const results = await testPage.locator("#native-results").innerText();
  assert.match(status, /PASSED/, "Native browser audio test failed: " + results);
  assert.match(results, /LIVE AUDIO RENDER/, "No measured live audio result was reported.");
  console.log("PASS · Native Felucca browser self-test: " + status);
  console.log(results);

  const appPage = await browser.newPage();
  appPage.on("pageerror", error => pageErrors.push("instrument app: " + error.message));
  const appResponse = await appPage.goto(baseUrl + "/", { waitUntil: "domcontentloaded" });
  assert(appResponse?.ok(), "The FM1-Lab product page did not load.");
  await appPage.locator("#enter").click();
  await appPage.waitForFunction(
    () => /FELUCCA FM6 (ENGINE )?ACTIVE/.test(document.querySelector("#engine-status")?.textContent || ""),
    null,
    { timeout: 120000 }
  );
  await appPage.waitForFunction(
    () => /4 \/ 4 NATIVE VOICES LOADED/.test(document.querySelector("#native-status")?.textContent || ""),
    null,
    { timeout: 120000 }
  );
  await appPage.locator('.edit-patch[data-patch="0"]').click();
  await appPage.locator("#editor-screen").waitFor({ state: "visible" });
  assert.equal(await appPage.locator("#patch-id").innerText(), "FM PATCH 01");
  await appPage.locator("#play").click();
  await appPage.waitForTimeout(250);
  assert.deepEqual(pageErrors, [], "Browser JavaScript errors: " + pageErrors.join("; "));
  console.log("PASS · Product boot, four native voices, patch editor and play control.");
} finally {
  await browser.close();
}
