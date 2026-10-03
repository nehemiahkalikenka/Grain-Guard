#pragma once

#include <Arduino.h>
#include "CommPrefs.h"

// Default target phone number verified in hardware test
#ifndef GSM_PHONE_NUMBER
#define GSM_PHONE_NUMBER "+260952421858"
#endif

enum class MsgType {
    ALERT,
    STATUS,
    LOG,
    INFO,
};

bool sendMessage(MsgType type, const char* message, const char* smsRecipient = nullptr);
bool sendAlert(const char* message);
bool sendStatus(const char* message, const char* recipient = nullptr);
bool sendInfo(const char* message);