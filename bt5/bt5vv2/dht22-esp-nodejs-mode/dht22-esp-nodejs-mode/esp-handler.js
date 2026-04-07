// esp-handler.js – Quản lý kết nối ESP và điều khiển thiết bị

let espClient  = null;
let currentMode= 'AUTO';   // mode hiện tại đồng bộ từ ESP
let lastData   = null;     // dữ liệu sensor mới nhất
let heartbeatTimer = null;
let onDisconnect = null;
let lastAliveAt = 0;

const ESP_HEARTBEAT_TIMEOUT_MS = Number(process.env.ESP_HEARTBEAT_TIMEOUT_MS || 15000);

function clearHeartbeatTimer() {
  if (heartbeatTimer) {
    clearTimeout(heartbeatTimer);
    heartbeatTimer = null;
  }
}

function refreshHeartbeatTimer() {
  clearHeartbeatTimer();
  if (!espClient) return;

  heartbeatTimer = setTimeout(() => {
    console.warn(`[ESP] Mất heartbeat quá ${ESP_HEARTBEAT_TIMEOUT_MS}ms, đóng kết nối`);
    if (espClient && espClient.readyState === 1) {
      espClient.terminate();
    }
    unregisterESP('heartbeat_timeout');
  }, ESP_HEARTBEAT_TIMEOUT_MS);
}

function markAlive() {
  if (!espClient) return;
  lastAliveAt = Date.now();
  refreshHeartbeatTimer();
}

function checkESPConnection() {
  if (!espClient)
    return { connected: false, reason: 'no_client' };

  if (espClient.readyState !== 1) {
    unregisterESP('socket_not_open');
    return { connected: false, reason: 'socket_not_open' };
  }

  if (lastAliveAt > 0 && Date.now() - lastAliveAt > ESP_HEARTBEAT_TIMEOUT_MS) {
    if (espClient.readyState === 1) {
      espClient.terminate();
    }
    unregisterESP('heartbeat_timeout');
    return { connected: false, reason: 'heartbeat_timeout' };
  }

  return { connected: true, reason: 'ok' };
}

function normalizeBuzzerState(value) {
  // 20302 is the integer representation of multi-char literal 'ON' in C/C++.
  if (value === 20302) return 'on';
  if (value === 20294) return 'off';
  if (value === 1 || value === '1' || value === true) return 'on';
  if (value === 0 || value === '0' || value === false) return 'off';

  if (typeof value === 'string') {
    const v = value.trim().toLowerCase();
    if (v === '20302') return 'on';
    if (v === '20294') return 'off';
    if (v === 'on' || v === 'off') return v;
  }

  return value;
}

function normalizeSensorPayload(msg) {
  const normalized = { ...msg };

  if (normalized.buzzer_state === undefined && normalized.buzzer !== undefined)
    normalized.buzzer_state = normalized.buzzer;

  if (normalized.buzzer_state !== undefined)
    normalized.buzzer_state = normalizeBuzzerState(normalized.buzzer_state);

  return normalized;
}

// ── Kết nối / Ngắt kết nối ───────────────────────────────────────────────────
function registerESP(ws, disconnectHandler) {
  espClient = ws;
  onDisconnect = typeof disconnectHandler === 'function' ? disconnectHandler : null;
  lastAliveAt = Date.now();
  console.log('[ESP] Kết nối');
  // Đồng bộ mode hiện tại xuống ESP ngay khi nối
  sendToESP({ mode: currentMode });
  markAlive();
}

function unregisterESP(reason = 'closed') {
  const wasConnected = espClient !== null;
  espClient = null;
  lastAliveAt = 0;
  clearHeartbeatTimer();
  console.log('[ESP] Ngắt kết nối');

  if (wasConnected && onDisconnect) {
    const cb = onDisconnect;
    onDisconnect = null;
    cb(reason);
  } else {
    onDisconnect = null;
  }
}

// ── Gửi JSON xuống ESP ───────────────────────────────────────────────────────
function sendToESP(payload) {
  if (espClient && espClient.readyState === 1) {
    espClient.send(JSON.stringify(payload));
    console.log('[ESP] Gửi:', payload);
  } else {
    console.warn('[ESP] Chưa kết nối, bỏ qua lệnh:', payload);
  }
}

// ── Xử lý tin nhắn từ ESP ────────────────────────────────────────────────────
function handleESPMessage(raw) {
  try {
    markAlive();
    const msg = JSON.parse(raw);

    // Queue event: { id, data: { event: "..." } }
    if (typeof msg.id === 'number' && msg.data) {
      sendToESP({ ack: msg.id });
      console.log(`[ESP] Event id=${msg.id} event=${msg.data.event}`);
      return { type: 'event', event: msg.data.event };
    }

    // Realtime data từ ESP
    // Chuẩn hóa nhẹ một số field để FE luôn nhận format ổn định.
    if (msg.temp !== undefined && msg.humi !== undefined) {
      const normalizedMsg = normalizeSensorPayload(msg);

      currentMode  = normalizedMsg.mode || 'AUTO';
      lastData = { ...normalizedMsg, timestamp: new Date().toISOString() };
      console.log(`[ESP] Data:`, normalizedMsg);
      return { type: 'sensor', data: lastData };
    }

    // Heartbeat ping
    if (msg.topic === 'ping') return null;

    console.warn('[ESP] Tin nhắn không xác định:', raw);
    return null;
  } catch {
    console.error('[ESP] Parse lỗi:', raw);
    return null;
  }
}

// ── Lệnh từ FE: Bật/Tắt quạt ────────────────────────────────────────────────
function setFan(state) {
  if (currentMode === 'AUTO') {
    console.warn('[ESP] Chặn lệnh quạt — đang ở chế độ AUTO');
    return false;
  }
  sendToESP({ fan: state });
  return true;
}

// ── Lệnh từ FE: Tắt còi ─────────────────────────────────────────────────────
function buzzerOff() {
  if (currentMode === 'AUTO') {
    console.warn('[ESP] Chặn lệnh tắt còi — đang ở chế độ AUTO');
    return false;
  }
  sendToESP({ buzzer: 'OFF' });
  return true;
}

// ── Lệnh từ FE: Chuyển mode ─────────────────────────────────────────────────
function setMode(mode) {
  if (mode !== 'AUTO' && mode !== 'MANUAL') return;
  currentMode = mode;
  sendToESP({ mode });
  console.log(`[ESP] Mode -> ${mode}`);
}

// ── Exports ──────────────────────────────────────────────────────────────────
module.exports = {
  registerESP,
  unregisterESP,
  handleESPMessage,
  checkESPConnection,
  setFan,
  buzzerOff,
  setMode,
  getMode:        () => currentMode,
  getLastData:    () => lastData,
};
