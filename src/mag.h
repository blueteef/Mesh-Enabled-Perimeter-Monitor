#pragma once
// QMC5883L magnetometer + tilt-compensated heading.
#include <Arduino.h>

namespace mag {

struct Data {
    bool valid = false;
    float x = 0, y = 0, z = 0;      // calibrated, arbitrary units (raw counts after offset/scale)
    int16_t rx = 0, ry = 0, rz = 0; // raw counts
    float heading_deg = NAN;        // tilt-compensated, 0..360, includes declination
    bool overflow = false;
};

void begin();
void update();
const Data& data();
bool present();

// Calibration: rotate the unit through all orientations between start and stop.
void calStart();
bool calStop();                     // writes mag.off_* / mag.scale_* (RAM); user runs 'save'
bool calActive();

}  // namespace mag
