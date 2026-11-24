#pragma once
// USB-serial command line. Modules register their own commands with cli::add().
// Reply convention (machine-parseable for the web app):
//   success: lines of data, then  "OK"        error: "ERR <reason>"
#include <Arduino.h>

namespace cli {

// argv[0] is the command name. Handler prints data via cli::out and returns
// nullptr on success or a short error string.
using Handler = const char* (*)(int argc, char** argv);

void add(const char* name, const char* usage, Handler fn);
void begin();
void poll();            // call from loop(); non-blocking

extern Stream* out;     // current output (USB Serial)

}  // namespace cli
