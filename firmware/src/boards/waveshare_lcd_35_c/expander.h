#pragma once

// Initialize the TCA9554 and release the LCD reset line.
void expander_init(void);

// Control the NS4150B shutdown input on EXIO7.
void expander_set_pa_enabled(bool enabled);
