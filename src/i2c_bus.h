#pragma once
// Thin register-level helpers over Wire1 (the shared sensor bus).
#include <Arduino.h>
#include <Wire.h>

namespace i2c {

inline bool probe(uint8_t addr) {
    Wire1.beginTransmission(addr);
    return Wire1.endTransmission() == 0;
}

inline bool write(uint8_t addr, uint8_t reg, const uint8_t* data, size_t n) {
    Wire1.beginTransmission(addr);
    Wire1.write(reg);
    if (n) Wire1.write(data, n);
    return Wire1.endTransmission() == 0;
}

inline bool write8(uint8_t addr, uint8_t reg, uint8_t v) { return write(addr, reg, &v, 1); }

// Read n bytes starting at reg. `skip` leading bytes are discarded (BMI323 dummy bytes).
inline bool read(uint8_t addr, uint8_t reg, uint8_t* buf, size_t n, size_t skip = 0) {
    Wire1.beginTransmission(addr);
    Wire1.write(reg);
    if (Wire1.endTransmission(false) != 0) return false;
    size_t want = n + skip;
    if ((size_t)Wire1.requestFrom((int)addr, (int)want) != want) return false;
    for (size_t i = 0; i < skip; ++i) Wire1.read();
    for (size_t i = 0; i < n; ++i) buf[i] = Wire1.read();
    return true;
}

inline bool read8(uint8_t addr, uint8_t reg, uint8_t& v) { return read(addr, reg, &v, 1); }

}  // namespace i2c
