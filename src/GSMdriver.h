#pragma once

#include <Arduino.h>
#include "CommPrefs.h"

#define SIM800_RX_PIN 16
#define SIM800_TX_PIN 17
#define SIM800_BAUD   9600

enum class GSMCommand {
    NONE,
    STATUS,
    LOG,
    BATTERY,
    HELP,
    OPEN,
    CLOSE
};

struct IncomingSMS {
    GSMCommand command = GSMCommand::NONE;
    char sender[20] = {0};
    char raw[160]   = {0};
};

class SIM800LDriver {
public:
    bool init();
    IncomingSMS checkIncoming();
    bool sendSMSTo(const char* recipient, const char* message);

private:
    String _rxBuffer = "";
};

extern SIM800LDriver gsm;
extern bool gsmAvailable;