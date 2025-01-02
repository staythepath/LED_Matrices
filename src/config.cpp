#include "config.h"
#include "credentials.h" // Use macro definitions

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
const long  gmtOffset_sec = -18000; // Example: -5 hours for EST
const int   daylightOffset_sec = 3600; // 1-hour daylight saving time
