#include "sensor.h"
#include "Arduino.h"

DHT dht(DHT_PIN, DHT_TYPE);

// ─── LEDC buzzer init for ESP32 core v2.x ─────────────────────────────────
#define BUZZER_CHANNEL    0      // LEDC channel (0–15)
#define BUZZER_FREQ    2000      // Hz
#define BUZZER_RES        8      // 8-bit resolution (0–255)

void sensorsSetup() {
    pinMode(PIN_PIR,      INPUT);
    pinMode(PIN_MOISTURE, INPUT);
    pinMode(PIN_BUZZER,   OUTPUT);

    // Initialize LEDC channel then attach to buzzer pin
    ledcSetup(BUZZER_CHANNEL, BUZZER_FREQ, BUZZER_RES);
    ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
    ledcWrite(BUZZER_CHANNEL, 0);   // start silent

    analogReadResolution(10);
    analogSetAttenuation(ADC_11db);
}