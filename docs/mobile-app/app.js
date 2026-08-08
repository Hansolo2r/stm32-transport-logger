const SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb";
const CHARACTERISTIC_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb";
const decoder = new TextDecoder("utf-8");
const { formatTimeSetCommand, splitCommandPayload } = window.CommandUtils;

const $ = (selector) => document.querySelector(selector);
const elements = {
  connectButton: $("#connectButton"), connectionDot: $("#connectionDot"),
  connectionText: $("#connectionText"), deviceName: $("#deviceName"), deviceClock: $("#deviceClock"), lastUpdate: $("#lastUpdate"),
  temperature: $("#temperature"), motionState: $("#motionState"), motionCount: $("#motionCount"),
  shockCount: $("#shockCount"), recordCount: $("#recordCount"), storageState: $("#storageState"),
  axisX: $("#axisX"), axisY: $("#axisY"), axisZ: $("#axisZ"), eventBanner: $("#eventBanner"),
  drawer: $("#drawer"), drawerBackdrop: $("#drawerBackdrop"), drawerTitle: $("#drawerTitle"),
  drawerEyebrow: $("#drawerEyebrow"), terminal: $("#terminal"), commandForm: $("#commandForm"),
  commandInput: $("#commandInput"), toast: $("#toast"), recordsLoading: $("#recordsLoading"),
  eventsLoading: $("#eventsLoading"), shockEventsLoading: $("#shockEventsLoading"), recordsTableBody: $("#recordsTableBody"),
  motionTableBody: $("#motionTableBody"), shockTableBody: $("#shockTableBody"),
  recordsSummary: $("#recordsSummary"), motionSummary: $("#motionSummary"), shockSummary: $("#shockSummary"),
  motionPanelCount: $("#motionPanelCount"), motionPanelState: $("#motionPanelState"),
  shockPanelCount: $("#shockPanelCount"), maxShock: $("#maxShock"), temperatureChart: $("#temperatureChart"),
  chartAxisLabel: $("#chartAxisLabel"),
  configStatus: $("#configStatus"), configForm: $("#configForm"), shockThreshold: $("#shockThreshold"),
  motionThreshold: $("#motionThreshold"), motionConfirm: $("#motionConfirm"), stillConfirm: $("#stillConfirm"),
  shockCooldown: $("#shockCooldown"), temperatureInterval: $("#temperatureInterval"),
  loadConfigButton: $("#loadConfigButton"), saveConfigButton: $("#saveConfigButton"),
  syncTimeButton: $("#syncTimeButton")
};

let bleDevice = null;
let uartCharacteristic = null;
let pollTimer = null;
let receiveBuffer = "";
let transferMode = null;
let transferLines = [];
let transferTotal = 0;
let transferTimer = null;
let recentRecords = [];
let eventRecords = [];
let activePanel = null;
let loggingPaused = false;
let deviceSleeping = false;
let configBusy = false;

function appendDiagnostic(message, prefix = "") {
  const line = document.createElement("div");
  line.textContent = `${prefix}${message}`;
  elements.terminal.append(line);
  elements.terminal.scrollTop = elements.terminal.scrollHeight;
}

function showToast(message) {
  elements.toast.textContent = message;
  elements.toast.hidden = false;
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => { elements.toast.hidden = true; }, 2600);
}

function setConnected(connected, name = "") {
  elements.connectionDot.classList.toggle("connected", connected);
  elements.connectionText.textContent = connected ? "设备在线" : "设备离线";
  elements.deviceName.textContent = connected ? name : "点击连接设备";
  elements.connectButton.textContent = connected ? "断开" : "连接设备";
  if (!connected) {
    elements.lastUpdate.textContent = "尚未同步";
    elements.deviceClock.textContent = "设备时间 --";
  }
}

function stopPolling() {
  clearInterval(pollTimer);
  pollTimer = null;
}

function startPolling() {
  stopPolling();
  pollTimer = setInterval(() => {
    if (!deviceSleeping && !transferMode && !transferTimer && !configBusy) sendCommand("status_json");
  }, 3000);
}

function updateConfiguration(fields) {
  if (fields.length < 6 || fields.slice(0, 6).some((value) => !Number.isFinite(Number(value)))) return;
  [elements.shockThreshold.value, elements.motionThreshold.value, elements.motionConfirm.value,
    elements.stillConfirm.value, elements.shockCooldown.value, elements.temperatureInterval.value] = fields.slice(0, 6);
  elements.configStatus.textContent = "已读取设备当前参数";
  elements.configForm.dataset.loaded = "true";
}

function restoreAwakeState(message, requestStatus = true) {
  if (!deviceSleeping) return;
  deviceSleeping = false;
  elements.connectionText.textContent = "设备在线";
  elements.deviceName.textContent = bleDevice?.name || "JDY-16";
  $("#sleepButton").textContent = "设备休眠";
  showToast(message);
  startPolling();
  if (requestStatus) sendCommand("status_json");
}

function setLoggingPaused(paused) {
  loggingPaused = paused;
  $("#pauseButton").textContent = paused ? "继续记录" : "暂停记录";
}

function markSynced() {
  const now = new Date();
  elements.lastUpdate.textContent = `更新于 ${now.toLocaleTimeString("zh-CN", { hour12: false })}`;
}

function normalizeTimestamp(value) {
  if (!/^20\d{2}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/.test(value || "")) return null;
  const parsed = new Date(value.replace(" ", "T"));
  return Number.isNaN(parsed.getTime()) ? null : value;
}

function setDeviceClock(timestamp) {
  const normalized = normalizeTimestamp(timestamp);
  elements.deviceClock.textContent = normalized ? `设备时间 ${normalized}` : "设备时间未设置";
}

function showEvent(message) {
  elements.eventBanner.textContent = message;
  elements.eventBanner.hidden = false;
  clearTimeout(showEvent.timer);
  showEvent.timer = setTimeout(() => { elements.eventBanner.hidden = true; }, 2500);
}

function updateStatus(fields) {
  if (fields.length < 10) return;
  const temperatureTenths = Number(fields[0]);
  elements.temperature.textContent = temperatureTenths === -32768 ? "--.-" : (temperatureTenths / 10).toFixed(1);
  elements.motionCount.textContent = fields[1];
  elements.shockCount.textContent = fields[2];
  elements.recordCount.textContent = fields[3];
  elements.storageState.textContent = fields[4] === "1" && fields[5] === "1" ? "正常" : "异常";
  const paused = fields.length >= 11 && fields[10] === "1";
  if (fields.length >= 12) setDeviceClock(fields[11]);
  elements.motionState.textContent = paused ? "已暂停" : fields[6] === "1" ? "运动中" : "静止";
  elements.axisX.textContent = `${fields[7]} mg`;
  elements.axisY.textContent = `${fields[8]} mg`;
  elements.axisZ.textContent = `${fields[9]} mg`;
  elements.motionPanelCount.textContent = fields[1];
  elements.shockPanelCount.textContent = fields[2];
  elements.motionPanelState.textContent = paused ? "已暂停" : fields[6] === "1" ? "运动中" : "静止";
  setLoggingPaused(paused);
  markSynced();
}

function finishTransfer(mode) {
  clearTimeout(transferTimer);
  transferTimer = null;
  if (mode === "preview") {
    recentRecords = parsePreview(transferLines);
    renderRecords();
    elements.recordsLoading.hidden = true;
  } else if (mode === "events") {
    eventRecords = parseEvents(transferLines);
    renderEvents();
    elements.eventsLoading.hidden = true;
    elements.shockEventsLoading.hidden = true;
  } else if (mode === "csv") {
    downloadCsv(transferLines.join("\r\n"));
    showToast(`已接收 ${Math.max(0, transferLines.length - 1)} 条完整记录`);
  }
  transferMode = null;
  transferLines = [];
}

function processLine(rawLine) {
  const line = rawLine.replace(/\r$/, "");
  if (!line) return;
  if (line === "PREVIEW_BEGIN") { transferMode = "preview"; transferLines = []; return; }
  if (line.startsWith("EVENTS_BEGIN")) { transferMode = "events"; transferLines = []; transferTotal = Number(line.split(",")[1]) || 0; return; }
  if (line === "CSV_BEGIN") { transferMode = "csv"; transferLines = []; return; }
  if (line === "PREVIEW_END") { finishTransfer("preview"); return; }
  if (line === "EVENTS_END") { finishTransfer("events"); return; }
  if (line === "CSV_END") { finishTransfer("csv"); return; }
  if (/(PREVIEW|EVENTS|CSV)_ERROR/.test(line)) {
    clearTimeout(transferTimer);
    transferTimer = null;
    elements.recordsLoading.hidden = true;
    elements.eventsLoading.hidden = true;
    elements.shockEventsLoading.hidden = true;
    transferMode = null;
    transferLines = [];
    showToast("读取失败，请重试");
    appendDiagnostic(line, "设备：");
    return;
  }
  if (transferMode) { transferLines.push(line); return; }

  if (line.startsWith("@STATUS,")) {
    restoreAwakeState("检测到设备已重新运行", false);
    updateStatus(line.slice(8).split(","));
    return;
  }
  if (line.startsWith("@TEMP,")) {
    restoreAwakeState("检测到设备已重新运行");
    const [valueText, timestamp] = line.slice(6).split(",");
    const value = Number(valueText);
    if (Number.isFinite(value)) elements.temperature.textContent = (value / 10).toFixed(1);
    if (timestamp) setDeviceClock(timestamp);
    markSynced();
    return;
  }
  if (line.startsWith("@EVENT,")) {
    restoreAwakeState("检测到设备已重新运行");
    const [, eventName, count, timestamp] = line.split(",");
    if (timestamp) setDeviceClock(timestamp);
    if (eventName === "SHOCK") {
      elements.shockCount.textContent = count;
      elements.shockPanelCount.textContent = count;
      showEvent(`检测到碰撞，累计 ${count} 次`);
    } else if (eventName === "MOTION_START") {
      elements.motionCount.textContent = count;
      elements.motionPanelCount.textContent = count;
      elements.motionState.textContent = "运动中";
      elements.motionPanelState.textContent = "运动中";
      showEvent(`检测到运动，累计 ${count} 次`);
    } else if (eventName === "MOTION_END") {
      elements.motionState.textContent = "静止";
      elements.motionPanelState.textContent = "静止";
    }
    markSynced();
    return;
  }
  if (line === "@CONTROL,PAUSED") {
    setLoggingPaused(true);
    elements.motionState.textContent = "已暂停";
    showToast("记录已暂停，实时数据显示仍会更新");
    return;
  }
  if (line === "@CONTROL,RUNNING") {
    setLoggingPaused(false);
    elements.motionState.textContent = "静止";
    elements.motionPanelState.textContent = "静止";
    showToast("已继续记录");
    sendCommand("status_json");
    return;
  }
  if (line === "@SLEEPING") {
    deviceSleeping = true;
    stopPolling();
    elements.connectionText.textContent = "设备休眠";
    elements.deviceName.textContent = "蓝牙保持连接，可点击唤醒";
    $("#sleepButton").textContent = "唤醒设备";
    showToast("设备已休眠，蓝牙连接保持");
    return;
  }
  if (line === "@AWAKE") {
    restoreAwakeState("设备已唤醒并恢复运行");
    return;
  }
  if (line.startsWith("@CFG,")) {
    const fields = line.slice(5).split(",");
    if (fields[0] === "SAVED") {
      elements.configStatus.textContent = "参数已保存，断电后仍然有效";
      showToast("设备参数已永久保存");
    } else if (fields[0] === "DEFAULTS") {
      elements.configStatus.textContent = "已恢复默认值，点击保存后永久生效";
      showToast("默认参数已载入");
    } else if (fields[0] === "ERROR") {
      const messages = { FORMAT: "参数命令格式错误", RANGE: "参数超出允许范围", FLASH: "Flash保存失败" };
      elements.configStatus.textContent = messages[fields[1]] || "参数操作失败";
      showToast(elements.configStatus.textContent);
    } else {
      updateConfiguration(fields);
    }
    return;
  }
  if (line.startsWith("DS1302时间：")) {
    const timestamp = normalizeTimestamp(line.slice("DS1302时间：".length));
    setDeviceClock(timestamp);
    showToast(timestamp ? "时间同步成功" : "设备返回的时间无效");
    return;
  }
  if (line.includes("DS1302：读取值无效") || line.includes("DS1302：时间写入失败")) {
    showToast("时间模块读取无效，请检查供电和三根信号线");
    appendDiagnostic(line, "设备：");
    return;
  }
  appendDiagnostic(line, "设备：");
}

function handleNotification(event) {
  receiveBuffer += decoder.decode(event.target.value, { stream: true });
  const lines = receiveBuffer.split("\n");
  receiveBuffer = lines.pop() || "";
  lines.forEach(processLine);
}

async function sendCommand(command, { silent = true, transfer = null } = {}) {
  if (!uartCharacteristic) {
    if (!silent) showToast("设备尚未连接");
    return false;
  }
  if (transferMode || transferTimer) {
    if (!silent) showToast("设备正在传输数据");
    return false;
  }
  const trimmed = command.trim();
  if (deviceSleeping && trimmed !== "wake") {
    if (!silent) showToast("请先唤醒设备");
    return false;
  }
  const chunks = splitCommandPayload(trimmed, 20);
  try {
    if (transfer) {
      transferLines = [];
      transferTimer = setTimeout(() => {
        transferTimer = null;
        transferMode = null;
        elements.recordsLoading.hidden = true;
        elements.eventsLoading.hidden = true;
        elements.shockEventsLoading.hidden = true;
        showToast("设备读取超时，请重试");
      }, 90000);
    }
    for (let index = 0; index < chunks.length; index += 1) {
      const chunk = chunks[index];
      if (typeof uartCharacteristic.writeValueWithoutResponse === "function") await uartCharacteristic.writeValueWithoutResponse(chunk);
      else await uartCharacteristic.writeValue(chunk);
      if (index + 1 < chunks.length) await new Promise((resolve) => setTimeout(resolve, 30));
    }
    if (!silent) appendDiagnostic(trimmed, "发送：");
    return true;
  } catch (error) {
    clearTimeout(transferTimer);
    transferTimer = null;
    appendDiagnostic(error.message, "发送失败：");
    showToast("发送失败");
    return false;
  }
}

async function syncPhoneTime() {
  if (!uartCharacteristic) { showToast("请先连接设备"); return; }
  configBusy = true;
  elements.syncTimeButton.disabled = true;
  try {
    const command = formatTimeSetCommand(new Date());
    if (!await sendCommand(command, { silent: true })) return;
    showToast("手机时间已发送，正在读取设备时间");
    await new Promise((resolve) => setTimeout(resolve, 500));
    await sendCommand("time", { silent: true });
    await new Promise((resolve) => setTimeout(resolve, 250));
    await sendCommand("status_json", { silent: true });
  } finally {
    configBusy = false;
    elements.syncTimeButton.disabled = false;
  }
}

function handleDisconnected() {
  stopPolling();
  uartCharacteristic = null;
  deviceSleeping = false;
  $("#sleepButton").textContent = "设备休眠";
  setConnected(false);
  showToast("蓝牙连接已断开");
}

async function connectDevice() {
  if (uartCharacteristic) { bleDevice?.gatt?.disconnect(); return; }
  if (!navigator.bluetooth) { showToast("iPhone 请使用 Bluefy 打开本页面"); return; }
  elements.connectButton.disabled = true;
  try {
    bleDevice = await navigator.bluetooth.requestDevice({ acceptAllDevices: true, optionalServices: [SERVICE_UUID] });
    bleDevice.addEventListener("gattserverdisconnected", handleDisconnected);
    const server = await bleDevice.gatt.connect();
    const service = await server.getPrimaryService(SERVICE_UUID);
    uartCharacteristic = await service.getCharacteristic(CHARACTERISTIC_UUID);
    await uartCharacteristic.startNotifications();
    uartCharacteristic.addEventListener("characteristicvaluechanged", handleNotification);
    setConnected(true, bleDevice.name || "JDY-16");
    showToast("设备连接成功");
    await sendCommand("status_json");
    startPolling();
  } catch (error) {
    setConnected(false);
    appendDiagnostic(error.message, "连接失败：");
    showToast("连接失败，请在诊断页查看原因");
  } finally {
    elements.connectButton.disabled = false;
  }
}

const panelTitles = {
  records: ["设备数据", "记录预览"], motion: ["事件分析", "运动记录"],
  shock: ["事件分析", "碰撞记录"], help: ["使用说明", "帮助"],
  more: ["应用设置", "更多"], config: ["设备设置", "检测参数"],
  diagnostics: ["开发工具", "通信诊断"]
};

function openPanel(panel) {
  activePanel = panel;
  document.querySelectorAll(".panel-view").forEach((view) => { view.hidden = view.dataset.view !== panel; });
  [elements.drawerEyebrow.textContent, elements.drawerTitle.textContent] = panelTitles[panel];
  elements.drawerBackdrop.hidden = false;
  elements.drawer.classList.add("open");
  elements.drawer.setAttribute("aria-hidden", "false");
  if (panel === "records" && uartCharacteristic && recentRecords.length === 0) loadPreview();
  if ((panel === "motion" || panel === "shock") && uartCharacteristic && eventRecords.length === 0) loadEvents();
  if (panel === "config" && uartCharacteristic) loadConfiguration();
}

function closePanel() {
  activePanel = null;
  elements.drawer.classList.remove("open");
  elements.drawer.setAttribute("aria-hidden", "true");
  setTimeout(() => { if (!elements.drawer.classList.contains("open")) elements.drawerBackdrop.hidden = true; }, 210);
}

function parsePreview(lines) {
  const hasTimestamp = lines[0]?.includes("timestamp");
  return lines.slice(1).map((line) => {
    const fields = line.split(",");
    const [sequence, session, timestamp, time, temp, event] = hasTimestamp ? fields :
      [fields[0], fields[1], "NA", fields[2], fields[3], fields[4]];
    return { sequence: Number(sequence), session: Number(session), timestamp: normalizeTimestamp(timestamp), time: Number(time), temp: Number(temp), event };
  }).filter((item) => Number.isFinite(item.sequence));
}

function parseEvents(lines) {
  const hasTimestamp = lines[0]?.includes("timestamp");
  return lines.slice(1).map((line) => {
    const fields = line.split(",");
    const [sequence, session, timestamp, time, type, temp, x, y, z] = hasTimestamp ? fields :
      [fields[0], fields[1], "NA", fields[2], fields[3], fields[4], fields[5], fields[6], fields[7]];
    return { sequence: Number(sequence), session: Number(session), timestamp: normalizeTimestamp(timestamp), time: Number(time), type, temp: Number(temp), x: Number(x), y: Number(y), z: Number(z) };
  }).filter((item) => Number.isFinite(item.sequence));
}

const eventLabel = (event) => ({ TEMP: "温度", MOTION_START: "运动开始", MOTION_END: "运动结束", SHOCK: "碰撞" }[event] || event);
const formatTime = (seconds) => `${Math.floor(seconds / 3600).toString().padStart(2, "0")}:${Math.floor(seconds % 3600 / 60).toString().padStart(2, "0")}:${Math.floor(seconds % 60).toString().padStart(2, "0")}`;
const formatRecordTime = (item) => item.timestamp || `会话内 ${formatTime(item.time)}`;

function setEmptyRow(body, columns, text) { body.innerHTML = `<tr><td class="empty-cell" colspan="${columns}">${text}</td></tr>`; }

function renderRecords() {
  elements.recordsSummary.textContent = `显示最近 ${recentRecords.length} 条，设备共 ${elements.recordCount.textContent} 条`;
  if (!recentRecords.length) setEmptyRow(elements.recordsTableBody, 4, "暂无可预览记录");
  else elements.recordsTableBody.innerHTML = recentRecords.map((item) =>
    `<tr><td>${item.session}</td><td>${formatRecordTime(item)}</td><td>${Number.isFinite(item.temp) ? item.temp.toFixed(1) + " °C" : "--"}</td><td>${eventLabel(item.event)}</td></tr>`).join("");
  drawTemperatureChart();
}

function renderEvents() {
  const motion = eventRecords.filter((item) => item.type === "MOTION_START" || item.type === "MOTION_END");
  const shocks = eventRecords.filter((item) => item.type === "SHOCK");
  elements.motionSummary.textContent = `最近事件中包含 ${motion.length} 条运动状态变化`;
  elements.shockSummary.textContent = `最近事件中包含 ${shocks.length} 次碰撞`;
  if (!motion.length) setEmptyRow(elements.motionTableBody, 4, "暂无运动记录");
  else elements.motionTableBody.innerHTML = motion.map((item) =>
    `<tr><td>${item.session}</td><td>${formatRecordTime(item)}</td><td>${eventLabel(item.type)}</td><td>${item.temp.toFixed(1)} °C</td></tr>`).join("");
  if (!shocks.length) {
    setEmptyRow(elements.shockTableBody, 4, "暂无碰撞记录");
    elements.maxShock.textContent = "-- g";
  } else {
    const magnitudes = shocks.map((item) => Math.sqrt(item.x ** 2 + item.y ** 2 + item.z ** 2) / 1000);
    elements.maxShock.textContent = `${Math.max(...magnitudes).toFixed(2)} g`;
    elements.shockTableBody.innerHTML = shocks.map((item) => {
      const magnitude = Math.sqrt(item.x ** 2 + item.y ** 2 + item.z ** 2) / 1000;
      return `<tr><td>${item.session}</td><td>${formatRecordTime(item)}</td><td>${magnitude.toFixed(2)} g</td><td>${item.x}, ${item.y}, ${item.z}</td></tr>`;
    }).join("");
  }
  transferTotal = 0;
}

function drawTemperatureChart() {
  const canvas = elements.temperatureChart;
  const rect = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  const width = Math.max(280, rect.width);
  const height = 230;
  const plotLeft = 46;
  const plotRight = width - 14;
  const plotTop = 28;
  const plotBottom = 170;
  canvas.width = width * dpr;
  canvas.height = height * dpr;
  const ctx = canvas.getContext("2d");
  ctx.scale(dpr, dpr);
  ctx.clearRect(0, 0, width, height);
  ctx.strokeStyle = "#e3e7ea";
  ctx.lineWidth = 1;
  for (let i = 0; i < 4; i++) {
    const y = plotTop + i * ((plotBottom - plotTop) / 3);
    ctx.beginPath(); ctx.moveTo(plotLeft, y); ctx.lineTo(plotRight, y); ctx.stroke();
  }
  ctx.strokeStyle = "#aeb7bf";
  ctx.beginPath(); ctx.moveTo(plotLeft, plotBottom); ctx.lineTo(plotRight, plotBottom); ctx.stroke();

  const parseTimestampMs = (timestamp) => timestamp ? new Date(timestamp.replace(" ", "T")).getTime() : NaN;
  const anchors = recentRecords.map((item) => ({
    session: item.session,
    time: item.time,
    timestampMs: parseTimestampMs(item.timestamp)
  })).filter((item) => Number.isFinite(item.timestampMs) && Number.isFinite(item.time));
  const allPoints = recentRecords.filter((item) => Number.isFinite(item.temp) && item.temp > -100);
  const estimatedPoints = allPoints.map((item) => {
    const directTimestampMs = parseTimestampMs(item.timestamp);
    if (Number.isFinite(directTimestampMs)) return { ...item, axisMs: directTimestampMs, estimated: false };
    const sameSessionAnchors = anchors.filter((anchor) => anchor.session === item.session);
    if (!sameSessionAnchors.length || !Number.isFinite(item.time)) return { ...item, axisMs: NaN, estimated: false };
    const nearest = sameSessionAnchors.reduce((best, anchor) =>
      Math.abs(anchor.time - item.time) < Math.abs(best.time - item.time) ? anchor : best);
    return { ...item, axisMs: nearest.timestampMs + (item.time - nearest.time) * 1000, estimated: true };
  }).filter((item) => Number.isFinite(item.axisMs));
  const calendarTimed = estimatedPoints.length >= 2;
  const points = calendarTimed ? estimatedPoints : allPoints;
  const values = points.map((item) => item.temp);
  if (values.length < 2) {
    elements.chartAxisLabel.textContent = "等待温度记录";
    ctx.fillStyle = "#7b858f"; ctx.font = "13px sans-serif"; ctx.fillText("至少需要两条有效温度记录", 54, 100); return;
  }
  const min = Math.min(...values), max = Math.max(...values), span = Math.max(1, max - min);
  const calendarTimes = calendarTimed ? points.map((item) => item.axisMs) : [];
  const firstTime = calendarTimed ? Math.min(...calendarTimes) : 0;
  const lastTime = calendarTimed ? Math.max(...calendarTimes) : 0;
  const sameSession = points.every((item) => item.session === points[0].session);
  const relativeTimed = !calendarTimed && sameSession && points.every((item) => Number.isFinite(item.time));
  const relativeTimes = relativeTimed ? points.map((item) => item.time) : [];
  const firstRelative = relativeTimed ? Math.min(...relativeTimes) : 0;
  const lastRelative = relativeTimed ? Math.max(...relativeTimes) : 0;
  const timeSpan = Math.max(1, lastTime - firstTime);
  const relativeSpan = Math.max(1, lastRelative - firstRelative);
  ctx.fillStyle = "#66717e"; ctx.font = "11px sans-serif";
  ctx.fillText(`${max.toFixed(1)}°`, 5, plotTop + 4); ctx.fillText(`${min.toFixed(1)}°`, 5, plotBottom + 4);
  ctx.strokeStyle = "#176b55"; ctx.lineWidth = 2; ctx.beginPath();
  points.forEach((point, index) => {
    const ratio = calendarTimed && lastTime > firstTime ? (calendarTimes[index] - firstTime) / timeSpan :
      relativeTimed && lastRelative > firstRelative ? (relativeTimes[index] - firstRelative) / relativeSpan :
      index / (points.length - 1);
    const x = plotLeft + ratio * (plotRight - plotLeft);
    const value = point.temp;
    const y = plotBottom - (value - min) / span * (plotBottom - plotTop);
    if (index === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  });
  ctx.stroke();
  ctx.fillStyle = "#59636f";
  ctx.font = "12px sans-serif";
  if (calendarTimed) {
    const approximate = points.some((item) => item.estimated);
    const formatAxisDate = (milliseconds) => {
      const date = new Date(milliseconds);
      return `${String(date.getMonth() + 1).padStart(2, "0")}-${String(date.getDate()).padStart(2, "0")} ${String(date.getHours()).padStart(2, "0")}:${String(date.getMinutes()).padStart(2, "0")}`;
    };
    elements.chartAxisLabel.textContent = approximate ? "推算时间轴" : "真实时间轴";
    ctx.textAlign = "left";
    ctx.fillText(formatAxisDate(firstTime), plotLeft, 207);
    ctx.textAlign = "right";
    ctx.fillText(formatAxisDate(lastTime), plotRight, 207);
    ctx.textAlign = "start";
  } else if (relativeTimed) {
    elements.chartAxisLabel.textContent = "会话时间轴";
    ctx.textAlign = "left";
    ctx.fillText(formatTime(firstRelative), plotLeft, 207);
    ctx.textAlign = "right";
    ctx.fillText(formatTime(lastRelative), plotRight, 207);
    ctx.textAlign = "start";
  } else {
    elements.chartAxisLabel.textContent = "记录顺序";
    ctx.fillText("较早", plotLeft, 207);
    ctx.textAlign = "right";
    ctx.fillText("较新", plotRight, 207);
    ctx.textAlign = "start";
  }
}

async function loadPreview() {
  elements.recordsLoading.hidden = false;
  const ok = await sendCommand("preview", { transfer: "preview", silent: false });
  if (!ok) elements.recordsLoading.hidden = true;
}

async function loadEvents() {
  elements.eventsLoading.hidden = false;
  elements.shockEventsLoading.hidden = false;
  const ok = await sendCommand("events", { transfer: "events", silent: false });
  if (!ok) {
    elements.eventsLoading.hidden = true;
    elements.shockEventsLoading.hidden = true;
  }
}

const wait = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds));

async function loadConfiguration() {
  if (!uartCharacteristic) { showToast("请先连接设备"); return; }
  elements.configStatus.textContent = "正在读取设备参数…";
  await sendCommand("cfg get", { silent: true });
}

async function saveConfiguration(event) {
  event.preventDefault();
  if (!uartCharacteristic) { showToast("请先连接设备"); return; }
  if (!elements.configForm.reportValidity()) return;
  const commands = [
    `cfg shock ${Math.trunc(Number(elements.shockThreshold.value))}`,
    `cfg motion ${Math.trunc(Number(elements.motionThreshold.value))}`,
    `cfg mconf ${Math.trunc(Number(elements.motionConfirm.value))}`,
    `cfg sconf ${Math.trunc(Number(elements.stillConfirm.value))}`,
    `cfg cool ${Math.trunc(Number(elements.shockCooldown.value))}`,
    `cfg temp ${Math.trunc(Number(elements.temperatureInterval.value))}`,
  ];
  configBusy = true;
  elements.saveConfigButton.disabled = true;
  elements.configStatus.textContent = "正在写入并校验参数…";
  try {
    for (const command of commands) {
      if (!await sendCommand(command, { silent: true })) throw new Error("参数发送失败");
      await wait(120);
    }
    if (!await sendCommand("cfg save", { silent: true })) throw new Error("保存命令发送失败");
  } catch (error) {
    elements.configStatus.textContent = error.message;
    showToast("参数保存失败");
  } finally {
    configBusy = false;
    elements.saveConfigButton.disabled = false;
  }
}

async function restoreDefaultConfiguration() {
  if (!uartCharacteristic) { showToast("请先连接设备"); return; }
  if (!confirm("恢复出厂检测参数？恢复后还需要点击“保存到设备”才会永久生效。")) return;
  elements.configStatus.textContent = "正在恢复默认参数…";
  await sendCommand("cfg defaults", { silent: true });
}

function downloadCsv(contents) {
  const blob = new Blob([contents], { type: "text/csv;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = `transport-logger-${new Date().toISOString().replace(/[:.]/g, "-")}.csv`;
  document.body.append(link); link.click(); link.remove(); URL.revokeObjectURL(url);
}

elements.connectButton.addEventListener("click", connectDevice);
$("#helpButton").addEventListener("click", () => openPanel("help"));
$("#menuButton").addEventListener("click", () => openPanel("more"));
$("#closeDrawerButton").addEventListener("click", closePanel);
elements.drawerBackdrop.addEventListener("click", closePanel);
document.querySelectorAll("[data-panel]").forEach((button) => button.addEventListener("click", () => openPanel(button.dataset.panel)));
$("#refreshButton").addEventListener("click", () => sendCommand("status_json", { silent: false }));
elements.syncTimeButton.addEventListener("click", syncPhoneTime);
$("#pauseButton").addEventListener("click", () => sendCommand(loggingPaused ? "resume" : "pause", { silent: false }));
$("#sleepButton").addEventListener("click", async () => {
  if (deviceSleeping) {
    await sendCommand("wake", { silent: false });
  } else if (confirm("设备将进入可由蓝牙唤醒的休眠状态，并暂时停止采集和记录。确认继续吗？")) {
    await sendCommand("sleep", { silent: false });
  }
});
$("#reloadRecordsButton").addEventListener("click", loadPreview);
document.querySelectorAll(".reload-events").forEach((button) => button.addEventListener("click", loadEvents));
$("#downloadButton").addEventListener("click", () => sendCommand("export", { transfer: "csv", silent: false }));
elements.loadConfigButton.addEventListener("click", loadConfiguration);
elements.configForm.addEventListener("submit", saveConfiguration);
$("#defaultConfigButton").addEventListener("click", restoreDefaultConfiguration);
$("#clearButton").addEventListener("click", async () => {
  if (confirm("确认删除设备中的全部温度和事件记录？此操作无法撤销。")) {
    if (await sendCommand("clear confirm", { silent: false })) {
      recentRecords = []; eventRecords = []; renderRecords(); renderEvents(); showToast("已发送清空命令");
      setTimeout(() => sendCommand("status_json"), 1500);
    }
  }
});
elements.commandForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const command = elements.commandInput.value.trim();
  if (command) { sendCommand(command, { silent: false }); elements.commandInput.value = ""; }
});
$("#clearTerminalButton").addEventListener("click", () => elements.terminal.replaceChildren());
window.addEventListener("resize", () => { if (activePanel === "records") drawTemperatureChart(); });
window.addEventListener("keydown", (event) => { if (event.key === "Escape" && activePanel) closePanel(); });

setConnected(false);
renderRecords();
renderEvents();
