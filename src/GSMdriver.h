#pragma once

#include <Arduino.h>

// ─── UART Configuration ────────────────────────────────────────────────────
#define GSM_SERIAL      Serial2
#define GSM_BAUD        9600
#define GSM_PIN_RX      16       // ESP32 GPIO16 ← SIM800L TX  (direct, no divider)
#define GSM_PIN_TX      17       // ESP32 GPIO17 → SIM800L RX  (via 10kΩ/20kΩ divider)

// ─── Phone number ─────────────────────────────────────────────────────────
#define GSM_PHONE_NUMBER  "+260770547896"   // edit to your number

// ─── Timeouts ─────────────────────────────────────────────────────────────
#define GSM_TIMEOUT_SHORT    3000UL
#define GSM_TIMEOUT_NET     15000UL
#define GSM_TIMEOUT_SMS      5000UL

// ─── SMS commands ─────────────────────────────────────────────────────────
enum class GSMCommand {
  NONE,
  STATUS,     // current sensor readings + danger level
  LOG,        // last SD card log entries
  BATTERY,    // battery voltage and percent
  HELP,       // list all commands
  SET,        // set RTC time: SET YYYY-MM-DD HH:MM:SS
  UNKNOWN
};

struct IncomingSMS {
  GSMCommand command;
  char       raw[161];     // full message text (160 char SMS limit + null)
  char       sender[20];   // number who sent it — used for replies
};

// ─── Driver ───────────────────────────────────────────────────────────────
class SIM800LDriver {
public:
  // Start Serial2, handshake, register on network, send boot SMS.
  bool init();

  // Send SMS to GSM_PHONE_NUMBER.
  bool sendSMS(const char* message);

  // Send SMS to a specific number (used when replying to commands).
  bool sendSMSTo(const char* number, const char* message);

  // Check for incoming SMS. Non-blocking — call every loop().
  // Returns NONE if nothing arrived.
  IncomingSMS checkIncoming();

  // Check if SIM800L is registered on a network.
  bool isNetworkAvailable();

  // Basic AT ping — true if module responds.
  bool isAlive();

private:
  // Send raw AT command with \r terminator (matches what works on Mega).
  void   _sendAT(const char* cmd);

  // Read serial response into internal buffer until expected string
  // found or timeout. Returns true on match.
  bool   _waitFor(const char* expected, uint32_t timeout);

  // Read all available serial data into a String (same approach as Mega).
  String _readResponse(uint32_t timeout = 1000);

  // Drain the RX buffer.
  void   _flushRx();

  // Parse raw SMS text into a GSMCommand.
  void   _parseCommand(IncomingSMS& sms);
};
