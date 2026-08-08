const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const vm = require("node:vm");

function loadUtilities() {
  const source = fs.readFileSync(path.join(__dirname, "..", "command-utils.js"), "utf8");
  const context = { TextEncoder, window: {} };
  vm.runInNewContext(source, context, { filename: "command-utils.js" });
  return context.window.CommandUtils;
}

test("formats the phone local time as a firmware time-set command", () => {
  const { formatTimeSetCommand } = loadUtilities();
  const date = new Date(2026, 7, 8, 9, 5, 3);

  assert.equal(formatTimeSetCommand(date), "time set 2026-08-08 09:05:03");
});

test("splits a long command into BLE-safe writes with one final newline", () => {
  const { splitCommandPayload } = loadUtilities();
  const command = "time set 2026-08-08 09:05:03";
  const chunks = splitCommandPayload(command, 20);

  assert.equal(chunks.length, 2);
  chunks.forEach((chunk) => assert.ok(chunk.byteLength <= 20));

  const combined = Buffer.concat(chunks.map((chunk) => Buffer.from(chunk)));
  assert.equal(combined.toString("utf8"), `${command}\n`);
  assert.equal(combined.toString("utf8").split("\n").length - 1, 1);
});

test("keeps an existing short command in one write", () => {
  const { splitCommandPayload } = loadUtilities();
  const chunks = splitCommandPayload("status_json", 20);

  assert.equal(chunks.length, 1);
  assert.equal(Buffer.from(chunks[0]).toString("utf8"), "status_json\n");
});
