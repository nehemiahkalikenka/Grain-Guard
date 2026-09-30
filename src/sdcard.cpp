#include "sdcard.h"

bool sdAvailable = false;
const char* LOG_FILE_PATH = "/logs.csv";

void setupSDCard() {
    // Explicitly initialize custom SPI pins for SD module
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS)) {
        Serial.println(F("SD card not found — logging disabled"));
        sdAvailable = false;
        return;
    }

    Serial.println(F("SD card ready."));
    sdAvailable = true;

    // Check if /logs.csv exists; create with CSV header if new
    if (!SD.exists(LOG_FILE_PATH)) {
        File f = SD.open(LOG_FILE_PATH, FILE_WRITE);
        if (f) {
            f.println("Timestamp,Temperature_C,Humidity_Pct,Moisture_Pct,Motion_Detected");
            f.close();
            Serial.println(F("Created /logs.csv with header."));
        } else {
            Serial.println(F("Failed to create /logs.csv header!"));
        }
    }
}

void logToSD(const char* data) {
    if (!sdAvailable) return;

    File f = SD.open(LOG_FILE_PATH, FILE_APPEND);
    if (f) {
        f.println(data);
        f.close(); // Flushes buffer to flash
    } else {
        Serial.println(F("[SD Error] Could not open /logs.csv for appending"));
    }
}

void logSensorCSV(const char* timestamp, float temp, float hum, int moisture, bool motion) {
    if (!sdAvailable) return;

    // Format row: 2026-09-30 13:30:00,25.40,60.20,14,0
    char csvRow[128];
    snprintf(csvRow, sizeof(csvRow), "%s,%.2f,%.2f,%d,%d",
             timestamp, temp, hum, moisture, motion ? 1 : 0);

    logToSD(csvRow);
}

void getSDStorageInfo(uint64_t &totalBytes, uint64_t &usedBytes) {
    if (!sdAvailable) {
        totalBytes = 0;
        usedBytes = 0;
        return;
    }
    totalBytes = SD.cardSize();
    usedBytes  = SD.usedBytes();
}