#pragma once

#include <DHT.h>
#include <Arduino.h>

constexpr int DHT_PIN = 27;
constexpr uint8_t DHT_TYPE = DHT11;

constexpr float TEMP_THRESHOLD     = 35.0f;  // °C
constexpr float HUMIDITY_THRESHOLD = 85.0f;  // %
constexpr int   MOISTURE_CRITICAL  = 70;     // grain moisture %
constexpr int   MOISTURE_WARNING   = 60;     // grain moisture %
constexpr int   PIR_TRIGGER_SEC    = 10;     // seconds before PIR alert fires

constexpr float MOISTURE_CALIBRATION_FACTOR = 1.0f;  // from paper regression
constexpr float ADC_DRY                     = 590.0f;

// ============= PIN MAP =============
constexpr int PIN_PIR = 13;
constexpr int PIN_BUZZER   = 15;
constexpr int PIN_MOISTURE = 35;

extern DHT dht;   // ← declare it so main.cpp can see it

void sensorsSetup();