#include "LCDManager.h"
#include "config.h" // If you need global config or Wi-Fi creds
#include <Arduino.h>

// -----------------------------------------------------------------
// Define OLED pins here (like you did in Display.cpp).
// Adjust these to match your wiring on the ESP32.
// -----------------------------------------------------------------
static const uint8_t OLED_CS   = 15;
static const uint8_t OLED_DC   = 16;
static const uint8_t OLED_RST  = 17;
static const uint8_t OLED_MOSI = 12;
static const uint8_t OLED_SCLK = 18;

// -----------------------------------------------------------------
// Constructor
//  - We keep the old signature for compatibility,
//    but we ignore (rs,e,d4,d5,d6,d7) for the actual OLED wiring.
// -----------------------------------------------------------------
LCDManager::LCDManager(int rs, int e, int d4, int d5, int d6, int d7, int cols, int rows)
    : _rs(rs), _e(e), _d4(d4), _d5(d5), _d6(d6), _d7(d7)
    // Create a U8G2 object for a 4-wire SW SPI SSD1309 128×64 display
    , _u8g2(U8G2_R0, 
            OLED_SCLK,  // SCK pin
            OLED_MOSI,  // MOSI pin
            OLED_CS,    // CS pin
            OLED_DC,    // DC pin
            OLED_RST    // RESET pin
           )
{
    // The 'cols' and 'rows' aren’t strictly used by U8g2,
    // but we accept them to match the old constructor signature.
}

// -----------------------------------------------------------------
// begin() - Initialize the OLED hardware instead of an LCD
// -----------------------------------------------------------------
void LCDManager::begin() {
    // Print out the pins we got from constructor (for debug)
    // (We don't actually use them for the OLED, but let's be consistent)
    Serial.print("Initializing OLED (pretending pins: ");
    Serial.print("RS=");  Serial.print(_rs);
    Serial.print(", E="); Serial.print(_e);
    Serial.print(", D4=");Serial.print(_d4);
    Serial.print(", D5=");Serial.print(_d5);
    Serial.print(", D6=");Serial.print(_d6);
    Serial.print(", D7=");Serial.print(_d7);
    Serial.println(")");

    // Now init the U8G2 display
    _u8g2.begin();

    // Clear the screen and show something
    _u8g2.clearBuffer();
    _u8g2.setFont(u8g2_font_6x12_tr); // pick a simple font
    _u8g2.drawStr(0, 12, "Initializing...");
    _u8g2.sendBuffer();
}

// -----------------------------------------------------------------
// _getDayAbbrev(): same as old code
// -----------------------------------------------------------------
String LCDManager::_getDayAbbrev(int wday) {
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

// -----------------------------------------------------------------
// _formatTime(): same 12-hour format with AM/PM
// -----------------------------------------------------------------
String LCDManager::_formatTime(int hour, int minute) {
    String ampm = (hour < 12) ? "AM" : "PM";
    int hour12 = hour % 12;
    if(hour12 == 0) hour12 = 12;
    String timeStr = String(hour12) + ":" + (minute < 10 ? "0" : "") + String(minute) + ampm;
    return timeStr;
}

// -----------------------------------------------------------------
// updateDisplay(): replicate the 2-line approach from the old LCD
//   - We'll draw line1 at y=12, line2 at y=28 (just arbitrary).
// -----------------------------------------------------------------
void LCDManager::updateDisplay(int month, int mday, int wday,
                               int hour, int minute,
                               int temp_f, int hum) 
{
    // Build the first line (date/time)
    String dayAbbrev = _getDayAbbrev(wday);
    String timeStr = _formatTime(hour, minute);
    // e.g. "12/8 Wed 5:01PM"
    String line1 = String(month) + "/" + String(mday) + " " + dayAbbrev + " " + timeStr;

    // Build the second line (temp/hum)
    // e.g. "T:72F H:55%"
    String line2 = "T:" + String(temp_f) + "F H:" + String(hum) + "%";

    // Clear the internal buffer
    _u8g2.clearBuffer();
    _u8g2.setFont(u8g2_font_6x12_tr);

    // Draw the first line at (0,12)
    _u8g2.setCursor(0, 12);
    _u8g2.print(line1);

    // Draw the second line at (0,28)
    _u8g2.setCursor(0, 28);
    _u8g2.print(line2);

    // Send buffer to display
    _u8g2.sendBuffer();
}
