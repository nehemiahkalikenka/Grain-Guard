#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <ESPmDNS.h>

// Global references accessible across modules
extern AsyncWebServer server;
extern AsyncWebSocket ws;

// Core Wi-Fi and Web Server interface functions
void setupWiFi();
void setupWebServer();
void resetWiFiCredentials();
bool isWiFiConnected();
void broadcastSensorData(); // Call this from loop() every second
void requestWiFiReset();

// Endpoint Handlers
void handleResetWiFi(AsyncWebServerRequest* request);
void handleSDLog(AsyncWebServerRequest* request);
void logAlert(const char* level, const char* msg);

extern const char INDEX_HTML[] PROGMEM;