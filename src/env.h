#pragma once
// Environment sensor. One driver for BMP280 (0x58) and BME280 (0x60); the chip
// is auto-detected by ID, so swapping parts needs no firmware change.
#include <Arduino.h>

namespace env {

struct Data {
    bool valid = false;
    float temp_c = NAN;
    float press_hpa = NAN;
    float hum_pct = NAN;      // NAN on BMP280
    float alt_m = NAN;        // from pressure + env.sealevel_hpa
};

void begin();
void update();
const Data& data();
const char* chipName();       // "none", "BMP280", "BME280"
uint8_t address();

}  // namespace env
