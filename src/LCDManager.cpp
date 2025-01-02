// LCDManager.cpp
#include "LCDManager.h"
#include "config.h" // Include if you need access to config definitions
#include <Arduino.h>

// Constructor: Initializes the LiquidCrystal object with the provided pins
// Constructor: Initializes the LiquidCrystal object and stores the pin numbers
LCDManager::LCDManager(int rs, int e, int d4, int d5, int d6, int d7, int cols, int rows)
    : _lcd(rs, e, d4, d5, d6, d7),  // Initialize the LCD with the provided pins
      _rs(rs), _e(e), _d4(d4), _d5(d5), _d6(d6), _d7(d7) { // Store pin values
}

// Initialize the LCD display
void LCDManager::begin() {
    // Debug: Print out the pin numbers being passed into the LCD
    Serial.print("Initializing LCD with pins: ");
    Serial.print("RS=");
    Serial.print(_rs); // Print the RS pin
    Serial.print(", E=");
    Serial.print(_e);  // Print the E pin
    Serial.print(", D4=");
    Serial.print(_d4); // Print the D4 pin
    Serial.print(", D5=");
    Serial.print(_d5); // Print the D5 pin
    Serial.print(", D6=");
    Serial.print(_d6); // Print the D6 pin
    Serial.print(", D7=");
    Serial.println(_d7); // Print the D7 pin

    // Initialize the LCD
    _lcd.begin(32, 2); // 32 columns, 2 rows
    _lcd.clear();      // Clear display
    _lcd.setCursor(0, 0);
    _lcd.print("Initializing...");
}



// Helper function to get day abbreviation
String LCDManager::_getDayAbbrev(int wday){
    switch(wday){
        case 0: return "Sun";
        case 1: return "Mon";
        case 2: return "Tue";
        case 3: return "Wed";
        case 4: return "Thu";
        case 5: return "Fri";
        case 6: return "Sat";
        default: return "N/A";
    }
}

// Helper function to format time in 12-hour format with AM/PM
String LCDManager::_formatTime(int hour, int minute){
    String ampm = (hour < 12) ? "AM" : "PM";
    int hour12 = hour % 12;
    if(hour12 == 0) hour12 = 12;
    String timeStr = String(hour12) + ":" + (minute < 10 ? "0" : "") + String(minute) + ampm;
    return timeStr;
}

// Update the LCD display with current date, time, temperature, and humidity
void LCDManager::updateDisplay(int month, int mday, int wday, int hour, int minute, int temp_f, int hum){
    String dayAbbrev = _getDayAbbrev(wday);
    String timeStr = _formatTime(hour, minute);

    // Format: MM/DD Day HH:MMAM/PM
    String line1 = String(month) + "/" + String(mday) + " " + dayAbbrev + " " + timeStr;
    _lcd.setCursor(0, 0);
    _lcd.print(line1);
    
    // Clear the rest of the first line by printing spaces
    if(line1.length() < 32){
        for(int i = line1.length(); i < 32; i++) {
            _lcd.print(" ");
        }
    }

    // Format: T:xxF H:xx%
    String line2 = "T:" + String(temp_f) + "F H:" + String(hum) + "%";
    _lcd.setCursor(0, 1);
    _lcd.print(line2);
    
    // Clear the rest of the second line by printing spaces
    if(line2.length() < 32){
        for(int i = line2.length(); i < 32; i++) {
            _lcd.print(" ");
        }
    }
}
