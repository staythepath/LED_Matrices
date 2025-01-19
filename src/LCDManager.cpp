#include "LCDManager.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>

// -----------------------------------------------------------------
// We define **I2C** pins for SDA/SCL here.
// Replace these with your actual wiring if needed
// or rely on hardware I2C defaults if your board does so automatically.
// -----------------------------------------------------------------
static const uint8_t OLED_I2C_SDA = 17;
static const uint8_t OLED_I2C_SCL = 16;

// Typically the SSD1309 or SSD1306 I2C address is 0x3C
static const uint8_t OLED_I2C_ADDR = 0x3C;

// -----------------------------------------------------------------
// Constructor
// We'll keep the old signature so we don't break main.cpp
// but we won't use (rs,e,d4,d5,d6,d7) except for logging
// -----------------------------------------------------------------
LCDManager::LCDManager(int rs, int e, int d4, int d5, int d6, int d7, int cols, int rows)
    : _rs(rs), _e(e), _d4(d4), _d5(d5), _d6(d6), _d7(d7)
    // We'll now do an I2C-based constructor:
    // e.g. U8G2_SSD1309_128X64_NONAME0_F_HW_I2C(rotation, [reset,] [opt. address])
    // If your board doesn't have custom i2c pins, we do hardware I2C:
    , _u8g2(U8G2_R0, U8X8_PIN_NONE) // hardware I2C, no explicit reset pin
{
    // ignoring (cols, rows) as well
}

// -----------------------------------------------------------------
// begin()
//   - Initialize the I2C-based U8G2 display
// -----------------------------------------------------------------
void LCDManager::begin() {
    Serial.print("Initializing I2C OLED (pretending pins: ");
    Serial.print("RS=");  Serial.print(_rs);
    Serial.print(", E="); Serial.print(_e);
    Serial.print(", D4=");Serial.print(_d4);
    Serial.print(", D5=");Serial.print(_d5);
    Serial.print(", D6=");Serial.print(_d6);
    Serial.print(", D7=");Serial.print(_d7);
    Serial.println(")");

    // If your board uses non-default i2c pins, you may need to do:
    //   Wire.begin(OLED_I2C_SDA, OLED_I2C_SCL);
    // or you can rely on the default hardware I2C pins for ESP32 (SDA=21, SCL=22).
    Wire.begin(OLED_I2C_SDA, OLED_I2C_SCL);

    // We can specify the i2c address if needed:
    _u8g2.setI2CAddress(OLED_I2C_ADDR << 1); // shift by 1 if required

    // Now init the display
    _u8g2.begin();

    // Show something for debugging
    _u8g2.clearBuffer();
    _u8g2.setFont(u8g2_font_6x12_tr);
    _u8g2.drawStr(0,12, "OLED (I2C) Init OK");
    _u8g2.sendBuffer();
}

// -----------------------------------------------------------------
// updateDisplay(): same logic you had for date/time/temps
// -----------------------------------------------------------------
void LCDManager::updateDisplay(int month, int mday, int wday,
                               int hour, int minute,
                               int temp_f, int hum)
{
    String dayAbbrev = _getDayAbbrev(wday);
    String timeStr   = _formatTime(hour, minute);

    // e.g. "12/8 Wed 5:01PM"
    String line1 = String(month) + "/" + String(mday) + " " 
                 + dayAbbrev + " " + timeStr;

    // e.g. "T:72F H:55%"
    String line2 = "T:" + String(temp_f) + "F H:" + String(hum) + "%";

    _u8g2.clearBuffer();
    _u8g2.setFont(u8g2_font_6x12_tr);

    _u8g2.setCursor(0, 12);
    _u8g2.print(line1);

    _u8g2.setCursor(0, 28);
    _u8g2.print(line2);

    _u8g2.sendBuffer();
}

// -----------------------------------------------------------------
// _getDayAbbrev(), _formatTime() as before
// -----------------------------------------------------------------
String LCDManager::_getDayAbbrev(int wday) {
    switch(wday) {
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
String LCDManager::_formatTime(int hour, int minute) {
    String ampm   = (hour < 12) ? "AM" : "PM";
    int hour12    = hour % 12;
    if(hour12==0) hour12=12;
    String timeStr= String(hour12) + ":" 
                  + (minute<10 ? "0" : "") + String(minute) + ampm;
    return timeStr;
}
