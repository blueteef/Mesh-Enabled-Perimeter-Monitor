# Web app (tuning console)

A single-page console that talks to the node directly over USB using the browser's Web Serial feature.
There is no server, no account and no internet requirement; it is plain HTML/JS in `webapp/`.

## Requirements

- Desktop **Chrome or Edge** (Web Serial is not available in Firefox or Safari, or on most phones).
- The page must be served from `localhost` or HTTPS. Easiest:

```
cd webapp
python serve.py 8000
```
then open <http://localhost:8000>.

## Trying it without hardware

Click **Demo**, or open `http://localhost:8000/?demo`. A simulated node walks "people" through the three radar sectors and
supports the same commands, so you can click through every screen. Add `#params`, `#graphs`, `#tools` or `#console`
to open a tab directly.

## Connecting

1. Plug the node in over USB.
2. Click **Connect USB** and pick the "USB Serial Device" port.
3. The app reads the firmware version, loads all parameters, and starts the telemetry stream.

If you `reboot` the node the page reconnects by itself after a few seconds.

## Tabs

**Dashboard** - at a glance:
- *Radar coverage:* top-down view; three 120-degree sectors (A at the front, B 120 degrees clockwise, C 240 degrees).
  A sector lights up and a dot shows the target's distance when it detects something. A dashed line marks magnetic north
  once the compass is working.
- *Heading,* *Orientation & tamper* (level bubble, tilt limit ring, armed/alarm state, **Arm now / Disarm**),
  *Environment*, *GPS*, and a live *Events* log.

**Graphs** - rolling charts (30 s to 10 min): radar distance and energy, acceleration, tilt, vibration, temperature, pressure,
heading, and per-gate radar energy (needs engineering mode, see Tools). Hover any chart for exact values.

**Parameters** - every setting from the node, grouped, searchable:
- Edit values; changed fields are outlined and checked against the allowed range as you type.
- **Apply** sends the changes (live, held in RAM). **Save to flash** makes them permanent. Both steps are deliberate.
- A *reboot* tag means the setting only takes effect after saving and rebooting.
- **Export / Import** save and load all settings as a JSON file, useful for cloning one node's tuning to another.
  Import only *stages* values; you still click Apply.

**Tools**
- *Compass calibration:* start, rotate the unit through every orientation for about 20 s, stop.
- *Radar modules:* push the `radar.*` gate and sensitivity settings into the modules, read each module's version,
  turn engineering mode on or off, restart a module.
- *Diagnostics:* I2C scan, status, a 5-second raw GPS NMEA capture.
- *Device:* reboot, or enter firmware-update mode (the node appears as an `RPI-RP2` drive).

**Console** - the raw command line with history (arrow keys). The stream and telemetry lines are hidden by default
so replies stay readable.

## Colours and accessibility

Radar A, B and C are always blue, orange and green everywhere. Light and dark themes follow your system, with a toggle
in the header. The colours come from a palette checked for colour-blind separation; state is also shown in text.

## How it works (for developers)

- `js/protocol.js` turns the serial character stream into commands/events. It has no browser dependencies and is tested:
  `cd webapp && npm test` (Node 20+, no packages to install). The tests use real captured firmware output in `tests/fixtures/`.
- `js/transport.js` has the Web Serial transport and the demo transport.
- `js/charts.js` has small canvas charts; `js/app.js` is the UI.
- Commands are queued one at a time because the firmware replies have no request IDs.
