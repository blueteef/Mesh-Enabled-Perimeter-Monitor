#include "cli.h"
#include <Wire.h>
#include "config_store.h"
#include "pins.h"
#include "version.h"

namespace cli {

Stream* out = &Serial;

struct Cmd { const char* name; const char* usage; Handler fn; };
static constexpr size_t kMaxCmds = 48;
static Cmd gCmds[kMaxCmds];
static size_t gNum = 0;

void add(const char* name, const char* usage, Handler fn) {
    if (gNum < kMaxCmds) gCmds[gNum++] = {name, usage, fn};
}

// ------------------------------------------------------------------ built-ins
static const char* cmdHelp(int, char**) {
    for (size_t i = 0; i < gNum; ++i) out->printf("  %-10s %s\n", gCmds[i].name, gCmds[i].usage);
    return nullptr;
}

static const char* cmdVer(int, char**) {
    out->printf("%s %s (built %s %s)\n", FW_NAME, FW_VERSION, __DATE__, __TIME__);
    return nullptr;
}

static const char* cmdStatus(int, char**) {
    out->printf("uptime_s=%lu\n", millis() / 1000);
    out->printf("node=%s\n", cfg::str("node.name"));
    out->printf("cfg_dirty=%d\n", cfg::dirty());
    out->printf("free_heap=%u\n", rp2040.getFreeHeap());
    out->printf("cpu_mhz=%lu temp_c=%.1f\n", (unsigned long)(rp2040.f_cpu() / 1000000), analogReadTemp());
    return nullptr;
}

static const char* cmdPins(int, char**) {
    using namespace pins;
    out->printf("GPS      UART0 TX=GP%u RX=GP%u PPS=GP%u EN=GP%u\n", GPS_TX, GPS_RX, GPS_PPS, GPS_EN);
    out->printf("I2C1     SDA=GP%u SCL=GP%u\n", I2C_SDA, I2C_SCL);
    out->printf("MESH     UART1 TX=GP%u RX=GP%u\n", MESH_TX, MESH_RX);
    out->printf("IMU      INT1=GP%u INT2=GP%u\n", IMU_INT1, IMU_INT2);
    for (int i = 0; i < 3; ++i)
        out->printf("RADAR%c   PIO TX=GP%u RX=GP%u\n", 'A' + i, RADAR_TX[i], RADAR_RX[i]);
    out->printf("MISC     LED=GP%u MAG_DRDY=GP%u VBAT=GP%u\n", LED_WS2812, MAG_DRDY, VBAT_ADC);
    return nullptr;
}

static const char* cmdList(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    for (size_t i = 0; i < cfg::count(); ++i) {
        const Param& p = cfg::info(i);
        if (filter && strncmp(p.name, filter, strlen(filter)) != 0) continue;
        out->printf("%-24s = %-10s%s  %s%s\n", p.name, cfg::get(i).c_str(),
                    cfg::isDefault(i) ? " " : "*", p.desc, p.reboot ? " [reboot]" : "");
    }
    out->println("(* = differs from default)");
    return nullptr;
}

static const char* cmdGet(int argc, char** argv) {
    if (argc < 2) return "usage: get <name>";
    int i = cfg::find(argv[1]);
    if (i < 0) return "unknown parameter";
    out->println(cfg::get(i));
    return nullptr;
}

static const char* cmdSet(int argc, char** argv) {
    if (argc < 3) return "usage: set <name> <value>";
    const char* err = cfg::set(argv[1], argv[2]);
    if (err) return err;
    int i = cfg::find(argv[1]);
    out->printf("%s = %s%s\n", argv[1], cfg::get(i).c_str(),
                cfg::info(i).reboot ? "  (applies after reboot; 'save' then 'reboot')" : "");
    return nullptr;
}

static const char* cmdSave(int, char**) { return cfg::save() ? nullptr : "flash write failed"; }
static const char* cmdLoad(int, char**) { cfg::load(); return nullptr; }

static const char* cmdDefaults(int argc, char** argv) {
    if (argc < 2) return "usage: defaults <name|all>   (then 'save' to persist)";
    if (!strcmp(argv[1], "all")) { cfg::resetAll(); return nullptr; }
    int i = cfg::find(argv[1]);
    if (i < 0) return "unknown parameter";
    cfg::resetOne(i);
    return nullptr;
}

static const char* cmdReboot(int, char**) {
    out->println("rebooting");
    out->flush();
    delay(50);
    rp2040.reboot();
    return nullptr;
}

static const char* cmdBootsel(int, char**) {
    out->println("entering UF2 bootloader");
    out->flush();
    delay(50);
    rp2040.rebootToBootloader();
    return nullptr;
}

// JSON dump of the whole registry for the web app.
static const char* cmdParamsJson(int, char**) {
    out->print("[");
    for (size_t i = 0; i < cfg::count(); ++i) {
        const Param& p = cfg::info(i);
        static const char* tn[] = {"u32", "i32", "f32", "bool", "str"};
        out->printf("%s{\"name\":\"%s\",\"type\":\"%s\",\"value\":\"%s\",\"def\":\"%s\",\"min\":%g,\"max\":%g,\"reboot\":%d,\"desc\":\"%s\"}",
                    i ? "," : "", p.name, tn[(int)p.type], cfg::get(i).c_str(), p.def,
                    p.minv, p.maxv, p.reboot, p.desc);
    }
    out->println("]");
    return nullptr;
}

static const char* cmdI2cScan(int, char**) {
    int found = 0;
    for (uint8_t a = 0x08; a < 0x78; ++a) {
        Wire1.beginTransmission(a);
        if (Wire1.endTransmission() == 0) {
            const char* name = "";
            switch (a) {
                case 0x0D: name = "QMC5883L"; break;
                case 0x68: case 0x69: name = "BMI323 (SAO)"; break;
                case 0x76: case 0x77: name = "BMP280/BME280"; break;
            }
            out->printf("0x%02X %s\n", a, name);
            ++found;
        }
    }
    out->printf("%d device(s)\n", found);
    return nullptr;
}

// -------------------------------------------------------------------- engine
void begin() {
    add("help",     "list commands", cmdHelp);
    add("ver",      "firmware version", cmdVer);
    add("status",   "uptime, memory, temperature", cmdStatus);
    add("pins",     "show GPIO assignment", cmdPins);
    add("list",     "list [prefix]  show parameters", cmdList);
    add("get",      "get <name>", cmdGet);
    add("set",      "set <name> <value>  (RAM only until 'save')", cmdSet);
    add("save",     "write parameters to flash", cmdSave);
    add("load",     "reload parameters from flash", cmdLoad);
    add("defaults", "defaults <name|all>", cmdDefaults);
    add("params",   "parameter registry as JSON (web app)", cmdParamsJson);
    add("i2cscan",  "scan I2C bus", cmdI2cScan);
    add("reboot",   "restart", cmdReboot);
    add("bootsel",  "reboot into UF2 bootloader", cmdBootsel);
}

static char gLine[96];
static size_t gLen = 0;

static void run(char* line) {
    char* argv[8];
    int argc = 0;
    for (char* t = strtok(line, " \t"); t && argc < 8; t = strtok(nullptr, " \t")) argv[argc++] = t;
    if (!argc) return;
    for (size_t i = 0; i < gNum; ++i) {
        if (strcasecmp(argv[0], gCmds[i].name) == 0) {
            const char* err = gCmds[i].fn(argc, argv);
            if (err) out->printf("ERR %s\n", err); else out->println("OK");
            return;
        }
    }
    out->println("ERR unknown command (try 'help')");
}

void poll() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r' || c == '\n') {
            if (gLen) { gLine[gLen] = 0; gLen = 0; run(gLine); }
        } else if (c == 0x08 || c == 0x7F) {
            if (gLen) --gLen;
        } else if (gLen < sizeof(gLine) - 1) {
            gLine[gLen++] = c;
        }
    }
}

}  // namespace cli
