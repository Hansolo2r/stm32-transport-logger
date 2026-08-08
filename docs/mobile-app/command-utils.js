(function attachCommandUtilities(global) {
  "use strict";

  function pad2(value) {
    return String(value).padStart(2, "0");
  }

  function formatTimeSetCommand(date) {
    return `time set ${date.getFullYear()}-${pad2(date.getMonth() + 1)}-${pad2(date.getDate())} `
      + `${pad2(date.getHours())}:${pad2(date.getMinutes())}:${pad2(date.getSeconds())}`;
  }

  function splitCommandPayload(command, maxBytes = 20) {
    if (!Number.isInteger(maxBytes) || maxBytes < 1) throw new RangeError("maxBytes must be positive");
    const payload = new TextEncoder().encode(`${command.trim()}\n`);
    const chunks = [];
    for (let offset = 0; offset < payload.byteLength; offset += maxBytes) {
      chunks.push(payload.slice(offset, offset + maxBytes));
    }
    return chunks;
  }

  global.CommandUtils = { formatTimeSetCommand, splitCommandPayload };
}(window));
