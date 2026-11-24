#pragma once
// Three HLK-LD2410C 24 GHz radars on PIO UARTs, sectors 0/1/2 (120 deg apart).
#include <Arduino.h>

namespace radar {

constexpr uint8_t N = 3;

struct Data {
    bool online = false;          // frames received within the last second
    bool present = false;         // after thresholds + hold
    uint8_t state = 0;            // 0 none, 1 moving, 2 stationary, 3 both
    uint16_t move_cm = 0, static_cm = 0, detect_cm = 0;
    uint8_t move_energy = 0, static_energy = 0;
    bool engineering = false;
    uint8_t gate_move[9] = {0}, gate_static[9] = {0};   // engineering mode only
    uint32_t frames = 0, bad_frames = 0, last_frame_ms = 0;
    uint32_t detections = 0;
};

void begin();
void update();
const Data& data(uint8_t i);

// Module configuration (stored in the LD2410's own flash). Each returns true on ACK.
bool version(uint8_t i, char* out, size_t n);
bool setEngineering(uint8_t i, bool on);
bool setGates(uint8_t i, uint8_t maxMove, uint8_t maxStatic, uint16_t nobodyS);
bool setSensitivity(uint8_t i, uint16_t gate /*0..8, 0xFFFF=all*/, uint8_t move, uint8_t stat);
bool restart(uint8_t i);
bool factoryReset(uint8_t i);
void setRaw(int8_t i);            // hex-dump bytes from radar i to USB; -1 off

}  // namespace radar
