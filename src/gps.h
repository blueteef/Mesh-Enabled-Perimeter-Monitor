#pragma once
// Adafruit Ultimate GPS (PA1616D / MTK3333) NMEA reader on UART0.
#include <Arduino.h>

namespace gps {

struct Data {
    bool fix = false;
    uint8_t quality = 0;          // GGA fix quality
    uint8_t sats = 0;
    float hdop = NAN;
    double lat = 0, lon = 0;
    float alt_m = NAN;
    float speed_kmh = NAN;
    float course_deg = NAN;
    uint16_t year = 0; uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
    bool time_valid = false;
    uint32_t last_sentence_ms = 0;    // millis() of last good sentence
    uint32_t sentences = 0, bad_checksum = 0;
    uint32_t pps_count = 0;
};

void begin();
void update();
const Data& data();
void setRaw(bool on);                 // echo NMEA to USB
void sendPmtk(const char* body);      // body without '$' and checksum, e.g. "PMTK220,1000"
void setPower(bool on);

}  // namespace gps
