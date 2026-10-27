# LCD-3.5-C — `waveshare_lcd_35_c`

Hardware notes for this board. Shared firmware rules are in `firmware/CLAUDE.md`.

Waveshare ESP32-S3-Touch-LCD-3.5-C (ST7796 320×480 over 4-wire SPI, FT6336 touch @ 0x38, AXP2101 PMU, QMI8658 IMU, ES8311 audio). Build env: `waveshare_lcd_35_c`.

- LCD reset is TCA9554 P1 at 0x20; `board_init()` releases it before display and touch initialization.
- Display pins are MOSI=1, SCLK=5, DC=3, CS/reset not connected, and backlight=6.
- The FT6336 has no INT or reset wire, so touch is polled at 0x38.
- PWR is the AXP2101 PKEY (short → brightness, hold → pair, same scheme as the AMOLED-2.16); BOOT (GPIO 0) is Space/PTT. No secondary button.
- QMI8658 drives the 180° flip: upright reads `ax` ≈ -1 g and upside down `ax` ≈ +1 g. The panel flips in hardware (ST7796 MADCTL), so the frame stays 320×480 and touch is mapped to the flipped frame.
- ES8311 uses MCLK/BCLK/WS/DOUT/DIN GPIOs 12/13/15/16/14. The chime codec volume is 70 (50 was too quiet on this speaker). The codec's power rail is unconfirmed; `power.cpp` enables all four AXP2101 ALDOs at 3.3V to bring it up.
