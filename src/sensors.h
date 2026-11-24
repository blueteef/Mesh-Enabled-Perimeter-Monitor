#pragma once
namespace sensors {
void registerCli();   // add sensor commands to the CLI
void tick();          // emit JSON telemetry when streaming
}
