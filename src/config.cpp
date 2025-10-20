#include "config.h"

// Use optional credentials.h if present; otherwise provide safe defaults
#if defined(__has_include)
#  if __has_include("credentials.h")
#    include "credentials.h"
#  else
#    define WIFI_SSID     "ESP32_LEDMatrix"
#    define WIFI_PASSWORD "changeme1234"
#  endif
#else
// Fallback for toolchains without __has_include
#  include "credentials.h"
#endif

// Define LCD pins here (values assigned once)
const int rs = 19;
const int e  = 23;
const int d4 = 18;
const int d5 = 17;
const int d6 = 16;
const int d7 = 15;

// Assign Wi-Fi credentials from macros in credentials.h
const char* ssid = WIFI_SSID;       // Use the macro
const char* password = WIFI_PASSWORD; // Use the macro
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600; // Example: -5 hours for EST
const int   daylightOffset_sec = 0; // 1-hour daylight saving time
