#include "board.h"
#include "expander.h"
#include "flame/flame.h"
#include <Arduino.h>
#include <Wire.h>

extern "C" void board_init(void) {
    flame_plugin_bus_init();
    Wire.begin(IIC_SDA, IIC_SCL);
    expander_init();
}
