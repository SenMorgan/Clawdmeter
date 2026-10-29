#include "../../hal/sound_hal.h"
#include "board.h"
#include "expander.h"

#if BOARD_HAS_SOUND

#include <Arduino.h>
#include "../../chime.h"

// LCD-3.5-C: ES8311 codec and NS4150B speaker amplifier on I2S. The amplifier
// is enabled through TCA9554 EXIO7 and stays off between chimes.

// Main codec volume for the Waveshare LCD 3.5-C board.
#define CHIME_VOLUME 65

static void amp_enable(bool on) { expander_set_pa_enabled(on); }

void sound_hal_init(void) {
    // Let the AXP2101 ALDO1 rail settle before probing the codec.
    delay(100);
    const ChimeConfig cfg = {
        SND_I2S_MCLK, SND_I2S_BCLK, SND_I2S_WS, SND_I2S_DOUT, SND_I2S_DIN,
        SND_SAMPLE_RATE, SND_ES8311_ADDR, CHIME_VOLUME, amp_enable
    };
    chime_init(cfg);
}

void sound_hal_chime(void) { chime_play(); }
void sound_hal_tune(float gain, int volume) { chime_tune(gain, volume); }
void sound_hal_tick(void)       { chime_tick(); }

#endif  // BOARD_HAS_SOUND
