# Design principles

Why the project is built the way it is, including the trade-offs we accepted.

## 1. Standalone first, mesh second
Debugging a radio mesh is slow and opaque. Every feature is first made to work and be observable over
USB (command line + web app). The mesh layer then only has to *move events*, not discover bugs.

## 2. Nothing is hard-coded that you might want to tune
Every threshold, rate, limit and calibration value is a **parameter** in one table in the firmware
(`src/config_store.cpp`). The command line, the web app and the generated docs all read that table, so adding a setting
is one line and shows up everywhere. Values are stored as readable text in the chip's flash and written atomically
(write a temp file, then rename) so a power cut cannot leave a half-written config. Unknown or invalid lines in an
old config file are skipped, so newer or older firmware never fails to boot because of it.

## 3. One file owns the wiring
`include/pins.h` is the single source of truth for every GPIO. At compile time it checks that each signal is on a pin
that supports that function (for example that a UART's TX really is a UART TX pin) and that no two signals share a pin.
A wiring mistake in the *code* fails the build instead of showing up as a mystery on the bench.

## 4. Five serial links on a chip with two UARTs
The RP2040 has two hardware UARTs. The GPS and the mesh radio get those. The three radars use PIO serial: a UART built
from the chip's programmable I/O. Each full-duplex PIO port uses two of the eight PIO state machines, so the radars use six,
the onboard LED uses one, and **one is spare**. This is the tightest resource in the design; do not add another PIO
peripheral without re-checking.

## 5. Tamper detection in firmware, not the sensor's feature engine
The BMI323 has built-in motion detection, but it requires an initialisation sequence that community reports describe as
fragile. We read raw acceleration and rotation and do three simple, transparent checks in code:
- **Tilt:** angle between gravity now and gravity when armed, held for a minimum time.
- **Vibration:** how far each reading is from a slow average, tracked as a decaying peak.
- **Rotation:** gyro rate above a threshold.
Any of these latches an alarm; after a hold-off the unit re-arms at its new pose, so one incident gives one alert,
not a flood. The cost is some CPU time (negligible) and no hardware wake-on-motion yet.

## 6. Events, not polling, between modules
Sensors never call the radio. They push small **events** (presence, clear, tamper, GPS fix change, boot) into a queue.
The USB console prints them today; the mesh layer will consume the same queue. That keeps the mesh work decoupled and testable.

## 7. Swappable parts behind one interface
The BMP280-to-BME280 swap is handled by reading the chip ID and using one driver for both. The same approach
(detect, then adapt) is the model for any future sensor change.

## 8. Fail visible, not silent
Every sensor command reports *why* it failed ("BMI323 not found - check CS tied to 3V3, SAO, wiring"). The status LED
turns orange while there are unsaved settings. Radar presence is dropped if the radar goes silent, so a dead radar never
holds a stale detection.

## 9. Radar tuning happens in two places
- **In the node** (`radar.min_energy`, `radar.max_cm`, `radar.hold_ms`): decides what counts as a detection. Changes apply instantly.
- **In each LD2410 module** (`radar.max_move_gate`, `radar.sens_*`, ...): the module's own thresholds. Pushed with `radar apply`
  and stored in the module.

## Trade-offs we accepted

| Decision | Gain | Cost |
|---|---|---|
| Polling sensors in a simple loop | Easy to read and debug | Not the lowest power; wake-on-motion later |
| Text config file in flash | Human-readable, tolerant of change | Slightly more code than a binary struct |
| PIO serial for radars | Frees hardware UARTs | Uses 6 of 8 state machines; baud limits untested above 256000 |
| Software tamper | Fully tunable | No hardware interrupt wake yet (INT1/INT2 wired but unused) |
| Web Serial app | No install, works offline | Chrome/Edge desktop only |
