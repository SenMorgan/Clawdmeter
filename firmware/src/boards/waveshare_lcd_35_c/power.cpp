#include "../../hal/power_hal.h"
#include "../../hal/i2c_bus_hal.h"
#include "board.h"
#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>

// PWR comes from the AXP2101 PKEY IRQs (same scheme as the AMOLED-2.16):
//   SHORT    — quick tap (cycle brightness)
//   LONG     — ~1.5s mark, starts the hold-to-pair countdown
//   POSITIVE — release edge, completes/cancels the gesture

#define BATTERY_POLL_MS  2000
#define CHARGING_POLL_MS 500
#define PWR_POLL_MS      50

// AXP2101 interrupt status bytes. PKEY events are reported in INTSTS2.
constexpr uint8_t AXP2101_INTSTS_START = 0x48;
constexpr uint8_t AXP2101_PKEY_POSITIVE = 1u << 0;
constexpr uint8_t AXP2101_PKEY_LONG = 1u << 2;
constexpr uint8_t AXP2101_PKEY_SHORT = 1u << 3;

static XPowersPMU pmu;

static int      cached_pct        = -1;
static bool     cached_charging   = false;
static bool     cached_vbus       = false;
static bool     pwr_pressed_flag  = false;
static bool     pwr_long_flag     = false;
static bool     pwr_released_flag = false;
static uint32_t last_battery_ms   = 0;
static uint32_t last_charging_ms  = 0;
static uint32_t last_pwr_ms       = 0;

// Read the status bytes directly so an I2C failure cannot become a false
// short press. AXP2101 clears interrupt bits by writing 1s to the status
// registers.
static bool read_and_clear_pmu_irq(uint8_t& status2) {
    Wire.beginTransmission(AXP2101_ADDR);
    Wire.write(AXP2101_INTSTS_START);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(AXP2101_ADDR, (uint8_t)3) != 3) {
        while (Wire.available()) Wire.read();
        return false;
    }

    Wire.read();
    status2 = Wire.read();
    Wire.read();

    Wire.beginTransmission(AXP2101_ADDR);
    Wire.write(AXP2101_INTSTS_START);
    Wire.write(0xFF);
    Wire.write(0xFF);
    Wire.write(0xFF);
    return Wire.endTransmission() == 0;
}

void power_hal_init(void) {
    if (board_i2c_lock) board_i2c_lock();
    if (!pmu.begin(Wire, AXP2101_ADDR, IIC_SDA, IIC_SCL)) {
        Serial.println("AXP2101 init failed");
        if (board_i2c_unlock) board_i2c_unlock();
        return;
    }
    Serial.println("AXP2101 init OK");

    // Power the ES8311 codec and peripherals from the ALDO rail 1 at 3.3V.
    // The NS4150B power amplifier is supplied separately from VSYS.
    // ALDO2~3 are not used on this board.
    pmu.setALDO1Voltage(3300);
    pmu.enableALDO1();

    pmu.enableBattDetection();
    pmu.enableBattVoltageMeasure();
    pmu.enableGauge();
    pmu.resetGauge();

    pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    pmu.clearIrqStatus();
    pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ
                | XPOWERS_AXP2101_PKEY_LONG_IRQ
                | XPOWERS_AXP2101_PKEY_POSITIVE_IRQ);

    pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_8S);

    cached_charging = pmu.isCharging();
    cached_vbus     = pmu.isVbusIn();
    cached_pct = pmu.getBatteryPercent();
    if (board_i2c_unlock) board_i2c_unlock();
}

void power_hal_tick(void) {
    uint32_t now = millis();
    if (board_i2c_lock) board_i2c_lock();

    if (now - last_charging_ms >= CHARGING_POLL_MS) {
        last_charging_ms = now;
        cached_charging = pmu.isCharging();
        cached_vbus     = pmu.isVbusIn();
    }
    if (now - last_battery_ms >= BATTERY_POLL_MS) {
        last_battery_ms = now;
        cached_pct = pmu.getBatteryPercent();
    }
    if (now - last_pwr_ms >= PWR_POLL_MS) {
        last_pwr_ms = now;
        uint8_t status2 = 0;
        if (read_and_clear_pmu_irq(status2)) {
            if (status2 & AXP2101_PKEY_SHORT)    pwr_pressed_flag = true;
            if (status2 & AXP2101_PKEY_LONG)     pwr_long_flag = true;
            if (status2 & AXP2101_PKEY_POSITIVE) pwr_released_flag = true;
        }
    }
    if (board_i2c_unlock) board_i2c_unlock();
}

int  power_hal_battery_pct(void) { return cached_pct; }
bool power_hal_is_charging(void) { return cached_charging; }
bool power_hal_is_vbus_in(void)  { return cached_vbus; }

bool power_hal_pwr_pressed(void) {
    if (pwr_pressed_flag) { pwr_pressed_flag = false; return true; }
    return false;
}

bool power_hal_pwr_long_pressed(void) {
    if (pwr_long_flag) { pwr_long_flag = false; return true; }
    return false;
}

bool power_hal_pwr_released(void) {
    if (pwr_released_flag) { pwr_released_flag = false; return true; }
    return false;
}
