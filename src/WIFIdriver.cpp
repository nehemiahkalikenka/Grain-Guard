#include "WIFIdriver.h"
#include "sensor.h" // Thresholds: MOISTURE_CRITICAL, TEMP_THRESHOLD, HUMIDITY_THRESHOLD, etc.
#include <SD.h>

// ─── External Globals Defined elsewhere ────────────────────────────────────
extern float currentTemp;
extern float currentHumidity;
extern int   currentMoisturePercent;
extern int   currentMoistureRaw;
extern bool  motionDetected;
extern int   totalAlerts;
extern bool  sdAvailable;
extern char  wifiIPStr[16];

// ─── Async Server & WebSocket Instances ────────────────────────────────────
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dns;

static bool restartPending = false; // Deferred reset flag

// ─── WebSocket events ──────────────────────────────────────────────────────
void onWsEvent(AsyncWebSocket* srv, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client #%u disconnected\n", client->id());
  }
}

// ─── JSON Telemetry Generation ─────────────────────────────────────────────
static String buildJson() {
  const char* level =
    (currentMoisturePercent > MOISTURE_CRITICAL || currentTemp > TEMP_THRESHOLD ||
     currentHumidity > HUMIDITY_THRESHOLD)                            ? "DANGER"  :
    (currentMoisturePercent > MOISTURE_WARNING)                       ? "WARNING" : "SAFE";

  char json[320];
  snprintf(json, sizeof(json),
    "{\"temp\":%.1f,\"humidity\":%.1f,\"moisture\":%d,\"moistureRaw\":%d,"
    "\"motion\":%s,\"danger\":\"%s\",\"alerts\":%d,\"ip\":\"%s\"}",
    currentTemp, currentHumidity, currentMoisturePercent, currentMoistureRaw,
    motionDetected ? "true" : "false", level, totalAlerts, wifiIPStr);
  return String(json);
}

// ─── Called from loop() periodically ───────────────────────────────────────
void broadcastSensorData() {
  if (restartPending) {
    resetWiFiCredentials(); // Performed outside ISR / HTTP context
    return;
  }
  if (!isWiFiConnected() || ws.count() == 0) return;
  ws.textAll(buildJson());
}

// ─── Dashboard HTML (Stored in Flash Memory) ──────────────────────────────
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>GrainGuard Dashboard</title>
<style>
body{font-family:Arial,sans-serif;background:#121212;color:#fff;padding:16px;margin:0}
.card{background:#1e1e1e;padding:16px;border-radius:8px;margin-bottom:10px}
.big{font-size:2em;font-weight:bold}
.SAFE{color:#4caf50}.WARNING{color:#ffb300}.DANGER{color:#f44336}
button{background:#008CBA;color:#fff;border:0;padding:10px 14px;border-radius:4px;margin-right:6px;cursor:pointer}
button:hover{background:#007399}
</style></head><body>
<h2>GrainGuard Live Dashboard</h2>
<div class="card">Status: <span id="danger" class="big">--</span></div>
<div class="card">Temperature: <span id="temp" class="big">--</span></div>
<div class="card">Humidity: <span id="humidity" class="big">--</span></div>
<div class="card">Grain Moisture: <span id="moisture" class="big">--</span></div>
<div class="card">Motion: <span id="motion">--</span> &nbsp; Alerts: <span id="alerts">--</span></div>
<div class="card">
  <button onclick="location.href='/api/sdlog'">Download Log</button>
  <button onclick="resetWiFi()">Reset WiFi</button>
</div>
<script>
var ws;
function connect(){
  ws = new WebSocket('ws://'+location.hostname+'/ws');
  ws.onmessage = function(e){
    var d = JSON.parse(e.data);
    document.getElementById('temp').innerText = d.temp + ' °C';
    document.getElementById('humidity').innerText = d.humidity + ' %';
    document.getElementById('moisture').innerText = d.moisture + ' %';
    document.getElementById('motion').innerText = d.motion ? 'DETECTED' : 'Clear';
    document.getElementById('alerts').innerText = d.alerts;
    var s = document.getElementById('danger');
    s.innerText = d.danger; s.className = 'big ' + d.danger;
  };
  ws.onclose = function(){ setTimeout(connect, 3000); };
}
connect();
function resetWiFi(){
  if(confirm('Erase stored WiFi settings and restart captive portal?'))
    fetch('/api/resetwifi', {method:'POST'}).catch(function(){});
}
</script></body></html>
)rawliteral";

// ─── setupWiFi Implementation ──────────────────────────────────────────────
void setupWiFi() {
  AsyncWiFiManager wifiManager(&server, &dns);
  
  // Set timeout for AP setup portal before continuing (in seconds)
  wifiManager.setConfigPortalTimeout(180);

  Serial.println(F("[WiFi] Connecting or launching captive portal..."));
  if (!wifiManager.autoConnect("GrainGuard-Setup")) {
    Serial.println(F("[WiFi] Failed to connect / timed out. Running offline..."));
    snprintf(wifiIPStr, sizeof(wifiIPStr), "Offline");
    return;
  }

  // Record allocated local IP address
  snprintf(wifiIPStr, sizeof(wifiIPStr), "%s", WiFi.localIP().toString().c_str());
  Serial.printf("[WiFi] Connected! IP Address: %s\n", wifiIPStr);
}

// ─── setupWebServer Implementation ────────────────────────────────────────
void setupWebServer() {
  if (!isWiFiConnected()) {
    Serial.println(F("[WebServer] Skipped — WiFi non-functional."));
    return;
  }

  // Setup mDNS Responder
  if (MDNS.begin("grainguard")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println(F("[mDNS] Responder active at http://grainguard.local"));
  }

  // Attach WebSocket handler to Web Server
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Serve Main Dashboard
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", INDEX_HTML);
  });

  // REST API Route for direct polling
  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", buildJson());
  });

  // SD Card Download Route
  server.on("/api/sdlog", HTTP_GET, handleSDLog);

  // WiFi Reset trigger endpoint
  server.on("/api/resetwifi", HTTP_POST, handleResetWiFi);

  // Start Server
  server.begin();
  Serial.printf("[WebServer] Dashboard active at http://%s (or http://grainguard.local)\n", wifiIPStr);
}

// ─── Endpoint & Utility Implementations ────────────────────────────────────

void handleResetWiFi(AsyncWebServerRequest* req) {
  req->send(200, "application/json", "{\"ok\":true}");
  restartPending = true; // Request actual erase & reboot in loop context
}

void resetWiFiCredentials() {
  Serial.println(F("[WiFi] Clearing saved WiFi settings and restarting..."));
  AsyncWiFiManager wifiManager(&server, &dns);
  wifiManager.resetSettings();
  delay(500);
  ESP.restart();
}

void handleSDLog(AsyncWebServerRequest* req) {
  if (!sdAvailable) {
    req->send(503, "text/plain", "SD card not available");
    return;
  }
  if (!SD.exists("/datalog.txt")) {
    req->send(404, "text/plain", "Log file not found");
    return;
  }
  req->send(SD, "/datalog.txt", "text/plain", true); // Send file as download attachment
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void requestWiFiReset() {
    restartPending = true;
}