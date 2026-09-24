#include "Messenger.h"
#include "WIFIdriver.h"
#include "GSMdriver.h"

extern SIM800LDriver gsm;
extern bool          gsmAvailable;
extern void         logAlert(const char* level, const char* msg);

// ─── sendMessage ───────────────────────────────────────────────────────────
// Add "= nullptr" to make the 3rd argument optional
bool sendMessage(MsgType type, const char* message, const char* smsRecipient) {
  bool wifiOk = false;
  bool gsmOk  = false;

  // ── WiFi channel: log to in-memory alert buffer + web dashboard ───────
  if (commPrefs.useWiFi()) {
    const char* level =
      (type == MsgType::ALERT) ? "CRIT" :
      (type == MsgType::INFO)  ? "INFO" : "INFO";

    logAlert(level, message);   // appears on web dashboard immediately
    wifiOk = true;
    Serial.printf("[MESSENGER] WiFi → %s\n", message);
  }

  // ── GSM channel: send SMS ─────────────────────────────────────────────
  if (commPrefs.useGSM() && gsmAvailable) {
    // If smsRecipient was omitted, it will be nullptr and fall back to GSM_PHONE_NUMBER
    const char* target = smsRecipient ? smsRecipient : GSM_PHONE_NUMBER;
    gsmOk = gsm.sendSMSTo(target, message);
    Serial.printf("[MESSENGER] GSM → %s : %s\n", target,
                  gsmOk ? "OK" : "FAIL");
  }

  // ── Log to Serial regardless of mode ─────────────────────────────────
  Serial.printf("[MESSENGER] Mode:%s WiFi:%s GSM:%s | %s\n",
                commModeLabel(commPrefs.current()),
                wifiOk ? "OK" : "skip",
                gsmOk  ? "OK" : "skip",
                message);

  return wifiOk || gsmOk;
}


// ─── Convenience wrappers ──────────────────────────────────────────────────
bool sendAlert(const char* message) {
  // This now works perfectly because the 3rd argument is optional!
  return sendMessage(MsgType::ALERT, message); 
}

bool sendStatus(const char* message, const char* recipient) {
  return sendMessage(MsgType::STATUS, message, recipient);
}

bool sendInfo(const char* message) {
  return sendMessage(MsgType::INFO, message);
}
