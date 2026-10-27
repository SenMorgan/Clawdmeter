#include "board.h"
#include <Arduino.h>
#include <Wire.h>

static void iox_write(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(IO_EXPANDER_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

extern "C" void board_init(void) {
    Wire.begin(IIC_SDA, IIC_SCL);
    // TCA9554 P1 is the panel reset line. Configure only that output and
    // release it before the display or touch drivers probe their buses.
    iox_write(0x03, static_cast<uint8_t>(~(1u << IOX_LCD_RESET)));
    iox_write(0x01, 0x00);
    delay(100);
    iox_write(0x01, static_cast<uint8_t>(1u << IOX_LCD_RESET));
    delay(100);
}
