// CLI commands + USB JSON telemetry stream for all sensor modules.
#include <math.h>
#include "cli.h"
#include "config_store.h"
#include "env.h"
#include "events.h"
#include "gps.h"
#include "imu.h"
#include "mag.h"
#include "radar.h"
#include "sensors.h"

namespace sensors {

static bool streaming = false;

// ------------------------------------------------------------------ helpers
// NaN-safe text formatting for the human CLI ("--" instead of "inf"/"nan").
static const char* fmtf(float v, int dp = 1) {
    static char b[4][16];
    static uint8_t i = 0;
    char* o = b[i++ & 3];
    if (isnan(v) || isinf(v)) strcpy(o, "--"); else snprintf(o, 16, "%.*f", dp, v);
    return o;
}

static void jf(const char* key, float v, int dp = 2, bool comma = true) {
    if (isnan(v)) cli::out->printf("\"%s\":null%s", key, comma ? "," : "");
    else cli::out->printf("\"%s\":%.*f%s", key, dp, v, comma ? "," : "");
}

static void printStream() {
    Stream* o = cli::out;
    const auto& e = env::data();
    const auto& i = imu::data();
    const auto& m = mag::data();
    const auto& g = gps::data();
    o->printf("{\"t\":%lu,\"env\":{\"chip\":\"%s\",", millis(), env::chipName());
    jf("temp", e.temp_c); jf("press", e.press_hpa); jf("hum", e.hum_pct, 1); jf("alt", e.alt_m, 1, false);
    o->printf("},\"imu\":{\"ok\":%d,\"state\":\"%s\",", i.valid, imu::stateName(i.state));
    jf("ax", i.ax, 3); jf("ay", i.ay, 3); jf("az", i.az, 3);
    jf("gx", i.gx, 1); jf("gy", i.gy, 1); jf("gz", i.gz, 1);
    jf("roll", i.roll, 1); jf("pitch", i.pitch, 1); jf("tilt", i.tilt_deg, 1); jf("motion", i.motion_mg, 0, false);
    o->printf("},\"mag\":{\"ok\":%d,\"raw\":[%d,%d,%d],", m.valid, m.rx, m.ry, m.rz);
    jf("heading", m.heading_deg, 1, false);
    o->printf("},\"gps\":{\"fix\":%d,\"sats\":%u,\"lat\":%.6f,\"lon\":%.6f,", g.fix, g.sats, g.lat, g.lon);
    jf("alt", g.alt_m, 1); jf("hdop", g.hdop, 1); jf("kmh", g.speed_kmh, 1);
    o->printf("\"pps\":%lu},\"radar\":[", (unsigned long)g.pps_count);
    for (uint8_t r = 0; r < radar::N; ++r) {
        const auto& d = radar::data(r);
        o->printf("%s{\"on\":%d,\"pres\":%d,\"st\":%u,\"mcm\":%u,\"me\":%u,\"scm\":%u,\"se\":%u,\"eng\":%d",
                  r ? "," : "", d.online, d.present, d.state, d.move_cm, d.move_energy, d.static_cm, d.static_energy, d.engineering);
        if (d.engineering) {   // per-gate energies (0.75 m per gate), engineering mode only
            o->print(",\"gm\":[");
            for (int k = 0; k < 9; ++k) o->printf("%s%u", k ? "," : "", d.gate_move[k]);
            o->print("],\"gs\":[");
            for (int k = 0; k < 9; ++k) o->printf("%s%u", k ? "," : "", d.gate_static[k]);
            o->print("]");
        }
        o->print("}");
    }
    o->println("]}");
}

void tick() {
    static uint32_t last = 0;
    uint32_t hz = cfg::u32("sys.stream_hz");
    if (!streaming || !hz) return;
    if (millis() - last < 1000 / hz) return;
    last = millis();
    printStream();
}

// ------------------------------------------------------------------ commands
static const char* cmdEnv(int, char**) {
    const auto& e = env::data();
    cli::out->printf("chip=%s addr=0x%02X valid=%d\n", env::chipName(), env::address(), e.valid);
    cli::out->printf("temp=%sF press=%sinHg hum=%s%% alt=%sft\n", fmtf(e.temp_c * 1.8f + 32, 1), fmtf(e.press_hpa * 0.02953f, 3), fmtf(e.hum_pct), fmtf(e.alt_m * 3.28084f, 0));
    return env::address() ? nullptr : "no BMP280/BME280 found (check wiring / SDO address)";
}

static const char* cmdImu(int, char**) {
    if (!imu::present()) return "BMI323 not found (check CS tied to 3V3, SAO, wiring)";
    const auto& d = imu::data();
    cli::out->printf("addr=0x%02X chip_id=0x%04X samples=%lu temp=%.1fF\n", imu::address(), imu::chipId(), imu::sampleCount(), d.temp_c * 1.8f + 32);
    cli::out->printf("acc g   = %.3f %.3f %.3f\n", d.ax, d.ay, d.az);
    cli::out->printf("gyr dps = %.1f %.1f %.1f\n", d.gx, d.gy, d.gz);
    cli::out->printf("roll=%.1f pitch=%.1f tilt=%.1f motion=%.0fmg tamper=%s\n", d.roll, d.pitch, d.tilt_deg, d.motion_mg, imu::stateName(d.state));
    return nullptr;
}

static const char* cmdMag(int, char**) {
    if (!mag::present()) return "QMC5883L not found";
    const auto& d = mag::data();
    cli::out->printf("raw=%d %d %d  cal=%.0f %.0f %.0f  heading=%s%s\n", d.rx, d.ry, d.rz, d.x, d.y, d.z, fmtf(d.heading_deg), d.overflow ? " OVERFLOW" : "");
    return nullptr;
}

static const char* cmdMagCal(int argc, char** argv) {
    if (argc < 2) return "usage: magcal start|stop  (rotate unit through all orientations between)";
    if (!strcmp(argv[1], "start")) { mag::calStart(); cli::out->println("rotating... run 'magcal stop' after ~20 s"); return nullptr; }
    if (!strcmp(argv[1], "stop")) {
        if (!mag::calStop()) return "not enough rotation captured";
        cli::out->println("offsets/scales updated in RAM; run 'save' to keep");
        return nullptr;
    }
    return "usage: magcal start|stop";
}

static const char* cmdGps(int argc, char** argv) {
    if (argc >= 2 && !strcmp(argv[1], "raw")) { gps::setRaw(argc > 2 && !strcmp(argv[2], "on")); return nullptr; }
    if (argc >= 3 && !strcmp(argv[1], "cmd")) { gps::sendPmtk(argv[2]); return nullptr; }
    const auto& g = gps::data();
    cli::out->printf("fix=%d q=%u sats=%u hdop=%s pps=%lu\n", g.fix, g.quality, g.sats, fmtf(g.hdop), (unsigned long)g.pps_count);
    cli::out->printf("lat=%.6f lon=%.6f alt=%sft speed=%smph\n", g.lat, g.lon, fmtf(g.alt_m * 3.28084f, 0), fmtf(g.speed_kmh * 0.621371f, 1));
    cli::out->printf("utc=%04u-%02u-%02u %02u:%02u:%02u valid=%d\n", g.year, g.month, g.day, g.hour, g.minute, g.second, g.time_valid);
    cli::out->printf("nmea_ok=%lu bad_checksum=%lu last=%lums ago\n", (unsigned long)g.sentences, (unsigned long)g.bad_checksum,
                     g.last_sentence_ms ? millis() - g.last_sentence_ms : 0);
    if (!g.sentences) return "no NMEA received (check GPS TX->GP1, power, baud)";
    return nullptr;
}

static int idx(const char* s) { return (s[0] >= '0' && s[0] <= '2' && !s[1]) ? s[0] - '0' : (s[0] >= 'a' && s[0] <= 'c' && !s[1]) ? s[0] - 'a' : -1; }

static const char* cmdRadar(int argc, char** argv) {
    if (argc == 1) {
        for (uint8_t r = 0; r < radar::N; ++r) {
            const auto& d = radar::data(r);
            cli::out->printf("%c online=%d present=%d state=%u move=%.1fft/%u static=%.1fft/%u frames=%lu bad=%lu det=%lu\n", 'A' + r, d.online, d.present,
                             d.state, d.move_cm / 30.48f, d.move_energy, d.static_cm / 30.48f, d.static_energy, (unsigned long)d.frames, (unsigned long)d.bad_frames, (unsigned long)d.detections);
        }
        return nullptr;
    }
    const char* sub = argv[1];
    if (!strcmp(sub, "apply")) {   // push parameters into each module's own flash
        for (uint8_t r = 0; r < radar::N; ++r) {
            bool ok = radar::setGates(r, cfg::u32("radar.max_move_gate"), cfg::u32("radar.max_static_gate"), cfg::u32("radar.nobody_s"))
                   && radar::setSensitivity(r, 0xFFFF, cfg::u32("radar.sens_move"), cfg::u32("radar.sens_static"));
            cli::out->printf("%c %s\n", 'A' + r, ok ? "applied" : "NO ACK");
        }
        return nullptr;
    }
    if (argc < 3) return "usage: radar [apply | ver|restart|factory|raw <a|b|c> | eng <a|b|c> on|off | raw off]";
    if (!strcmp(sub, "raw") && !strcmp(argv[2], "off")) { radar::setRaw(-1); return nullptr; }
    int r = idx(argv[2]);
    if (r < 0) return "radar must be a, b or c";
    if (!strcmp(sub, "raw")) { radar::setRaw(r); return nullptr; }
    if (!strcmp(sub, "ver")) {
        char v[32];
        if (!radar::version(r, v, sizeof v)) return "no ACK from radar";
        cli::out->println(v); return nullptr;
    }
    if (!strcmp(sub, "restart")) return radar::restart(r) ? nullptr : "no ACK";
    if (!strcmp(sub, "factory")) return radar::factoryReset(r) ? nullptr : "no ACK";
    if (!strcmp(sub, "eng") && argc >= 4) return radar::setEngineering(r, !strcmp(argv[3], "on")) ? nullptr : "no ACK";
    return "bad subcommand";
}

// Wiring diagnostic. WARNING: changes pin mode; 'reboot' afterwards to restore.
//   gpio read <n>          level (does not change mode)
//   gpio edges <n> [ms]    reconfigure as input, count transitions
//   gpio high|low <n>      drive the pin
static const char* cmdGpio(int argc, char** argv) {
    if (argc < 3) return "usage: gpio read|edges|high|low <n> [ms]";
    int n = atoi(argv[2]);
    if (n < 0 || n > 29) return "pin 0..29";
    if (!strcmp(argv[1], "read")) { cli::out->printf("GP%d=%d\n", n, digitalRead(n)); return nullptr; }
    if (!strcmp(argv[1], "high") || !strcmp(argv[1], "low")) {
        pinMode(n, OUTPUT); digitalWrite(n, argv[1][0] == 'h'); cli::out->printf("GP%d driven %s\n", n, argv[1]); return nullptr;
    }
    if (!strcmp(argv[1], "edges")) {
        uint32_t ms = argc > 3 ? atoi(argv[3]) : 1000;
        pinMode(n, INPUT);
        int last = digitalRead(n), edges = 0, hi = 0, samples = 0;
        uint32_t t0 = millis();
        while (millis() - t0 < ms) { int v = digitalRead(n); if (v != last) { ++edges; last = v; } hi += v; ++samples; }
        cli::out->printf("GP%d edges=%d high=%d%% (no-pull input)\n", n, edges, samples ? hi * 100 / samples : 0);
        return nullptr;
    }
    return "usage: gpio read|edges|high|low <n> [ms]";
}

static const char* cmdArm(int, char**) { imu::arm(); return nullptr; }
static const char* cmdDisarm(int, char**) { imu::disarm(); return nullptr; }

static const char* cmdEvents(int argc, char** argv) {
    if (argc < 2) { cli::out->printf("events printing=%d total=%lu\n", events::printing(), (unsigned long)events::total()); return nullptr; }
    events::setPrint(!strcmp(argv[1], "on"));
    return nullptr;
}

static const char* cmdStream(int argc, char** argv) {
    if (argc < 2) return "usage: stream on|off|once";
    if (!strcmp(argv[1], "once")) { printStream(); return nullptr; }
    streaming = !strcmp(argv[1], "on");
    return nullptr;
}

void registerCli() {
    cli::add("env",     "BMP280/BME280 reading", cmdEnv);
    cli::add("imu",     "BMI323 reading + tamper state", cmdImu);
    cli::add("mag",     "QMC5883L reading + heading", cmdMag);
    cli::add("magcal",  "magcal start|stop  hard/soft-iron calibration", cmdMagCal);
    cli::add("gps",     "gps | gps raw on|off | gps cmd <PMTK body>", cmdGps);
    cli::add("radar",   "radar | apply | ver|restart|factory|raw <a-c> | eng <a-c> on|off", cmdRadar);
    cli::add("gpio",    "gpio read|edges|high|low <n>  wiring diagnostic (reboot after)", cmdGpio);
    cli::add("arm",     "arm tamper detection at current pose", cmdArm);
    cli::add("disarm",  "disarm tamper detection", cmdDisarm);
    cli::add("events",  "events on|off  echo detection events", cmdEvents);
    cli::add("stream",  "stream on|off|once  JSON telemetry (rate: sys.stream_hz)", cmdStream);
}

}  // namespace sensors
