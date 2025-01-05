// main.cpp
#include <WiFi.h>
#include <FastLED.h>
#include <LiquidCrystal.h>
#include <ArduinoOTA.h>
#include <time.h>


// Include your class headers
#include "config.h"
#include "SensorManager.h"
#include "LEDManager.h"
#include "LCDManager.h"
#include "TelnetManager.h"
#include "WebServerManager.h"


// -------------------- Configuration --------------------



// LED Configuration
#define LED_PIN     21
#define NUM_LEDS    512
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define DEFAULT_BRIGHTNESS 30 // Initial brightness (0-255)

// DHT Sensor Configuration
#define DHTPIN      4
#define DHTTYPE     DHT11


// Optional: LED for OTA status
//#define OTA_LED_PIN 2 // Replace with your LED pin if different

// -------------------- Object Instantiation --------------------

// Initialize LEDManager
LEDManager ledManager;

// Initialize SensorManager
SensorManager sensorManager(DHTPIN, DHTTYPE);

// Initialize TelnetManager on port 23
TelnetManager telnetManager(23, &ledManager);

// Initialize LCDManager with pin configurations
LCDManager lcdManager(rs, e, d4, d5, d6, d7); // Adjust rows/cols if necessary
 
//Initialize the WebServer
WebServerManager webServerManager(80); // Use port 80


// -------------------- Setup Function --------------------
void setup() {
    pinMode(rs, OUTPUT);
    pinMode(e, OUTPUT);
    pinMode(d4, OUTPUT);
    pinMode(d5, OUTPUT);
    pinMode(d6, OUTPUT);
    pinMode(d7, OUTPUT);


    Serial.begin(115200);
    Serial.println("Starting ESP32 LED Matrix Controller...");
    delay(1000); // Allow time for Serial to initialize
    Serial.println("OK Next is fucking ledManager.begin()");
    // Initialize OTA LED (Optional)
    //pinMode(OTA_LED_PIN, OUTPUT);
    //digitalWrite(OTA_LED_PIN, LOW); // Off by default
    
    // Initialize LEDManager
    ledManager.begin();

    Serial.println("Next is fucking lcdManager.begin()");
    // Initialize LCD
    lcdManager.begin(); // Call LCD initialization
    Serial.println("LCD intialization complete.");

    
    // Initialize LCD (assuming LEDManager handles it, else include separately)
    // ledManager.initializeLCD(); // If you have such a method
    
    // Initialize DHT Sensor via SensorManager
    sensorManager.begin();
    
    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connected. IP address: " + WiFi.localIP().toString());
    
    // Sync time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    // Initialize OTA
    ArduinoOTA.setHostname("ESP32_LEDMatrix");
    
    webServerManager.begin(); // Start the web server

    // Initialize Telnet Server
    telnetManager.begin();
}

// -------------------- Loop Function --------------------
void loop() {
    
    // Handle Telnet interactions
    telnetManager.handle();
    
    // Read sensor data
    // Read sensor data
    float temp_c, hum;
    if(!sensorManager.readSensor(temp_c, hum)){
        Serial.println("Failed to read from DHT sensor!");
        temp_c = 0; // Use default values to prevent crash
        hum = 0;
    }

    // Convert temperature to Fahrenheit
    float temp_f = (temp_c * 9 / 5) + 32;

    // Get time information
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        return;
    }

    // Update LCD Display
    lcdManager.updateDisplay(
        timeinfo.tm_mon + 1,    // Month (1-12)
        timeinfo.tm_mday,       // Day of the month
        timeinfo.tm_wday,       // Day of the week (0=Sun, 1=Mon)
        timeinfo.tm_hour,       // Hour (0-23)
        timeinfo.tm_min,        // Minute (0-59)
        (int)temp_f,            // Temperature in Fahrenheit
        (int)hum                // Humidity
    );

    
    // Update LCD and other components based on sensor data
    ledManager.update(); // Implement as needed
    

    
    // Ensure FastLED shows the updates
    FastLED.show();


    //Serial.println("Free heap: " + String(ESP.getFreeHeap()));

}
