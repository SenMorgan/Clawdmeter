#include "../../hal/touch_hal.h"
#include "../../hal/imu_hal.h"
#include "board.h"
#include <Arduino.h>
#include <Wire.h>

// FT6336 capacitive touch over I2C. Its data layout is the same FocalTech
// format used by the other touch ports, but this board has no INT wire, so it
// is polled once per LVGL read.
//   reg 0x02:        low nibble = active finger count
//   reg 0x03 / 0x04: X high (low nibble) + X low
//   reg 0x05 / 0x06: Y high (low nibble) + Y low

static volatile bool     touch_pressed = false;
static volatile uint16_t touch_x = 0;
static volatile uint16_t touch_y = 0;

static void touch_read_into_shared_state(void) {
    Wire.beginTransmission(FT6336_ADDR);
    Wire.write(0x02);
    if (Wire.endTransmission(false) != 0) { touch_pressed = false; return; }
    if (Wire.requestFrom(FT6336_ADDR, (uint8_t)5) != 5) { touch_pressed = false; return; }
    uint8_t fingers = Wire.read() & 0x0F;
    uint8_t xH = Wire.read();
    uint8_t xL = Wire.read();
    uint8_t yH = Wire.read();
    uint8_t yL = Wire.read();
    if (fingers == 0 || fingers > 5) { touch_pressed = false; return; }
    touch_x = ((uint16_t)(xH & 0x0F) << 8) | xL;
    touch_y = ((uint16_t)(yH & 0x0F) << 8) | yL;
    touch_pressed = true;
}

void touch_hal_init(void) {
    Wire.beginTransmission(FT6336_ADDR);
    Wire.write(0xA8);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom(FT6336_ADDR, (uint8_t)1) == 1) {
        Serial.printf("Touch FT6336 ID=0x%02X (addr 0x%02X)\n", Wire.read(), FT6336_ADDR);
    } else {
        Serial.printf("Touch ID read failed (addr 0x%02X)\n", FT6336_ADDR);
    }
}

void touch_hal_read(uint16_t* x, uint16_t* y, bool* pressed) {
    touch_read_into_shared_state();
    uint16_t tx = touch_x;
    uint16_t ty = touch_y;
    // When the panel is flipped (MADCTL 180), map touch into the flipped frame.
    if (imu_hal_rotation_quadrant() == 2) {
        if (tx >= LCD_WIDTH)  tx = LCD_WIDTH - 1;
        if (ty >= LCD_HEIGHT) ty = LCD_HEIGHT - 1;
        tx = LCD_WIDTH - 1 - tx;
        ty = LCD_HEIGHT - 1 - ty;
    }
    *x = tx;
    *y = ty;
    *pressed = touch_pressed;
}
