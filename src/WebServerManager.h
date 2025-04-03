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

    bool initSPIFFS();      // Initialize SPIFFS with error handling
    void setupRoutes();     // Function to define routes
    String createPageTemplate(const String& title, const String& content); // HTML template generator
};

#endif // WEBSERVERMANAGER_H
