#include "../../hal/sound_hal.h"
#include "board.h"

#if BOARD_HAS_SOUND

#include <Arduino.h>
#include "../../chime.h"

// LCD-3.5-C: ES8311 codec + speaker on I2S. No power-amp enable pin, so the
// amp hook is null. All codec/I2S/playback work lives in the shared chime
// engine (../../chime.cpp).

// Main codec volume for the Waveshare LCD 3.5-C board.
#define CHIME_VOLUME 65

void sound_hal_init(void) {
    // Wait for the AXP2101 rails (power.cpp) to settle before probing the
    // codec on the shared I2C bus.
    delay(100);
    const ChimeConfig cfg = {
        SND_I2S_MCLK, SND_I2S_BCLK, SND_I2S_WS, SND_I2S_DOUT, SND_I2S_DIN,
        SND_SAMPLE_RATE, SND_ES8311_ADDR, CHIME_VOLUME, nullptr
    };
    chime_init(cfg);
}

void sound_hal_chime(void) { chime_play(); }
void sound_hal_tune(float gain, int volume) { chime_tune(gain, volume); }
void sound_hal_tick(void)       { chime_tick(); }

#endif  // BOARD_HAS_SOUND
