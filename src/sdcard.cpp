#include "sdcard.h"

bool sdAvailable  = false;

void setupSDCard() {
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS)) {
        Serial.println(F("SD card not found — logging disabled"));
        sdAvailable = false;
    } else {
        Serial.println(F("SD card ready."));
        sdAvailable = true;
        logToSD("=== GrainGuard boot ===");
    }
}

void logToSD(const char* data) {
    if (!sdAvailable) return;
    File f = SD.open("/datalog.txt", FILE_APPEND);
    if (f) {
        f.println(data);
        f.close();
    }
}
