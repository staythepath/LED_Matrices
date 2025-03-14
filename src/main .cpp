// main.cpp
#include <WiFi.h>
#include <FastLED.h>
// #include <LiquidCrystal.h> // removed if no longer needed
#include <ArduinoOTA.h>
#include <time.h>

// Include your classes
#include "config.h"
#include "SensorManager.h"
#include "LEDManager.h"
#include "LCDManager.h"      // Our U8g2-based manager
#include "TelnetManager.h"
#include "WebServerManager.h"

// These are the NEW includes for menu & rotary
#include "Menu.h"            // <-- NEW!
#include "RotaryEncoder.h"   // <-- NEW!

// Global objects
LEDManager ledManager;
SensorManager sensorManager(DHTPIN, DHTTYPE);
TelnetManager telnetManager(23, &ledManager);

// Our U8G2-based "LCD" manager
LCDManager lcdManager(rs, e, d4, d5, d6, d7);
WebServerManager webServerManager(80);

// The new menu + rotary
Menu menu;               // <-- NEW
RotaryEncoder encoder;   // <-- NEW

// Are we in the menu, or on the normal screen?
static bool inMenu = false;                   // <-- NEW
static unsigned long lastActivity = 0;        // <-- NEW
static const unsigned long MENU_TIMEOUT_MS = 30000; // 30s <-- NEW

void setup() {
    Serial.begin(115200);
    delay(1000);

    ledManager.begin();
    lcdManager.begin();
    sensorManager.begin();

    // Initialize the new menu+encoder
    encoder.begin();      // <-- NEW
    menu.begin();         // <-- NEW

    // WiFi, OTA, webServer, etc.
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connected. IP: " + WiFi.localIP().toString());
    
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    webServerManager.begin();
    telnetManager.begin();
}

void loop() {
    telnetManager.handle();

    // 1) Read sensor
    float temp_c, hum;
    if (!sensorManager.readSensor(temp_c, hum)) {
        Serial.println("Failed to read from DHT sensor!");
        temp_c = 0;
        hum = 0;
    }
    float temp_f = (temp_c * 9 / 5) + 32;

    // 2) Check the time
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }

    // 3) Check if the user touched the rotary (movement/button)
    //    We'll do a small function for user activity detection:
    bool userActivity = false;

    // The RotaryEncoder class uses "update()" to detect new states
    encoder.update();

    // If the position changed OR the button was pressed => activity
    static int lastPos = 0;         // track old position
    int newPos = encoder.getPosition();
    if(newPos != lastPos) {
        userActivity = true;
        lastPos = newPos;
    }
    if(encoder.isButtonPressed()) {
        userActivity = true;
    }

    if(userActivity) {
        lastActivity = millis();
        // If we are on the normal screen, jump to menu mode
        if(!inMenu) {
            inMenu = true;
            Serial.println("Switching to menu mode...");
        }
    }

    // 4) If inMenu => update/draw the menu
    if(inMenu) {
        // The "Menu" logic: 
        //   menu.update() already called "encoder.update()" but we can call it again if needed.
        //   but let's keep it consistent with the approach above:
        // menu.update(encoder);

        // Actually, let's do this: we already called encoder.update() above, 
        // but the menu logic expects us to call "menu.update(encoder)" to handle the selection logic:
        menu.update(encoder);

        // Draw the menu on the same display as lcdManager uses
        menu.draw(lcdManager.getU8g2());

        // If no user input for 30s => exit menu
        if( (millis() - lastActivity) > MENU_TIMEOUT_MS ) {
            inMenu = false;
            Serial.println("Menu timed out. Returning to normal screen...");
        }

    } else {
        // If we're NOT in the menu, we show the normal date/time/hum screen
        // (like you had in your original loop)
        lcdManager.updateDisplay(
            timeinfo.tm_mon + 1,
            timeinfo.tm_mday,
            timeinfo.tm_wday,
            timeinfo.tm_hour,
            timeinfo.tm_min,
            (int)temp_f,
            (int)hum
        );
    }

    // 5) Update LEDs
    ledManager.update();
    FastLED.show();
}
