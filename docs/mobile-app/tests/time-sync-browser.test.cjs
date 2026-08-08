const assert = require("node:assert/strict");
const path = require("node:path");
const test = require("node:test");
const { pathToFileURL } = require("node:url");
const { chromium } = require("playwright");

async function openDashboard(viewport) {
  const browser = await chromium.launch({
    headless: true,
    executablePath: "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe"
  });
  const page = await browser.newPage({ viewport });
  const errors = [];
  page.on("pageerror", (error) => errors.push(error.message));
  page.on("console", (message) => { if (message.type() === "error") errors.push(message.text()); });
  await page.addInitScript(() => {
    window.__bleWrites = [];
    const characteristic = {
      startNotifications: async () => characteristic,
      addEventListener: () => {},
      writeValueWithoutResponse: async (value) => {
        const bytes = new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
        window.__bleWrites.push(Array.from(bytes));
      }
    };
    const device = {
      name: "JDY-16 Test",
      addEventListener: () => {},
      gatt: {
        connect: async () => ({
          getPrimaryService: async () => ({ getCharacteristic: async () => characteristic })
        })
      }
    };
    Object.defineProperty(navigator, "bluetooth", {
      configurable: true,
      value: { requestDevice: async () => device }
    });
  });
  const url = pathToFileURL(path.join(__dirname, "..", "index.html")).href;
  await page.goto(url);
  await page.click("#connectButton");
  await page.waitForFunction(() => window.__bleWrites.length >= 1);
  await page.evaluate(() => { window.__bleWrites = []; });
  return { browser, errors, page };
}

test("sends phone time as BLE-safe chunks and keeps the dashboard responsive", async () => {
  const { browser, errors, page } = await openDashboard({ width: 390, height: 844 });
  try {
    await page.click("#syncTimeButton");
    await page.waitForTimeout(1100);
    const writes = await page.evaluate(() => window.__bleWrites);
    assert.ok(writes.length >= 4);
    writes.forEach((write) => assert.ok(write.length <= 20));

    const combined = Buffer.concat(writes.map((write) => Buffer.from(write))).toString("utf8");
    assert.match(combined, /time set 20\d{2}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\ntime\nstatus_json\n/);
    assert.equal(await page.locator("#syncTimeButton").isEnabled(), true);
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth), true);
    assert.deepEqual(errors, []);
    await page.screenshot({ path: path.join(__dirname, "..", "..", "..", "..", "time-sync-mobile.png"), fullPage: true });
  } finally {
    await browser.close();
  }
});

test("shows the time-sync control without overflow on desktop", async () => {
  const { browser, errors, page } = await openDashboard({ width: 1440, height: 900 });
  try {
    assert.equal(await page.locator("#syncTimeButton").isVisible(), true);
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth), true);
    assert.deepEqual(errors, []);
    await page.screenshot({ path: path.join(__dirname, "..", "..", "..", "..", "time-sync-desktop.png"), fullPage: true });
  } finally {
    await browser.close();
  }
});
