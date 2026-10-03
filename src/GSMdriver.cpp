#include "GSMdriver.h"

SIM800LDriver gsm;
bool gsmAvailable = false;

// HardwareSerial 2 on ESP32 (GPIO 16 = RX, GPIO 17 = TX)
HardwareSerial SIM800L(2);

bool SIM800LDriver::init() {
    SIM800L.begin(SIM800_BAUD, SERIAL_8N1, SIM800_RX_PIN, SIM800_TX_PIN);
    delay(1000);

    // Initial ping
    SIM800L.print("AT\r");
    delay(200);

    // Force SMS Text Mode
    SIM800L.print("AT+CMGF=1\r");
    delay(200);

    // Route incoming SMS directly to UART stream
    SIM800L.print("AT+CNMI=2,2,0,0,0\r");
    delay(200);

    gsmAvailable = true;
    Serial.println(F("[GSM] SIM800L initialized successfully."));
    return true;
}

IncomingSMS SIM800LDriver::checkIncoming() {
    IncomingSMS sms;
    sms.command = GSMCommand::NONE;

    while (SIM800L.available() > 0) {
        char c = SIM800L.read();
        _rxBuffer += c;

        // Process stream line-by-line when a newline is received
        if (c == '\n') {
            String line = _rxBuffer;
            line.toLowerCase();

            if (line.indexOf("open") >= 0) {
                sms.command = GSMCommand::OPEN;
                snprintf(sms.raw, sizeof(sms.raw), "open");
                Serial.println(F("[GSM] Command received: OPEN"));
            } 
            else if (line.indexOf("close") >= 0) {
                sms.command = GSMCommand::CLOSE;
                snprintf(sms.raw, sizeof(sms.raw), "close");
                Serial.println(F("[GSM] Command received: CLOSE"));
            }
            else if (line.indexOf("status") >= 0) {
                sms.command = GSMCommand::STATUS;
                snprintf(sms.raw, sizeof(sms.raw), "status");
            }

            _rxBuffer = ""; // Reset line buffer
            break;          // Yield execution to loop()
        }
    }

    return sms;
}

bool SIM800LDriver::sendSMSTo(const char* recipient, const char* message) {
    if (!recipient || strlen(recipient) == 0) {
        Serial.println(F("[GSM] Error: Null/empty recipient number."));
        return false;
    }

    // Always enforce Text Mode prior to CMGS
    SIM800L.print("AT+CMGF=1\r");
    delay(200);

    // Format target phone number
    SIM800L.print("AT+CMGS=\"");
    SIM800L.print(recipient);
    SIM800L.print("\"\r");
    delay(300);

    // Send payload
    SIM800L.print(message);
    delay(100);

    // Commit with Ctrl+Z (ASCII 26)
    SIM800L.write(26);
    delay(1000);

    Serial.printf("[GSM] SMS dispatched to %s\n", recipient);
    return true;
}