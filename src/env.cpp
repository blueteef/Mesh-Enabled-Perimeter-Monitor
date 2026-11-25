#include "env.h"
#include <initializer_list>
#include "config_store.h"
#include "i2c_bus.h"

namespace env {

static uint8_t gAddr = 0;
static uint8_t gChip = 0;            // 0x58 BMP280, 0x60 BME280
static Data gData;
static uint32_t gLast = 0;

// Calibration (Bosch datasheet naming)
static uint16_t T1; static int16_t T2, T3;
static uint16_t P1; static int16_t P2, P3, P4, P5, P6, P7, P8, P9;
static uint8_t H1, H3; static int16_t H2, H4, H5; static int8_t H6;
static int32_t t_fine;

static constexpr uint8_t REG_ID = 0xD0, REG_RESET = 0xE0, REG_CTRL_HUM = 0xF2,
                         REG_STATUS = 0xF3, REG_CTRL_MEAS = 0xF4, REG_CONFIG = 0xF5,
                         REG_DATA = 0xF7;

const char* chipName() {
    switch (gChip) { case 0x56: case 0x57: case 0x58: return "BMP280"; case 0x60: return "BME280"; }
    return "none";
}
uint8_t address() { return gAddr; }
const Data& data() { return gData; }

static bool readCal() {
    uint8_t b[26];
    if (!i2c::read(gAddr, 0x88, b, 24)) return false;
    T1 = b[0] | b[1] << 8;  T2 = b[2] | b[3] << 8;  T3 = b[4] | b[5] << 8;
    P1 = b[6] | b[7] << 8;  P2 = b[8] | b[9] << 8;  P3 = b[10] | b[11] << 8;
    P4 = b[12] | b[13] << 8; P5 = b[14] | b[15] << 8; P6 = b[16] | b[17] << 8;
    P7 = b[18] | b[19] << 8; P8 = b[20] | b[21] << 8; P9 = b[22] | b[23] << 8;
    if (gChip == 0x60) {
        uint8_t h[7];
        if (!i2c::read8(gAddr, 0xA1, H1)) return false;
        if (!i2c::read(gAddr, 0xE1, h, 7)) return false;
        H2 = h[0] | h[1] << 8;
        H3 = h[2];
        H4 = (int16_t)((h[3] << 4) | (h[4] & 0x0F));
        H5 = (int16_t)((h[5] << 4) | (h[4] >> 4));
        H6 = (int8_t)h[6];
    }
    return true;
}

void begin() {
    gChip = 0; gAddr = 0; gData = Data();
    for (uint8_t a : {(uint8_t)0x76, (uint8_t)0x77}) {
        uint8_t id;
        if (!i2c::probe(a) || !i2c::read8(a, REG_ID, id)) continue;
        if (id == 0x58 || id == 0x57 || id == 0x56 || id == 0x60) { gAddr = a; gChip = id; break; }
    }
    if (!gChip) return;
    i2c::write8(gAddr, REG_RESET, 0xB6);
    delay(5);
    if (!readCal()) { gChip = 0; return; }
    // Normal mode, T x2, P x4 (~ 0.01 hPa noise), IIR filter 4, standby 62.5 ms.
    if (gChip == 0x60) i2c::write8(gAddr, REG_CTRL_HUM, 0x01);     // H x1 (before CTRL_MEAS)
    i2c::write8(gAddr, REG_CONFIG, (0x01 << 5) | (0x02 << 2));
    i2c::write8(gAddr, REG_CTRL_MEAS, (0x02 << 5) | (0x03 << 2) | 0x03);
}

static float compT(int32_t adc) {
    int32_t v1 = ((((adc >> 3) - ((int32_t)T1 << 1))) * (int32_t)T2) >> 11;
    int32_t v2 = (((((adc >> 4) - (int32_t)T1) * ((adc >> 4) - (int32_t)T1)) >> 12) * (int32_t)T3) >> 14;
    t_fine = v1 + v2;
    return ((t_fine * 5 + 128) >> 8) / 100.0f;
}

static float compP(int32_t adc) {   // 64-bit integer formula from datasheet -> Pa
    int64_t v1 = (int64_t)t_fine - 128000;
    int64_t v2 = v1 * v1 * (int64_t)P6;
    v2 += (v1 * (int64_t)P5) << 17;
    v2 += (int64_t)P4 << 35;
    v1 = ((v1 * v1 * (int64_t)P3) >> 8) + ((v1 * (int64_t)P2) << 12);
    v1 = (((int64_t)1 << 47) + v1) * (int64_t)P1 >> 33;
    if (v1 == 0) return NAN;
    int64_t p = 1048576 - adc;
    p = (((p << 31) - v2) * 3125) / v1;
    v1 = ((int64_t)P9 * (p >> 13) * (p >> 13)) >> 25;
    v2 = ((int64_t)P8 * p) >> 19;
    p = ((p + v1 + v2) >> 8) + ((int64_t)P7 << 4);
    return p / 256.0f;
}

static float compH(int32_t adc) {
    int32_t v = t_fine - 76800;
    v = (((((adc << 14) - ((int32_t)H4 << 20) - ((int32_t)H5 * v)) + 16384) >> 15) *
         (((((((v * (int32_t)H6) >> 10) * (((v * (int32_t)H3) >> 11) + 32768)) >> 10) + 2097152) *
           (int32_t)H2 + 8192) >> 14));
    v -= (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)H1) >> 4);
    v = v < 0 ? 0 : v;
    v = v > 419430400 ? 419430400 : v;
    return (v >> 12) / 1024.0f;
}

void update() {
    if (!gChip) { return; }
    uint32_t every = cfg::u32("env.poll_ms");
    if (millis() - gLast < every) return;
    gLast = millis();
    uint8_t b[8];
    if (!i2c::read(gAddr, REG_DATA, b, gChip == 0x60 ? 8 : 6)) { gData.valid = false; return; }
    int32_t adcP = ((int32_t)b[0] << 12) | ((int32_t)b[1] << 4) | (b[2] >> 4);
    int32_t adcT = ((int32_t)b[3] << 12) | ((int32_t)b[4] << 4) | (b[5] >> 4);
    gData.temp_c = compT(adcT) + cfg::f32("env.temp_offset_f") / 1.8f;
    float pa = compP(adcP);
    gData.press_hpa = pa / 100.0f;
    gData.hum_pct = (gChip == 0x60) ? compH(((int32_t)b[6] << 8) | b[7]) : NAN;
    float sl = cfg::f32("env.sealevel_inhg") * 33.8639f;
    gData.alt_m = 44330.0f * (1.0f - powf(gData.press_hpa / sl, 0.1903f));
    gData.valid = !isnan(pa);
}

}  // namespace env
