#ifndef CONFIG_H
#define CONFIG_H

// -------------------- LED Configuration --------------------
#define LED_PIN     21
#define NUM_LEDS    768
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define DEFAULT_BRIGHTNESS 32 // Initial brightness (0-255)

// -------------------- DHT Sensor Configuration --------------------
#define DHTPIN      47
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
