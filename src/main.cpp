#include <Arduino.h>
#include "sensor.h"
#include "rtc.h"
#include "WIFIdriver.h"
#include "lcdMenu.h"
#include "sdcard.h"
#include "GSMdriver.h"
#include "CommPrefs.h"
#include "Messenger.h"



// ============= GLOBALS =============

SIM800LDriver gsm;
bool gsmAvailable = false;

// --- Sensor state ---
float currentTemp            = 0.0f;
float currentHumidity        = 0.0f;
int   currentMoisturePercent = 0;
int   currentMoistureRaw     = 0;
bool  motionDetected         = false;

// Rolling-average buffers (5 samples)
const int SMOOTH = 5;
float tempBuf[SMOOTH]  = {};
float humBuf[SMOOTH]   = {};
int   moistBuf[SMOOTH] = {};
int   smoothIdx        = 0;

// --- PIR state ---
unsigned long pirActiveStart = 0;
bool pirAlertSent = false;

// --- Alert cooldowns ---
unsigned long lastTempAlert     = 0;
unsigned long lastHumidityAlert = 0;
unsigned long lastMoistureAlert = 0;
const unsigned long ALERT_COOLDOWN = 300000UL; // 5 min

// --- System stats ---
unsigned long bootMillis = 0;
int totalAlerts = 0;

// --- Alert log ---
struct Alert {
    char level[5];   // "INFO", "WARN", "CRIT"
    char ts[20];     // "MM/DD/YYYY HH:MM:SS"
    char msg[80];    // message text
};
const int MAX_ALERTS = 30;
Alert alertLog[MAX_ALERTS];
int   alertCount = 0;

float batteryVoltage;
int   batteryPercent;

// --- Hardware flags ---


// ============= PROTOTYPES =============
void lcdMenuSetup();
void buttonListener();
void setupSDCard();
void setupRTC();
void readSensors();
void checkAlerts();
String getFormattedDateTime();
void logToSD(const char* data);
void activateBuzzer();
float rollingAvgF(float* buf, int n);
int   rollingAvgI(int*   buf, int n);
void updateBuzzer();
void triggerBuzzer();
void handleIncomingSMS();



// ============= SETUP =============
void setup() {
    Serial.begin(9600);
    delay(1000);
    Serial.println(F("\n========================================"));
    Serial.println(F(" GrainGuard v2.1 — Grain Moisture Monitor"));
    Serial.println(F("========================================\n"));

    sensorsSetup();

    // Initialize LCD menu
    lcdMenuSetup();
    
  

    // Pre-fill smoothing buffers with one real reading so averages
    // are valid immediately rather than starting at zero.
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int   m = analogRead(PIN_MOISTURE);
    for (int i = 0; i < SMOOTH; i++) {
        tempBuf[i]  = isnan(t) ? 25.0f : t;
        humBuf[i]   = isnan(h) ? 50.0f : h;
        moistBuf[i] = m;
    }

dht.begin();
setupRTC();
setupSDCard();
setupWiFi();
// In setup() after setupWiFi():
Serial.print("Current IP: ");
Serial.println(WiFi.localIP());
setupWebServer();

bootMillis = millis();

Serial.println(F("PIR warm-up (10 s)..."));
delay(10000);
Serial.println(F("All sensors ready."));
logAlert("INFO", "System started");

commPrefs.load();
gsmAvailable = gsm.init();
if (!gsmAvailable) Serial.println(F("[GSM] Unavailable — SMS disabled."));
}

// ============= LOOP =============
void loop() {
    buttonListener();
    menu.poll(100);
    updateBuzzer();

    if (isWiFiConnected()) {
    server.handleClient();   // only call when actually connected
}
    
    readSensors();
    checkAlerts();

    // SD periodic log (every 5 minutes)
    static unsigned long lastSDLog = 0;
    unsigned long now = millis();
    if (now - lastSDLog > 300000UL) {
        char buf[80];
        snprintf(buf, sizeof(buf),
            "%s,T:%.1f,H:%.1f,M:%d,RAW:%d,PIR:%d",
            getFormattedDateTime().c_str(),
            currentTemp, currentHumidity,
            currentMoisturePercent, currentMoistureRaw,
            motionDetected ? 1 : 0);
        logToSD(buf);
        lastSDLog = now;
    }


    if (gsmAvailable) handleIncomingSMS();
}


// ============= SENSOR READING =============
void readSensors() {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) tempBuf[smoothIdx] = t;
    if (!isnan(h)) humBuf[smoothIdx]  = h;

    moistBuf[smoothIdx] = analogRead(PIN_MOISTURE);
    smoothIdx = (smoothIdx + 1) % SMOOTH;

    currentTemp        = rollingAvgF(tempBuf,  SMOOTH);
    currentHumidity    = rollingAvgF(humBuf,   SMOOTH);
    currentMoistureRaw = rollingAvgI(moistBuf, SMOOTH);

    // ── Paper formula (Figure 6) ──────────────────────────────────────────
    // Step 1: apply calibration correction factor from regression
    float corrected = currentMoistureRaw / MOISTURE_CALIBRATION_FACTOR;

    // Step 2: convert to moisture percent
    // moisture% = 100 - (corrected / ADC_DRY * 100)
    // At dry (ADC = ADC_DRY): 100 - (ADC_DRY/ADC_DRY * 100) = 0%  ✓
    // At wet (ADC → 0):       100 - (0/ADC_DRY * 100)        = 100% ✓
    float moistureF = 100.0f - ((corrected / ADC_DRY) * 100.0f);
    currentMoisturePercent = (int)constrain(moistureF, 0.0f, 100.0f);

    // PIR edge detection (unchanged)
    int pir = digitalRead(PIN_PIR);
    if (pir == HIGH && !motionDetected) {
        motionDetected = true;
        pirActiveStart = millis();
        pirAlertSent   = false;
    } else if (pir == LOW && motionDetected) {
        motionDetected = false;
        pirAlertSent   = false;
    }
}

float rollingAvgF(float* buf, int n) {
    float s = 0.0f;
    for (int i = 0; i < n; i++) s += buf[i];
    return s / n;
}
int rollingAvgI(int* buf, int n) {
    long s = 0;
    for (int i = 0; i < n; i++) s += buf[i];
    return (int)(s / n);
}

// ============= ALERT LOGIC =============

    void checkAlerts() {

    if (millis() < 15000) return;

    unsigned long now = millis();

    if (currentTemp > TEMP_THRESHOLD && (now - lastTempAlert) > ALERT_COOLDOWN) {
        char msg[48];
        snprintf(msg, sizeof(msg), "Temp %.1f C exceeds %.0f C",
                 currentTemp, TEMP_THRESHOLD);
        sendAlert(msg);
        lastTempAlert = now;
        triggerBuzzer();              // ← non-blocking trigger, not activateBuzzer()
    }

    if (currentHumidity > HUMIDITY_THRESHOLD && (now - lastHumidityAlert) > ALERT_COOLDOWN) {
        char msg[48];
        snprintf(msg, sizeof(msg), "Humidity %.1f%% exceeds %.0f%%",
                 currentHumidity, HUMIDITY_THRESHOLD);
        sendAlert(msg);
        lastHumidityAlert = now;
        triggerBuzzer();
    }

    if (currentMoisturePercent > MOISTURE_CRITICAL && (now - lastMoistureAlert) > ALERT_COOLDOWN) {
        char msg[48];
        snprintf(msg, sizeof(msg), "Grain moisture %d%% — spoilage risk!",
                 currentMoisturePercent);
        sendAlert(msg);
        lastMoistureAlert = now;
        triggerBuzzer();
    }

    if (motionDetected) {
        static unsigned long lastDeterrentMs = 0;
        if (now - lastDeterrentMs > 3000UL) {
            triggerBuzzer();          // ← non-blocking
            lastDeterrentMs = now;
        }

        if ((now - pirActiveStart) > (PIR_TRIGGER_SEC * 1000UL) && !pirAlertSent) {
            char msg[48];
            snprintf(msg, sizeof(msg), "Security: motion >%ds detected",
                     PIR_TRIGGER_SEC);
            sendAlert(msg);
            pirAlertSent = true;
        }
    }
}

// ─── Replace your existing BuzzerState struct and all buzzer code ──────────
#define BUZZER_CHANNEL 0

struct BuzzerState {
    bool          active    = false;
    uint8_t       beepsDone = 0;
    uint8_t       groups    = 0;
    bool          toneOn    = false;
    bool          inGap     = false;   // ← was missing from your struct
    unsigned long lastMs    = 0;
};
BuzzerState buz;

void triggerBuzzer() {
    if (buz.active) return;
    buz.active    = true;
    buz.beepsDone = 0;
    buz.groups    = 0;
    buz.toneOn    = false;
    buz.inGap     = false;
    buz.lastMs    = millis();
}

void updateBuzzer() {
    if (!buz.active) return;

    unsigned long now     = millis();
    unsigned long elapsed = now - buz.lastMs;

    if (buz.inGap) {
        if (elapsed >= 500) {
            buz.inGap  = false;
            buz.lastMs = now;
            ledcWrite(BUZZER_CHANNEL, 128);
            buz.toneOn = true;
        }
        return;
    }

    if (buz.toneOn && elapsed >= 150) {
        ledcWrite(BUZZER_CHANNEL, 0);
        buz.toneOn = false;
        buz.lastMs = now;
        return;
    }

    if (!buz.toneOn && elapsed >= 100) {
        buz.beepsDone++;
        if (buz.beepsDone >= 3) {
            buz.beepsDone = 0;
            buz.groups++;
            if (buz.groups >= 3) {
                buz.active = false;
                return;
            }
            buz.inGap  = true;
            buz.lastMs = now;
        } else {
            ledcWrite(BUZZER_CHANNEL, 128);
            buz.toneOn = true;
            buz.lastMs = now;
        }
    }
}



// ============= ALERT LOG =============
void logAlert(const char* level, const char* msg) {
    String ts = getFormattedDateTime();

    // Serial
    Serial.print('['); Serial.print(level); Serial.print("] ");
    Serial.print(ts); Serial.print(" — "); Serial.println(msg);

    // Circular buffer — shift left and overwrite slot [MAX_ALERTS-1] when full
    int idx;
    if (alertCount < MAX_ALERTS) {
        idx = alertCount++;
    } else {
        memmove(&alertLog[0], &alertLog[1], (MAX_ALERTS - 1) * sizeof(Alert));
        idx = MAX_ALERTS - 1;
    }
    strncpy(alertLog[idx].level, level, sizeof(alertLog[idx].level) - 1);
    alertLog[idx].level[sizeof(alertLog[idx].level) - 1] = '\0';
    strncpy(alertLog[idx].ts,  ts.c_str(), sizeof(alertLog[idx].ts)  - 1);
    alertLog[idx].ts[sizeof(alertLog[idx].ts) - 1] = '\0';
    strncpy(alertLog[idx].msg, msg,        sizeof(alertLog[idx].msg) - 1);
    alertLog[idx].msg[sizeof(alertLog[idx].msg) - 1] = '\0';

    totalAlerts++;

    // SD
    char line[128];
    snprintf(line, sizeof(line), "[%s] %s %s", level, ts.c_str(), msg);
    logToSD(line);
}

// ============= WEB HANDLERS =============
void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

void handleAPI() {
    // Build JSON in a fixed stack buffer — no heap fragmentation
    char json[1024];
    int pos = 0;

    // Core sensor values
    pos += snprintf(json + pos, sizeof(json) - pos,
        "{\"temp\":%.1f,\"humidity\":%.1f,\"moisture\":%d,"
        "\"moisture_raw\":%d,\"motion\":%s,"
        "\"rssi\":%d,\"uptime_ms\":%lu,\"total_alerts\":%d,"
        "\"timestamp\":\"%s\",\"alerts\":[",
        currentTemp, currentHumidity, currentMoisturePercent,
        currentMoistureRaw, motionDetected ? "true" : "false",
        WiFi.RSSI(), millis() - bootMillis, totalAlerts,
        getFormattedDateTime().c_str());

    // Alert array (last 20)
    int start = (alertCount > 20) ? alertCount - 20 : 0;
    for (int i = start; i < alertCount && pos < (int)sizeof(json) - 80; i++) {
        if (i > start) json[pos++] = ',';
        // Escape any quotes in msg
        char escapedMsg[90];
        int ei = 0;
        for (int j = 0; alertLog[i].msg[j] && ei < (int)sizeof(escapedMsg) - 2; j++) {
            if (alertLog[i].msg[j] == '"') escapedMsg[ei++] = '\\';
            escapedMsg[ei++] = alertLog[i].msg[j];
        }
        escapedMsg[ei] = '\0';

        pos += snprintf(json + pos, sizeof(json) - pos,
            "{\"ts\":\"%s\",\"level\":\"%s\",\"msg\":\"%s\"}",
            alertLog[i].ts, alertLog[i].level, escapedMsg);
    }

    // Close
    if (pos < (int)sizeof(json) - 3) {
        json[pos++] = ']';
        json[pos++] = '}';
        json[pos]   = '\0';
    }

    server.send(200, "application/json", json);
}

void handleClearAlerts() {
    alertCount  = 0;
    totalAlerts = 0;
    logAlert("INFO", "Alert log cleared");
    server.send(200, "application/json", "{\"ok\":true}");
}

void handleSDLog() {
    if (!sdAvailable) {
        server.send(503, "text/plain", "SD card not available");
        return;
    }
    File f = SD.open("/datalog.txt", FILE_READ);
    if (!f) {
        server.send(404, "text/plain", "Log file not found");
        return;
    }
    // Send as a downloadable text file with today's date in the filename
    String filename = "grainguard_" + getFormattedDateTime().substring(0, 10) + ".txt";
    filename.replace("/", "-");   // MM-DD-YYYY safe for filenames
    server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    server.sendHeader("Content-Length", String(f.size()));
    server.streamFile(f, "text/plain");
    f.close();
}

// ============= UTILITIES =============
String getFormattedDateTime() {
    RtcDateTime now = Rtc.GetDateTime();
    char buf[20];
    snprintf(buf, sizeof(buf),
        "%02u/%02u/%04u %02u:%02u:%02u",
        now.Month(), now.Day(), now.Year(),
        now.Hour(), now.Minute(), now.Second());
    return String(buf);
}

void handleIncomingSMS() {
  IncomingSMS sms = gsm.checkIncoming();
  if (sms.command == GSMCommand::NONE) return;

  switch (sms.command) {

    // ── STATUS ────────────────────────────────────────────────────────────
    case GSMCommand::STATUS: {
      char msg[160];
      const char* level =
        currentMoisturePercent > MOISTURE_CRITICAL ? "DANGER"  :
        currentMoisturePercent > MOISTURE_WARNING  ? "WARNING" : "SAFE";
      snprintf(msg, sizeof(msg),
        "%s | M:%d%% T:%.1fC H:%.1f%%",
        level,
        currentMoisturePercent,
        currentTemp,
        currentHumidity);
      gsm.sendSMSTo(sms.sender, msg);
      break;
    }

    // ── LOG ───────────────────────────────────────────────────────────────
    case GSMCommand::LOG: {
      if (!sdAvailable) {
        gsm.sendSMSTo(sms.sender, "LOG error: SD card not mounted.");
        break;
      }
      File f = SD.open("/datalog.txt", FILE_READ);
      if (!f) {
        gsm.sendSMSTo(sms.sender, "Log file not found.");
        break;
      }
      // Read last 155 chars — fits in one SMS
      size_t sz = f.size();
      if (sz > 155) f.seek(sz - 155);
      char buf[160] = {0};
      f.readBytes(buf, 155);
      f.close();
      // Skip partial first line after seek
      char* nl = strchr(buf, '\n');
      gsm.sendSMSTo(sms.sender, nl ? nl + 1 : buf);
      break;
    }

    // ── BATTERY ───────────────────────────────────────────────────────────
    case GSMCommand::BATTERY: {
      char msg[160];
      snprintf(msg, sizeof(msg),
        "Battery: %.2fV  %.0f%%  %s",
        batteryVoltage,
        batteryPercent,
        batteryPercent <= 10 ? "CRITICAL" :
        batteryPercent <= 30 ? "LOW"      : "OK");
      gsm.sendSMSTo(sms.sender, msg);
      break;
    }

    // ── HELP ──────────────────────────────────────────────────────────────
    case GSMCommand::HELP: {
      gsm.sendSMSTo(sms.sender,
        "Commands: STATUS, LOG, BATTERY, HELP, "
        "SET YYYY-MM-DD HH:MM:SS");
      break;
    }

    // ── SET ───────────────────────────────────────────────────────────────
    case GSMCommand::SET: {
      // sms.raw = "SET 2025-06-15 08:30:00" — skip past "SET "
      const char* dtStr = sms.raw + 4;
      uint16_t yr; uint8_t mo, dy, hr, mn, sc;
      int parsed = sscanf(dtStr, "%hu-%hhu-%hhu %hhu:%hhu:%hhu",
                          &yr, &mo, &dy, &hr, &mn, &sc);
      if (parsed == 6 && yr >= 2024) {
        Rtc.SetIsWriteProtected(false);
        Rtc.SetDateTime(RtcDateTime(yr, mo, dy, hr, mn, sc));
        char reply[80];
        snprintf(reply, sizeof(reply),
          "RTC set: %04d-%02d-%02d %02d:%02d:%02d",
          yr, mo, dy, hr, mn, sc);
        gsm.sendSMSTo(sms.sender, reply);
      } else {
        gsm.sendSMSTo(sms.sender,
          "SET failed. Use: SET YYYY-MM-DD HH:MM:SS");
      }
      break;
    }

    // ── UNKNOWN ───────────────────────────────────────────────────────────
    default: {
      gsm.sendSMSTo(sms.sender,
        "Unknown command. Send HELP for list.");
      break;
    }
  }
}



