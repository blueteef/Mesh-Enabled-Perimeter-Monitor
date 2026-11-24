# Hardware guide

Each part: what it is, what we use it for, key facts, and gotchas we have already hit.
"Verified" means confirmed on the real prototype; everything else comes from datasheets/vendor docs and
should be treated as expected behaviour until tested.

## RP2040-Zero (main computer)

- **What:** Waveshare RP2040-Zero. A thumb-sized board with a Raspberry Pi RP2040 chip, USB-C, an onboard
  3.3 V regulator and an onboard RGB LED (WS2812) on GP16.
- **Use:** Reads every sensor, runs the tamper/radar logic, stores settings in flash, serves the USB command line.
- **Key facts:**
  - Pins GP0-GP15 and GP17-GP29 are broken out on this build (bottom headers added). GP16 is the LED.
  - Only **2 hardware UARTs**, but this design needs 5 serial links (GPS, mesh radio, 3 radars). The three radars
    use **PIO serial** (a software UART built from the chip's programmable I/O blocks). See [Design principles](Design-Principles.md).
  - Runs from 5 V in; the board makes its own 3.3 V.
- **Gotcha:** The BOOT button is wired to the flash chip, not a normal pin, so no GPIO can accidentally block booting.

## Adafruit Feather Ultimate GPS (GPS)

- **What:** A GPS/GNSS receiver (PA1616D-class module) on a Feather board with a ceramic patch antenna, a PPS
  (1-pulse-per-second) output, an EN (enable) pin and a fix LED.
- **Use:** Position, altitude, speed and UTC time. The PPS pulse lets us timestamp very accurately.
- **Key facts:**
  - Talks NMEA text at **9600 baud** by default over a 3.3 V serial link.
  - **EN is active-LOW-enabled:** pulled low on the module = ON. Driving it HIGH switches the GPS **off**.
    (We had this backwards at first and the GPS stayed dead; see [Troubleshooting](Troubleshooting.md).)
  - A first fix can take minutes outdoors with a clear sky; indoors it may never fix. A coin cell on the
    board (if fitted) lets it remember satellite data for faster restarts.
- **Verified:** NMEA received; fix acquired with 11 satellites; PPS pulses counted.

## BMP280 (now) / BME280 (later): temperature and pressure

- **What:** Bosch environmental sensors on I2C. The BMP280 measures temperature and pressure; the BME280 adds humidity.
- **Use:** Weather trends, rough altitude, and a sanity check on enclosure temperature.
- **Swap-in design:** the firmware reads the chip ID at start-up (0x58 = BMP280, 0x60 = BME280) and uses the matching
  maths. Replacing the part needs **no firmware change**, as long as the I2C address strap (SDO pin) stays the same.
- **I2C address:** 0x76 (SDO to GND) or 0x77 (SDO to 3V3). The prototype uses 0x76.
- **Verified:** BMP280 reads sensible temperature and pressure. Humidity path is written but untested until a BME280 arrives.

## BMI323 (accelerometer + gyroscope): tamper and tilt

- **What:** Bosch 6-axis motion sensor on I2C (accelerometer + gyro). Breakout pins: VCC, GND, SCL, SDA, SAO, CS, INT1, INT2.
- **Use:** Measures gravity direction (so we know roll/pitch and tilt) and vibration/rotation (so we can detect
  being knocked, lifted or carried).
- **Key facts:**
  - **CS must be tied to 3V3** for I2C mode. If CS ever goes low the chip can switch to SPI and stop answering on I2C.
  - **SAO** picks the I2C address: GND = 0x68, 3V3 = 0x69. The prototype uses 0x68.
  - Over I2C, every read returns **2 dummy bytes** before the data. The firmware skips them.
  - Tamper detection runs in firmware on the raw readings (not the chip's built-in motion engine), so every
    threshold is a live-tunable parameter.
  - INT1/INT2 are wired for future use (hardware wake-up); the current firmware polls instead.
- **Verified:** Chip found, readings streaming at about 100 Hz, plausible gravity vector. **Not yet verified:**
  the exact output-data-rate setting (see `imu.odr`), and tamper thresholds against real handling.

## QMC5883L (magnetometer): heading

- **What:** 3-axis magnetic sensor on I2C, address 0x0D.
- **Use:** Compass heading. Combined with the accelerometer's tilt, so the heading stays right if the box is not level.
- **Key facts:** Nearby metal, magnets and current-carrying wires distort it. Run **magnetometer calibration**
  once the box is fully assembled (`magcal start`, rotate, `magcal stop`, `save`). Set `mag.declination_deg`
  for your location to turn magnetic north into true north. If the heading turns the wrong way or is mirrored,
  flip the axis signs `mag.sx/sy/sz`.
- **Verified:** Data readable. **Not yet verified:** that the heading direction is correct on the finished unit.

## HLK-LD2410C x 3 (24 GHz radar)

- **What:** A tiny 24 GHz presence radar. Unlike a PIR sensor it detects **stationary** people (by breathing/micro
  movement) as well as moving ones, and is not fooled by temperature.
- **Use:** The perimeter "eyes". Three units 120 degrees apart cover a full circle.
- **Key facts:**
  - Powered from **5 V** (about 80 mA each); its serial pins are **3.3 V logic**, so no level shifting is needed.
  - Serial at **256000 baud**, 8N1. Reports state (none / moving / stationary / both), distance and an energy value
    (0-100), several times a second.
  - Range is divided into **gates of 0.75 m**. Moving and stationary sensitivity can be set per gate; the module stores
    these in its own memory (`radar apply`).
  - Nominal coverage is roughly a 120-degree cone out to about 6 m; real range depends on target size, mounting
    and environment. Treat the figures as starting points and measure.
  - **Interference:** three 24 GHz radars side by side can disturb each other. We have not seen a problem yet;
    the plan is staggered operation if we do.
- **Verified:** All three online, no bad frames, version query answered, engineering-mode gate data received.
  **Not yet verified:** detection range/accuracy against real people, mutual interference, and mounting angles.

## Meshtastic radio (XIAO nRF52840 + LoRa module, or RAK WisBlock)

- **What:** A separate small LoRa radio running Meshtastic firmware. The perimeter node talks to it over a serial link
  (UART, 3.3 V, 38400 baud by default).
- **Use:** Sends alerts and status across the mesh.
- **Status:** Pins are reserved (GP4/GP5) and the settings exist, but the mesh link firmware is not written yet.
  See [Mesh protocol (draft)](Mesh-Protocol-DRAFT.md).

## Power

- **Input:** 5 V from a battery or solar charge controller into the RP2040-Zero's 5 V pin. The board's own regulator
  supplies the 3.3 V rail for the GPS and I2C sensors; the radars take 5 V directly.
- **Budget (rough, datasheet-typical):** radars about 240 mA, GPS about 30 mA while tracking, everything else a few mA.
  Battery sizing, solar sizing and low-power modes are future work.
