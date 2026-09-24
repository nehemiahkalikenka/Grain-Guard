#pragma once

#include <Arduino.h>
#include <Preferences.h>

// ─── Communication modes ───────────────────────────────────────────────────
enum class CommMode : uint8_t {
  WIFI_ONLY  = 0,   // Web dashboard only — no SMS
  GSM_ONLY   = 1,   // SMS only — web dashboard disabled
  BOTH       = 2,   // WiFi + GSM simultaneously
  AUTO       = 3    // WiFi preferred; GSM fallback if WiFi disconnects
};

// ─── Human-readable labels (used by LCD menu and SMS replies) ──────────────
inline const char* commModeLabel(CommMode mode) {
  switch (mode) {
    case CommMode::WIFI_ONLY: return "WiFi Only";
    case CommMode::GSM_ONLY:  return "GSM Only";
    case CommMode::BOTH:      return "WiFi + GSM";
    case CommMode::AUTO:      return "Auto";
    default:                  return "Unknown";
  }
}

// ─── Manager interface ─────────────────────────────────────────────────────
class CommPrefs {
public:
  // Load saved preference from NVS. Call once in setup().
  // Returns current mode (default: AUTO if never set).
  CommMode load();

  // Save new mode to NVS and update current mode in RAM.
  void save(CommMode mode);

  // Get current mode without reading NVS.
  CommMode current() const { return _mode; }

  // Helpers — used by the unified messenger to decide routing
  bool useWiFi() const;
  bool useGSM()  const;

private:
  CommMode   _mode = CommMode::AUTO;
  Preferences _prefs;
  static constexpr const char* NVS_NS  = "grainguard";
  static constexpr const char* NVS_KEY = "commmode";
};

// ─── Global instance — included by main.cpp and lcdMenu.cpp ───────────────
extern CommPrefs commPrefs;
