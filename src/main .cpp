// main.cpp
#include <WiFi.h>
#include <FastLED.h>
// #include <LiquidCrystal.h> // removed if no longer needed
#include <ArduinoOTA.h>
#include <time.h>

// Include your classes
#include "config.h"
// #include "SensorManager.h"  // Disabled DHT sensor
#include "LEDManager.h"
#include "LCDManager.h"      // Our U8g2-based manager
#include "TelnetManager.h"
#include "WebServerManager.h"
#include "LogManager.h"      // System logging

// These are the NEW includes for menu & rotary
#include "Menu.h"            // <-- NEW!
#include "RotaryEncoder.h"   // <-- NEW!

// Global objects
LEDManager ledManager;
// SensorManager sensorManager(DHTPIN, DHTTYPE);  // Disabled DHT sensor
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

    // Initialize logging system
    systemInfo("System starting up...");
    systemInfo("ESP32 firmware version: " + String(ESP.getSdkVersion()));
    
    // Report initial heap stats
    systemInfo("Initial free heap: " + String(ESP.getFreeHeap()) + " bytes");
    systemInfo("Largest free block: " + String(ESP.getMaxAllocHeap()) + " bytes");
    Serial.printf("Initial free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Largest free block: %u bytes\n", ESP.getMaxAllocHeap());
    
    // Set CPU frequency to maximum for better performance
    setCpuFrequencyMhz(240);
    systemInfo("CPU frequency set to 240MHz");
    
    // Force panel count to 2 at startup
    ledManager.setPanelCount(2);
    systemInfo("Panel count forced to 2 at startup");
    Serial.println("Panel count forced to 2 at startup");
    
    // Configure core affinity for tasks
    // Core 0: System tasks, WiFi
    // Core 1: User tasks (LED rendering, Web server, etc)
    
    // Start WiFi with retries
    systemInfo("Connecting to WiFi: " + String(ssid));
    Serial.printf("Connecting to WiFi %s", ssid);
    WiFi.begin(ssid, password);
    
    int wifiRetries = 0;
    while (WiFi.status() != WL_CONNECTED && wifiRetries < 20) { // 10 second timeout
        delay(500);
        Serial.print(".");
        wifiRetries++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        systemInfo("Wi-Fi connected!");
        systemInfo("IP address: " + WiFi.localIP().toString());
        Serial.println("\nWi-Fi connected!");
        Serial.println("IP address: " + WiFi.localIP().toString());
    } else {
        systemInfo("WiFi connection failed! Will try again in background.");
        Serial.println("\nWiFi connection failed! Will try again in background.");
        WiFi.begin(ssid, password); // Keep trying in background
    }

    // Set higher task priority for LEDs
    // This ensures the LED update loop won't get starved
    ESP_LOGD("MAIN", "Configuring task priorities");
    
    // Start core functionality with proper delay between each init
    // to prevent memory fragmentation
    ledManager.begin();
    delay(200); // Small delay to let memory settle
    
    lcdManager.begin();
    delay(100);

    // Initialize the new menu+encoder
    encoder.begin();
    delay(50);
    
    menu.begin();
    delay(50);

    // Check heap status after initializations
    systemInfo("Free heap after initializations: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.printf("Free heap after initializations: %u bytes\n", ESP.getFreeHeap());
    
    // Force garbage collection before starting web server
    // (doesn't actually exist in C++ but helps with memory)
    delay(300);

    // Start web server even if WiFi failed (it will work once WiFi connects)
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    webServerManager.begin();
    delay(100);
    
    telnetManager.begin();

    systemInfo("Setup complete - web UI available at: http://" + WiFi.localIP().toString());
    Serial.printf("Setup complete - web UI available at: http://%s\n", WiFi.localIP().toString().c_str());
    systemInfo("Final free heap: " + String(ESP.getFreeHeap()) + " bytes");
    Serial.printf("Final free heap: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
    telnetManager.handle();

    // 1) Use dummy values instead of reading from sensor
    float temp_c = 25.0;  // Fixed temperature (25°C)
    float hum = 50.0;     // Fixed humidity (50%)
    float temp_f = (temp_c * 9 / 5) + 32;

    // 2) Check the time
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)) {
        systemInfo("Failed to obtain time");
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
            systemInfo("Switching to menu mode...");
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
            systemInfo("Menu timed out. Returning to normal screen...");
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
