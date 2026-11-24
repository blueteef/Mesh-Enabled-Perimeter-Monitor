#pragma once
// Event bus: sensors push events here; the mesh layer (next batch) and the CLI
// 'events on' printer consume them.
#include <Arduino.h>

enum class EvType : uint8_t {
    RadarPresence = 1,   // a = sector (0..2), v = distance cm, w = energy
    RadarClear    = 2,   // a = sector
    TamperTilt    = 3,   // v = tilt angle x10 deg
    TamperMotion  = 4,   // v = motion level mg
    GpsFix        = 5,   // v = 1 acquired, 0 lost
    Boot          = 6,
};

struct Event {
    uint32_t ms;
    EvType type;
    uint8_t a;
    int32_t v;
    int32_t w;
};

namespace events {
void push(EvType t, uint8_t a = 0, int32_t v = 0, int32_t w = 0);
bool pop(Event& e);                 // consumer for mesh layer
const char* name(EvType t);
void setPrint(bool on);             // echo events to USB CLI
bool printing();
uint32_t total();
}  // namespace events
