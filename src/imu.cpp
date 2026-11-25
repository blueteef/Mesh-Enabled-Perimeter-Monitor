#include "imu.h"
#include <initializer_list>
#include <math.h>
#include "config_store.h"
#include "axes.h"
#include "events.h"
#include "i2c_bus.h"

namespace imu {

// BMI323 registers (16-bit words, little endian). Over I2C every read returns 2
// dummy bytes before the data.
static constexpr uint8_t REG_CHIP_ID = 0x00, REG_ERR = 0x01, REG_ACC_X = 0x03,
                         REG_ACC_CONF = 0x20, REG_GYR_CONF = 0x21, REG_CMD = 0x7E;
static constexpr uint16_t CMD_SOFT_RESET = 0xDEAF;

static uint8_t gAddr = 0;
static uint16_t gChipId = 0;
static Data gData;
static uint32_t gSamples = 0, gLast = 0;
static float accLsb = 8192.0f, gyrLsb = 65.536f;

// Filters / tamper state
static float fx, fy, fz;                // fast low-pass gravity (attitude, tilt)
static float sx, sy, sz;                // slow low-pass (vibration high-pass baseline)
static bool filtInit = false;
static float refx = 0, refy = 0, refz = 1;
static uint32_t tiltSince = 0, motionSince = 0, alarmAt = 0, bootAt = 0;
static bool autoArmDone = false;

bool present() { return gAddr != 0; }
uint8_t address() { return gAddr; }
uint16_t chipId() { return gChipId; }
uint32_t sampleCount() { return gSamples; }
const Data& data() { return gData; }

const char* stateName(TamperState s) {
    switch (s) { case TamperState::Disarmed: return "DISARMED"; case TamperState::Armed: return "ARMED"; default: return "ALARM"; }
}

static bool readWords(uint8_t reg, uint16_t* w, uint8_t n) {
    uint8_t b[16];
    if (n > 8 || !i2c::read(gAddr, reg, b, n * 2, 2)) return false;
    for (uint8_t i = 0; i < n; ++i) w[i] = b[2 * i] | (b[2 * i + 1] << 8);
    return true;
}
static bool writeWord(uint8_t reg, uint16_t v) {
    uint8_t b[2] = {(uint8_t)(v & 0xFF), (uint8_t)(v >> 8)};
    return i2c::write(gAddr, reg, b, 2);
}

void begin() {
    gAddr = 0; gData = Data(); filtInit = false; gSamples = 0;
    bootAt = millis(); autoArmDone = false;
    if (!cfg::boolean("imu.enable")) return;
    // SAO low = 0x68, high = 0x69
    for (uint8_t a : {(uint8_t)0x68, (uint8_t)0x69}) {
        if (!i2c::probe(a)) continue;
        gAddr = a;
        uint16_t id = 0;
        if (readWords(REG_CHIP_ID, &id, 1) && (id & 0xFF) == 0x43) { gChipId = id; break; }
        gAddr = 0;
    }
    if (!gAddr) return;
    writeWord(REG_CMD, CMD_SOFT_RESET);
    delay(5);
    uint32_t odr = cfg::u32("imu.odr"), ar = cfg::u32("imu.acc_range"), gr = cfg::u32("imu.gyr_range");
    // mode 7 = high performance, bw bit set = ODR/2 corner
    writeWord(REG_ACC_CONF, (7u << 12) | (1u << 7) | (ar << 4) | odr);
    writeWord(REG_GYR_CONF, (7u << 12) | (1u << 7) | (gr << 4) | odr);
    accLsb = 16384.0f / (float)(1u << ar);
    gyrLsb = 262.144f / (float)(1u << gr);
    delay(5);
}

void arm() {
    refx = fx; refy = fy; refz = fz;
    float n = sqrtf(refx * refx + refy * refy + refz * refz);
    if (n > 0.1f) { refx /= n; refy /= n; refz /= n; }
    tiltSince = motionSince = 0;
    gData.state = TamperState::Armed;
}
void disarm() { gData.state = TamperState::Disarmed; }

static void tamper(uint32_t now) {
    if (!cfg::boolean("tamper.enable")) { gData.state = TamperState::Disarmed; return; }

    uint32_t delayS = cfg::u32("tamper.arm_delay_s");
    if (!autoArmDone && delayS && now - bootAt > delayS * 1000UL && filtInit) {
        autoArmDone = true;
        arm();
    }
    if (gData.state == TamperState::Disarmed) return;

    float n = sqrtf(fx * fx + fy * fy + fz * fz);
    float dot = n > 0.1f ? (fx * refx + fy * refy + fz * refz) / n : 1.0f;
    dot = constrain(dot, -1.0f, 1.0f);
    gData.tilt_deg = acosf(dot) * 57.29578f;

    if (gData.state == TamperState::Alarm) {
        if (now - alarmAt > cfg::u32("tamper.holdoff_s") * 1000UL) arm();  // re-reference at new pose
        return;
    }

    bool tilt = gData.tilt_deg > cfg::f32("tamper.tilt_deg");
    float gyro = sqrtf(gData.gx * gData.gx + gData.gy * gData.gy + gData.gz * gData.gz);
    bool motion = gData.motion_mg > cfg::f32("tamper.motion_mg") || gyro > cfg::f32("tamper.gyro_dps");

    if (tilt) { if (!tiltSince) tiltSince = now; } else tiltSince = 0;
    if (motion) { if (!motionSince) motionSince = now; } else motionSince = 0;

    if (tiltSince && now - tiltSince >= cfg::u32("tamper.tilt_ms")) {
        events::push(EvType::TamperTilt, 0, (int32_t)(gData.tilt_deg * 10));
        gData.state = TamperState::Alarm; alarmAt = now;
    } else if (motionSince && now - motionSince >= cfg::u32("tamper.motion_ms")) {
        events::push(EvType::TamperMotion, 0, (int32_t)gData.motion_mg);
        gData.state = TamperState::Alarm; alarmAt = now;
    }
}

void update() {
    if (!gAddr) return;
    uint32_t now = millis();
    uint32_t hz = cfg::u32("imu.poll_hz");
    if (!hz || now - gLast < 1000 / hz) return;
    gLast = now;

    uint16_t w[7];
    if (!readWords(REG_ACC_X, w, 7)) { gData.valid = false; return; }
    // Remap chip axes to the unit frame: x = forward, y = right, z = up (accel reads +1 g up when flat)
    static char lastSpec[20] = ""; static axes::Map map;
    const char* spec = cfg::str("imu.map");
    if (strcmp(spec, lastSpec)) {
        strncpy(lastSpec, spec, sizeof lastSpec - 1);
        if (!axes::parse(spec, map)) axes::parse("y,x,z", map);
    }
    float ca[3] = {(int16_t)w[0] / accLsb, (int16_t)w[1] / accLsb, (int16_t)w[2] / accLsb}, a3[3];
    float cg[3] = {(int16_t)w[3] / gyrLsb, (int16_t)w[4] / gyrLsb, (int16_t)w[5] / gyrLsb}, g3[3];
    map.apply(ca, a3); map.apply(cg, g3);
    float ax = a3[0], ay = a3[1], az = a3[2];
    gData.ax = ax; gData.ay = ay; gData.az = az;
    gData.gx = g3[0]; gData.gy = g3[1]; gData.gz = g3[2];
    gData.temp_c = 23.0f + (int16_t)w[6] / 512.0f;
    gData.valid = true;
    ++gSamples;

    // Gravity vector in forward/right/down terms (what the roll/pitch formulas expect)
    float gxv = -ax, gyv = -ay, gzv = az;
    if (!filtInit) { fx = gxv; fy = gyv; fz = gzv; sx = ax; sy = ay; sz = az; filtInit = true; }
    float a = cfg::f32("imu.lpf");
    fx += a * (gxv - fx); fy += a * (gyv - fy); fz += a * (gzv - fz);
    sx += 0.005f * (ax - sx); sy += 0.005f * (ay - sy); sz += 0.005f * (az - sz);

    gData.roll = atan2f(fy, fz) * 57.29578f;
    gData.pitch = atan2f(-fx, sqrtf(fy * fy + fz * fz)) * 57.29578f;

    // Vibration envelope: magnitude of (instant - slow baseline), peak-hold with decay.
    float vib = sqrtf((ax - sx) * (ax - sx) + (ay - sy) * (ay - sy) + (az - sz) * (az - sz)) * 1000.0f;
    gData.motion_mg = vib > gData.motion_mg ? vib : gData.motion_mg * 0.95f;

    tamper(now);
}

}  // namespace imu
