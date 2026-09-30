#include "GSMdriver.h"

// ─── init ──────────────────────────────────────────────────────────────────
bool SIM800LDriver::init() {
  GSM_SERIAL.begin(GSM_BAUD, SERIAL_8N1, GSM_PIN_RX, GSM_PIN_TX);
  delay(3000);   // Let SIM800L finish booting
  _flushRx();

  // ── 1. Alive check ────────────────────────────────────────────────────
  Serial.println(F("[GSM] Checking module..."));
  _sendAT("AT");
  if (!_waitFor("OK", GSM_TIMEOUT_SHORT)) {
    Serial.println(F("[GSM] ERROR: No response to AT."));
    Serial.println(F("[GSM] Check: power (3.9-4.2V 2A peak), wiring, shared GND."));
    return false;
  }
  Serial.println(F("[GSM] Module alive."));

  // ── 2. Disable echo ───────────────────────────────────────────────────
  _sendAT("ATE0");
  _waitFor("OK", GSM_TIMEOUT_SHORT);

  // ── 3. Check SIM card readiness ───────────────────────────────────────
  _sendAT("AT+CPIN?");
  if (!_waitFor("READY", GSM_TIMEOUT_SHORT)) {
    Serial.println(F("[GSM] ERROR: SIM card missing, busy, or PIN locked."));
    return false;
  }
  Serial.println(F("[GSM] SIM Card Ready."));

  // ── 4. Wait for network registration FIRST ────────────────────────────
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

  // ── 5. SMS text mode ──────────────────────────────────────────────────
  _sendAT("AT+CMGF=1");
  if (!_waitFor("OK", GSM_TIMEOUT_SHORT)) {
    Serial.println(F("[GSM] ERROR: Could not set SMS text mode."));
    return false;
  }

  // ── 6. Incoming SMS push mode ─────────────────────────────────────────
  _sendAT("AT+CNMI=2,2,0,0,0");
  if (!_waitFor("OK", GSM_TIMEOUT_SHORT)) {
    Serial.println(F("[GSM] WARNING: CNMI failed — incoming SMS may not work."));
  }

  // ── 7. Signal quality report ──────────────────────────────────────────
  _sendAT("AT+CSQ");
  String csq = _readResponse(1000);
  Serial.print(F("[GSM] Signal: ")); Serial.println(csq);

  Serial.println(F("[GSM] Registered on network."));

  // ── 8. Boot confirmation SMS ──────────────────────────────────────────
  sendSMS("GrainGuard online. Send HELP for commands.");
  return true;
}

// ─── sendSMS ───────────────────────────────────────────────────────────────
bool SIM800LDriver::sendSMS(const char* message) {
  return sendSMSTo(GSM_PHONE_NUMBER, message);
}

// ─── sendSMSTo ─────────────────────────────────────────────────────────────
bool SIM800LDriver::sendSMSTo(const char* number, const char* message) {
  Serial.printf("[GSM] Sending SMS to %s...\n", number);

  _sendAT("AT+CMGF=1");
  delay(100);

  GSM_SERIAL.print("AT+CMGS=\"");
  GSM_SERIAL.print(number);
  GSM_SERIAL.print("\"\r");
  delay(100);

  if (!_waitFor(">", GSM_TIMEOUT_SHORT)) {
    Serial.println(F("[GSM] ERROR: No '>' prompt."));
    _flushRx();
    return false;
  }

  char safe[156];
  strncpy(safe, message, 155);
  safe[155] = '\0';
  GSM_SERIAL.println(safe);
  delay(100);

  GSM_SERIAL.println((char)26);

  if (!_waitFor("+CMGS:", GSM_TIMEOUT_SMS)) {
    Serial.println(F("[GSM] ERROR: No +CMGS confirmation."));
    _flushRx();
    return false;
  }

  Serial.println(F("[GSM] SMS sent OK."));
  return true;
}

// ─── checkIncoming ─────────────────────────────────────────────────────────
IncomingSMS SIM800LDriver::checkIncoming() {
  IncomingSMS result;
  result.command = GSMCommand::NONE;
  memset(result.raw,    0, sizeof(result.raw));
  memset(result.sender, 0, sizeof(result.sender));

  if (!GSM_SERIAL.available()) return result;

  String data = GSM_SERIAL.readString();
  delay(100);

  Serial.print(F("[GSM] Received: ")); Serial.println(data);

  if (data.indexOf("+CMT:") < 0) return result;

  int q1 = data.indexOf('"');
  int q2 = data.indexOf('"', q1 + 1);
  if (q1 >= 0 && q2 > q1) {
    String num = data.substring(q1 + 1, q2);
    num.toCharArray(result.sender, sizeof(result.sender));
  }

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
  return (resp.indexOf(",1") >= 0 || resp.indexOf(",5") >= 0);
}

// ─── isAlive ──────────────────────────────────────────────────────────────
bool SIM800LDriver::isAlive() {
  _sendAT("AT");
  return _waitFor("OK", GSM_TIMEOUT_SHORT);
}

// ─── _parseCommand ─────────────────────────────────────────────────────────
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