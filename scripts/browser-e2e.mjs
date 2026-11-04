import assert from "node:assert/strict";
import { chromium } from "playwright";

const baseUrl = "http://127.0.0.1:4173";
const browser = await chromium.launch({
  headless: true,
  args: ["--autoplay-policy=no-user-gesture-required"]
});

const browserErrors = [];
const networkErrors = [];

function observePage(page, label) {
  page.on("pageerror", error => browserErrors.push(label + " pageerror: " + error.stack));
  page.on("console", message => {
    if (message.type() === "error") {
      const location = message.location();
      browserErrors.push(label + " console: " + message.text() +
        (location.url ? " (" + location.url + ":" + location.lineNumber + ")" : ""));
    }
  });
  page.on("requestfailed", request => {
    networkErrors.push(label + " request failed: " + request.url() +
      " · " + (request.failure()?.errorText || "unknown"));
  });
  page.on("response", response => {
    if (response.status() >= 400) {
      networkErrors.push(label + " HTTP " + response.status() + ": " + response.url());
    }
  });
}

async function browserSnapshot(page) {
  return page.evaluate(() => ({
    url: location.href,
    readyState: document.readyState,
    title: document.title,
    nativeStatus: document.querySelector("#native-status")?.textContent ?? null,
    nativeResults: document.querySelector("#native-results")?.innerText ?? null,
    smokeReady: document.querySelector("#run-native-smoke")?.dataset.nativeSmokeReady ?? null,
    smokeDisabled: document.querySelector("#run-native-smoke")?.disabled ?? null,
    engineStatus: document.querySelector("#engine-status")?.textContent ?? null,
    bodyText: document.body?.innerText?.slice(0, 1800) ?? null
  })).catch(error => ({ snapshotError: error.message }));
}

async function waitForNativeCompletion(page) {
  try {
    await page.waitForFunction(
      () => /NATIVE BROWSER SELF-TEST (PASSED|FAILED)/.test(
        document.querySelector("#native-status")?.textContent || ""
      ),
      null,
      { timeout: 45000 }
    );
  } catch (error) {
    const snapshot = await browserSnapshot(page);
    throw new Error(
      "Native smoke did not reach a terminal state. " +
      JSON.stringify(snapshot) +
      ". Browser errors: " + browserErrors.join(" | ") +
      ". Network errors: " + networkErrors.join(" | ") +
      ". Wait error: " + error.message
    );
  }
}

try {
  const testPage = await browser.newPage();
  observePage(testPage, "native test console");

  let response;
  for (let attempt = 0; attempt < 40; attempt++) {
    try {
      response = await testPage.goto(baseUrl + "/testing.html", {
        waitUntil: "domcontentloaded",
        timeout: 3000
      });
      if (response?.ok()) break;
    } catch {}
    await new Promise(resolve => setTimeout(resolve, 250));
  }
  assert(response?.ok(), "The local Pages artifact did not become available.");

  try {
    await testPage.waitForFunction(
      () => document.querySelector("#run-native-smoke")?.dataset.nativeSmokeReady === "true",
      null,
      { timeout: 15000 }
    );
  } catch (error) {
    throw new Error(
      "Native smoke module did not initialize. " +
      JSON.stringify(await browserSnapshot(testPage)) +
      ". Browser errors: " + browserErrors.join(" | ") +
      ". Network errors: " + networkErrors.join(" | ") +
      ". Wait error: " + error.message
    );
  }

  await testPage.locator("#run-native-smoke").click();
  await waitForNativeCompletion(testPage);

  const status = await testPage.locator("#native-status").innerText();
  const results = await testPage.locator("#native-results").innerText();
  assert.match(status, /PASSED/, "Native browser audio test failed: " + results);
  assert.match(results, /LIVE AUDIO RENDER/, "No measured live audio result was reported.");
  console.log("PASS · Native Felucca browser self-test: " + status);
  console.log(results);

  const appPage = await browser.newPage();
  observePage(appPage, "instrument app");
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
  assert.deepEqual(
    [...browserErrors, ...networkErrors],
    [],
    "Browser diagnostics: " + [...browserErrors, ...networkErrors].join(" | ")
  );
  console.log("PASS · Product boot, four native voices, patch editor and play control.");
} finally {
  await browser.close();
}
