#pragma once
#include <Arduino.h>
#include <RtcDS1302.h> // Ensure your specific RTC library header is here

// Define your pins as constants if they aren't already
constexpr uint8_t DS1302_IO   = 25;   // Data pin for DS1302
constexpr uint8_t DS1302_SCLK = 32;  // Clock pin for DS1302
constexpr uint8_t DS1302_CE   = 33;  // rst pin for DS1302

// Clean blueprint declarations only (No constructor parameters allowed here!)
extern ThreeWire rtcWire;
extern RtcDS1302<ThreeWire> Rtc;

// Your function prototypes
void setupRTC();