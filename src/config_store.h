#pragma once
// Parameter registry + flash persistence (LittleFS, text file "name=value").
// Every tunable lives in the table in config_store.cpp. The CLI and (later) the
// web app discover parameters from that table, so adding one is a one-line change.
#include <Arduino.h>

enum class PType : uint8_t { U32, I32, F32, BOOL, STR };

struct Param {
    const char* name;     // "group.key"
    PType type;
    const char* def;      // default, as text
    float minv, maxv;     // numeric limits (ignored for BOOL/STR; STR max length = maxv)
    bool reboot;          // true = takes effect after reboot
    const char* desc;
};

namespace cfg {

bool begin();                                  // mount FS and load; defaults if absent/corrupt
size_t count();
const Param& info(size_t i);
int find(const char* name);                    // index or -1

// Typed getters (by registry index or name). Unknown name returns 0 / "".
uint32_t u32(const char* name);
int32_t  i32(const char* name);
float    f32(const char* name);
bool     boolean(const char* name);
const char* str(const char* name);

// Text get/set (validates type and range). set() returns an error string or nullptr.
String   get(size_t i);
const char* set(const char* name, const char* value);
bool     isDefault(size_t i);
void     resetOne(size_t i);
void     resetAll();

bool save();                                   // atomic write to flash
bool load();
bool dirty();                                  // changed since last save/load

}  // namespace cfg
