#pragma once

// Boards with more than one I2C client may provide a recursive mutex here.
// The weak hooks keep the shared HAL independent of FreeRTOS and other boards.
extern "C" void board_i2c_lock(void) __attribute__((weak));
extern "C" void board_i2c_unlock(void) __attribute__((weak));
