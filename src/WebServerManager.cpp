/**************************************************************
 * WebServerManager.cpp
 * 
 * Refactored to fix palette indexing (0-based) and
 * properly separate 'fade' vs. 'tail length' routes.
 * Added stability improvements to prevent ESP32 resets.
 **************************************************************/

#include "WebServerManager.h"
#include "config.h"       // e.g. WiFi credentials if needed
#include <Update.h>
#include <FS.h>           
#include <SPIFFS.h>       // For SPIFFS
#include <WiFi.h>
#include "LEDManager.h"   // So we can call LEDManager methods
#include "LogManager.h"   // Include LogManager for system logs
#include <vector>

// Format SPIFFS if mount fails
#define FORMAT_SPIFFS_IF_FAILED true

// If you have a global LEDManager instance in main.cpp:
//   LEDManager ledManager;
// then declare it as extern so we can use it here
extern LEDManager ledManager;

// Constructor
WebServerManager::WebServerManager(int port)
    : _server(port) {}

// Helper function to safely access LEDManager (shared with loop task)
static bool acquireLEDManager(uint32_t timeout = 250) {
    return ledManager.lock(timeout);
}

static void releaseLEDManager() {
    ledManager.unlock();
}

static String jsonEscape(const String& input) {
    String escaped;
    escaped.reserve(input.length() + 4);
    for (size_t i = 0; i < input.length(); ++i) {
        const char c = input.charAt(i);
        switch (c) {
            case '\"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (static_cast<uint8_t>(c) < 0x20) {
                    char buffer[7];
                    snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                    escaped += buffer;
                } else {
                    escaped += c;
                }
        }
    }
    return escaped;
}

// Initialize SPIFFS with better error handling
bool WebServerManager::initSPIFFS() {
    Serial.println("Initializing SPIFFS...");
    
    // Report free heap before SPIFFS init
    Serial.printf("Free heap before SPIFFS init: %d bytes\n", ESP.getFreeHeap());
    
    // Attempt to mount SPIFFS
    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
        Serial.println("ERROR: SPIFFS mount failed!");
        return false;
    }
    
    // Get SPIFFS info
    size_t total = SPIFFS.totalBytes();
    size_t used = SPIFFS.usedBytes();
    Serial.printf("SPIFFS initialized - Total: %u bytes, Used: %u bytes, Free: %u bytes\n", 
                 total, used, total - used);
    
    // Report free heap after SPIFFS init
    Serial.printf("Free heap after SPIFFS init: %d bytes\n", ESP.getFreeHeap());
    
    return true;
}

// Begin the web server
void WebServerManager::begin() {
    // Init SPIFFS first with proper error handling
    if (!initSPIFFS()) {
        Serial.println("CRITICAL: Failed to initialize SPIFFS, web server will be limited");
        // Continue anyway, but some features won't work
    }

    setupWebSocket();

    Serial.println("Starting Web Server...");

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
    // System logs page
    _server.serveStatic("/logs", SPIFFS, "/logs.html");

    /****************************************************
     * OTA Firmware Update
     ****************************************************/
    _server.on("/update", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            bool error = Update.hasError();
            request->send(200, "text/plain", error ? "FAIL" : "OK");
            Serial.println("Firmware update complete, restarting in 1 second...");
            delay(1000); 
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
            Serial.println("SPIFFS update complete, restarting in 1 second...");
            delay(1000);
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
        Serial.println("Manual reboot requested, restarting in 1 second...");
        delay(1000);
        ESP.restart();
    });

    /****************************************************
     * List Animations
     ****************************************************/
    _server.on("/api/listAnimations", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        String json = "{\"animations\":[";
        for(size_t i=0; i< ledManager.getAnimationCount(); i++){
            json += "\"" + ledManager.getAnimationName(i) + "\"";
            if(i < ledManager.getAnimationCount()-1){
                json += ",";
            }
        }
        json += "],\"current\":" + String(ledManager.getAnimation()) + "}";
        
        releaseLEDManager();
        request->send(200, "application/json", json);
    });


     /****************************************************
     * Set Animations
     ****************************************************/
    _server.on("/api/setAnimation", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        if(!request->hasParam("val")){
            releaseLEDManager();
            request->send(400, "text/plain", "Missing 'val' param");
            return;
        }
        int animIndex = request->getParam("val")->value().toInt();
        if(animIndex < 0 || animIndex >= (int)ledManager.getAnimationCount()){
            releaseLEDManager();
            request->send(400, "text/plain","Invalid animation index");
            return;
        }
        
        ledManager.setAnimation(animIndex);
        String msg = "Animation " + String(animIndex) + " ("
            + ledManager.getAnimationName(animIndex) + ") selected.";
        
        releaseLEDManager();

        request->send(200, "text/plain", msg);
        Serial.println(msg);
    });

    /****************************************************
     * Get Current Animation
     ****************************************************/
    _server.on("/api/getAnimation", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        int currentAnim = ledManager.getAnimation();
        
        releaseLEDManager();
        request->send(200, "text/plain", String(currentAnim));
    });

    /****************************************************
     * API endpoints
     ****************************************************/
    _server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        const bool connected = WiFi.isConnected();
        const String ssid = connected ? WiFi.SSID() : "";
        const int32_t rssi = connected ? WiFi.RSSI() : 0;
        const String ip = connected ? WiFi.localIP().toString() : "";
        const String subnet = connected ? WiFi.subnetMask().toString() : "";
        const String gateway = connected ? WiFi.gatewayIP().toString() : "";
        const uint32_t uptimeSeconds = millis() / 1000;

        String json = "{";
        json += "\"wifi\":{";
        json += "\"connected\":"; json += connected ? "true" : "false";
        json += ",\"ssid\":\"" + jsonEscape(ssid) + "\"";
        json += ",\"rssi\":" + String(rssi);
        json += "},";

        json += "\"network\":{";
        json += "\"ip\":\"" + ip + "\"";
        json += ",\"subnet\":\"" + subnet + "\"";
        json += ",\"gateway\":\"" + gateway + "\"";
        json += "},";

        json += "\"system\":{";
        json += "\"uptime\":" + String(uptimeSeconds);
        json += ",\"freeMemory\":" + String(ESP.getFreeHeap());
        json += ",\"version\":\"" + String(FIRMWARE_VERSION) + "\"";
        json += ",\"buildDate\":\"" + String(__DATE__) + " " + String(__TIME__) + "\"";
        json += "}";
        json += "}";

        request->send(200, "application/json", json);
    });

    // 1) listPalettes => JSON array of palette names
    _server.on("/api/listPalettes", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        String json = "{\"palettes\":[";
        for (size_t i = 0; i < ledManager.getPaletteCount(); i++) {
            json += "\"" + ledManager.getPaletteNameAt(i) + "\"";
            if (i < ledManager.getPaletteCount() - 1) json += ",";
        }
        json += "],\"current\":" + String(ledManager.getCurrentPalette()) + "}";
        
        releaseLEDManager();
        request->send(200, "application/json", json);
    });

    // 2) listPaletteDetails => (unused for now, same as above)
    _server.on("/api/listPaletteDetails", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        String json = "[";
        for (size_t i = 0; i < ledManager.getPaletteCount(); i++) {
            json += "\"" + ledManager.getPaletteNameAt(i) + "\"";
            if (i < ledManager.getPaletteCount() - 1) json += ",";
        }
        json += "]";
        
        releaseLEDManager();
        request->send(200, "application/json", json);
    });


    /****************************************************
    * setPalette => expects ?val=<0-based index>
    * Example: /api/setPalette?val=2
    ****************************************************/
    _server.on("/api/setPalette", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        // 1) Check for parameter
        if(!request->hasParam("val")){
            releaseLEDManager();
            request->send(400, "text/plain", "Missing 'val' parameter");
            return;
        }

        // 2) Convert param to an integer (already 0-based from the web UI)
        int zeroIndex = request->getParam("val")->value().toInt();

        // 3) Validate range
        int paletteCount = (int)ledManager.getPaletteCount();
        if(zeroIndex < 0 || zeroIndex >= paletteCount){
            releaseLEDManager();
            request->send(400, "text/plain",
                "Invalid palette index. Must be 0.." + String(paletteCount - 1));
            return;
        }

        // 4) Set the palette in LEDManager (0-based index)
        ledManager.setPalette(zeroIndex);

        // 5) Respond with success
        String paletteName = ledManager.getPaletteNameAt(zeroIndex);
        String msg = "Palette " + String(zeroIndex) + " (" + paletteName + ") selected.";
        
        releaseLEDManager();
        request->send(200, "text/plain", msg);
        Serial.println(msg);
    });

    _server.on("/api/getCustomPalette", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }

        String json = "{\"index\":";
        json += String(ledManager.getCustomPaletteIndex());
        json += ",\"colors\":[";

        auto colors = ledManager.getCustomPaletteHex();
        for (size_t i = 0; i < colors.size(); ++i) {
            json += "\"" + colors[i] + "\"";
            if (i + 1 < colors.size()) {
                json += ",";
            }
        }
        json += "]}";

        releaseLEDManager();
        request->send(200, "application/json", json);
    });

    _server.on("/api/setCustomPalette", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }

        std::vector<String> hexColors;
        int paramCount = request->params();
        for (int i = 0; i < paramCount; ++i) {
            const AsyncWebParameter* param = request->getParam(i);
            if (param && !param->isPost() && !param->isFile() && param->name().equalsIgnoreCase("color")) {
                String value = param->value();
                value.trim();
                if (!value.isEmpty()) {
                    hexColors.push_back(value);
                }
            }
        }

        if (hexColors.empty()) {
            releaseLEDManager();
            request->send(400, "text/plain", "Provide at least one 'color' parameter (#RRGGBB).");
            return;
        }

        if (!ledManager.setCustomPaletteHex(hexColors)) {
            releaseLEDManager();
            request->send(400, "text/plain", "Invalid colour list. Use hex values like #FF0088.");
            return;
        }

        bool applyImmediately = false;
        if (request->hasParam("select")) {
            applyImmediately = request->getParam("select")->value().toInt() != 0;
        }

        if (applyImmediately) {
            int idx = ledManager.getCustomPaletteIndex();
            ledManager.setPalette(idx);
        }

        String msg = "Custom palette updated with ";
        msg += String(hexColors.size());
        msg += " colours.";

        if (applyImmediately) {
            msg += " Custom palette selected.";
        }

        releaseLEDManager();
        request->send(200, "text/plain", msg);
        Serial.println(msg);
    });

    _server.on("/api/savePalette", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }

        if (!request->hasParam("name")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing 'name' parameter.");
            return;
        }

        String rawName = request->getParam("name")->value();
        rawName.trim();

        std::vector<String> hexColors;
        int paramCount = request->params();
        for (int i = 0; i < paramCount; ++i) {
            const AsyncWebParameter* param = request->getParam(i);
            if (param && !param->isPost() && !param->isFile() && param->name().equalsIgnoreCase("color")) {
                String value = param->value();
                value.trim();
                if (!value.isEmpty()) {
                    hexColors.push_back(value);
                }
            }
        }

        if (hexColors.empty()) {
            releaseLEDManager();
            request->send(400, "text/plain", "Provide at least one 'color' parameter (#RRGGBB).");
            return;
        }

        int newIndex = -1;
        bool saved = ledManager.addUserPalette(rawName, hexColors, newIndex);
        if (!saved || newIndex < 0) {
            releaseLEDManager();
            request->send(400, "text/plain", "Unable to save palette. Use a unique name and valid colours.");
            return;
        }

        String storedName = ledManager.getPaletteNameAt(newIndex);
        releaseLEDManager();

        String escapedName = storedName;
        escapedName.replace("\\", "\\\\");
        escapedName.replace("\"", "\\\"");

        String json = "{\"index\":" + String(newIndex) + ",\"name\":\"" + escapedName + "\"}";
        request->send(200, "application/json", json);
        Serial.printf("User palette '%s' saved at index %d\n", storedName.c_str(), newIndex);
    });



    // 4) getPalette => returns { current: X, name: "..." }
    _server.on("/api/getPalette", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        int current = ledManager.getCurrentPalette(); // 0-based
        String name = ledManager.getPaletteNameAt(current);
        // for the UI, we might show them 1-based in the JSON if you want
        String json = "{\"current\":" + String(current) + ",\"name\":\"" + name + "\"}";
        
        releaseLEDManager();
        request->send(200, "application/json", json);
    });

    // 5) setBrightness => param "val" (0..255)
    _server.on("/api/setBrightness", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int value = request->getParam("val")->value().toInt();
        if(value>=0 && value<=255){
            ledManager.setBrightness((uint8_t)value);
            String msg = "Brightness set to " + String(value);
            
            releaseLEDManager();
            request->send(200, "text/plain", msg);
            Serial.println(msg);
        } else {
            releaseLEDManager();
            request->send(400, "text/plain","Brightness must be 0..255");
        }
    });

    // 6) getBrightness
    _server.on("/api/getBrightness", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        int b = ledManager.getBrightness();
        
        releaseLEDManager();
        request->send(200, "text/plain", String(b));
    });

    // 7) setTailLength => param "val" (0..30)
    _server.on("/api/setTailLength", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int tLen = request->getParam("val")->value().toInt();
        if(tLen < 0 || tLen > 30){
            releaseLEDManager();
            request->send(400, "text/plain", "Tail length must be 0..30");
            return;
        }
        ledManager.setTailLength(tLen);
        String msg = "Tail length set to " + String(tLen);
        
        releaseLEDManager();
        request->send(200, "text/plain", msg);
        Serial.println(msg);
    });

    // 8) getTailLength
    _server.on("/api/getTailLength", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        int tailLen = ledManager.getTailLength();
        
        releaseLEDManager();
        request->send(200, "text/plain", String(tailLen));
    });

    // 9) setFadeAmount => param "val" (0..255)
    _server.on("/api/setFadeAmount", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        if(!request->hasParam("val")){
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int fadeVal = request->getParam("val")->value().toInt();
        if(fadeVal<0) fadeVal=0;
        if(fadeVal>255) fadeVal=255;
        ledManager.setFadeAmount((uint8_t)fadeVal);
        String msg = "Fade amount set to " + String(fadeVal);
        
        releaseLEDManager();
        request->send(200, "text/plain", msg);
        Serial.println(msg);
    });

    // 10) getFadeAmount
    _server.on("/api/getFadeAmount", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        uint8_t f = ledManager.getFadeAmount();
        
        releaseLEDManager();
        request->send(200, "text/plain", String(f));
    });

    // 11) setSpawnRate => param "val" (0..1 float)
    _server.on("/api/setSpawnRate", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        if(!request->hasParam("val")){
            releaseLEDManager();
            request->send(400, "text/plain","Missing val param");
            return;
        }
        float rate = request->getParam("val")->value().toFloat();
        if(rate<0.0f || rate>1.0f){
            releaseLEDManager();
            request->send(400, "text/plain","Spawn rate must be 0..1");
            return;
        }
        ledManager.setSpawnRate(rate);
        String msg = "Spawn rate set to " + String(rate,2);
        
        releaseLEDManager();
        request->send(200, "text/plain", msg);
        Serial.println(msg);
    });

    // 12) getSpawnRate
    _server.on("/api/getSpawnRate", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        float r = ledManager.getSpawnRate();
        
        releaseLEDManager();
        request->send(200, "text/plain", String(r,2));
    });

    // 13) setMaxFlakes => param "val" (1..500)
    _server.on("/api/setMaxFlakes", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        if(!request->hasParam("val")){
            releaseLEDManager();
            request->send(400, "text/plain","Missing val param");
            return;
        }
        int maxF = request->getParam("val")->value().toInt();
        if(maxF < 1 || maxF > 500){
            releaseLEDManager();
            request->send(400, "text/plain", "Max flakes must be 1..500");
            return;
        }
        ledManager.setMaxFlakes(maxF);
        String msg = "Max flakes set to " + String(maxF);
        
        releaseLEDManager();
        request->send(200, "text/plain", msg);
        Serial.println(msg);
    });

    // 14) getMaxFlakes
    _server.on("/api/getMaxFlakes", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        int mf = ledManager.getMaxFlakes();
        
        releaseLEDManager();
        request->send(200, "text/plain", String(mf));
    });

    // 15) swapPanels
    _server.on("/api/swapPanels", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        ledManager.swapPanels();
        
        releaseLEDManager();
        request->send(200, "text/plain", "Panels swapped successfully.");
    });

    // 16) setPanelOrder => param "val" (left|right)
    _server.on("/api/setPanelOrder", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        if(!request->hasParam("val")){
            releaseLEDManager();
            request->send(400, "text/plain","Missing val param");
            return;
        }
        String order = request->getParam("val")->value();
        if(order.equalsIgnoreCase("left") || order.equalsIgnoreCase("right")){
            ledManager.setPanelOrder(order);
            String msg = "Panel order set to " + order;
            
            releaseLEDManager();
            request->send(200, "text/plain", msg);
            Serial.println(msg);
        } else {
            releaseLEDManager();
            request->send(400,"text/plain","Invalid panel order (left or right).");
        }
    });

    // 17) rotatePanel1 => param "val" in {0,90,180,270}
    _server.on("/api/rotatePanel1", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        if(!request->hasParam("val")){
            releaseLEDManager();
            request->send(400,"text/plain","Missing val param");
            return;
        }
        int angle = request->getParam("val")->value().toInt();
        if(angle==0||angle==90||angle==180||angle==270){
            ledManager.rotatePanel("PANEL1", angle);
            String msg="Rotation angle for PANEL1 set to "+String(angle);
            
            releaseLEDManager();
            request->send(200,"text/plain", msg);
            Serial.println(msg);
        } else {
            releaseLEDManager();
            request->send(400,"text/plain","Valid angles: 0,90,180,270");
        }
    });

    // 18) rotatePanel2 => param "val" in {0,90,180,270}
    _server.on("/api/rotatePanel2", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        if(!request->hasParam("val")){
            releaseLEDManager();
            request->send(400,"text/plain","Missing val param");
            return;
        }
        int angle=request->getParam("val")->value().toInt();
        if(angle==0||angle==90||angle==180||angle==270){
            ledManager.rotatePanel("PANEL2", angle);
            String msg="Rotation angle for PANEL2 set to "+String(angle);
            
            releaseLEDManager();
            request->send(200,"text/plain", msg);
            Serial.println(msg);
        } else {
            releaseLEDManager();
            request->send(400,"text/plain","Valid angles: 0,90,180,270");
        }
    });

    // 18.5) rotatePanel3 => param "val" in {0,90,180,270}
    _server.on("/api/rotatePanel3", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        if(!request->hasParam("val")){
            releaseLEDManager();
            request->send(400,"text/plain","Missing val param");
            return;
        }
        int angle=request->getParam("val")->value().toInt();
        if(angle==0||angle==90||angle==180||angle==270){
            ledManager.rotatePanel("PANEL3", angle);
            String msg="Rotation angle for PANEL3 set to "+String(angle);
            
            releaseLEDManager();
            request->send(200,"text/plain", msg);
            Serial.println(msg);
        } else {
            releaseLEDManager();
            request->send(400,"text/plain","Valid angles: 0,90,180,270");
        }
    });

    // 19) setSpeed => param "val" in milliseconds
    _server.on("/api/setSpeed", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        if(!request->hasParam("val")){
            releaseLEDManager();
            request->send(400,"text/plain","Missing val param");
            return;
        }
        int speed = request->getParam("val")->value().toInt();
        if(speed>=0 && speed<=100){
            ledManager.setSpeed(speed);
            String msg = "Speed set to " + String(speed);
            
            releaseLEDManager();
            request->send(200,"text/plain", msg);
            Serial.println(msg);
        } else {
            releaseLEDManager();
            request->send(400,"text/plain","Speed must be 0..100");
        }
    });

    _server.on("/api/getSpeed", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        int speed = ledManager.getSpeed();
        
        releaseLEDManager();
        request->send(200,"text/plain", String(speed));
    });

    // 20) setPanelCount => param "val" (1..8)
    _server.on("/api/setPanelCount", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        if(!request->hasParam("val")){
            releaseLEDManager();
            request->send(400,"text/plain","Missing val param");
            return;
        }
        int count = request->getParam("val")->value().toInt();
        if(count < 1) count = 1;
        if(count > 8) count = 8;
        
        ledManager.setPanelCount(count);
        String msg = "Panel count set to " + String(count);
        
        releaseLEDManager();
        request->send(200,"text/plain", msg);
        Serial.println(msg);
    });

    // 21) getPanelCount => returns current panel count
    _server.on("/api/getPanelCount", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        int count = ledManager.getPanelCount();
        String json = "{\"panelCount\":" + String(count) + "}";
        
        releaseLEDManager();
        request->send(200,"application/json", json);
    });

    // Text ticker configuration endpoints
    _server.on("/api/text/getConfig", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        String json = "{";
        json += "\"mode\":" + String(ledManager.getTextScrollMode()) + ",";
        json += "\"speed\":" + String(ledManager.getTextScrollSpeed()) + ",";
        json += "\"direction\":" + String(ledManager.getTextScrollDirection()) + ",";
        json += "\"mirror\":" + String(ledManager.getTextMirrorGlyphs() ? 1 : 0) + ",";
        json += "\"compact\":" + String(ledManager.getTextCompactHeight() ? 1 : 0) + ",";
        json += "\"reverse\":" + String(ledManager.getTextReverseOrder() ? 1 : 0) + ",";
        json += "\"primary\":\"" + jsonEscape(ledManager.getTextScrollMessage(0)) + "\",";
        json += "\"left\":\"" + jsonEscape(ledManager.getTextScrollMessage(1)) + "\",";
        json += "\"right\":\"" + jsonEscape(ledManager.getTextScrollMessage(2)) + "\"";
        json += "}";
        releaseLEDManager();
        request->send(200, "application/json", json);
    });

    _server.on("/api/text/setMode", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        uint8_t mode = static_cast<uint8_t>(request->getParam("val")->value().toInt());
        ledManager.setTextScrollMode(mode);
        releaseLEDManager();
        request->send(200, "text/plain", "Text mode updated");
    });

    _server.on("/api/text/setSpeed", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int speed = request->getParam("val")->value().toInt();
        if (speed < 0) speed = 0;
        if (speed > 100) speed = 100;
        ledManager.setTextScrollSpeed(static_cast<uint8_t>(speed));
        releaseLEDManager();
        request->send(200, "text/plain", "Text speed updated");
    });

    _server.on("/api/text/setDirection", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        uint8_t direction = static_cast<uint8_t>(request->getParam("val")->value().toInt());
        ledManager.setTextScrollDirection(direction);
        releaseLEDManager();
        request->send(200, "text/plain", "Text direction updated");
    });

    _server.on("/api/text/setMirror", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        bool mirror = request->getParam("val")->value().toInt() != 0;
        ledManager.setTextMirrorGlyphs(mirror);
        releaseLEDManager();
        request->send(200, "text/plain", "Text glyph mirroring updated");
    });

    _server.on("/api/text/setCompact", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        bool compact = request->getParam("val")->value().toInt() != 0;
        ledManager.setTextCompactHeight(compact);
        releaseLEDManager();
        request->send(200, "text/plain", "Text compact height updated");
    });

    _server.on("/api/text/setOrder", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        bool reverse = request->getParam("val")->value().toInt() != 0;
        ledManager.setTextReverseOrder(reverse);
        releaseLEDManager();
        request->send(200, "text/plain", "Text order updated");
    });

    _server.on("/api/text/setMessage", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("slot") || !request->hasParam("value")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing slot or value param");
            return;
        }
        String slot = request->getParam("slot")->value();
        slot.toLowerCase();
        uint8_t slotIndex = 0;
        if (slot == "left") slotIndex = 1;
        else if (slot == "right") slotIndex = 2;
        ledManager.setTextScrollMessage(slotIndex, request->getParam("value")->value());
        releaseLEDManager();
        request->send(200, "text/plain", "Text message updated");
    });

    // 22) identifyPanels => flash each panel in sequence
    _server.on("/api/identifyPanels", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        ledManager.identifyPanels();
        
        releaseLEDManager();
        request->send(200,"text/plain", "Identifying panels...");
    });

    /****************************************************
     * System Logs API
     ****************************************************/
    // Get system logs
    _server.on("/api/getLogs", HTTP_GET, [](AsyncWebServerRequest *request) {
        String level = "info";
        if (request->hasParam("level")) {
            level = request->getParam("level")->value();
        }
        
        LogManager::LogLevel logLevel = LogManager::INFO;
        if (level == "debug") logLevel = LogManager::DEBUG;
        else if (level == "warning") logLevel = LogManager::WARNING;
        else if (level == "error") logLevel = LogManager::ERROR;
        else if (level == "critical") logLevel = LogManager::CRITICAL;
        
        // Don't acquire the LED manager for logs - this can cause deadlocks
        try {
            // Get logs directly - avoid mutex contention
            String logs = LogManager::getInstance().getLogs(logLevel);
            request->send(200, "text/plain", logs);
        } catch (...) {
            // Catch any exception to prevent server crashes
            request->send(200, "text/plain", "Error retrieving logs - see serial console");
            Serial.println("Error retrieving logs in /api/getLogs");
        }
    });
    
    // Clear system logs
    _server.on("/api/clearLogs", HTTP_GET, [](AsyncWebServerRequest *request) {
        try {
            LogManager::getInstance().clearLogs();
            request->send(200, "text/plain", "Logs cleared successfully");
        } catch (...) {
            request->send(200, "text/plain", "Error clearing logs - see serial console");
            Serial.println("Error clearing logs in /api/clearLogs");
        }
    });

    // 23) setColumnSkip => sets column skip value for Game of Life animation
    _server.on("/api/setColumnSkip", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        if (request->hasParam("val")) {
            int val = request->getParam("val")->value().toInt();
            // Ensure valid range
            if (val < 1) val = 1;
            if (val > 20) val = 20;
            
            // Set the column skip value on the LEDManager
            ledManager.setColumnSkip(val);
            
            String msg = "Sweep speed set to " + String(val);
            Serial.println(msg);
            LogManager::getInstance().debug(msg);
            
            releaseLEDManager();
            request->send(200, "text/plain", msg);
        } else {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val parameter");
        }
    });
    
    // 24) getColumnSkip => gets the current column skip value for Game of Life animation
    _server.on("/api/getColumnSkip", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        // Get the current column skip value
        int currentColumnSkip = ledManager.getColumnSkip();
        
        // Create a JSON response
        String jsonResponse = "{\"value\":"; 
        jsonResponse += String(currentColumnSkip);
        jsonResponse += "}";
        
        Serial.printf("Returning column skip value: %d\n", currentColumnSkip);
        
        releaseLEDManager();
        request->send(200, "application/json", jsonResponse);
    });

    _server.on("/api/gol/getHighlight", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }

        const uint8_t width = ledManager.getGoLHighlightWidth();
        const String color = ledManager.getGoLHighlightColorHex();

        releaseLEDManager();

        String json = "{\"width\":";
        json += String(width);
        json += ",\"color\":\"";
        json += jsonEscape(color);
        json += "\"}";

        request->send(200, "application/json", json);
    });

    _server.on("/api/gol/getConfig", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }

        String json = "{";
        json += "\"updateMode\":" + String(ledManager.getGoLUpdateMode());
        json += ",\"neighborMode\":" + String(ledManager.getGoLNeighborMode());
        json += ",\"wrapEdges\":" + String(ledManager.getGoLWrapEdges() ? "true" : "false");
        json += ",\"rule\":\"" + jsonEscape(ledManager.getGoLRules()) + "\"";
        json += ",\"clusterColorMode\":" + String(ledManager.getGoLClusterColorMode());
        json += ",\"seedDensity\":" + String(ledManager.getGoLSeedDensity());
        json += ",\"mutationChance\":" + String(ledManager.getGoLMutationChance());
        json += ",\"uniformBirths\":" + String(ledManager.getGoLUniformBirths() ? "true" : "false");
        json += ",\"birthColor\":\"" + jsonEscape(ledManager.getGoLBirthColorHex()) + "\"";
        json += "}";

        releaseLEDManager();
        request->send(200, "application/json", json);
    });

    _server.on("/api/gol/setHighlightWidth", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }

        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val parameter");
            return;
        }

        int width = request->getParam("val")->value().toInt();
        if (width < 0) width = 0;
        if (width > 8) width = 8;
        ledManager.setGoLHighlightWidth(static_cast<uint8_t>(width));
        releaseLEDManager();

        request->send(200, "text/plain", "Highlight width updated");
    });

    _server.on("/api/gol/setHighlightColor", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }

        if (!request->hasParam("hex")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing hex parameter");
            return;
        }

        const String hex = request->getParam("hex")->value();
        const bool ok = ledManager.setGoLHighlightColorHex(hex);
        releaseLEDManager();

        if (!ok) {
            request->send(400, "text/plain", "Invalid hex colour");
            return;
        }

        request->send(200, "text/plain", "Highlight colour updated");
    });

    _server.on("/api/gol/setUniformBirths", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }

        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val parameter");
            return;
        }

        bool enabled = request->getParam("val")->value().toInt() != 0;
        ledManager.setGoLUniformBirths(enabled);
        releaseLEDManager();

        request->send(200, "text/plain", String("Uniform birth colour ") + (enabled ? "enabled" : "disabled"));
    });

    _server.on("/api/gol/setBirthColor", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }

        if (!request->hasParam("hex")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing hex parameter");
            return;
        }

        const String hex = request->getParam("hex")->value();
        const bool ok = ledManager.setGoLBirthColorHex(hex);
        releaseLEDManager();

        if (!ok) {
            request->send(400, "text/plain", "Invalid hex colour");
            return;
        }

        request->send(200, "text/plain", "Uniform birth colour updated");
    });

    _server.on("/api/presets/list", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }

        auto names = ledManager.listPresets();
        releaseLEDManager();

        String json = "{\"presets\":[";
        for (size_t i = 0; i < names.size(); ++i) {
            json += "\"" + jsonEscape(names[i]) + "\"";
            if (i + 1 < names.size()) {
                json += ",";
            }
        }
        json += "]}";
        request->send(200, "application/json", json);
    });

    _server.on("/api/presets/save", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("name")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing name parameter");
            return;
        }
        String error;
        const String name = request->getParam("name")->value();
        const bool ok = ledManager.savePreset(name, error);
        releaseLEDManager();
        if (!ok) {
            request->send(400, "text/plain", error);
            return;
        }
        request->send(200, "text/plain", "Preset saved");
    });

    _server.on("/api/presets/load", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("name")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing name parameter");
            return;
        }
        String error;
        const String name = request->getParam("name")->value();
        const bool ok = ledManager.loadPreset(name, error);
        releaseLEDManager();
        if (!ok) {
            request->send(400, "text/plain", error);
            return;
        }
        request->send(200, "text/plain", "Preset loaded");
    });

    _server.on("/api/presets/delete", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("name")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing name parameter");
            return;
        }
        String error;
        const String name = request->getParam("name")->value();
        const bool ok = ledManager.deletePreset(name, error);
        releaseLEDManager();
        if (!ok) {
            request->send(400, "text/plain", error);
            return;
        }
        request->send(200, "text/plain", "Preset deleted");
    });

    _server.on("/api/panel/getConfig", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        const int orderValue = ledManager.getPanelOrder();
        const String orderString = (orderValue == 0) ? "left" : "right";
        const int rotation1 = ledManager.getRotation("panel1");
        const int rotation2 = ledManager.getRotation("panel2");
        const int rotation3 = ledManager.getRotation("panel3");
        releaseLEDManager();

        String json = "{";
        json += "\"order\":\"" + orderString + "\"";
        json += ",\"rotation1\":" + String(rotation1);
        json += ",\"rotation2\":" + String(rotation2);
        json += ",\"rotation3\":" + String(rotation3);
        json += "}";
        request->send(200, "application/json", json);
    });

    _server.on("/api/gol/setUpdateMode", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val parameter");
            return;
        }
        uint8_t mode = static_cast<uint8_t>(request->getParam("val")->value().toInt());
        ledManager.setGoLUpdateMode(mode);
        releaseLEDManager();
        request->send(200, "text/plain", "Update mode set");
    });

    _server.on("/api/gol/setNeighborMode", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val parameter");
            return;
        }
        uint8_t mode = static_cast<uint8_t>(request->getParam("val")->value().toInt());
        ledManager.setGoLNeighborMode(mode);
        releaseLEDManager();
        request->send(200, "text/plain", "Neighbor mode set");
    });

    _server.on("/api/gol/setWrapEdges", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        bool wrap = request->hasParam("val") ? (request->getParam("val")->value().toInt() != 0) : true;
        ledManager.setGoLWrapEdges(wrap);
        releaseLEDManager();
        request->send(200, "text/plain", String("Wrap edges ") + (wrap ? "enabled" : "disabled"));
    });

    _server.on("/api/gol/setRules", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val parameter");
            return;
        }
        String value = request->getParam("val")->value();
        bool ok = ledManager.setGoLRules(value);
        releaseLEDManager();
        if (!ok) {
            request->send(400, "text/plain", "Invalid rule string");
        } else {
            request->send(200, "text/plain", "Rule updated");
        }
    });

    _server.on("/api/gol/setClusterMode", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val parameter");
            return;
        }
        uint8_t mode = static_cast<uint8_t>(request->getParam("val")->value().toInt());
        ledManager.setGoLClusterColorMode(mode);
        releaseLEDManager();
        request->send(200, "text/plain", "Cluster colour mode updated");
    });

    _server.on("/api/gol/setSeedDensity", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val parameter");
            return;
        }
        int value = request->getParam("val")->value().toInt();
        if (value < 0) value = 0;
        if (value > 100) value = 100;
        ledManager.setGoLSeedDensity(static_cast<uint8_t>(value));
        releaseLEDManager();
        request->send(200, "text/plain", "Seed density updated");
    });

    _server.on("/api/gol/setMutation", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val parameter");
            return;
        }
        int value = request->getParam("val")->value().toInt();
        if (value < 0) value = 0;
        if (value > 100) value = 100;
        ledManager.setGoLMutationChance(static_cast<uint8_t>(value));
        releaseLEDManager();
        request->send(200, "text/plain", "Mutation chance updated");
    });

    _server.on("/api/automata/getConfig", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        const uint8_t mode = ledManager.getAutomataMode();
        const uint8_t primary = ledManager.getAutomataPrimary();
        const uint8_t secondary = ledManager.getAutomataSecondary();
        releaseLEDManager();

        String json = "{\"mode\":" + String(mode) +
                      ",\"primary\":" + String(primary) +
                      ",\"secondary\":" + String(secondary) + "}";
        request->send(200, "application/json", json);
    });

    _server.on("/api/automata/setMode", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val parameter");
            return;
        }
        int value = request->getParam("val")->value().toInt();
        if (value < 0) value = 0;
        if (value > 2) value = 2;
        ledManager.setAutomataMode(static_cast<uint8_t>(value));
        releaseLEDManager();
        request->send(200, "text/plain", "StrangeLoop mode updated");
    });

    _server.on("/api/automata/setPrimary", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val parameter");
            return;
        }
        int value = request->getParam("val")->value().toInt();
        if (value < 0) value = 0;
        if (value > 100) value = 100;
        ledManager.setAutomataPrimary(static_cast<uint8_t>(value));
        releaseLEDManager();
        request->send(200, "text/plain", "Automata primary control updated");
    });

    _server.on("/api/automata/setSecondary", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("val")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing val parameter");
            return;
        }
        int value = request->getParam("val")->value().toInt();
        if (value < 0) value = 0;
        if (value > 100) value = 100;
        ledManager.setAutomataSecondary(static_cast<uint8_t>(value));
        releaseLEDManager();
        request->send(200, "text/plain", "Automata secondary control updated");
    });

    _server.on("/api/automata/reset", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        ledManager.resetAutomataPattern();
        releaseLEDManager();
        request->send(200, "text/plain", "Automata pattern reseeded");
    });

    // 25) Reboot endpoint - restarts the ESP32
    _server.on("/api/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Send a response before restarting
        request->send(200, "text/plain", "Rebooting ESP32...");

        // Log the reboot request
        Serial.println("Reboot requested via web interface");
        LogManager::getInstance().info("System reboot initiated via web interface");
        
        // Schedule the restart to happen after response is sent
        // This ensures the client gets the response before reboot
        delay(500); // Small delay to ensure response is sent
        ESP.restart();
    });

    // Start server
    _server.begin();
    Serial.println("Web Server started on port 80.");
    
    // Now that everything is initialized, tell LEDManager to start the actual animation
    if (acquireLEDManager()) {
        ledManager.finishInitialization();
        releaseLEDManager();
    }
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

void WebServerManager::setupWebSocket() {
    _logSocket.onEvent([](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                          void* arg, uint8_t* data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            String snapshot = LogManager::getInstance().getLogs(LogManager::DEBUG);
            if (snapshot.length()) {
                client->text(snapshot);
            }
        } else if (type == WS_EVT_DATA) {
            AwsFrameInfo* info = static_cast<AwsFrameInfo*>(arg);
            if (info->final && info->opcode == WS_TEXT) {
                // Simple echo for keep-alive
                String payload;
                payload.reserve(len);
                for (size_t i = 0; i < len; ++i) {
                    payload += static_cast<char>(data[i]);
                }
                if (payload == "ping") {
                    client->text("pong");
                }
            }
        }
    });

    _server.addHandler(&_logSocket);

    LogManager::getInstance().setListener([this](const String& line) {
        broadcastLogLine(line);
    });
}

void WebServerManager::broadcastLogLine(const String& line) {
    if (_logSocket.count() == 0) {
        return;
    }
    _logSocket.textAll(line);
}

