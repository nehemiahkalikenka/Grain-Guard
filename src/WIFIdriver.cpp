#include "WIFIdriver.h"
#include "lcdMenu.h"
WebServer server(80);

// ─── setupWiFi ─────────────────────────────────────────────────────────────
// On first boot (or after resetWiFiCredentials):
//   1. ESP32 creates an Access Point named "GrainGuard-AP"
//   2. Connect your phone to that AP (password: 12345678)
//   3. A captive portal opens automatically — select your network, enter password
//   4. ESP32 saves credentials to NVS flash and connects
//
// On subsequent boots:
//   Credentials are loaded from NVS — connects automatically, no portal needed.
void setupWiFi() {
    WiFiManager wm;

    // ── Optional tuning ─────────────────────────────────────────────────
    // Portal stays open for 3 minutes before giving up and going offline.
    // Remove or increase if you need more time to configure in the field.
    wm.setConfigPortalTimeout(180);

    // Dark mode for the portal UI — easier to read outdoors on a phone
    wm.setClass("invert");

    // Show a custom title in the portal
    wm.setTitle("GrainGuard Setup");

    Serial.println(F("\n[WiFi] Starting WiFiManager..."));
    Serial.println(F("[WiFi] If not connected, join AP 'GrainGuard-AP' (pw: 12345678)"));
    strncpy(wifiIPStr, "No WiFi", sizeof(wifiIPStr));
    Serial.println(F("[WiFi] Then open 192.168.4.1 in your browser"));

    // autoConnect:
    //   - If saved credentials exist and work → connects silently
    //   - If no saved credentials or connection fails → launches AP + portal
    bool connected = wm.autoConnect("GrainGuard-AP", "12345678");

    if (!connected) {
        Serial.println(F("[WiFi] Could not connect — running in offline mode."));
        Serial.println(F("[WiFi] Sensor logging and LCD still active."));
        Serial.println(F("[WiFi] Send 'Reset WiFi' from LCD menu to reconfigure."));
        // System continues — SD card logging, LCD, and SMS still work without WiFi
    } else {
        Serial.print(F("[WiFi] Connected! IP: "));
        Serial.println(WiFi.localIP());
        Serial.print(F("[WiFi] Signal strength: "));
        Serial.print(WiFi.RSSI());
        Serial.println(F(" dBm"));
    }
}

// ─── resetWiFiCredentials ──────────────────────────────────────────────────
// Clears saved SSID/password from NVS and restarts into config portal.
// Called from LCD menu "Reset WiFi" option or web dashboard button.
void resetWiFiCredentials() {
    Serial.println(F("[WiFi] Clearing stored credentials..."));
    WiFiManager wm;
    wm.resetSettings();
    Serial.println(F("[WiFi] Credentials cleared. Restarting in 2s..."));
    delay(2000);
    ESP.restart();
}

// ─── isWiFiConnected ───────────────────────────────────────────────────────
bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

// ─── setupWebServer ────────────────────────────────────────────────────────
void setupWebServer() {
    if (!isWiFiConnected()) {
        Serial.println(F("[WebServer] Skipped — no WiFi connection."));
        return;
    }

    server.on("/",             HTTP_GET,  handleRoot);
    server.on("/api/data",     HTTP_GET,  handleAPI);
    server.on("/api/alerts/clear", HTTP_POST, handleClearAlerts);
    server.on("/api/sdlog",    HTTP_GET,  handleSDLog);
    server.on("/api/resetwifi", HTTP_POST, handleResetWiFi);  // Dashboard trigger
    server.begin();

    Serial.print(F("[WebServer] Running at http://"));
    Serial.println(WiFi.localIP());
}

// ─── handleResetWiFi ───────────────────────────────────────────────────────
// POST /api/resetwifi — triggers credential wipe from web dashboard
// Add a button to INDEX_HTML: fetch('/api/resetwifi', {method:'POST'})
void handleResetWiFi() {
    server.send(200, "application/json",
        "{\"ok\":true,\"msg\":\"Resetting WiFi. Device will restart.\"}");
    delay(500);
    resetWiFiCredentials();
}


// HTML (PROGMEM) 
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GrainGuard</title>
<style>
  
  :root {
    --bg:      #f7f6f2;
    --surface: #ffffff;
    --border:  #dddbd3;
    --amber:   #854F0B;
    --green:   #3B6D11;
    --red:     #A32D2D;
    --yellow:  #854F0B;
    --text:    #2C2C2A;
    --muted:   #888780;
    --mono: 'Courier New', Courier, monospace;
    --sans: Arial, Helvetica, sans-serif;
  }

  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  body {
    background: var(--bg);
    color: var(--text);
    font-family: var(--sans);
    min-height: 100vh;
    padding: 0 0 40px;
  }

  header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 16px 24px;
    border-bottom: 0.5px solid var(--border);
    background: var(--surface);
  }
  .logo {
    display: flex;
    align-items: center;
    gap: 9px;
    font-family: var(--mono);
    font-size: 14px;
    font-weight: 500;
    color: var(--amber);
  }
  .logo svg { width:20px; height:20px; }
  .status-pill {
    display: flex;
    align-items: center;
    gap: 6px;
    font-family: var(--mono);
    font-size: 11px;
    color: var(--muted);
    background: #f1efe8;
    border: 0.5px solid var(--border);
    border-radius: 20px;
    padding: 4px 12px;
  }
  .dot {
    width: 7px; height: 7px;
    border-radius: 50%;
    background: var(--green);
    animation: pulse 2s infinite;
  }
  .dot.offline { background: var(--red); animation: none; }
  @keyframes pulse { 0%,100%{ opacity:1; } 50%{ opacity:.35; } }

  .page { max-width: 900px; margin: 0 auto; padding: 24px 16px; }

  .hero {
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 28px 0 20px;
    gap: 8px;
  }
  .ring-wrap { position: relative; width: 200px; height: 200px; }
  .ring-wrap svg { transform: rotate(-90deg); }
  .ring-center {
    position: absolute; inset: 0;
    display: flex; flex-direction: column;
    align-items: center; justify-content: center;
  }
  .ring-value {
    font-family: var(--mono);
    font-size: 44px;
    font-weight: 500;
    line-height: 1;
    color: var(--green);
    transition: color .4s;
  }
  .ring-unit { font-family: var(--mono); font-size: 16px; color: var(--muted); margin-top: 2px; }
  .ring-label {
    font-size: 11px; color: var(--muted);
    letter-spacing: .09em; text-transform: uppercase; font-weight: 500;
  }
  .hero-status {
    font-family: var(--mono); font-size: 11px;
    padding: 5px 18px; border-radius: 20px; letter-spacing: .05em;
  }

  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    gap: 10px; margin-bottom: 16px;
  }
  .card {
    background: var(--surface);
    border: 0.5px solid var(--border);
    border-radius: 8px;
    padding: 16px 14px 12px;
    display: flex; flex-direction: column; gap: 5px;
    transition: border-color .3s;
  }
  .card.alerted { border-color: #E24B4A; }
  .card.warned  { border-color: #BA7517; }
  .card-label { font-size: 11px; letter-spacing: .08em; text-transform: uppercase; color: var(--muted); font-weight: 500; }
  .card-value {
    font-family: var(--mono); font-size: 26px; font-weight: 500;
    line-height: 1.1; transition: color .4s;
  }
  .card-value.ok    { color: #3B6D11; }
  .card-value.warn  { color: #854F0B; }
  .card-value.alert { color: #A32D2D; }
  .card-value.neu   { color: var(--text); }
  .card-badge {
    font-size: 10px; font-family: var(--mono);
    padding: 2px 7px; border-radius: 4px;
    display: inline-block; width: fit-content;
  }
  .badge-ok   { background:#EAF3DE; color:#27500A; }
  .badge-warn { background:#FAEEDA; color:#633806; }
  .badge-bad  { background:#FCEBEB; color:#791F1F; }
  .badge-neu  { background:#F1EFE8; color:#5F5E5A; }

  .stats-bar {
    display: flex;
    background: var(--surface);
    border: 0.5px solid var(--border);
    border-radius: 8px;
    overflow: hidden;
    margin-bottom: 16px;
  }
  .stat { flex:1; padding: 12px 10px; border-right: 0.5px solid var(--border); text-align: center; }
  .stat:last-child { border-right: none; }
  .stat-n { font-family: var(--mono); font-size: 20px; font-weight: 500; color: var(--amber); }
  .stat-l { font-size: 10px; color: var(--muted); text-transform: uppercase; letter-spacing: .07em; margin-top: 2px; }

  .section-head {
    display: flex; align-items: center; justify-content: space-between; margin-bottom: 8px;
  }
  .section-title { font-size: 11px; font-weight: 500; letter-spacing: .1em; text-transform: uppercase; color: var(--muted); }
  .clear-btn {
    font-size: 11px; font-family: var(--mono); color: var(--muted);
    background: none; border: 0.5px solid var(--border); border-radius: 4px;
    padding: 3px 10px; cursor: pointer; transition: color .2s, border-color .2s;
  }
  .clear-btn:hover { color: var(--red); border-color: #E24B4A; }

  .alert-list {
    background: var(--surface); border: 0.5px solid var(--border);
    border-radius: 8px; max-height: 260px; overflow-y: auto;
  }
  .alert-row {
    display: flex; align-items: flex-start; gap: 12px;
    padding: 9px 14px; border-bottom: 0.5px solid var(--border); font-size: 12px;
  }
  .alert-row:last-child { border-bottom: none; }
  .alert-time { font-family: var(--mono); color: var(--muted); flex-shrink: 0; font-size: 11px; padding-top: 1px; }
  .alert-msg { color: var(--text); line-height: 1.4; }
  .alert-row.level-CRIT .alert-msg { color: #A32D2D; }
  .alert-row.level-WARN .alert-msg { color: #854F0B; }
  .alert-row.level-INFO .alert-msg { color: var(--muted); }
  .empty-state { padding: 24px; text-align: center; color: var(--muted); font-size: 13px; }

  .footer {
    margin-top: 20px; display: flex; justify-content: space-between;
    font-size: 11px; color: var(--muted); font-family: var(--mono);
  }
  #countdown { color: var(--amber); }

  @media (max-width: 500px) {
    .stats-bar { flex-wrap: wrap; }
    .stat { flex: 1 1 45%; border-right: none; border-bottom: 0.5px solid var(--border); }
    .stat:nth-child(odd) { border-right: 0.5px solid var(--border); }
    header { padding: 14px 16px; }
  }
</style>
</head>
<body>

<header>
  <div class="logo">
    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <path d="M12 2C8 2 5 6 5 10c0 4 3 8 7 10 4-2 7-6 7-10 0-4-3-8-7-8z"/>
      <path d="M12 6v8M9 9l3-3 3 3"/>
    </svg>
    GrainGuard
  </div>
  <div class="status-pill">
    <div class="dot" id="connDot"></div>
    <span id="connLabel">ONLINE</span>
  </div>
</header>

<div class="page">

  <div class="hero">
    <div class="ring-wrap">
      <svg width="200" height="200" viewBox="0 0 200 200">
        <circle cx="100" cy="100" r="82" fill="none" stroke="#D3D1C7" stroke-width="15"/>
        <circle cx="100" cy="100" r="82" fill="none" stroke="#3B6D11" stroke-width="15"
          stroke-linecap="round"
          stroke-dasharray="515"
          stroke-dashoffset="515"
          id="moistureArc"
          style="transition: stroke-dashoffset .8s cubic-bezier(.4,0,.2,1), stroke .4s"/>
      </svg>
      <div class="ring-center">
        <div class="ring-value" id="moistVal">--</div>
        <div class="ring-unit">%</div>
      </div>
    </div>
    <div class="ring-label">Grain moisture</div>
    <div class="hero-status badge-ok" id="moistStatus">Loading…</div>
  </div>

  <div class="grid">
    <div class="card" id="cTemp">
      <div class="card-label">Temperature</div>
      <div class="card-value neu" id="vTemp">--</div>
      <div class="card-badge badge-neu" id="bTemp">–</div>
    </div>
    <div class="card" id="cHum">
      <div class="card-label">Air humidity</div>
      <div class="card-value neu" id="vHum">--</div>
      <div class="card-badge badge-neu" id="bHum">–</div>
    </div>
    <div class="card" id="cMot">
      <div class="card-label">Motion</div>
      <div class="card-value neu" id="vMot">--</div>
      <div class="card-badge badge-neu" id="bMot">–</div>
    </div>
    <div class="card" id="cRaw">
      <div class="card-label">Raw ADC</div>
      <div class="card-value neu" id="vRaw">--</div>
      <div class="card-badge badge-neu" id="bRaw">12-bit</div>
    </div>
  </div>

  <div class="stats-bar">
    <div class="stat"><div class="stat-n" id="sUptime">--</div><div class="stat-l">Uptime</div></div>
    <div class="stat"><div class="stat-n" id="sAlerts">--</div><div class="stat-l">Alerts</div></div>
    <div class="stat"><div class="stat-n" id="sRSSI">--</div><div class="stat-l">Wi-Fi RSSI</div></div>
    <div class="stat"><div class="stat-n" id="sTime">--</div><div class="stat-l">Last reading</div></div>
  </div>

  <div class="section-head">
    <div class="section-title">Alert log</div>
    <button class="clear-btn" onclick="clearAlerts()">Clear</button>
  </div>
  <div class="alert-list" id="alertList">
    <div class="empty-state">Fetching data…</div>
  </div>

  <div class="footer">
    <span>GrainGuard v2.1 · ESP32</span>
    
    <div style="display:flex;align-items:center;gap:12px">
      <button onclick="resetWiFi()" 
        style="background:#c0392b;color:white;padding:8px 16px;border:none;border-radius:4px;cursor:pointer">
  Reset WiFi
</button>


      <button class="clear-btn" id="dlBtn" onclick="downloadLog()" style="display:flex;align-items:center;gap:5px">
        <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" aria-hidden="true"><path d="M12 3v13M5 14l7 7 7-7"/><path d="M3 21h18"/></svg>
        SD log
      </button>
      <span>Refresh in <span id="countdown">5</span>s</span>
    </div>
  </div>

</div>

<script>
const CIRC = 2 * Math.PI * 82;

function lerp(a, b, t) { return a + (b - a) * t; }

function setCard(id, val, unit, level) {
  const vEl = document.getElementById('v' + id);
  vEl.textContent = val + (unit || '');
  vEl.className = 'card-value ' + level;
  const bEl = document.getElementById('b' + id);
  const map = { ok:['badge-ok','Normal'], warn:['badge-warn','Warning'], alert:['badge-bad','Alert'], neu:['badge-neu','–'] };
  const [cls, txt] = map[level] || map.neu;
  bEl.className = 'card-badge ' + cls;
  bEl.textContent = txt;
  const cEl = document.getElementById('c' + id);
  cEl.classList.remove('alerted','warned');
  if (level === 'alert') cEl.classList.add('alerted');
  if (level === 'warn')  cEl.classList.add('warned');
}

function updateMoistureRing(pct) {
  const arc = document.getElementById('moistureArc');
  arc.style.strokeDashoffset = CIRC * (1 - pct / 100);
  const t = Math.max(0, (pct - 50) / 50);
  const r = Math.round(lerp(0x3B, 0xA3, t));
  const g = Math.round(lerp(0x6D, 0x2D, t));
  const b = Math.round(lerp(0x11, 0x2D, t));
  arc.style.stroke = 'rgb(' + r + ',' + g + ',' + b + ')';

  const valEl = document.getElementById('moistVal');
  valEl.textContent = pct;
  valEl.style.color = pct > 70 ? '#A32D2D' : pct > 60 ? '#854F0B' : '#3B6D11';

  const st = document.getElementById('moistStatus');
  if (pct > 70) {
    st.className = 'hero-status badge-bad';
    st.textContent = 'Critical — risk of spoilage';
  } else if (pct > 60) {
    st.className = 'hero-status badge-warn';
    st.textContent = 'Warning — monitor closely';
  } else {
    st.className = 'hero-status badge-ok';
    st.textContent = 'Safe — in range';
  }
}

function fmtUptime(ms) {
  const s = Math.floor(ms / 1000);
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  if (h > 0) return h + 'h ' + m + 'm';
  return m + 'm ' + (s % 60) + 's';
}

function renderAlerts(alerts) {
  const el = document.getElementById('alertList');
  if (!alerts || alerts.length === 0) {
    el.innerHTML = '<div class="empty-state">No alerts recorded</div>';
    return;
  }
  el.innerHTML = alerts.slice().reverse().map(a =>
    '<div class="alert-row level-' + (a.level || 'WARN') + '">' +
    '<div class="alert-time">' + a.ts + '</div>' +
    '<div class="alert-msg">' + a.msg + '</div></div>'
  ).join('');
}

function resetWiFi() {
  if (!confirm('This will erase WiFi settings and restart the device. Continue?')) return;
  fetch('/api/resetwifi', { method: 'POST' })
    .then(r => r.json())
    .then(d => {
      alert('Device restarting. Rejoin GrainGuard-AP to reconfigure.');
    })
    .catch(() => {
      // ESP32 already restarted — fetch will fail, that's expected
      alert('Device restarting. Rejoin GrainGuard-AP to reconfigure.');
    });
}

async function fetchData() {
  try {
    const d = await (await fetch('/api/data')).json();
    document.getElementById('connDot').className = 'dot';
    document.getElementById('connLabel').textContent = 'Online';
    updateMoistureRing(d.moisture);
    setCard('Temp', d.temp.toFixed(1), '\u00b0C', d.temp > 35 ? 'alert' : d.temp > 30 ? 'warn' : 'ok');
    setCard('Hum',  d.humidity.toFixed(1), '%',   d.humidity > 85 ? 'alert' : d.humidity > 70 ? 'warn' : 'ok');
    setCard('Mot',  d.motion ? 'Motion' : 'Clear', '', d.motion ? 'alert' : 'ok');
    setCard('Raw',  d.moisture_raw, '', 'neu');
    document.getElementById('sUptime').textContent = fmtUptime(d.uptime_ms);
    document.getElementById('sAlerts').textContent = d.total_alerts;
    document.getElementById('sRSSI').textContent   = d.rssi + ' dBm';
    document.getElementById('sTime').textContent   = (d.timestamp.split(' ')[1] || d.timestamp);
    renderAlerts(d.alerts);
  } catch(e) {
    document.getElementById('connDot').className = 'dot offline';
    document.getElementById('connLabel').textContent = 'Offline';
  }
}

async function clearAlerts() {
  await fetch('/api/clear-alerts', { method: 'POST' });
  fetchData();
}

async function downloadLog() {
  const btn = document.getElementById('dlBtn');
  btn.textContent = 'Fetching…';
  btn.disabled = true;
  try {
    const r = await fetch('/api/sdlog');
    if (!r.ok) {
      const txt = await r.text();
      alert('Could not download log: ' + txt);
      return;
    }
    // Derive filename from Content-Disposition or fall back
    const disp = r.headers.get('Content-Disposition') || '';
    const match = disp.match(/filename="([^"]+)"/);
    const name = match ? match[1] : 'grainguard_log.txt';
    const blob = await r.blob();
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement('a');
    a.href = url; a.download = name;
    document.body.appendChild(a); a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  } catch(e) {
    alert('Download failed: ' + e.message);
  } finally {
    btn.innerHTML = '<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M12 3v13M5 14l7 7 7-7"/><path d="M3 21h18"/></svg> SD log';
    btn.disabled = false;
  }
}

let tick = 5;
const cdEl = document.getElementById('countdown');
setInterval(() => { tick--; if (tick <= 0) { tick = 5; fetchData(); } cdEl.textContent = tick; }, 1000);
fetchData();
</script>
</body>
</html>
)rawliteral";