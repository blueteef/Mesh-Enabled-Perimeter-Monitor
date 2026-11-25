#include "config_store.h"
#include <LittleFS.h>

// ---------------------------------------------------------------------------
// PARAMETER TABLE - add new tunables here.
// ---------------------------------------------------------------------------
static const Param kParams[] = {
    // name                  type        default     min   max    reboot desc
    {"node.name",            PType::STR, "NODE1",     0,   15,    false, "Short node label sent in mesh messages"},
    {"sys.led_enable",       PType::BOOL,"1",         0,    1,    false, "Status LED on/off"},
    {"sys.led_brightness",   PType::U32, "20",        0,  255,    false, "Status LED brightness"},
    {"sys.stream_hz",        PType::U32, "5",         0,   50,    false, "USB telemetry stream rate (0 = off)"},
    {"mesh.baud",            PType::U32, "38400",  1200, 921600,  true,  "UART baud to Meshtastic radio"},
    {"mesh.heartbeat_s",     PType::U32, "300",       0, 86400,   false, "Periodic status message interval (0 = off)"},
    {"mesh.alert_holdoff_s", PType::U32, "30",        0,  3600,   false, "Minimum seconds between alerts of the same type"},
    {"gps.baud",             PType::U32, "9600",   1200, 921600,  true,  "UART baud to GPS (Adafruit Ultimate GPS default 9600)"},
    {"gps.enable",           PType::BOOL,"1",         0,    1,    true,  "GPS powered via EN pin at boot"},

    {"env.poll_ms",          PType::U32, "1000",     100, 60000,  false, "BMP280/BME280 read interval"},
    {"env.sealevel_inhg",    PType::F32, "29.92",     26.5, 32.5, false, "Sea-level pressure (inHg) for altitude calc"},
    {"env.temp_offset_f",    PType::F32, "0",        -36,   36,   false, "Temperature calibration offset (degF)"},

    {"imu.enable",           PType::BOOL,"1",         0,    1,    true,  "Enable BMI323"},
    {"imu.odr",              PType::U32, "8",         1,   12,    true,  "BMI323 ODR code (8 = 100 Hz, 9 = 200 Hz; VERIFY on bench)"},
    {"imu.acc_range",        PType::U32, "1",         0,    3,    true,  "Accel range: 0=2g 1=4g 2=8g 3=16g"},
    {"imu.gyr_range",        PType::U32, "2",         0,    4,    true,  "Gyro range: 0=125 1=250 2=500 3=1000 4=2000 dps"},
    {"imu.poll_hz",          PType::U32, "100",       1,  400,    false, "Firmware IMU read rate"},
    {"imu.map",              PType::STR, "y,x,z",     0,   15,    false, "Chip axes pointing forward,right,up (e.g. y,x,z or x,-y,z)"},
    {"imu.lpf",              PType::F32, "0.05",   0.001,    1,   false, "Gravity low-pass coefficient (smaller = smoother)"},

    {"tamper.enable",        PType::BOOL,"1",         0,    1,    false, "Tilt/motion tamper detection"},
    {"tamper.arm_delay_s",   PType::U32, "60",        0, 3600,    false, "Auto-arm this long after boot (0 = manual 'arm')"},
    {"tamper.tilt_deg",      PType::F32, "15",        1,  180,    false, "Tilt from armed pose that triggers alarm"},
    {"tamper.tilt_ms",       PType::U32, "500",       0, 60000,   false, "Tilt must persist this long"},
    {"tamper.motion_mg",     PType::F32, "150",       5, 4000,    false, "Vibration/shake envelope threshold (mg)"},
    {"tamper.gyro_dps",      PType::F32, "60",        5, 2000,    false, "Rotation rate threshold"},
    {"tamper.motion_ms",     PType::U32, "200",       0, 60000,   false, "Motion must persist this long"},
    {"tamper.holdoff_s",     PType::U32, "60",        1, 3600,    false, "After an alarm, re-arm at the new pose after this long"},

    {"mag.enable",           PType::BOOL,"1",         0,    1,    true,  "Enable QMC5883L"},
    {"mag.poll_hz",          PType::U32, "20",        1,   50,    false, "Mag read rate"},
    {"mag.map",              PType::STR, "x,-y,z",    0,   15,    false, "Compass chip axes pointing forward,right,up (same format as imu.map)"},
    {"mag.mount_deg",        PType::F32, "0",       -180,  180,   false, "Compass chip rotation on the board: added to heading to line it up with the forward edge"},
    {"mag.declination_deg",  PType::F32, "0",       -30,   30,    false, "Magnetic declination (east positive)"},
    {"mag.sx",               PType::I32, "1",        -1,    1,    false, "Axis sign flip X (mounting)"},
    {"mag.sy",               PType::I32, "1",        -1,    1,    false, "Axis sign flip Y (mounting)"},
    {"mag.sz",               PType::I32, "1",        -1,    1,    false, "Axis sign flip Z (mounting)"},
    {"mag.off_x",            PType::F32, "0",     -40000, 40000,  false, "Hard-iron offset X (set by magcal)"},
    {"mag.off_y",            PType::F32, "0",     -40000, 40000,  false, "Hard-iron offset Y (set by magcal)"},
    {"mag.off_z",            PType::F32, "0",     -40000, 40000,  false, "Hard-iron offset Z (set by magcal)"},
    {"mag.scale_x",          PType::F32, "1",       0.1,   10,    false, "Soft-iron scale X (set by magcal)"},
    {"mag.scale_y",          PType::F32, "1",       0.1,   10,    false, "Soft-iron scale Y (set by magcal)"},
    {"mag.scale_z",          PType::F32, "1",       0.1,   10,    false, "Soft-iron scale Z (set by magcal)"},

    {"radar.baud",           PType::U32, "256000", 9600, 460800,  true,  "UART baud to LD2410 modules (factory 256000)"},
    {"radar.enable_mask",    PType::U32, "7",         0,    7,    false, "Bitmask of radars that raise detections (A=1 B=2 C=4)"},
    {"radar.min_energy",     PType::U32, "30",        0,  100,    false, "Minimum target energy to count as detection"},
    {"radar.min_ft",         PType::F32, "3",         0,   20,    false, "Ignore targets closer than this (feet): hides near-field clutter"},
    {"radar.use_static",     PType::BOOL,"1",         0,    1,    false, "Count stationary targets (0 = moving targets only: ignores walls and furniture)"},
    {"radar.max_ft",         PType::F32, "20",        1,   20,    false, "Ignore targets farther than this (feet)"},
    {"radar.hold_ms",        PType::U32, "2000",      0, 60000,   false, "Keep 'present' this long after last hit"},
    {"radar.max_move_gate",  PType::U32, "8",         1,    8,    false, "Module max moving gate, 0.75 m each ('radar apply')"},
    {"radar.max_static_gate",PType::U32, "8",         1,    8,    false, "Module max stationary gate ('radar apply')"},
    {"radar.nobody_s",       PType::U32, "5",         0,  65535,  false, "Module no-one timeout ('radar apply')"},
    {"radar.sens_move",      PType::U32, "50",        0,  100,    false, "Module moving threshold, all gates ('radar apply')"},
    {"radar.sens_static",    PType::U32, "40",        0,  100,    false, "Module stationary threshold, all gates ('radar apply')"},
};
static constexpr size_t kCount = sizeof(kParams) / sizeof(kParams[0]);
static constexpr const char* kPath = "/config.txt";
static constexpr size_t kStrMax = 24;

// Runtime values, kept as text so every type shares one storage path.
static char gVal[kCount][kStrMax + 1];
static bool gDirty = false;
static bool gMounted = false;

namespace cfg {

size_t count() { return kCount; }
const Param& info(size_t i) { return kParams[i]; }

int find(const char* name) {
    for (size_t i = 0; i < kCount; ++i)
        if (strcmp(kParams[i].name, name) == 0) return (int)i;
    return -1;
}

uint32_t u32(const char* n) { int i = find(n); return i < 0 ? 0 : (uint32_t)strtoul(gVal[i], nullptr, 10); }
int32_t  i32(const char* n) { int i = find(n); return i < 0 ? 0 : (int32_t)strtol(gVal[i], nullptr, 10); }
float    f32(const char* n) { int i = find(n); return i < 0 ? 0 : strtof(gVal[i], nullptr); }
bool     boolean(const char* n) { int i = find(n); return i >= 0 && gVal[i][0] == '1'; }
const char* str(const char* n) { int i = find(n); return i < 0 ? "" : gVal[i]; }

String get(size_t i) { return String(gVal[i]); }
bool isDefault(size_t i) { return strcmp(gVal[i], kParams[i].def) == 0; }
bool dirty() { return gDirty; }

// Validate + normalise `value` for parameter p into out. Returns error or nullptr.
static const char* normalise(const Param& p, const char* value, char* out) {
    if (!value || !*value) return p.type == PType::STR ? (out[0] = 0, nullptr) : "empty value";
    switch (p.type) {
        case PType::BOOL: {
            if (!strcasecmp(value, "1") || !strcasecmp(value, "on") || !strcasecmp(value, "true"))
                strcpy(out, "1");
            else if (!strcasecmp(value, "0") || !strcasecmp(value, "off") || !strcasecmp(value, "false"))
                strcpy(out, "0");
            else return "expected 0/1/on/off";
            return nullptr;
        }
        case PType::STR: {
            if (strlen(value) > (size_t)p.maxv) return "string too long";
            for (const char* c = value; *c; ++c)
                if (*c < 0x20 || *c == '=' || *c == '\n') return "illegal character";
            strcpy(out, value);
            return nullptr;
        }
        default: {
            char* end = nullptr;
            float v = strtof(value, &end);
            if (end == value || *end) return "not a number";
            if (v < p.minv || v > p.maxv) return "out of range";
            if (p.type == PType::F32) snprintf(out, kStrMax + 1, "%g", v);
            else if (p.type == PType::I32) snprintf(out, kStrMax + 1, "%ld", (long)v);
            else snprintf(out, kStrMax + 1, "%lu", (unsigned long)v);
            return nullptr;
        }
    }
}

const char* set(const char* name, const char* value) {
    int i = find(name);
    if (i < 0) return "unknown parameter";
    char tmp[kStrMax + 1];
    const char* err = normalise(kParams[i], value, tmp);
    if (err) return err;
    if (strcmp(tmp, gVal[i]) != 0) { strcpy(gVal[i], tmp); gDirty = true; }
    return nullptr;
}

void resetOne(size_t i) {
    if (strcmp(gVal[i], kParams[i].def) != 0) { strcpy(gVal[i], kParams[i].def); gDirty = true; }
}
void resetAll() { for (size_t i = 0; i < kCount; ++i) resetOne(i); }

bool save() {
    if (!gMounted) return false;
    File f = LittleFS.open("/config.tmp", "w");
    if (!f) return false;
    f.println("# perimeter-sensor config v1");
    for (size_t i = 0; i < kCount; ++i)
        if (!isDefault(i)) f.printf("%s=%s\n", kParams[i].name, gVal[i]);  // only non-defaults
    f.close();
    LittleFS.remove(kPath);
    if (!LittleFS.rename("/config.tmp", kPath)) return false;
    gDirty = false;
    return true;
}

bool load() {
    resetAll();
    gDirty = false;
    if (!gMounted) return false;
    File f = LittleFS.open(kPath, "r");
    if (!f) return false;                    // first boot: defaults
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.isEmpty() || line[0] == '#') continue;
        int eq = line.indexOf('=');
        if (eq < 1) continue;
        // Unknown or invalid entries are skipped, so removing/changing params in
        // new firmware never bricks an old config file.
        set(line.substring(0, eq).c_str(), line.substring(eq + 1).c_str());
    }
    f.close();
    gDirty = false;
    return true;
}

bool begin() {
    gMounted = LittleFS.begin();
    if (!gMounted) {                          // blank flash: format once
        LittleFS.format();
        gMounted = LittleFS.begin();
    }
    return load();
}

}  // namespace cfg
