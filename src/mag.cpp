#include "mag.h"
#include <math.h>
#include "config_store.h"
#include "i2c_bus.h"
#include "imu.h"

namespace mag {

static constexpr uint8_t ADDR = 0x0D;
static constexpr uint8_t REG_DATA = 0x00, REG_STATUS = 0x06, REG_CTRL1 = 0x09,
                         REG_CTRL2 = 0x0A, REG_PERIOD = 0x0B, REG_CHIP = 0x0D;

static bool gPresent = false;
static Data gData;
static uint32_t gLast = 0;
static bool calOn = false;
static int16_t mn[3], mx[3];

bool present() { return gPresent; }
const Data& data() { return gData; }
bool calActive() { return calOn; }

void begin() {
    gPresent = false; gData = Data();
    if (!cfg::boolean("mag.enable") || !i2c::probe(ADDR)) return;
    gPresent = true;
    i2c::write8(ADDR, REG_CTRL2, 0x80);        // soft reset
    delay(10);
    i2c::write8(ADDR, REG_CTRL2, 0x00);
    i2c::write8(ADDR, REG_PERIOD, 0x01);       // recommended SET/RESET period
    // OSR 512, range +/-8 G, ODR 50 Hz, continuous
    i2c::write8(ADDR, REG_CTRL1, (0 << 6) | (1 << 4) | (2 << 2) | 1);
}

void calStart() {
    for (int i = 0; i < 3; ++i) { mn[i] = 32767; mx[i] = -32768; }
    calOn = true;
}

bool calStop() {
    calOn = false;
    for (int i = 0; i < 3; ++i) if (mx[i] - mn[i] < 200) return false;   // not enough rotation
    float off[3], rad[3], avg = 0;
    for (int i = 0; i < 3; ++i) { off[i] = (mx[i] + mn[i]) / 2.0f; rad[i] = (mx[i] - mn[i]) / 2.0f; avg += rad[i]; }
    avg /= 3.0f;
    const char* on[] = {"mag.off_x", "mag.off_y", "mag.off_z"};
    const char* sn[] = {"mag.scale_x", "mag.scale_y", "mag.scale_z"};
    char b[24];
    for (int i = 0; i < 3; ++i) {
        snprintf(b, sizeof b, "%g", off[i]);        cfg::set(on[i], b);
        snprintf(b, sizeof b, "%.4f", avg / rad[i]); cfg::set(sn[i], b);
    }
    return true;
}

void update() {
    if (!gPresent) return;
    uint32_t now = millis();
    uint32_t hz = cfg::u32("mag.poll_hz");
    if (!hz || now - gLast < 1000 / hz) return;
    gLast = now;

    uint8_t st;
    if (!i2c::read8(ADDR, REG_STATUS, st)) { gData.valid = false; return; }
    if (!(st & 0x01)) return;                  // no new sample yet
    uint8_t b[6];
    if (!i2c::read(ADDR, REG_DATA, b, 6)) { gData.valid = false; return; }
    gData.overflow = st & 0x02;
    int16_t r[3] = {(int16_t)(b[0] | b[1] << 8), (int16_t)(b[2] | b[3] << 8), (int16_t)(b[4] | b[5] << 8)};
    gData.rx = r[0]; gData.ry = r[1]; gData.rz = r[2];
    if (calOn) for (int i = 0; i < 3; ++i) { if (r[i] < mn[i]) mn[i] = r[i]; if (r[i] > mx[i]) mx[i] = r[i]; }

    // Axis sign flips (mounting) -> hard-iron offset -> soft-iron scale
    float v[3] = {(float)r[0] * cfg::i32("mag.sx"), (float)r[1] * cfg::i32("mag.sy"), (float)r[2] * cfg::i32("mag.sz")};
    gData.x = (v[0] - cfg::f32("mag.off_x")) * cfg::f32("mag.scale_x");
    gData.y = (v[1] - cfg::f32("mag.off_y")) * cfg::f32("mag.scale_y");
    gData.z = (v[2] - cfg::f32("mag.off_z")) * cfg::f32("mag.scale_z");
    gData.valid = true;

    // Tilt-compensated heading (NXP AN4248). Needs a valid IMU attitude.
    if (imu::present() && imu::data().valid) {
        float phi = imu::data().roll * 0.0174533f, th = imu::data().pitch * 0.0174533f;
        float bfx = gData.x * cosf(th) + gData.y * sinf(th) * sinf(phi) + gData.z * sinf(th) * cosf(phi);
        float bfy = gData.y * cosf(phi) - gData.z * sinf(phi);
        float h = atan2f(-bfy, bfx) * 57.29578f + cfg::f32("mag.declination_deg");
        while (h < 0) h += 360.0f;
        while (h >= 360.0f) h -= 360.0f;
        gData.heading_deg = h;
    } else {
        gData.heading_deg = NAN;
    }
}

}  // namespace mag
