// WiFiManager.h
#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

class WiFiManager {
public:
    // Constructor
    WiFiManager(const char* ssid, const char* password, int otaLedPin);
    
    // Initialize WiFi and OTA
    void begin();
    
    // Handle OTA updates (call in loop)
    void handleOTA();
    
    // Get local time with a timeout
    bool getLocalTimeCustom(struct tm *info, uint32_t ms = 1000);
    
private:
    const char* _ssid;
    const char* _password;
    int _otaLedPin;
};

#endif // WIFIMANAGER_H
