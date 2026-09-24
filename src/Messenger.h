#pragma once

#include <Arduino.h>
#include "CommPrefs.h"

// ─── Message types ─────────────────────────────────────────────────────────
// Passed to sendMessage() so routing logic can make type-aware decisions.
enum class MsgType {
  ALERT,    // danger condition — highest priority, always send if possible
  STATUS,   // response to STATUS command
  LOG,      // response to LOG command
  INFO,     // boot confirmation, mode change notification, etc.
};

// ─── Unified send ──────────────────────────────────────────────────────────
// Routes message to WiFi (alert log), GSM (SMS), or both
// based on current CommPrefs mode.
//
// WiFi "send" means: log to the in-memory alert buffer (visible on dashboard)
// GSM  "send" means: send SMS to GSM_PHONE_NUMBER
//
// Returns true if at least one channel delivered successfully.
bool sendMessage(MsgType type, const char* message,
                 const char* smsRecipient = nullptr);

// ─── Convenience wrappers ──────────────────────────────────────────────────
bool sendAlert(const char* message);
bool sendStatus(const char* message, const char* recipient = nullptr);
bool sendInfo(const char* message);
