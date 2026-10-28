#include "expander.h"
#include "board.h"
#include <Arduino.h>
#include <Wire.h>

namespace {

constexpr uint8_t REG_OUTPUT = 0x01;
constexpr uint8_t REG_CONFIG = 0x03;
uint8_t output_latch = 0;

bool write_register(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(IO_EXPANDER_ADDR);
    Wire.write(reg);
    Wire.write(value);
    const uint8_t error = Wire.endTransmission();
    if (error != 0) {
        Serial.printf("TCA9554 write 0x%02X failed: %u\n", reg, error);
        return false;
    }
    return true;
}

void set_output(uint8_t pin, bool enabled) {
    const uint8_t mask = static_cast<uint8_t>(1u << pin);
    if (enabled) output_latch |= mask;
    else         output_latch &= static_cast<uint8_t>(~mask);
    write_register(REG_OUTPUT, output_latch);
}

}  // namespace

void expander_init(void) {
    const uint8_t output_mask = static_cast<uint8_t>(
        (1u << IOX_LCD_RESET) | (1u << IOX_PA_CTRL));
    write_register(REG_CONFIG, static_cast<uint8_t>(~output_mask));

    output_latch = 0;
    write_register(REG_OUTPUT, output_latch);
    delay(100);

    set_output(IOX_LCD_RESET, true);
    delay(100);
}

void expander_set_pa_enabled(bool enabled) {
    set_output(IOX_PA_CTRL, enabled);
}
