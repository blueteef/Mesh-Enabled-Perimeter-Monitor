#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include "cli.h"
#include "config_store.h"
#include "env.h"
#include "events.h"
#include "gps.h"
#include "imu.h"
#include "mag.h"
#include "radar.h"
#include "sensors.h"
#include "pins.h"
#include "version.h"

static Adafruit_NeoPixel led(1, pins::LED_WS2812, NEO_GRB + NEO_KHZ800);

static void updateLed() {
    static uint32_t last = 0;
    uint32_t now = millis();
    if (now - last < 100) return;
    last = now;
    if (!cfg::boolean("sys.led_enable")) { led.clear(); led.show(); return; }
    // Heartbeat: short green pulse once a second. Orange while config is unsaved.
    bool on = (now % 1000) < 100;
    uint8_t b = cfg::u32("sys.led_brightness");
    uint8_t r = cfg::dirty() ? b : 0;
    uint8_t g = cfg::dirty() ? b / 2 : b;
    led.setPixelColor(0, on ? led.Color(r, g, 0) : 0);
    led.show();
}

void setup() {
    Serial.begin(115200);
    cfg::begin();
    led.begin();
    Wire1.setSDA(pins::I2C_SDA);
    Wire1.setSCL(pins::I2C_SCL);
    Wire1.begin();
    Wire1.setClock(400000);
    cli::begin();
    sensors::registerCli();
    env::begin();
    imu::begin();
    mag::begin();
    gps::begin();
    radar::begin();
    events::push(EvType::Boot);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 1500) delay(10);  // don't hang when no USB host
    Serial.printf("\n%s %s ready. Type 'help'.\n", FW_NAME, FW_VERSION);
}

void loop() {
    cli::poll();
    gps::update();
    radar::update();
    imu::update();
    mag::update();
    env::update();
    sensors::tick();
    updateLed();
}
