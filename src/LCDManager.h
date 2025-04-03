#ifndef LCDMANAGER_H
#define LCDMANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>

/**
 * A drop-in replacement for the old LiquidCrystal-based LCDManager,
 * but now using U8G2 with SSD1309.
 */
class LCDManager {
private:
    // We’ll keep these pins purely for compatibility/logging
    int _rs;  
    int _e;   
    int _d4;  
    int _d5;  
    int _d6;  
    int _d7;

public:
    // Same constructor signature as the old one, so main.cpp won't break
    LCDManager(int rs, int e, int d4, int d5, int d6, int d7, int cols = 32, int rows = 2);

    // Initialize the OLED (instead of an LCD)
    void begin();

    // Replicate the same "updateDisplay" logic: date/time/temperature/humidity
    void updateDisplay(int month, int mday, int wday, int hour, int minute, int temp_f, int hum);
    U8G2& getU8g2() {
        return _u8g2; 
    }

private:
    // The U8G2 object for SSD1309, 128×64, SW SPI
    // (You could also do hardware SPI or I2C if you prefer)
    U8G2_SSD1309_128X64_NONAME0_F_4W_SW_SPI _u8g2;

    // Helper functions to abbreviate days and format time
    String _getDayAbbrev(int wday);
    String _formatTime(int hour, int minute);
};

#endif // LCDMANAGER_H
