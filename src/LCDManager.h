// LCDManager.h
#ifndef LCDMANAGER_H
#define LCDMANAGER_H

#include <LiquidCrystal.h>
#include <Arduino.h>

class LCDManager {
private:
    int _rs;  // RS pin
    int _e;   // Enable pin
    int _d4;  // Data pin 4
    int _d5;  // Data pin 5
    int _d6;  // Data pin 6
    int _d7;  // Data pin 7


public:
    // Constructor with default columns and rows
    LCDManager(int rs, int e, int d4, int d5, int d6, int d7, int cols=32, int rows=2);
    
    // Initialize the LCD
    void begin();
    
    // Update the LCD display with current data
    void updateDisplay(int month, int mday, int wday, int hour, int minute, int temp_f, int hum);
    
private:
    LiquidCrystal _lcd;
    
    // Helper functions
    String _getDayAbbrev(int wday);
    String _formatTime(int hour, int minute);
};

#endif // LCDMANAGER_H
