#include "rtc.h"

ThreeWire rtcWire(DS1302_IO, DS1302_SCLK, DS1302_CE);
RtcDS1302<ThreeWire> Rtc(rtcWire);

void setupRTC() {
    Rtc.Begin();
    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

    if (!Rtc.IsDateTimeValid())     Rtc.SetDateTime(compiled);
    if (Rtc.GetIsWriteProtected())  Rtc.SetIsWriteProtected(false);
    if (!Rtc.GetIsRunning())        Rtc.SetIsRunning(true);

    // Advance stale clock to compile time
    if (Rtc.GetDateTime() < compiled) Rtc.SetDateTime(compiled);

    Serial.println(F("RTC ready."));
}