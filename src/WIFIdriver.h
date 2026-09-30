#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

// ─── Configuration Parameters ────────────────────────────────────────────────
#define DNS_PORT           53
#define AP_SSID            "GrainGuard-AP"
#define AP_PASSWORD        "12345678"          // Minimum 8 chars
#define WS_BROADCAST_INTERVAL 1000UL            // Broadcast sensor data every 1s

// ─── Exposed Objects & Functions ─────────────────────────────────────────────
extern AsyncWebServer server;
extern AsyncWebSocket ws;
extern DNSServer      dnsServer;

// Core Interface Functions
void setupWiFi();
void loopCaptivePortal();                     // Call continuously inside loop()
void broadcastSensorData();                   // Call to push live sensor metrics over WebSocket
void requestWiFiReset();                      // Deferred reset flag
bool isWiFiConnected();
// System log function declared in main.cpp
void logAlert(const char* level, const char* msg);

// Embedded Web Dashboard HTML (Stored in Flash Memory)
extern const char INDEX_HTML[] PROGMEM;