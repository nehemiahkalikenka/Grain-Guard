#include "CommPrefs.h"
#include "WIFIdriver.h"   // for isWiFiConnected()

// Global instance definition
CommPrefs commPrefs;

CommMode CommPrefs::load() {
    _prefs.begin(NVS_NS, true);   // read-only open
    uint8_t stored = _prefs.getUChar(NVS_KEY, (uint8_t)CommMode::AUTO);
    _prefs.end();

    if (stored > (uint8_t)CommMode::AUTO) {
        stored = (uint8_t)CommMode::AUTO;
    }

    _mode = (CommMode)stored;
    Serial.printf("[COMM] Mode loaded: %s\n", commModeLabel(_mode));
    return _mode;
}

void CommPrefs::save(CommMode mode) {
    _mode = mode;
    _prefs.begin(NVS_NS, false);   // read-write open
    _prefs.putUChar(NVS_KEY, (uint8_t)mode);
    _prefs.end();
    Serial.printf("[COMM] Mode saved: %s\n", commModeLabel(_mode));
}

bool CommPrefs::useWiFi() const {
    switch (_mode) {
        case CommMode::WIFI_ONLY: return true;
        case CommMode::GSM_ONLY:  return false;
        case CommMode::BOTH:      return true;
        case CommMode::AUTO:      return isWiFiConnected();
        default:                  return false;
    }
}

bool CommPrefs::useGSM() const {
    switch (_mode) {
        case CommMode::WIFI_ONLY: return false;
        case CommMode::GSM_ONLY:  return true;
        case CommMode::BOTH:      return true;
        case CommMode::AUTO:      return !isWiFiConnected();
        default:                  return false;
    }
}