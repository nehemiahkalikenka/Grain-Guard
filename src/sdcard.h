#pragma once

#include <SD.h>
#include <SPI.h>
#include <Arduino.h>

constexpr uint8_t SD_CS   = 2;
constexpr uint8_t SD_MOSI = 23;
constexpr uint8_t SD_MISO = 19;
constexpr uint8_t SD_SCK  = 18;

extern bool sdAvailable;

void setupSDCard();

void logToSD(const char* data);