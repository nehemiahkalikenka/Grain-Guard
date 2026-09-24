#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>

// ─── No hardcoded credentials — WiFiManager handles storage in NVS ─────────

extern WebServer server;

// ─── Core setup ────────────────────────────────────────────────────────────
void setupWiFi();
void setupWebServer();

// ─── Reset stored credentials and relaunch config portal ──────────────────
// Call from LCD menu "Reset WiFi" or web dashboard button
void resetWiFiCredentials();

// ─── Returns true if ESP32 is connected to a network ──────────────────────
bool isWiFiConnected();

// ─── Web handlers ─────────────────────────────────────────────────────────
void handleRoot();
void handleAPI();
void handleClearAlerts();
void handleSDLog();
void handleResetWiFi();     // Web dashboard trigger for reset

void logAlert(const char* level, const char* msg);

extern const char INDEX_HTML[] PROGMEM;