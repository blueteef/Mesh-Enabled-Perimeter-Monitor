#pragma once
// BMI323 over I2C + attitude (roll/pitch) + software tamper detection.
// Tamper uses raw accel/gyro in firmware (not the BMI323 feature engine) so every
// threshold is tunable at runtime and there is no feature-engine init dependency.
#include <Arduino.h>

namespace imu {

enum class TamperState : uint8_t { Disarmed, Armed, Alarm };

struct Data {
    bool valid = false;
    float ax = 0, ay = 0, az = 0;      // g
    float gx = 0, gy = 0, gz = 0;      // deg/s
    float roll = 0, pitch = 0;         // deg (from low-passed gravity)
    float temp_c = NAN;
    float tilt_deg = 0;                // angle from armed reference
    float motion_mg = 0;               // high-passed vibration envelope
    TamperState state = TamperState::Disarmed;
};

void begin();
void update();
const Data& data();
bool present();
uint8_t address();
uint16_t chipId();
uint32_t sampleCount();

void arm();
void disarm();
const char* stateName(TamperState s);

}  // namespace imu
