/**************************************************************
 * WebServerManager.cpp
 * 
 * Refactored to fix palette indexing (0-based) and
 * properly separate 'fade' vs. 'tail length' routes.
 **************************************************************/

#include "WebServerManager.h"
#include "config.h"       // e.g. WiFi credentials if needed
#include <Update.h>
#include <FS.h>           
#include <SPIFFS.h>       // For SPIFFS
#include "LEDManager.h"   // So we can call LEDManager methods

// If you have a global LEDManager instance in main.cpp:
//   LEDManager ledManager;
// Then do:
extern LEDManager ledManager;

// Constructor
WebServerManager::WebServerManager(int port)
    : _server(port) {}

// Begin the web server
void WebServerManager::begin() {
    Serial.println("Starting Web Server...");

    // Mount SPIFFS (format on fail = true)
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS!");
        return;
    }
    Serial.println("SPIFFS mounted successfully.");

    // Define routes
    setupRoutes();

    // Start server
    _server.begin();
    Serial.println("Web Server started.");
}

// Handle clients (AsyncWebServer manages this automatically)
void WebServerManager::handleClient() {
    // Nothing special needed here
}

// Optional page template method
String WebServerManager::createPageTemplate(const String& title, const String& content) {
    return "<!DOCTYPE html><html><head><title>" + title + 
           "</title></head><body>" + content + "</body></html>";
}

// Define HTTP routes
void WebServerManager::setupRoutes() {

    /****************************************************
     * Serve SPIFFS-based files
     ****************************************************/
    _server.serveStatic("/",       SPIFFS, "/").setDefaultFile("index.html");
    _server.serveStatic("/status", SPIFFS, "/status.html");
    _server.serveStatic("/update", SPIFFS, "/update.html");
    _server.serveStatic("/reboot", SPIFFS, "/reboot.html");
    _server.serveStatic("/updatefs", SPIFFS, "/updatefs.html");
    // The main UI for controlling LED parameters
    _server.serveStatic("/control", SPIFFS, "/control.html");

    /****************************************************
     * OTA Firmware Update
     ****************************************************/
    _server.on("/update", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            bool error = Update.hasError();
            request->send(200, "text/plain", error ? "FAIL" : "OK");
            ESP.restart();
        },
        [](AsyncWebServerRequest *request, const String &filename, size_t index,
           uint8_t *data, size_t len, bool final) 
        {
            if (!index) {
                Serial.printf("Firmware Update Start: %s\n", filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                    Update.printError(Serial);
                }
            }
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
            if (final) {
                if (Update.end(true)) {
                    Serial.printf("Firmware Update Success: %u bytes\n", index + len);
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );

    /****************************************************
     * OTA SPIFFS Update
     ****************************************************/
    _server.on("/updatefs", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            bool error = Update.hasError();
            request->send(200, "text/plain", error ? "FS Update Failed" : "FS Update OK");
            ESP.restart();
        },
        [](AsyncWebServerRequest *request, const String &filename, size_t index,
           uint8_t *data, size_t len, bool final) 
        {
            if (!index) {
                Serial.printf("SPIFFS Update Start: %s\n", filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
                    Update.printError(Serial);
                }
            }
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
            if (final) {
                if (Update.end(true)) {
                    Serial.printf("SPIFFS Update Success: %u bytes\n", index + len);
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );

    /****************************************************
     * RebootNow route
     ****************************************************/
    _server.on("/rebootNow", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Rebooting...");
        delay(500);
        ESP.restart();
    });

    /****************************************************
     * API endpoints
     ****************************************************/
    // 1) listPalettes => JSON array of palette names
    _server.on("/api/listPalettes", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "[";
        for (size_t i = 0; i < ledManager.getPaletteCount(); i++) {
            json += "\"" + ledManager.getPaletteNameAt(i) + "\"";
            if (i < ledManager.getPaletteCount() - 1) json += ",";
        }
        json += "]";
        request->send(200, "application/json", json);
    });

    // 2) listPaletteDetails => (unused for now, same as above)
    _server.on("/api/listPaletteDetails", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "[";
        for (size_t i = 0; i < ledManager.getPaletteCount(); i++) {
            json += "\"" + ledManager.getPaletteNameAt(i) + "\"";
            if (i < ledManager.getPaletteCount() - 1) json += ",";
        }
        json += "]";
        request->send(200, "application/json", json);
    });

    // 3) setPalette?val=XX => 0-based index
    // in WebServerManager.cpp
    _server.on("/api/setPalette", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->hasParam("val")){
            request->send(400, "text/plain", "Missing val param");
            return;
        }

        // Here's your snippet:
        int userValue = request->getParam("val")->value().toInt();
        // userValue is 1-based. So if user picks the second palette, userValue = 2.

        // Convert it to zero-based:
        int zeroIndex = userValue - 1;

        // Then check range:
        if (zeroIndex < 0 || zeroIndex >= (int)ledManager.getPaletteCount()) {
            // Out of range
            request->send(400, "text/plain",
                "Invalid palette. Must be 1.." + String(ledManager.getPaletteCount()));
            return;
        }

        // Now set the palette with zeroIndex
        ledManager.setPalette(zeroIndex);

        String paletteName = ledManager.getPaletteNameAt(zeroIndex);
        String msg = "Palette " + String(userValue) +
                    " (" + paletteName + ") selected.";
        request->send(200, "text/plain", msg);
        Serial.println(msg);
    });


    // 4) getPalette => returns { current: X, name: "..." }
    _server.on("/api/getPalette", HTTP_GET, [](AsyncWebServerRequest *request){
        int current = ledManager.getCurrentPalette(); // 0-based
        String name = ledManager.getPaletteNameAt(current);
        // for the UI, we might show them 1-based in the JSON if you want
        String json = "{\"current\":" + String(current) + ",\"name\":\"" + name + "\"}";
        request->send(200, "application/json", json);
    });

    // 5) setBrightness => param "val" (0..255)
    _server.on("/api/setBrightness", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int value = request->getParam("val")->value().toInt();
        if(value>=0 && value<=255){
            ledManager.setBrightness((uint8_t)value);
            String msg = "Brightness set to " + String(value);
            request->send(200, "text/plain", msg);
            Serial.println(msg);
        } else {
            request->send(400, "text/plain","Brightness must be 0..255");
        }
    });

    // 6) getBrightness
    _server.on("/api/getBrightness", HTTP_GET, [](AsyncWebServerRequest *request){
        int b = ledManager.getBrightness();
        request->send(200, "text/plain", String(b));
    });

    // 7) setTailLength => param "val" (1..30)
    _server.on("/api/setTailLength", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int tLen = request->getParam("val")->value().toInt();
        if(tLen<1 || tLen>30){
            request->send(400, "text/plain","Tail length must be 1..30");
            return;
        }
        ledManager.setTailLength(tLen);
        String msg = "Tail length set to " + String(tLen);
        request->send(200, "text/plain", msg);
        Serial.println(msg);
    });

    // 8) getTailLength
    _server.on("/api/getTailLength", HTTP_GET, [](AsyncWebServerRequest *request){
        int tailLen = ledManager.getTailLength();
        request->send(200, "text/plain", String(tailLen));
    });

    // 9) setFadeAmount => param "val" (0..255)
    _server.on("/api/setFadeAmount", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->hasParam("val")){
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int fadeVal = request->getParam("val")->value().toInt();
        if(fadeVal<0) fadeVal=0;
        if(fadeVal>255) fadeVal=255;
        ledManager.setFadeAmount((uint8_t)fadeVal);
        String msg = "Fade amount set to " + String(fadeVal);
        request->send(200, "text/plain", msg);
        Serial.println(msg);
    });

    // 10) getFadeAmount
    _server.on("/api/getFadeAmount", HTTP_GET, [](AsyncWebServerRequest *request){
        uint8_t f = ledManager.getFadeAmount();
        request->send(200, "text/plain", String(f));
    });

    // 11) setSpawnRate => param "val" (0..1 float)
    _server.on("/api/setSpawnRate", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->hasParam("val")){
            request->send(400, "text/plain","Missing val param");
            return;
        }
        float rate = request->getParam("val")->value().toFloat();
        if(rate<0.0f || rate>1.0f){
            request->send(400, "text/plain","Spawn rate must be 0..1");
            return;
        }
        ledManager.setSpawnRate(rate);
        String msg = "Spawn rate set to " + String(rate,2);
        request->send(200, "text/plain", msg);
        Serial.println(msg);
    });

    // 12) getSpawnRate
    _server.on("/api/getSpawnRate", HTTP_GET, [](AsyncWebServerRequest *request){
        float r = ledManager.getSpawnRate();
        request->send(200, "text/plain", String(r,2));
    });

    // 13) setMaxFlakes => param "val" (10..500)
    _server.on("/api/setMaxFlakes", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->hasParam("val")){
            request->send(400, "text/plain","Missing val param");
            return;
        }
        int maxF = request->getParam("val")->value().toInt();
        if(maxF<10 || maxF>500){
            request->send(400, "text/plain","Max flakes must be 10..500");
            return;
        }
        ledManager.setMaxFlakes(maxF);
        String msg = "Max flakes set to " + String(maxF);
        request->send(200, "text/plain", msg);
        Serial.println(msg);
    });

    // 14) getMaxFlakes
    _server.on("/api/getMaxFlakes", HTTP_GET, [](AsyncWebServerRequest *request){
        int mf = ledManager.getMaxFlakes();
        request->send(200, "text/plain", String(mf));
    });

    // 15) swapPanels
    _server.on("/api/swapPanels", HTTP_GET, [](AsyncWebServerRequest *request){
        ledManager.swapPanels();
        request->send(200, "text/plain", "Panels swapped successfully.");
    });

    // 16) setPanelOrder => param "val" (left|right)
    _server.on("/api/setPanelOrder", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->hasParam("val")){
            request->send(400, "text/plain","Missing val param");
            return;
        }
        String order = request->getParam("val")->value();
        if(order.equalsIgnoreCase("left") || order.equalsIgnoreCase("right")){
            ledManager.setPanelOrder(order);
            String msg = "Panel order set to " + order;
            request->send(200, "text/plain", msg);
            Serial.println(msg);
        } else {
            request->send(400,"text/plain","Invalid panel order (left or right).");
        }
    });

    // 17) rotatePanel1 => param "val" in {0,90,180,270}
    _server.on("/api/rotatePanel1", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->hasParam("val")){
            request->send(400,"text/plain","Missing val param");
            return;
        }
        int angle = request->getParam("val")->value().toInt();
        if(angle==0||angle==90||angle==180||angle==270){
            ledManager.rotatePanel("PANEL1", angle);
            String msg="Rotation angle for PANEL1 set to "+String(angle);
            request->send(200,"text/plain", msg);
            Serial.println(msg);
        } else {
            request->send(400,"text/plain","Valid angles: 0,90,180,270");
        }
    });

    // 18) rotatePanel2 => param "val" in {0,90,180,270}
    _server.on("/api/rotatePanel2", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->hasParam("val")){
            request->send(400,"text/plain","Missing val param");
            return;
        }
        int angle=request->getParam("val")->value().toInt();
        if(angle==0||angle==90||angle==180||angle==270){
            ledManager.rotatePanel("PANEL2", angle);
            String msg="Rotation angle for PANEL2 set to "+String(angle);
            request->send(200,"text/plain", msg);
            Serial.println(msg);
        } else {
            request->send(400,"text/plain","Valid angles: 0,90,180,270");
        }
    });

    // 19) getRotationPanel1
    _server.on("/api/getRotationPanel1", HTTP_GET, [](AsyncWebServerRequest *request){
        int ang=ledManager.getRotation("PANEL1");
        request->send(200, "text/plain", String(ang));
    });

    // 20) getRotationPanel2
    _server.on("/api/getRotationPanel2", HTTP_GET, [](AsyncWebServerRequest *request){
        int ang=ledManager.getRotation("PANEL2");
        request->send(200, "text/plain", String(ang));
    });

    // 21) setSpeed => param "val" (10..60000)
    _server.on("/api/setSpeed", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->hasParam("val")){
            request->send(400,"text/plain","Missing val param");
            return;
        }
        unsigned long speed=request->getParam("val")->value().toInt();
        if(speed>=10 && speed<=60000){
            ledManager.setUpdateSpeed(speed);
            String msg="LED update speed set to "+String(speed)+" ms";
            request->send(200,"text/plain",msg);
            Serial.println(msg);
        } else {
            request->send(400,"text/plain","Valid speed is 10..60000 ms");
        }
    });

    // 22) getSpeed
    _server.on("/api/getSpeed", HTTP_GET, [](AsyncWebServerRequest *request){
        unsigned long spd=ledManager.getUpdateSpeed();
        request->send(200,"text/plain",String(spd));
    });

    /****************************************************
     * 404 handler
     ****************************************************/
    _server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/html", "<h1>404: Not Found</h1>");
    });
}
