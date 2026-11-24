# Command line reference

Connect with any serial terminal at **115200 baud, 8N1** (the port is a USB virtual serial port; the baud rate is
nominal). Examples: `pio device monitor`, PuTTY, or the Console tab of the [web app](Webapp.md). Type `help`
for the live list. Commands are case-insensitive; names and values are separated by spaces.

## Reply format

Every command ends with one line:

- `OK` - it worked (data lines, if any, came before it)
- `ERR <reason>` - it failed, with a short reason

Two other kinds of line can appear at any time, so tools should recognise them:

| Line | Meaning |
|---|---|
| `{...}` (starts with `{`) | One JSON telemetry frame, only while `stream on` |
| `EVT <ms> <NAME> a=<n> v=<n> w=<n>` | An event, only while `events on` |

## Settings

| Command | What it does |
|---|---|
| `list [prefix]` | Show parameters (optionally only those starting with `prefix`, e.g. `list radar`). `*` marks values that differ from default; `[reboot]` marks settings that apply after a reboot |
| `get <name>` | Print one value |
| `set <name> <value>` | Change a value **in RAM**, validated against its type and range. The LED turns orange while unsaved |
| `save` | Write all changes to flash (survives power loss) |
| `load` | Discard unsaved changes and reload from flash |
| `defaults <name\|all>` | Reset one or all to defaults (in RAM; `save` to keep) |
| `params` | Whole parameter registry as one JSON line (used by the web app) |

Booleans accept `0/1/on/off/true/false`. See [Parameters](Parameters.md) for every name, default and range.

Typical session: `set tamper.tilt_deg 20` then `save`.

## System

| Command | What it does |
|---|---|
| `ver` | Firmware name, version and build time |
| `status` | Uptime, node name, unsaved-changes flag, free memory, chip temperature |
| `pins` | The GPIO map in use |
| `i2cscan` | List I2C devices found, with names for the known ones |
| `reboot` | Restart the node |
| `bootsel` | Restart into the USB bootloader (an `RPI-RP2` drive appears) to update firmware |

## Sensors

| Command | What it does |
|---|---|
| `env` | BMP280/BME280 reading: chip, address, temperature, pressure, humidity, altitude |
| `imu` | BMI323: address, chip ID, acceleration, rotation, roll/pitch, tilt, vibration, tamper state |
| `mag` | QMC5883L: raw counts, calibrated values, heading |
| `magcal start` / `magcal stop` | Compass calibration. Rotate the unit through all orientations between the two, then `save` |
| `gps` | Fix, satellites, position, UTC time, NMEA counters |
| `gps raw on\|off` | Echo raw NMEA sentences to the console |
| `gps cmd <body>` | Send a PMTK command to the GPS, checksum added for you, e.g. `gps cmd PMTK220,1000` |
| `radar` | One status line per radar: online, present, state, moving/stationary distance (cm) and energy, frame counters |
| `radar ver <a\|b\|c>` | Ask a radar module for its firmware version (proves two-way serial works) |
| `radar eng <a\|b\|c> on\|off` | Engineering mode: adds per-gate energy to the stream |
| `radar apply` | Push `radar.max_*_gate`, `radar.nobody_s`, `radar.sens_*` into all three modules |
| `radar restart <x>` / `radar factory <x>` | Restart / factory-reset one radar module |
| `radar raw <a\|b\|c>` / `radar raw off` | Hex-dump the bytes arriving from one radar |

## Tamper

| Command | What it does |
|---|---|
| `arm` | Arm now, remembering the current pose as the reference |
| `disarm` | Disarm |

Tamper also auto-arms `tamper.arm_delay_s` seconds after boot (0 disables auto-arm).

## Events and streaming

| Command | What it does |
|---|---|
| `events on\|off` | Print events as they happen. `events` alone shows the running total |
| `stream on\|off\|once` | Continuous JSON telemetry at `sys.stream_hz`, or a single frame |

Event names: `RADAR_PRESENCE` (a=sector 0-2, v=distance cm, w=energy), `RADAR_CLEAR` (a=sector),
`TAMPER_TILT` (v=tilt in tenths of a degree), `TAMPER_MOTION` (v=vibration mg), `GPS_FIX` (v=1 gained / 0 lost), `BOOT`.

## Wiring diagnostic

| Command | What it does |
|---|---|
| `gpio read <n>` | Read a pin's level without changing it |
| `gpio edges <n> [ms]` | Make the pin an input and count transitions (shows whether a line is alive) |
| `gpio high <n>` / `gpio low <n>` | Drive a pin |

These change pin modes. **`reboot` afterwards.** Do not drive a pin that is connected to another output.

## Telemetry JSON

One object per line, for example (abridged):

```json
{"t":8527,"env":{"chip":"BMP280","temp":24.05,"press":853.87,"hum":null,"alt":1420.5},
 "imu":{"ok":1,"state":"ARMED","ax":-0.149,"ay":-0.028,"az":1.005,"gx":-0.1,"gy":0.0,"gz":0.0,
        "roll":-1.5,"pitch":8.4,"tilt":0.0,"motion":2},
 "mag":{"ok":1,"raw":[547,-203,-1095],"heading":31.3},
 "gps":{"fix":1,"sats":11,"lat":0.0,"lon":0.0,"alt":1542.6,"hdop":1.0,"kmh":0.1,"pps":8},
 "radar":[{"on":1,"pres":1,"st":2,"mcm":30,"me":69,"scm":248,"se":58,"eng":0}, ...]}
```

Missing or invalid values are `null`. With engineering mode on, a radar object also carries `"gm":[9 numbers]`
and `"gs":[9 numbers]` (moving and stationary energy per 0.75 m gate).
