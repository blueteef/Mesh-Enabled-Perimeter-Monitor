#include "events.h"
#include "cli.h"

namespace events {

static constexpr size_t N = 32;
static Event q[N];
static size_t head = 0, tail = 0;
static bool prn = false;
static uint32_t count = 0;

const char* name(EvType t) {
    switch (t) {
        case EvType::RadarPresence: return "RADAR_PRESENCE";
        case EvType::RadarClear:    return "RADAR_CLEAR";
        case EvType::TamperTilt:    return "TAMPER_TILT";
        case EvType::TamperMotion:  return "TAMPER_MOTION";
        case EvType::GpsFix:        return "GPS_FIX";
        case EvType::Boot:          return "BOOT";
    }
    return "?";
}

void push(EvType t, uint8_t a, int32_t v, int32_t w) {
    Event e{millis(), t, a, v, w};
    ++count;
    if (prn) cli::out->printf("EVT %lu %s a=%u v=%ld w=%ld\n", e.ms, name(t), a, (long)v, (long)w);
    size_t next = (head + 1) % N;
    if (next == tail) tail = (tail + 1) % N;   // overflow: drop oldest
    q[head] = e;
    head = next;
}

bool pop(Event& e) {
    if (head == tail) return false;
    e = q[tail];
    tail = (tail + 1) % N;
    return true;
}

void setPrint(bool on) { prn = on; }
bool printing() { return prn; }
uint32_t total() { return count; }

}  // namespace events
