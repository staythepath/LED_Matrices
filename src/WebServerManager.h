#ifndef WEBSERVERMANAGER_H
#define WEBSERVERMANAGER_H

#include <ESPAsyncWebServer.h> // Async WebServer
#include <Arduino.h>

class WebServerManager {
public:
    // Constructor
    WebServerManager(int port);

    // Start the web server
    void begin();

    // Handle web server requests
    void handleClient();

private:
    AsyncWebServer _server; // Async WebServer instance
    AsyncWebSocket _logSocket{"/ws"};

    bool initSPIFFS();      // Initialize SPIFFS with error handling
    String createPageTemplate(const String& title, const String& content); // HTML template generator
    void setupWebSocket();
    void broadcastLogLine(const String& line);
};

#endif // WEBSERVERMANAGER_H
