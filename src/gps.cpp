#include "gps.h"
#include "cli.h"
#include "config_store.h"
#include "events.h"
#include "pins.h"

namespace gps {

static Data gData;
static bool raw = false;
static char buf[100];
static size_t len = 0;
static volatile uint32_t ppsCount = 0;

const Data& data() { return gData; }
void setRaw(bool on) { raw = on; }

static void ppsIsr() { ++ppsCount; }

// Adafruit Ultimate GPS EN is active-LOW-enabled: pulled low on the module = ON,
// drive HIGH = power disabled. (Confirmed from the Adafruit datasheet.)
void setPower(bool on) { digitalWrite(pins::GPS_EN, on ? LOW : HIGH); }

void begin() {
    pinMode(pins::GPS_EN, OUTPUT);
    setPower(cfg::boolean("gps.enable"));
    pinMode(pins::GPS_PPS, INPUT);
    attachInterrupt(digitalPinToInterrupt(pins::GPS_PPS), ppsIsr, RISING);
    Serial1.setTX(pins::GPS_TX);
    Serial1.setRX(pins::GPS_RX);
    Serial1.setFIFOSize(256);
    Serial1.begin(cfg::u32("gps.baud"));
}

static uint8_t checksum(const char* s) {   // s between '$' and '*'
    uint8_t c = 0;
    while (*s) c ^= (uint8_t)*s++;
    return c;
}

void sendPmtk(const char* body) {
    char out[96];
    snprintf(out, sizeof out, "$%s*%02X\r\n", body, checksum(body));
    Serial1.print(out);
}

// Split a sentence in place on commas. Returns field count.
static int split(char* s, char** f, int max) {
    int n = 0;
    f[n++] = s;
    for (; *s && n < max; ++s) if (*s == ',') { *s = 0; f[n++] = s + 1; }
    return n;
}

static double ddmm(const char* v, const char* hemi) {   // (d)ddmm.mmmm -> decimal degrees
    if (!*v) return 0;
    double x = atof(v);
    int deg = (int)(x / 100);
    double d = deg + (x - deg * 100) / 60.0;
    return (*hemi == 'S' || *hemi == 'W') ? -d : d;
}

static void parse(char* s) {
    // s = "GPGGA,....": talker (2) + type (3)
    char* f[20];
    int n = split(s, f, 20);
    const char* type = f[0] + 2;
    if (!strcmp(type, "GGA") && n >= 10) {
        bool wasFix = gData.fix;
        gData.quality = atoi(f[6]);
        gData.sats = atoi(f[7]);
        gData.hdop = *f[8] ? atof(f[8]) : NAN;
        if (gData.quality > 0) {
            gData.lat = ddmm(f[2], f[3]);
            gData.lon = ddmm(f[4], f[5]);
            gData.alt_m = atof(f[9]);
        }
        gData.fix = gData.quality > 0;
        if (gData.fix != wasFix) events::push(EvType::GpsFix, 0, gData.fix);
    } else if (!strcmp(type, "RMC") && n >= 10) {
        if (strlen(f[1]) >= 6 && strlen(f[9]) == 6) {
            gData.hour = (f[1][0] - '0') * 10 + (f[1][1] - '0');
            gData.minute = (f[1][2] - '0') * 10 + (f[1][3] - '0');
            gData.second = (f[1][4] - '0') * 10 + (f[1][5] - '0');
            gData.day = (f[9][0] - '0') * 10 + (f[9][1] - '0');
            gData.month = (f[9][2] - '0') * 10 + (f[9][3] - '0');
            gData.year = 2000 + (f[9][4] - '0') * 10 + (f[9][5] - '0');
            gData.time_valid = *f[2] == 'A';
        }
        gData.speed_kmh = *f[7] ? atof(f[7]) * 1.852f : NAN;
        gData.course_deg = *f[8] ? atof(f[8]) : NAN;
    }
}

static void line() {
    buf[len] = 0;
    if (raw) { cli::out->print(buf); cli::out->print("\n"); }
    if (buf[0] != '$') return;
    char* star = strrchr(buf, '*');
    if (!star) return;
    *star = 0;
    uint8_t want = (uint8_t)strtoul(star + 1, nullptr, 16);
    if (checksum(buf + 1) != want) { ++gData.bad_checksum; return; }
    ++gData.sentences;
    gData.last_sentence_ms = millis();
    parse(buf + 1);
}

void update() {
    gData.pps_count = ppsCount;
    // Lost-data watchdog: no sentences for 3 s means no fix we can trust
    if (gData.fix && millis() - gData.last_sentence_ms > 3000) gData.fix = false;
    while (Serial1.available()) {
        char c = Serial1.read();
        if (c == '\n' || c == '\r') {
            if (len) { line(); len = 0; }
        } else if (len < sizeof(buf) - 1) {
            buf[len++] = c;
        } else {
            len = 0;     // overlong garbage: resync
        }
    }
}

}  // namespace gps
