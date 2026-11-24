# Pin map

Source of truth: [`include/pins.h`](../../include/pins.h). If this page and that file ever disagree, the file wins
(and this page needs fixing). The firmware command `pins` prints the live map.

All signals are **3.3 V logic** except the radar power pins (5 V).

## RP2040-Zero connections

| RP2040 pin | Function | Connects to | Direction |
|---|---|---|---|
| GP0 | UART0 TX | GPS **RX** | out |
| GP1 | UART0 RX | GPS **TX** | in |
| GP2 | I2C1 SDA | SDA on BMI323, BMP/BME280, QMC5883L | both |
| GP3 | I2C1 SCL | SCL on the same three | out |
| GP4 | UART1 TX | Meshtastic radio **RX** | out |
| GP5 | UART1 RX | Meshtastic radio **TX** | in |
| GP6 | GPS PPS | GPS **PPS** | in |
| GP7 | GPS EN | GPS **EN** (LOW = on, HIGH = off) | out |
| GP8 | IMU INT1 | BMI323 INT1 (reserved) | in |
| GP9 | IMU INT2 | BMI323 INT2 (reserved) | in |
| GP10 | PIO TX | Radar A **RX** | out |
| GP11 | PIO RX | Radar A **TX** | in |
| GP12 | PIO TX | Radar B **RX** | out |
| GP13 | PIO RX | Radar B **TX** | in |
| GP14 | PIO TX | Radar C **RX** | out |
| GP15 | PIO RX | Radar C **TX** | in |
| GP16 | Onboard WS2812 LED | (not routed) | out |
| GP26 | ADC0 | Battery voltage divider (optional, not yet used by firmware) | in |
| GP27 | Input | QMC5883L DRDY (optional, not yet used by firmware) | in |
| 3V3 | Power | VCC on BMI323, BMP/BME280, QMC5883L; GPS 3V | |
| 5V | Power | +5V on all three radars | |
| GND | Ground | GND on everything (common ground) | |

Spare: GP17-GP25, GP28, GP29.

## Straps on the modules (not wires to the RP2040)

| Module | Pin | Tie to | Why |
|---|---|---|---|
| BMI323 | CS | **3V3** (permanently) | Selects I2C mode. Never leave floating, never to a GPIO |
| BMI323 | SAO | GND | Address 0x68 |
| BMP280 / BME280 | SDO | GND | Address 0x76 |
| QMC5883L | - | - | Fixed address 0x0D |

## I2C address summary

| Address | Device |
|---|---|
| 0x0D | QMC5883L |
| 0x68 | BMI323 (SAO = GND) |
| 0x76 | BMP280 / BME280 (SDO = GND) |

No conflicts. The bus runs at 400 kHz.

## Why these pins

- **UART0** can use GP0/GP1; **UART1** can use GP4/GP5. Both pairs are valid per the RP2040 pin-mux table.
- **I2C1** can use GP2 (SDA) / GP3 (SCL).
- The PIO radar pairs can use any pins; consecutive pairs keep the wiring tidy.
- The I2C pair sits between two UART pairs only on the pin *numbers*. Keep the physical wires short and, if possible,
  twisted with a ground to limit crosstalk on the wire-wrapped board.
- Every assignment is verified at compile time (see Design principles, item 3).
