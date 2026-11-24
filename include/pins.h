#pragma once
// ============================================================================
// pins.h - SINGLE SOURCE OF TRUTH for every GPIO on the RP2040-Zero.
// Wiki / build guide wiring tables are derived from this file. Change a pin
// here, the static_asserts below re-verify the RP2040 pin-mux rules at compile
// time, then update the wiring table.
//
// Status: DRAFT v1 - not yet wired. Owner confirmed GP0-15 and GP17-29 are all
// broken out to headers on this board (bottom headers added by owner).
// ============================================================================
#include <stdint.h>

namespace pins {

// --- GPS (Adafruit Feather Ultimate GPS) on hardware UART0 -------------------
constexpr uint8_t GPS_TX   = 0;   // MCU TX -> GPS RX
constexpr uint8_t GPS_RX   = 1;   // MCU RX <- GPS TX
constexpr uint8_t GPS_PPS  = 6;   // input, 1 Hz pulse (rising edge)
constexpr uint8_t GPS_EN   = 7;   // output, HIGH = GPS on (power gating)

// --- I2C1: BMI323, BMP280/BME280, QMC5883L ---------------------------------
constexpr uint8_t I2C_SDA  = 2;
constexpr uint8_t I2C_SCL  = 3;

// --- Meshtastic radio on hardware UART1 -------------------------------------
constexpr uint8_t MESH_TX  = 4;   // MCU TX -> radio RX
constexpr uint8_t MESH_RX  = 5;   // MCU RX <- radio TX

// --- BMI323 interrupts -------------------------------------------------------
constexpr uint8_t IMU_INT1 = 8;
constexpr uint8_t IMU_INT2 = 9;

// --- LD2410C radars on PIO UARTs (SerialPIO) ---------------------------------
constexpr uint8_t RADAR_TX[3] = {10, 12, 14};  // MCU TX -> radar RX
constexpr uint8_t RADAR_RX[3] = {11, 13, 15};  // MCU RX <- radar TX

// --- Misc --------------------------------------------------------------------
constexpr uint8_t LED_WS2812 = 16;  // onboard, not routed
constexpr uint8_t MAG_DRDY   = 27;  // optional, QMC5883L data ready (verify header)
constexpr uint8_t VBAT_ADC   = 26;  // optional, ADC0 via divider (verify header)

// ----------------------------------------------------------------------------
// Compile-time RP2040 pin-mux verification (datasheet table 'GPIO Functions')
// ----------------------------------------------------------------------------
constexpr bool uart0_tx_ok(uint8_t p) { return p == 0 || p == 12 || p == 16 || p == 28; }
constexpr bool uart0_rx_ok(uint8_t p) { return p == 1 || p == 13 || p == 17 || p == 29; }
constexpr bool uart1_tx_ok(uint8_t p) { return p == 4 || p == 8 || p == 20 || p == 24; }
constexpr bool uart1_rx_ok(uint8_t p) { return p == 5 || p == 9 || p == 21 || p == 25; }
constexpr bool i2c1_sda_ok(uint8_t p) { return p == 2 || p == 6 || p == 10 || p == 14 || p == 18 || p == 26; }
constexpr bool i2c1_scl_ok(uint8_t p) { return p == 3 || p == 7 || p == 11 || p == 15 || p == 19 || p == 27; }

static_assert(uart0_tx_ok(GPS_TX),  "GPS_TX is not a valid UART0 TX pin");
static_assert(uart0_rx_ok(GPS_RX),  "GPS_RX is not a valid UART0 RX pin");
static_assert(uart1_tx_ok(MESH_TX), "MESH_TX is not a valid UART1 TX pin");
static_assert(uart1_rx_ok(MESH_RX), "MESH_RX is not a valid UART1 RX pin");
static_assert(i2c1_sda_ok(I2C_SDA), "I2C_SDA is not a valid I2C1 SDA pin");
static_assert(i2c1_scl_ok(I2C_SCL), "I2C_SCL is not a valid I2C1 SCL pin");

// No two signals may share a pin.
constexpr uint8_t ALL[] = {
    GPS_TX, GPS_RX, GPS_PPS, GPS_EN, I2C_SDA, I2C_SCL, MESH_TX, MESH_RX,
    IMU_INT1, IMU_INT2,
    RADAR_TX[0], RADAR_RX[0], RADAR_TX[1], RADAR_RX[1], RADAR_TX[2], RADAR_RX[2],
    LED_WS2812, MAG_DRDY, VBAT_ADC};
constexpr bool all_unique() {
    for (unsigned i = 0; i < sizeof(ALL); ++i)
        for (unsigned j = i + 1; j < sizeof(ALL); ++j)
            if (ALL[i] == ALL[j]) return false;
    return true;
}
static_assert(all_unique(), "Duplicate GPIO assignment in pins.h");

// Pins the user reported as available on this board.
constexpr bool pin_available(uint8_t p) { return p <= 29 && p != 16; }
constexpr bool all_available() {
    for (unsigned i = 0; i < sizeof(ALL); ++i)
        if (ALL[i] != LED_WS2812 && !pin_available(ALL[i])) return false;
    return true;
}
static_assert(all_available(), "A signal uses a GPIO that is not broken out");

}  // namespace pins
