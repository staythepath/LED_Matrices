#include "WebServerManager.h"
#include "config.h"
#include <Update.h>

// Constructor
WebServerManager::WebServerManager(int port) : _server(port) {}

// Begin the web server
void WebServerManager::begin() {
    Serial.println("Starting Web Server...");

    // Define routes
    setupRoutes();

    // Start server
    _server.begin();
    Serial.println("Web Server started.");
}

// Handle clients (AsyncWebServer handles requests automatically)
void WebServerManager::handleClient() {
    // No explicit handling required; AsyncWebServer manages connections automatically.
}

// Generate a page template with navigation links
String WebServerManager::createPageTemplate(const String& title, const String& content) {
    String html = "<!DOCTYPE html><html lang='en'><head>";
    html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>" + title + "</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; padding: 0; }";
    html += "h1 { color: #333; }";
    html += "nav { margin-bottom: 20px; }";
    html += "nav a { margin-right: 10px; text-decoration: none; color: #007BFF; }";
    html += "nav a:hover { text-decoration: underline; }";
    html += "form { margin-top: 20px; }";
    html += "</style></head><body>";
    html += "<nav><a href='/'>Home</a> | <a href='/status'>Status</a> | <a href='/update'>OTA Update</a></nav>";
    html += "<h1>" + title + "</h1>";
    html += content;
    html += "</body></html>";
    return html;
}

// Define HTTP routes
// Define HTTP routes
// Define HTTP routes
void WebServerManager::setupRoutes() {
    // Serve root URL
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html",
            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<title>ESP32 Controller</title>"
            "<style>"
            "body { font-family: Arial, sans-serif; margin: 20px; padding: 0; background: #f4f4f9; color: #333; }"
            "h1 { color: #0056b3; }"
            "nav { margin-bottom: 20px; padding: 10px; background: #0056b3; color: white; }"
            "nav a { margin-right: 15px; text-decoration: none; color: white; font-weight: bold; }"
            "nav a:hover { text-decoration: underline; }"
            "footer { margin-top: 20px; text-align: center; font-size: 0.8em; color: #555; }"
            "</style>"
            "</head>"
            "<body>"
            "<nav>"
            "<a href='/'>Home</a>"
            "<a href='/status'>Status</a>"
            "<a href='/update'>Update Firmware</a>"
            "<a href='/reboot'>Reboot Device</a>"
            "</nav>"
            "<h1>ESP32 Controller</h1>"
            "<p>Welcome to the ESP32 Web Interface.</p>"
            "<ul>"
            "<li><a href='/status'>Check Device Status</a></li>"
            "<li><a href='/update'>Update Firmware</a></li>"
            "<li><a href='/reboot'>Reboot the Device</a></li>"
            "</ul>"
            "<footer>&copy; 2024 ESP32 Controller</footer>"
            "</body>"
            "</html>"
        );
    });

    // Status Page
    _server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        String message = "<!DOCTYPE html><html><head><title>Device Status</title><style>";
        message += "body { font-family: Arial, sans-serif; margin: 20px; padding: 0; background: #f4f4f9; color: #333; }";
        message += "h1 { color: #0056b3; } nav { margin-bottom: 20px; padding: 10px; background: #0056b3; color: white; }";
        message += "nav a { margin-right: 15px; text-decoration: none; color: white; font-weight: bold; }";
        message += "nav a:hover { text-decoration: underline; }";
        message += "</style></head><body>";
        message += "<nav><a href='/'>Home</a><a href='/status'>Status</a><a href='/update'>Update Firmware</a><a href='/reboot'>Reboot Device</a></nav>";
        message += "<h1>Device Status</h1>";
        message += "<p>Wi-Fi: Connected</p>";
        message += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
        message += "<footer>&copy; 2024 ESP32 Controller</footer>";
        message += "</body></html>";
        request->send(200, "text/html", message);
    });

    // OTA Update Page
    _server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html",
            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<title>Firmware Update</title>"
            "<style>"
            "body { font-family: Arial, sans-serif; margin: 20px; padding: 0; background: #f4f4f9; color: #333; }"
            "h1 { color: #0056b3; }"
            "nav { margin-bottom: 20px; padding: 10px; background: #0056b3; color: white; }"
            "nav a { margin-right: 15px; text-decoration: none; color: white; font-weight: bold; }"
            "nav a:hover { text-decoration: underline; }"
            "input[type='file'], input[type='submit'] { padding: 10px; margin: 10px 0; }"
            "progress { width: 100%; height: 20px; margin-top: 10px; }"
            "</style>"
            "</head>"
            "<body>"
            "<nav>"
            "<a href='/'>Home</a>"
            "<a href='/status'>Status</a>"
            "<a href='/update'>Update Firmware</a>"
            "<a href='/reboot'>Reboot Device</a>"
            "</nav>"
            "<h1>Firmware Update</h1>"
            "<form method='POST' enctype='multipart/form-data' action='/update' id='uploadForm'>"
            "<input type='file' name='update' id='fileInput'>"
            "<input type='submit' value='Upload Firmware'>"
            "</form>"
            "<progress id='progressBar' value='0' max='100'></progress>"
            "<p id='statusText'>Waiting for upload...</p>"
            "<script>"
            "document.getElementById('uploadForm').onsubmit = function(e) {"
            "  const fileInput = document.getElementById('fileInput');"
            "  const progressBar = document.getElementById('progressBar');"
            "  const statusText = document.getElementById('statusText');"
            "  if (!fileInput.files.length) { alert('Please select a file first!'); return false; }"
            "  const xhr = new XMLHttpRequest();"
            "  xhr.upload.onprogress = function(event) {"
            "    if (event.lengthComputable) {"
            "      const percent = Math.round((event.loaded / event.total) * 100);"
            "      progressBar.value = percent;"
            "      statusText.innerText = 'Uploading... ' + percent + '%';"
            "    }"
            "  };"
            "  xhr.onload = function() {"
            "    if (xhr.status === 200) {"
            "      statusText.innerText = 'Update Successful! Rebooting...';"
            "    } else {"
            "      statusText.innerText = 'Update Failed!';"
            "    }"
            "  };"
            "  xhr.onerror = function() { statusText.innerText = 'Error during upload.'; };"
            "  xhr.open('POST', '/update');"
            "  xhr.send(new FormData(document.getElementById('uploadForm')));"
            "  e.preventDefault();"
            "};"
            "</script>"
            "</body></html>"
        );
    });

    // OTA Update Process
    _server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
        ESP.restart();
    }, [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!index) {
            Serial.printf("Update Start: %s\n", filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                Update.printError(Serial);
            }
        }
        if (Update.write(data, len) != len) {
            Update.printError(Serial);
        }
        if (final) {
            if (Update.end(true)) {
                Serial.printf("Update Success: %u bytes\n", index + len);
            } else {
                Update.printError(Serial);
            }
        }
    });

    // Reboot Device
    _server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html",
            "<!DOCTYPE html><html><head><title>Reboot</title></head><body>"
            "<h1>Rebooting...</h1>"
            "<p>Device is restarting now.</p>"
            "</body></html>"
        );
        delay(500);
        ESP.restart();
    });

    // 404 handler
    _server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/html", "<h1>404: Not Found</h1>");
    });
}

