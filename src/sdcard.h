#pragma once

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// SPI Pin definitions for ESP32
constexpr uint8_t SD_CS   = 2;
constexpr uint8_t SD_MOSI = 23;
constexpr uint8_t SD_MISO = 19;
constexpr uint8_t SD_SCK  = 18;

extern bool sdAvailable;

// Initialize SD card SPI bus and verify/create log file header
void setupSDCard();

// Raw string append helper
void logToSD(const char* data);

// Formatted sensor row logger: "YYYY-MM-DD HH:MM:SS,temp,hum,moisture,motion"
void logSensorCSV(const char* timestamp, float temp, float hum, int moisture, bool motion);

// Utility to fetch total and free SD card storage capacity in MB
void getSDStorageInfo(uint64_t &totalBytes, uint64_t &usedBytes);