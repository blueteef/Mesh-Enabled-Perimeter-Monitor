# Troubleshooting

First step for almost everything: connect and run `i2cscan`, `env`, `imu`, `mag`, `gps`, `radar`. Each one says what it
found or what to check. Lines marked **(seen)** are mistakes we actually made while building the prototype.

## Can't connect / flash

| Symptom | Cause | Fix |
|---|---|---|
| No serial port appears | Charge-only USB cable | Use a data cable |
| Board never shows as a drive | Not in bootloader mode | Hold **BOOT**, plug in USB, release. An `RPI-RP2` drive appears. Or run `bootsel` if it is already running |
| `pio run -t upload` fails: "unable to connect" / needs Zadig **(seen)** | picotool has no USB driver for the bootloader on Windows | Copy `.pio/build/rp2040zero/firmware.uf2` onto the `RPI-RP2` drive instead (or install the driver with Zadig) |
| Web app / terminal says port busy | Only one program can own the serial port | Close PlatformIO's monitor, PuTTY, etc. before connecting |
| Web app has no Connect option | Browser lacks Web Serial | Use desktop Chrome or Edge |

## I2C sensors

| Symptom | Cause | Fix |
|---|---|---|
| `i2cscan` finds nothing | SDA/SCL swapped, no 3.3 V, no common ground, or missing pull-ups | Check GP2=SDA, GP3=SCL; 3V3 and GND on every module; if the breakouts have no pull-up resistors add 4.7 kOhm from SDA and SCL to 3V3 |
| `i2cscan` finds a device but `env`/`imu`/`mag` say "not found" **(seen)** | Sensors are detected only at boot | Run `reboot` after wiring a sensor |
| BMI323 not found | **CS not tied to 3V3**, or SAO strap differs from 0x68/0x69 | Tie CS firmly to 3V3 before power-up (a floating CS can drop it out of I2C mode); SAO to GND (0x68) or 3V3 (0x69) |
| BMP280 not found | SDO floating | SDO to GND (0x76) or 3V3 (0x77) |
| Device appears at an unexpected address | Strap pin the other way | Compare with the address table in the [pin map](Pin-Map.md) |
| Heading wrong direction / mirrored | Magnetometer axes do not match the board orientation | Flip signs: `set mag.sx -1` (or sy/sz), then `magcal` again |
| Heading drifts or jumps | Uncalibrated, or metal/magnet/wiring nearby | Calibrate in the final enclosure; move the QMC5883L away from the radar 5 V wiring and any magnets |
| Altitude looks wrong | Pressure altitude depends on weather | Set `env.sealevel_hpa` to the local sea-level pressure |

## GPS

| Symptom | Cause | Fix |
|---|---|---|
| `gps` says no NMEA received, `gpio edges` shows GP1 stuck low **(seen)** | GPS is **switched off**: its EN pin is active-low-enabled (LOW = on), and the firmware was driving it HIGH | Fixed in firmware. If you wire EN yourself: LOW or unconnected = on, HIGH = off |
| NMEA arrives but `fix=0` | No sky view, or still searching | Go outdoors; first fix can take minutes. `pps` counting up means it has a fix |
| Time shows a nonsense year (e.g. 2080) | No time lock yet | Normal before first fix |
| A few `bad_checksum` | Start-up garbage | Ignore unless it keeps climbing (then check baud / wiring) |

## Radars

| Symptom | Cause | Fix |
|---|---|---|
| `radar` shows `online=0` | No 5 V, no ground, or TX/RX crossed the wrong way | Check +5V and GND at the radar; MCU TX (GP10/12/14) goes to the radar **RX**, MCU RX (GP11/13/15) to the radar **TX** |
| `online=1` but `bad` frames count up | Wrong baud or noisy wiring | Check `radar.baud` is 256000; keep wires short; common ground |
| `radar ver a` -> `ERR no ACK` | Command path (MCU TX to radar RX) broken | Check that single wire. Receiving works without it, so a half-working radar is possible |
| Constant "present" with nobody there | Thresholds too low, or radars disturbing each other, or moving foliage | Raise `radar.min_energy`, lower `radar.max_cm`, adjust module sensitivity then `radar apply` |
| Misses stationary people | Stationary threshold too high | Lower `radar.sens_static`, then `radar apply` |
| A detection always reads exactly 0 cm | Target inside the module's blind zone | Ignore, or raise the minimum distance in the module |

## Tamper

| Symptom | Cause | Fix |
|---|---|---|
| `ALARM` right after boot | Unit was moved after auto-arm (default 60 s) | Place it first; `arm` when final, or raise `tamper.arm_delay_s` |
| False alarms in wind | Vibration threshold too low | Raise `tamper.motion_mg` and `tamper.motion_ms` |
| Slow tilt not detected | Tilt threshold too high or tilt time too long | Lower `tamper.tilt_deg` / `tamper.tilt_ms` |

## Settings

| Symptom | Cause | Fix |
|---|---|---|
| Change lost after power cycle | You did not `save` | `save` (LED is orange while changes are unsaved) |
| Change did nothing | Setting is marked `[reboot]` | `save`, then `reboot` |
| `ERR out of range` | Value outside the allowed range | `list <prefix>` shows the limits |
| Corrupted/unwanted settings | - | `defaults all`, then `save` |

## Diagnostic commands worth knowing

- `gpio edges <n> 2000` then `reboot`: is anything toggling on that line?
- `gps raw on`: see the raw NMEA.
- `radar raw a`: see raw bytes from a radar.
- `stream once`: one JSON frame of everything.
