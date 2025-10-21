#include <WiFi.h>
#include <time.h>

#include "config.h"
#include "LEDManager.h"
#include "LogManager.h"
#include "TelnetManager.h"
#include "WebServerManager.h"

namespace {
constexpr uint8_t TELNET_PORT = 23;

void connectToWifi() {
    systemInfo("Connecting to WiFi: " + String(ssid));
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    uint8_t retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
        delay(500);
        Serial.print('.');
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        const String ip = WiFi.localIP().toString();
        systemInfo("Wi-Fi connected (IP: " + ip + ')');
        Serial.printf("\nWi-Fi connected! IP: %s\n", ip.c_str());
    } else {
        systemWarning("Wi-Fi connection failed, continuing offline");
        Serial.println("\nWiFi connection failed; continuing without network.");
        WiFi.begin(ssid, password); // keep attempting in background
    }
}
} // namespace

LEDManager ledManager;
TelnetManager telnetManager(TELNET_PORT, &ledManager);
WebServerManager webServerManager(80);

void setup() {
    Serial.begin(115200);
    delay(1000);

    systemInfo("System starting up...");
    systemInfo("Firmware Version: " + String(FIRMWARE_VERSION));
    systemInfo("ESP32 SDK version: " + String(ESP.getSdkVersion()));

    const uint32_t initialHeap = ESP.getFreeHeap();
    const uint32_t maxBlock    = ESP.getMaxAllocHeap();
    systemInfo("Initial free heap: " + String(initialHeap) + " bytes");
    systemInfo("Largest free block: " + String(maxBlock) + " bytes");
    Serial.printf("Initial free heap: %u bytes\n", initialHeap);
    Serial.printf("Largest free block: %u bytes\n", maxBlock);

    setCpuFrequencyMhz(240);
    systemInfo("CPU frequency set to 240MHz");

    connectToWifi();

    ledManager.begin();
    delay(100);

    if (WiFi.isConnected()) {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    }

    webServerManager.begin();
    telnetManager.begin();

    const uint32_t finalHeap = ESP.getFreeHeap();
    systemInfo("Setup complete - free heap: " + String(finalHeap) + " bytes");
    if (WiFi.isConnected()) {
        Serial.printf("Web UI available at: http://%s\n", WiFi.localIP().toString().c_str());
    }
}

void loop() {
    telnetManager.handle();
    ledManager.update();
    ledManager.show();
}
