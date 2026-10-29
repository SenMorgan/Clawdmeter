#include "flame.h"

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "animation.h"

namespace {

constexpr uint8_t IS31FL3731_ADDR = 0x74;
constexpr uint8_t I2C_CHUNK_SIZE = 16;
constexpr uint32_t FRAME_INTERVAL_MS = 40;
constexpr uint8_t MATRIX_WIDTH = 9;
constexpr uint8_t MATRIX_HEIGHT = 16;
constexpr uint16_t IMAGE_SIZE = MATRIX_WIDTH * MATRIX_HEIGHT;

uint8_t page = 0;
const uint8_t* animation = anim;
uint8_t image[IMAGE_SIZE] = {};
bool available = false;
SemaphoreHandle_t i2c_mutex = nullptr;

// Keep each transaction short. The board-wide lock prevents the animation
// updates from interleaving with PMU, touch, IMU, codec, or expander traffic.
bool write_registers(uint8_t reg, const uint8_t* values, uint8_t count) {
    if (i2c_mutex) xSemaphoreTakeRecursive(i2c_mutex, portMAX_DELAY);
    Wire.beginTransmission(IS31FL3731_ADDR);
    Wire.write(reg);
    Wire.write(values, count);
    const bool ok = Wire.endTransmission() == 0;
    if (i2c_mutex) xSemaphoreGiveRecursive(i2c_mutex);
    return ok;
}

bool select_page(uint8_t page_number) {
    const uint8_t value = page_number;
    return write_registers(0xFD, &value, 1);
}

bool write_frame_registers(uint8_t page_number) {
    if (i2c_mutex) xSemaphoreTakeRecursive(i2c_mutex, portMAX_DELAY);
    if (!select_page(page_number)) {
        if (i2c_mutex) xSemaphoreGiveRecursive(i2c_mutex);
        return false;
    }

    for (uint16_t offset = 0; offset < IMAGE_SIZE; offset += I2C_CHUNK_SIZE - 1) {
        const uint8_t count = min<uint8_t>(I2C_CHUNK_SIZE - 1, IMAGE_SIZE - offset);
        if (!write_registers(static_cast<uint8_t>(0x24 + offset), image + offset, count)) {
            if (i2c_mutex) xSemaphoreGiveRecursive(i2c_mutex);
            return false;
        }
    }
    if (i2c_mutex) xSemaphoreGiveRecursive(i2c_mutex);
    return true;
}

void render_next_frame(void) {
    uint8_t bounds = pgm_read_byte(animation++);
    if (bounds >= 0x90) {
        animation = anim;
        bounds = pgm_read_byte(animation++);
    }

    const uint8_t x1 = bounds >> 4;
    const uint8_t y1 = bounds & 0x0F;
    bounds = pgm_read_byte(animation++);
    const uint8_t x2 = bounds >> 4;
    const uint8_t y2 = bounds & 0x0F;

    for (uint8_t x = x1; x <= x2; ++x) {
        for (uint8_t y = y1; y <= y2; ++y) {
            image[(x << 4) + y] = pgm_read_byte(animation++);
        }
    }
}

void disable(const char* reason) {
    Serial.printf("IS31FL3731 disabled: %s\n", reason);
    available = false;
}

bool write_next_frame(void) {
    if (i2c_mutex) xSemaphoreTakeRecursive(i2c_mutex, portMAX_DELAY);
    if (!select_page(0x0B)) {
        if (i2c_mutex) xSemaphoreGiveRecursive(i2c_mutex);
        disable("buffer select failed");
        return false;
    }
    const uint8_t display_select = page;
    if (!write_registers(0x01, &display_select, 1)) {
        if (i2c_mutex) xSemaphoreGiveRecursive(i2c_mutex);
        disable("display select failed");
        return false;
    }
    page ^= 1;

    render_next_frame();
    if (!write_frame_registers(page)) {
        if (i2c_mutex) xSemaphoreGiveRecursive(i2c_mutex);
        disable("frame write failed");
        return false;
    }
    if (i2c_mutex) xSemaphoreGiveRecursive(i2c_mutex);
    return true;
}

void flame_task(void*) {
    TickType_t wake_time = xTaskGetTickCount();
    while (available) {
        vTaskDelayUntil(&wake_time, pdMS_TO_TICKS(FRAME_INTERVAL_MS));
        if (!write_next_frame()) break;
    }
    vTaskDelete(nullptr);
}

}  // namespace

void flame_plugin_init(void) {
    flame_plugin_bus_init();
    if (i2c_mutex) xSemaphoreTakeRecursive(i2c_mutex, portMAX_DELAY);
    Wire.beginTransmission(IS31FL3731_ADDR);
    const bool found = Wire.endTransmission() == 0;
    if (i2c_mutex) xSemaphoreGiveRecursive(i2c_mutex);
    if (!found) {
        Serial.println("IS31FL3731 not found");
        return;
    }

    available = true;
    Serial.println("IS31FL3731 found");

    if (!select_page(0x0B)) {
        disable("function page write failed");
        return;
    }
    uint8_t function_registers[13] = {};
    function_registers[10] = 1;
    if (!write_registers(0, function_registers, sizeof(function_registers))) {
        disable("function register write failed");
        return;
    }

    for (uint8_t frame_page = 0; frame_page < 2; ++frame_page) {
        if (!select_page(frame_page)) {
            disable("page select failed");
            return;
        }
        uint8_t led_enable[18];
        memset(led_enable, 0xFF, sizeof(led_enable));
        if (!write_registers(0, led_enable, 15)
            || !write_registers(15, led_enable + 15, 3)) {
            disable("LED enable write failed");
            return;
        }

        for (uint8_t reg = 18; reg < 0xB4; reg += I2C_CHUNK_SIZE - 1) {
            const uint8_t count = min<uint8_t>(I2C_CHUNK_SIZE - 1, 0xB4 - reg);
            uint8_t zeros[I2C_CHUNK_SIZE - 1] = {};
            if (!write_registers(reg, zeros, count)) {
                disable("PWM register write failed");
                return;
            }
        }
    }

    render_next_frame();
    if (!write_frame_registers(page)) {
        disable("first frame write failed");
        return;
    }
    if (xTaskCreatePinnedToCore(flame_task, "flame", 3072, nullptr,
                                tskIDLE_PRIORITY, nullptr, 0) != pdPASS) {
        disable("task creation failed");
    }
}

void flame_plugin_bus_init(void) {
    if (!i2c_mutex) i2c_mutex = xSemaphoreCreateRecursiveMutex();
}

extern "C" void board_i2c_lock(void) {
    if (i2c_mutex) xSemaphoreTakeRecursive(i2c_mutex, portMAX_DELAY);
}

extern "C" void board_i2c_unlock(void) {
    if (i2c_mutex) xSemaphoreGiveRecursive(i2c_mutex);
}

extern "C" void board_plugin_init(void) {
    flame_plugin_init();
}
