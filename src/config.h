#ifndef CONFIG_H
#define CONFIG_H

// -------------------- LED Configuration --------------------
#define LED_PIN     21
#define NUM_LEDS    512
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB


// -------------------- DHT Sensor Configuration --------------------
#define DHTPIN      4
#define DHTTYPE     DHT11

// -------------------- LCD Configuration --------------------
// Declare LCD pins as extern variables
extern const int rs;
extern const int e;
extern const int d4;
extern const int d5;
extern const int d6;
extern const int d7;

// -------------------- Wi-Fi Credentials --------------------
// Use extern variables (values assigned in config.cpp)
extern const char* ssid;
extern const char* password;
extern const char* ntpServer;
extern const long  gmtOffset_sec;
extern const int   daylightOffset_sec;

#endif // CONFIG_H
