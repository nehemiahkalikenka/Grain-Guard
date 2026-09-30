#include "WIFIdriver.h"

// ─── Extern Sensor Values (Declared in main.cpp) ───────────────────────────
extern float currentTemp;
extern float currentHumidity;
extern int   currentMoisturePercent;

// ─── Network Instances ──────────────────────────────────────────────────────
DNSServer      dnsServer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Access Point Network IP Settings
const IPAddress apIP(192, 168, 4, 1);
const IPAddress netMask(255, 255, 255, 0);

static uint32_t lastWsBroadcast = 0;
static bool     restartPending   = false;

// ─── Embedded HTML Dashboard ────────────────────────────────────────────────
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>GrainGuard Dashboard</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background: #f4f7f6; margin: 0; padding: 20px; }
    .card { background: white; padding: 20px; margin: 15px auto; max-width: 400px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
    h1 { color: #2c3e50; font-size: 24px; }
    .value { font-size: 28px; font-weight: bold; color: #27ae60; margin: 10px 0; }
    .label { color: #7f8c8d; font-size: 14px; text-transform: uppercase; }
  </style>
</head>
<body>
  <h1>GrainGuard Monitor</h1>
  <div class="card"><div class="label">Grain Moisture</div><div id="moisture" class="value">-- %</div></div>
  <div class="card"><div class="label">Temperature</div><div id="temp" class="value">-- &deg;C</div></div>
  <div class="card"><div class="label">Humidity</div><div id="hum" class="value">-- %</div></div>

  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;
    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onmessage = onMessage;
    }
    function onMessage(event) {
      var data = JSON.parse(event.data);
      if(data.temp !== undefined) document.getElementById('temp').innerHTML = data.temp.toFixed(1) + ' &deg;C';
      if(data.hum !== undefined) document.getElementById('hum').innerHTML = data.hum.toFixed(1) + ' %';
      if(data.moisture !== undefined) document.getElementById('moisture').innerHTML = data.moisture + ' %';
    }
    window.addEventListener('load', initWebSocket);
  </script>
</body>
</html>
)rawliteral";

// ─── WiFi Connectivity Check ────────────────────────────────────────────────
bool isWiFiConnected() {
  // Checks if running in STA mode and connected to an AP,
  // or if running in SoftAP mode and has at least 1 client connected.
  if (WiFi.getMode() & WIFI_MODE_STA) {
    return (WiFi.status() == WL_CONNECTED);
  }
  return (WiFi.softAPgetStationNum() > 0);
}

// ─── WebSocket Event Handling ───────────────────────────────────────────────
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("[WS] Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      broadcastSensorData();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("[WS] Client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// ─── Setup Function ────────────────────────────────────────────────────────
void setupWiFi() {
  // 1. Configure Access Point (SoftAP)
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMask);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  Serial.println("[WiFi] Access Point started: " AP_SSID);
  Serial.print("[WiFi] IP Address: ");
  Serial.println(WiFi.softAPIP());

  // 2. Start Captive Portal DNS Server
  dnsServer.start(DNS_PORT, "*", apIP);

  // 3. Register WebSocket Endpoint
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // 4. Primary Root Route (Using send instead of deprecated send_P)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", INDEX_HTML);
  });

  // 5. Mobile OS Captive Portal Detection Endpoints
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) { request->send(200, "text/html", INDEX_HTML); });
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) { request->send(200, "text/html", INDEX_HTML); });
  server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *request) { request->send(200, "text/html", INDEX_HTML); });
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) { request->send(200, "text/html", INDEX_HTML); });

  // 6. Catch-All Handler for Captive Redirection
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->host() != "192.168.4.1") {
      request->redirect("http://192.168.4.1/");
    } else {
      request->send(404, "text/plain", "Not Found");
    }
  });

  // 7. Start Async WebServer
  server.begin();
  Serial.println("[WebServer] Dashboard and Captive Portal active at http://192.168.4.1");
}

// ─── Loop Processing ────────────────────────────────────────────────────────
void loopCaptivePortal() {
  dnsServer.processNextRequest();
  ws.cleanupClients();

  if (restartPending) {
    delay(500);
    ESP.restart();
  }
}

// ─── Broadcast Sensor Data Over WebSocket ──────────────────────────────────
void broadcastSensorData() {
  if (millis() - lastWsBroadcast >= WS_BROADCAST_INTERVAL) {
    lastWsBroadcast = millis();

    if (ws.count() > 0) {
      char jsonBuf[128];
      snprintf(jsonBuf, sizeof(jsonBuf), 
               "{\"temp\":%.1f,\"hum\":%.1f,\"moisture\":%d}", 
               currentTemp, currentHumidity, currentMoisturePercent);
      
      ws.textAll(jsonBuf);
    }
  }
}

// ─── Deferred Reset Request Handler ─────────────────────────────────────────
void requestWiFiReset() {
  Serial.println("[WiFi] System restart requested via menu...");
  restartPending = true;
}