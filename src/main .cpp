// main.cpp
#include <WiFi.h>
#include <FastLED.h>
#include <LiquidCrystal.h>
#include <ArduinoOTA.h>
#include <time.h>

// Include your classes
#include "config.h"
#include "SensorManager.h"
#include "LEDManager.h"
#include "LCDManager.h"
#include "TelnetManager.h"
#include "WebServerManager.h"

// [REMOVE or comment out these if you are using them in LEDManager.h]
// #define LED_PIN     21
// #define NUM_LEDS    786
// #define LED_TYPE    WS2812B
// #define COLOR_ORDER GRB
// #define DEFAULT_BRIGHTNESS 30

// Global objects
LEDManager ledManager;
SensorManager sensorManager(DHTPIN, DHTTYPE);
TelnetManager telnetManager(23, &ledManager);
LCDManager lcdManager(rs, e, d4, d5, d6, d7);
WebServerManager webServerManager(80);

void setup() {
    Serial.begin(115200);
    delay(1000);

    ledManager.begin();
    lcdManager.begin();
    sensorManager.begin();

    // WiFi, OTA, webServer, etc.
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Wi-Fi connected. IP: " + WiFi.localIP().toString());
    
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    webServerManager.begin();
    telnetManager.begin();
}

void loop() {
    telnetManager.handle();

    float temp_c, hum;
    if (!sensorManager.readSensor(temp_c, hum)) {
        Serial.println("Failed to read from DHT sensor!");
        temp_c = 0;
        hum = 0;
    }
    float temp_f = (temp_c * 9 / 5) + 32;

    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    lcdManager.updateDisplay(timeinfo.tm_mon + 1,
                             timeinfo.tm_mday,
                             timeinfo.tm_wday,
                             timeinfo.tm_hour,
                             timeinfo.tm_min,
                             (int)temp_f,
                             (int)hum);

    ledManager.update();
    FastLED.show();
}
