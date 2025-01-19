#ifndef LCDMANAGER_H
#define LCDMANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>

/**
 * A drop-in replacement for the old "LiquidCrystal" manager,
 * now using U8G2 with **I2C** for an SSD1309 or similar 128×64 OLED.
 *
 * The main difference from your SPI approach is that we:
 *   - use "U8G2_SSD1309_128X64_NONAME0_F_HW_I2C"
 *   - call Wire.begin(SDA,SCL) in begin()
 */
class LCDManager {
private:
    int _rs;  
    int _e;   
    int _d4;  
    int _d5;  
    int _d6;  
    int _d7;

public:
    // Same constructor signature so main.cpp won't break
    LCDManager(int rs, int e, int d4, int d5, int d6, int d7, int cols = 32, int rows = 2);

    void begin();

    // For the normal date/time/temperature/humidity display
    void updateDisplay(int month, int mday, int wday, int hour, int minute, int temp_f, int hum);

    // If the menu needs direct access to the U8G2, we provide this:
    U8G2& getU8g2() {
        return _u8g2;
    }

private:
    // We'll do hardware I2C for SSD1309 128×64
    // or if it's an SSD1306, you can rename the constructor:
    // e.g. U8G2_SSD1306_128X64_NONAME_F_HW_I2C
    U8G2_SSD1309_128X64_NONAME0_F_HW_I2C _u8g2;

    // Helper functions
    String _getDayAbbrev(int wday);
    String _formatTime(int hour, int minute);
};

#endif // LCDMANAGER_H
