#include "CommPrefs.h"
#include "WIFIdriver.h"   // for isWiFiConnected()

// ─── Global instance ───────────────────────────────────────────────────────
CommPrefs commPrefs;

// ─── load ──────────────────────────────────────────────────────────────────
CommMode CommPrefs::load() {
  _prefs.begin(NVS_NS, true);   // read-only open
  uint8_t stored = _prefs.getUChar(NVS_KEY, (uint8_t)CommMode::AUTO);
  _prefs.end();

  // Validate stored value — corrupt NVS could give garbage
  if (stored > (uint8_t)CommMode::AUTO) {
    stored = (uint8_t)CommMode::AUTO;
  }

  _mode = (CommMode)stored;
  Serial.printf("[COMM] Mode loaded: %s\n", commModeLabel(_mode));
  return _mode;
}

// ─── save ──────────────────────────────────────────────────────────────────
void CommPrefs::save(CommMode mode) {
  _mode = mode;
  _prefs.begin(NVS_NS, false);   // read-write open
  _prefs.putUChar(NVS_KEY, (uint8_t)mode);
  _prefs.end();
  Serial.printf("[COMM] Mode saved: %s\n", commModeLabel(_mode));
}

// ─── useWiFi ───────────────────────────────────────────────────────────────
bool CommPrefs::useWiFi() const {
  switch (_mode) {
    case CommMode::WIFI_ONLY: return true;
    case CommMode::GSM_ONLY:  return false;
    case CommMode::BOTH:      return true;
    case CommMode::AUTO:      return isWiFiConnected();
    default:                  return false;
  }
}

// ─── useGSM ────────────────────────────────────────────────────────────────
// In AUTO mode: use GSM only when WiFi is not connected (fallback)
bool CommPrefs::useGSM() const {
  switch (_mode) {
    case CommMode::WIFI_ONLY: return false;
    case CommMode::GSM_ONLY:  return true;
    case CommMode::BOTH:      return true;
    case CommMode::AUTO:      return !isWiFiConnected();
    default:                  return false;
  }
}
