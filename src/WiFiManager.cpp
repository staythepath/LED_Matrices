// WiFiManager.cpp
#include "WiFiManager.h"
#include "config.h" // Include configuration header for OTA_LED_PIN
#include <Arduino.h>

// Constructor: Initializes member variables
WiFiManager::WiFiManager(const char* ssid, const char* password, int otaLedPin)
    : _ssid(ssid), _password(password), _otaLedPin(otaLedPin)
{
    // You can add additional initialization here if needed
}

// Initialize WiFi connection and set up OTA
void WiFiManager::begin(){
    // Initialize OTA LED
    //pinMode(_otaLedPin, OUTPUT);
    //digitalWrite(_otaLedPin, LOW); // LED off by default

    // Connect to Wi-Fi
    WiFi.begin(_ssid, _password);
    Serial.print("Connecting to Wi-Fi");
    
    // Wait until connected
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nWi-Fi connected.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Setup OTA
    //ArduinoOTA.setHostname("ESP32_LEDMatrix"); // Optional: Set a unique hostname

    // Optional: Set OTA Password
    // ArduinoOTA.setPassword("your_password");

    // ArduinoOTA
    //     .onStart([this]() { // Capture 'this' to access _otaLedPin
    //         digitalWrite(_otaLedPin, HIGH); // Turn on LED during OTA
    //         String type;
    //         if (ArduinoOTA.getCommand() == U_FLASH)
    //             type = "sketch";
    //         else // U_SPIFFS
    //             type = "filesystem";

    //         Serial.println("Start updating " + type);
    //     })
    //     .onEnd([this]() { // Capture 'this'
    //         digitalWrite(_otaLedPin, LOW); // Turn off LED after OTA
    //         Serial.println("\nEnd");
    //     })
    //     .onProgress([](unsigned int progress, unsigned int total) {
    //         Serial.printf("Progress: %u%%\r\n", (progress / (total / 100)));
    //     })
    //     .onError([this](ota_error_t error) { // Capture 'this'
    //         digitalWrite(_otaLedPin, HIGH); // Keep LED on to indicate error
    //         Serial.printf("Error[%u]: ", error);
    //         if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    //         else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    //         else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    //         else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    //         else if (error == OTA_END_ERROR) Serial.println("End Failed");
    //     });

    // ArduinoOTA.begin();
    // Serial.println("OTA Initialized");
}

// Handle OTA updates (should be called repeatedly in loop)
void WiFiManager::handleOTA(){
    ArduinoOTA.handle();
}

// Get local time with a timeout
bool WiFiManager::getLocalTimeCustom(struct tm *info, uint32_t ms){
    // Wait for time to be set
    uint32_t start = millis();
    while(!getLocalTime(info)){
        delay(100);
        if(millis() - start > ms){
            return false;
        }
    }
    return true;
}
