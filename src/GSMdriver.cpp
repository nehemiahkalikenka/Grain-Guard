#include "GSMdriver.h"

// ─── init ──────────────────────────────────────────────────────────────────
// Mirrors the setup() sequence from the working Mega sketch,
// adapted for ESP32 Serial2 with explicit RX/TX pin assignment.
bool SIM800LDriver::init() {
  GSM_SERIAL.begin(GSM_BAUD, SERIAL_8N1, GSM_PIN_RX, GSM_PIN_TX);
  delay(3000);   // let SIM800L finish booting — same as Mega sketch
  _flushRx();

  // ── 1. Alive check ────────────────────────────────────────────────────
  Serial.println(F("[GSM] Checking module..."));
  _sendAT("AT");
  if (!_waitFor("OK", GSM_TIMEOUT_SHORT)) {
    Serial.println(F("[GSM] ERROR: No response to AT."));
    Serial.println(F("[GSM] Check: power (3.9-4.0V), wiring, voltage divider on RX line."));
    return false;
  }
  Serial.println(F("[GSM] Module alive."));

  // ── 2. Disable echo ───────────────────────────────────────────────────
  // Keeps responses clean — easier to parse
  _sendAT("ATE0");
  _waitFor("OK", GSM_TIMEOUT_SHORT);

  // ── 3. SMS text mode ──────────────────────────────────────────────────
  _sendAT("AT+CMGF=1");
  if (!_waitFor("OK", GSM_TIMEOUT_SHORT)) {
    Serial.println(F("[GSM] ERROR: Could not set SMS text mode."));
    return false;
  }

  // ── 4. Incoming SMS push mode ─────────────────────────────────────────
  // AT+CNMI=2,2,0,0,0 — matches exactly what worked on Mega.
  // First param = 2 (buffer + push), second = 2 (route to serial directly).
  // This means +CMT: lines appear on Serial2 automatically on receive —
  // no polling AT command needed in checkIncoming().
  _sendAT("AT+CNMI=2,2,0,0,0");
  if (!_waitFor("OK", GSM_TIMEOUT_SHORT)) {
    Serial.println(F("[GSM] WARNING: CNMI failed — incoming SMS may not work."));
  }

  // ── 5. Wait for network registration ──────────────────────────────────
  Serial.println(F("[GSM] Waiting for network..."));
  uint32_t start = millis();
  bool registered = false;

  while (millis() - start < GSM_TIMEOUT_NET) {
    if (isNetworkAvailable()) {
      registered = true;
      break;
    }
    delay(2000);
  }

  if (!registered) {
    Serial.println(F("[GSM] ERROR: Network timeout. Check SIM card and antenna."));
    return false;
  }

  // ── 6. Signal quality report ──────────────────────────────────────────
  _sendAT("AT+CSQ");
  String csq = _readResponse(1000);
  Serial.print(F("[GSM] Signal: ")); Serial.println(csq);
  // +CSQ: <rssi>,<ber> — rssi 99 means no signal, 0-31 is valid (higher = better)

  Serial.println(F("[GSM] Registered on network."));

  // ── 7. Boot confirmation SMS ──────────────────────────────────────────
  sendSMS("GrainGuard online. Send HELP for commands.");
  return true;
}

// ─── sendSMS ───────────────────────────────────────────────────────────────
bool SIM800LDriver::sendSMS(const char* message) {
  return sendSMSTo(GSM_PHONE_NUMBER, message);
}

// ─── sendSMSTo ─────────────────────────────────────────────────────────────
// Mirrors send_message() from Mega sketch exactly,
// using \r terminator and (char)26 for Ctrl+Z.
bool SIM800LDriver::sendSMSTo(const char* number, const char* message) {
  Serial.printf("[GSM] Sending SMS to %s...\n", number);

  // Set text mode each time — ensures clean state
  _sendAT("AT+CMGF=1");
  delay(100);

  // Address command
  GSM_SERIAL.print("AT+CMGS=\"");
  GSM_SERIAL.print(number);
  GSM_SERIAL.print("\"\r");
  delay(100);

  // Wait for '>' prompt
  if (!_waitFor(">", GSM_TIMEOUT_SHORT)) {
    Serial.println(F("[GSM] ERROR: No '>' prompt."));
    _flushRx();
    return false;
  }

  // Message body — truncate at 155 chars to stay under 160 limit
  // (leaves room for Ctrl+Z and any trailing CR)
  char safe[156];
  strncpy(safe, message, 155);
  safe[155] = '\0';
  GSM_SERIAL.println(safe);
  delay(100);

  // Ctrl+Z commits the send — same as (char)26 in Mega sketch
  GSM_SERIAL.println((char)26);

  // Wait for send confirmation
  if (!_waitFor("+CMGS:", GSM_TIMEOUT_SMS)) {
    Serial.println(F("[GSM] ERROR: No +CMGS confirmation."));
    _flushRx();
    return false;
  }

  Serial.println(F("[GSM] SMS sent OK."));
  return true;
}

// ─── checkIncoming ─────────────────────────────────────────────────────────
// Mirrors checkForIncomingCommand() from Mega sketch.
// SIM800L with CNMI=2,2,0,0,0 pushes:
//   +CMT: "+260XXXXXXXXX","","YY/MM/DD,HH:MM:SS"\r\n
//   MESSAGE TEXT\r\n
// We read the buffer, look for +CMT, extract sender and message.
IncomingSMS SIM800LDriver::checkIncoming() {
  IncomingSMS result;
  result.command = GSMCommand::NONE;
  memset(result.raw,    0, sizeof(result.raw));
  memset(result.sender, 0, sizeof(result.sender));

  if (!GSM_SERIAL.available()) return result;

  // Read full response — same readString() + delay approach as Mega
  String data = GSM_SERIAL.readString();
  delay(100);

  // Echo to Serial for debugging (same as Mega sketch)
  Serial.print(F("[GSM] Received: ")); Serial.println(data);

  if (data.indexOf("+CMT:") < 0) return result;

  // ── Extract sender number ──────────────────────────────────────────────
  int q1 = data.indexOf('"');
  int q2 = data.indexOf('"', q1 + 1);
  if (q1 >= 0 && q2 > q1) {
    String num = data.substring(q1 + 1, q2);
    num.toCharArray(result.sender, sizeof(result.sender));
  }

  // ── Extract message text (after second newline) ────────────────────────
  int nl1 = data.indexOf('\n');
  int nl2 = data.indexOf('\n', nl1 + 1);
  if (nl2 >= 0) {
    String msg = data.substring(nl2 + 1);
    msg.trim();
    msg.toCharArray(result.raw, sizeof(result.raw));
  }

  Serial.printf("[GSM] From: %s  Msg: %s\n", result.sender, result.raw);
  _parseCommand(result);
  return result;
}

// ─── isNetworkAvailable ────────────────────────────────────────────────────
bool SIM800LDriver::isNetworkAvailable() {
  _sendAT("AT+CREG?");
  String resp = _readResponse(GSM_TIMEOUT_SHORT);
  // ,1 = registered home  ,5 = roaming — both are valid
  return (resp.indexOf(",1") >= 0 || resp.indexOf(",5") >= 0);
}

// ─── isAlive ──────────────────────────────────────────────────────────────
bool SIM800LDriver::isAlive() {
  _sendAT("AT");
  return _waitFor("OK", GSM_TIMEOUT_SHORT);
}

// ─── _parseCommand ─────────────────────────────────────────────────────────
// Case-insensitive match against known commands.
void SIM800LDriver::_parseCommand(IncomingSMS& sms) {
  String msg = String(sms.raw);
  msg.trim();
  msg.toUpperCase();

  if      (msg == "STATUS")          sms.command = GSMCommand::STATUS;
  else if (msg == "LOG")             sms.command = GSMCommand::LOG;
  else if (msg == "BATTERY")         sms.command = GSMCommand::BATTERY;
  else if (msg == "HELP")            sms.command = GSMCommand::HELP;
  else if (msg.startsWith("SET "))   sms.command = GSMCommand::SET;
  else                               sms.command = GSMCommand::UNKNOWN;

  Serial.printf("[GSM] Command: %d\n", (int)sms.command);
}

// ─── _sendAT ──────────────────────────────────────────────────────────────
// Uses \r terminator — exactly as in Mega sketch (SIM800L.print("AT\r"))
void SIM800LDriver::_sendAT(const char* cmd) {
  _flushRx();
  GSM_SERIAL.print(cmd);
  GSM_SERIAL.print('\r');
  Serial.printf("[GSM] >> %s\n", cmd);
}

// ─── _waitFor ─────────────────────────────────────────────────────────────
bool SIM800LDriver::_waitFor(const char* expected, uint32_t timeout) {
  String buf;
  uint32_t start = millis();
  while (millis() - start < timeout) {
    while (GSM_SERIAL.available()) {
      buf += (char)GSM_SERIAL.read();
      if (buf.indexOf(expected) >= 0) {
        Serial.printf("[GSM] << %s\n", buf.c_str());
        return true;
      }
    }
    delay(10);
  }
  Serial.printf("[GSM] Timeout waiting for '%s'\n", expected);
  return false;
}

// ─── _readResponse ────────────────────────────────────────────────────────
String SIM800LDriver::_readResponse(uint32_t timeout) {
  String resp;
  uint32_t start = millis();
  while (millis() - start < timeout) {
    while (GSM_SERIAL.available()) {
      resp += (char)GSM_SERIAL.read();
    }
    delay(10);
  }
  return resp;
}

// ─── _flushRx ─────────────────────────────────────────────────────────────
void SIM800LDriver::_flushRx() {
  while (GSM_SERIAL.available()) GSM_SERIAL.read();
}
