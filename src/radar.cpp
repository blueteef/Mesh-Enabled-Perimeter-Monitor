#include "radar.h"
#include <SerialPIO.h>
#include "cli.h"
#include "config_store.h"
#include "events.h"
#include "pins.h"

namespace radar {

static const uint8_t HDR_DATA[4] = {0xF4, 0xF3, 0xF2, 0xF1};
static const uint8_t TAIL_DATA[4] = {0xF8, 0xF7, 0xF6, 0xF5};
static const uint8_t HDR_CMD[4] = {0xFD, 0xFC, 0xFB, 0xFA};
static const uint8_t TAIL_CMD[4] = {0x04, 0x03, 0x02, 0x01};

struct Unit {
    SerialPIO* port = nullptr;
    uint8_t buf[96];
    size_t n = 0;
    Data d;
    uint32_t lastPositive = 0;
    // last command ACK
    bool ackValid = false;
    uint8_t ack[64];
    size_t ackLen = 0;
};
static Unit u[N];
static int8_t rawIdx = -1;

const Data& data(uint8_t i) { return u[i].d; }
void setRaw(int8_t i) { rawIdx = i; }

void begin() {
    uint32_t baud = cfg::u32("radar.baud");
    for (uint8_t i = 0; i < N; ++i) {
        if (!u[i].port) u[i].port = new SerialPIO(pins::RADAR_TX[i], pins::RADAR_RX[i], 256);
        else u[i].port->end();
        u[i].port->begin(baud);
        u[i].n = 0;
    }
}

// ---------------------------------------------------------------- frame parsing
static void onDataFrame(uint8_t i, const uint8_t* p, size_t len) {
    Unit& s = u[i];
    if (len < 13 || p[1] != 0xAA) { ++s.d.bad_frames; return; }
    s.d.engineering = p[0] == 0x01;
    s.d.state = p[2];
    s.d.move_cm = p[3] | (p[4] << 8);
    s.d.move_energy = p[5];
    s.d.static_cm = p[6] | (p[7] << 8);
    s.d.static_energy = p[8];
    s.d.detect_cm = p[9] | (p[10] << 8);
    if (s.d.engineering && len >= 31) {
        for (int g = 0; g < 9; ++g) { s.d.gate_move[g] = p[13 + g]; s.d.gate_static[g] = p[22 + g]; }
    }
    ++s.d.frames;
    s.d.last_frame_ms = millis();

    // Threshold + hold -> presence
    uint32_t now = millis();
    bool enabled = cfg::u32("radar.enable_mask") & (1u << i);
    uint8_t minE = cfg::u32("radar.min_energy");
    uint16_t maxCm = (uint16_t)(cfg::f32("radar.max_ft") * 30.48f);
    uint16_t minCm = (uint16_t)(cfg::f32("radar.min_ft") * 30.48f);
    bool useStatic = cfg::boolean("radar.use_static");
    bool hit = false;
    uint16_t dist = 0; uint8_t en = 0;
    if (enabled && s.d.state) {
        if ((s.d.state & 1) && s.d.move_energy >= minE && s.d.move_cm <= maxCm && s.d.move_cm >= minCm) { hit = true; dist = s.d.move_cm; en = s.d.move_energy; }
        if (useStatic && (s.d.state & 2) && s.d.static_energy >= minE && s.d.static_cm <= maxCm && s.d.static_cm >= minCm && s.d.static_energy > en) { hit = true; dist = s.d.static_cm; en = s.d.static_energy; }
    }
    if (hit) {
        s.lastPositive = now;
        if (!s.d.present) { s.d.present = true; ++s.d.detections; events::push(EvType::RadarPresence, i, dist, en); }
    } else if (s.d.present && now - s.lastPositive > cfg::u32("radar.hold_ms")) {
        s.d.present = false;
        events::push(EvType::RadarClear, i);
    }
}

static void onFrame(uint8_t i, bool isCmd, const uint8_t* payload, size_t len) {
    if (!isCmd) { onDataFrame(i, payload, len); return; }
    Unit& s = u[i];
    s.ackLen = len > sizeof(s.ack) ? sizeof(s.ack) : len;
    memcpy(s.ack, payload, s.ackLen);
    s.ackValid = true;
}

static void feed(uint8_t i, uint8_t b) {
    Unit& s = u[i];
    if (s.n >= sizeof(s.buf)) { s.n = 0; ++s.d.bad_frames; }
    s.buf[s.n++] = b;
    if (s.n == 4) {                                  // validate header, else slide by one
        bool d = !memcmp(s.buf, HDR_DATA, 4), c = !memcmp(s.buf, HDR_CMD, 4);
        if (!d && !c) { memmove(s.buf, s.buf + 1, 3); s.n = 3; }
        return;
    }
    if (s.n < 10) return;
    uint16_t plen = s.buf[4] | (s.buf[5] << 8);
    if (plen > sizeof(s.buf) - 10) { s.n = 0; ++s.d.bad_frames; return; }
    if (s.n < (size_t)plen + 10) return;
    bool isCmd = !memcmp(s.buf, HDR_CMD, 4);
    if (memcmp(s.buf + 6 + plen, isCmd ? TAIL_CMD : TAIL_DATA, 4)) { ++s.d.bad_frames; s.n = 0; return; }
    onFrame(i, isCmd, s.buf + 6, plen);
    s.n = 0;
}

static void pump(uint8_t i) {
    Unit& s = u[i];
    while (s.port->available()) {
        uint8_t b = s.port->read();
        if (rawIdx == (int8_t)i) cli::out->printf("%02X ", b);
        feed(i, b);
    }
}

void update() {
    for (uint8_t i = 0; i < N; ++i) {
        if (!u[i].port) continue;
        pump(i);
        u[i].d.online = u[i].d.last_frame_ms && millis() - u[i].d.last_frame_ms < 1000;
        if (!u[i].d.online && u[i].d.present) {      // radar went silent: don't hold a stale detection
            u[i].d.present = false;
            events::push(EvType::RadarClear, i);
        }
    }
}

// ------------------------------------------------------------ command channel
// Sends one command frame and waits for its ACK (cmd | 0x0100).
static bool send(uint8_t i, uint16_t cmd, const uint8_t* val, size_t vlen, uint8_t* reply = nullptr, size_t* rlen = nullptr) {
    Unit& s = u[i];
    uint8_t f[40];
    size_t n = 0;
    memcpy(f, HDR_CMD, 4); n = 4;
    uint16_t plen = 2 + vlen;
    f[n++] = plen & 0xFF; f[n++] = plen >> 8;
    f[n++] = cmd & 0xFF;  f[n++] = cmd >> 8;
    for (size_t k = 0; k < vlen; ++k) f[n++] = val[k];
    memcpy(f + n, TAIL_CMD, 4); n += 4;
    s.ackValid = false;
    s.port->write(f, n);
    uint32_t t0 = millis();
    while (millis() - t0 < 400) {
        pump(i);
        if (s.ackValid && s.ackLen >= 4 && (s.ack[0] | (s.ack[1] << 8)) == (cmd | 0x0100)) {
            bool ok = (s.ack[2] | (s.ack[3] << 8)) == 0;
            if (reply && rlen) { *rlen = s.ackLen; memcpy(reply, s.ack, s.ackLen); }
            return ok;
        }
        s.ackValid = false;
        delay(1);
    }
    return false;
}

static bool enterCfg(uint8_t i) { const uint8_t v[2] = {1, 0}; return send(i, 0x00FF, v, 2); }
static void exitCfg(uint8_t i)  { send(i, 0x00FE, nullptr, 0); }

static void le32(uint8_t* p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

bool version(uint8_t i, char* out, size_t n) {
    uint8_t r[64]; size_t rl = 0;
    if (!enterCfg(i)) return false;
    bool ok = send(i, 0x00A0, nullptr, 0, r, &rl) && rl >= 12;
    if (ok) {   // ACK: cmd(2) status(2) type(2) major(2) minor(4) -> "V1.07.22091615"
        uint16_t major = r[6] | (r[7] << 8);
        uint32_t minor = r[8] | (r[9] << 8) | (r[10] << 16) | ((uint32_t)r[11] << 24);
        snprintf(out, n, "V%u.%02X.%08lX", major >> 8, major & 0xFF, (unsigned long)minor);
    }
    exitCfg(i);
    return ok;
}

bool setEngineering(uint8_t i, bool on) {
    if (!enterCfg(i)) return false;
    bool ok = send(i, on ? 0x0062 : 0x0063, nullptr, 0);
    exitCfg(i);
    return ok;
}

bool setGates(uint8_t i, uint8_t maxMove, uint8_t maxStatic, uint16_t nobodyS) {
    uint8_t v[18] = {0, 0}; le32(v + 2, maxMove);
    v[6] = 1; v[7] = 0; le32(v + 8, maxStatic);
    v[12] = 2; v[13] = 0; le32(v + 14, nobodyS);
    if (!enterCfg(i)) return false;
    bool ok = send(i, 0x0060, v, 18);
    exitCfg(i);
    return ok;
}

bool setSensitivity(uint8_t i, uint16_t gate, uint8_t move, uint8_t stat) {
    uint8_t v[18] = {0, 0};
    le32(v + 2, gate == 0xFFFF ? 0xFFFFFFFFu : gate);
    v[6] = 1; v[7] = 0; le32(v + 8, move);
    v[12] = 2; v[13] = 0; le32(v + 14, stat);
    if (!enterCfg(i)) return false;
    bool ok = send(i, 0x0064, v, 18);
    exitCfg(i);
    return ok;
}

bool restart(uint8_t i) {
    if (!enterCfg(i)) return false;
    bool ok = send(i, 0x00A3, nullptr, 0);
    return ok;      // module reboots; no end-config needed
}

bool factoryReset(uint8_t i) {
    if (!enterCfg(i)) return false;
    bool ok = send(i, 0x00A2, nullptr, 0);
    exitCfg(i);
    return ok;
}

}  // namespace radar
