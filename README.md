# Mesh-Enabled Perimeter Monitor

A small, battery/solar-powered sensor node that quietly watches a temporary perimeter
(a campsite, a work site, a field camp) and reports over a [Meshtastic](https://meshtastic.org)
LoRa mesh. Drop a few nodes around the edge, switch them on, and they form their own
network - no cell service, Wi-Fi or internet needed.

Each node combines:

| Sensor | Job |
|---|---|
| 3 x HLK-LD2410C 24 GHz radar, 120 degrees apart | Detects people/animals moving *or standing still* in a full circle around the node (about 6 m) |
| GPS (Adafruit Feather Ultimate GPS) | Position + accurate time |
| BMI323 accelerometer/gyro | Knows if the node is picked up, tilted or shaken (tamper) |
| QMC5883L magnetometer | Compass heading, so detections can be placed on a map |
| BMP280 (swap-in BME280) | Temperature, pressure (and humidity with the BME280) |
| RP2040-Zero | The brain. Fuses everything and talks to the radio |
| Meshtastic radio (XIAO nRF52840 / RAK) | Carries alerts across the mesh |

## Status

Early development. **The node works standalone over USB** and is the focus right now,
because debugging through a mesh is hard. The mesh link is designed but not built yet.
See [Status & roadmap](docs/wiki/Status-and-Roadmap.md) for exactly what is verified on real hardware.

## Where to start

| I want to... | Read |
|---|---|
| Build one | [Build guide](docs/BUILD_GUIDE.md) |
| Understand how it works, no engineering degree needed | [How it works](docs/wiki/How-It-Works.md) |
| Look up a command or setting | [CLI reference](docs/wiki/CLI-Reference.md), [Parameters](docs/wiki/Parameters.md) |
| Use the tuning web app | [Web app](docs/wiki/Webapp.md) |
| Fix a problem | [Troubleshooting](docs/wiki/Troubleshooting.md) |
| Everything else | [Wiki home](docs/wiki/Home.md) |

## Quick start (already built and wired)

1. Install [PlatformIO](https://platformio.org) and run `pio run` in this folder, or grab `firmware.uf2` from a build.
2. Hold **BOOT** on the RP2040-Zero while plugging in USB; copy `firmware.uf2` to the `RPI-RP2` drive.
3. Open the web app (Chrome or Edge): `cd webapp && python serve.py 8000`, browse to
   `http://localhost:8000`, click **Connect USB**. No hardware yet? Open `http://localhost:8000/?demo`.
4. Or use any serial terminal at 115200 baud and type `help`.

## Repository layout

```
include/pins.h      every GPIO assignment (single source of truth, checked at compile time)
src/                firmware (PlatformIO, Arduino-Pico)
webapp/             Web Serial tuning console (no build step, no dependencies)
docs/               wiki + build guide
tools/              helper scripts (parameter doc generator)
```

## License

Not yet chosen.
