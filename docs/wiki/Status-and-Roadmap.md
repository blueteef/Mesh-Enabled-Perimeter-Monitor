# Status and roadmap

Last updated: 2026-11-24. This page is the honest list of what has been proven on the real prototype and what has not.

## Verified on hardware

| Area | Result |
|---|---|
| Firmware build, flash, boot, USB command line | Works |
| Settings: change, save, reboot, still there | Works (checked across a real reboot); range/unknown-name errors work |
| I2C bus | QMC5883L (0x0D), BMI323 (0x68), BMP280 (0x76) all found |
| BMP280 | Plausible temperature/pressure |
| BMI323 | Chip ID read, readings streaming ~100 Hz, plausible gravity vector |
| QMC5883L | Raw data readable |
| GPS | NMEA parsed; fix acquired (11 satellites); PPS counted. EN polarity bug found and fixed |
| Radars A/B/C | All online, 0 bad frames, version query answered, engineering-mode gate data received |
| JSON telemetry stream | Valid JSON, correct keys |
| Web app | Runs in desktop Chrome in demo mode (all tabs, light and dark); protocol layer has 11 passing automated tests against real firmware output |

## Not yet verified

| Area | What is missing |
|---|---|
| Web app with real hardware | Only exercised in demo mode so far |
| BMI323 output-data-rate code | `imu.odr` 8 (meant to be 100 Hz) - datasheet reading differs from an older project note; confirm by measurement |
| Tamper detection | Thresholds not tuned against real handling; auto-arm behaviour only seen incidentally |
| Compass | Heading direction/mirroring on the finished unit; `magcal` not yet run |
| Radars | Detection range and accuracy on real people; mutual interference; mounting geometry; `radar apply` |
| BME280 | Humidity path written but no BME280 on hand |
| Power | Battery/solar design, current draw, low-power modes |
| Mesh | Everything (see roadmap) |
| Enclosure / outdoor behaviour | Not started |

## Roadmap

1. **Bench-verify the standalone node with the web app** (tamper tuning, compass calibration, radar tuning, ODR check).
2. **Mesh link, layer 1:** XIAO nRF52840 configured with the Serial module (TEXTMSG); firmware sends rate-limited text alerts and heartbeats; verify end to end with a phone.
3. **Mesh link, layer 2:** native position and environment telemetry (PROTO mode); settle pin numbering for RAK and XIAO.
4. **Power:** measure real current, add battery-voltage sensing (GP26 divider), sleep strategy, solar sizing.
5. **Radar interference mitigation** if testing shows it is needed.
6. **Enclosure and mounting guide** (radar angles, compass placement away from the radar power wiring).
7. **Multi-node field test** on a real perimeter.
8. **Inbound commands** with authentication.

## Known limitations

- Sensors are detected at boot; wire first, then reboot.
- Radar sectors are assumed at 0, 120 and 240 degrees clockwise from the node's forward direction. Where "forward" is on the
  enclosure is a build decision.
- The web app works in desktop Chrome/Edge only (Web Serial).
- No licence has been chosen yet.
