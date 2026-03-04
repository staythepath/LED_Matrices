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
#include "LEDManager.h"   // So we can call LEDManager methods
#include "esp_task_wdt.h" // Include ESP32 watchdog timer control
#include "LogManager.h"   // Include LogManager for system logs
#include <ESPmDNS.h>       // mDNS for hostname resolution
#include <WiFi.h>
#include <stdio.h>

static bool g_spiffsMounted = false;

// If you have a global LEDManager instance in main.cpp:
//   LEDManager ledManager;
// then declare it as extern so we can use it here
extern LEDManager ledManager;

static bool acquireLEDManager(uint32_t timeout = 1000) {
    return ledManager.beginExclusiveAccess(timeout);
}

static void releaseLEDManager() {
    ledManager.endExclusiveAccess();
}

static bool ensureSpiffsMounted() {
    if (g_spiffsMounted) {
        return true;
    }
    g_spiffsMounted = SPIFFS.begin(false);
    return g_spiffsMounted;
}

static String readApiTokenHeader(AsyncWebServerRequest* request) {
    if (request->hasHeader("X-API-Key")) {
        return request->getHeader("X-API-Key")->value();
    }
    if (request->hasHeader("Authorization")) {
        String auth = request->getHeader("Authorization")->value();
        if (auth.startsWith("Bearer ")) {
            return auth.substring(7);
        }
    }
    return "";
}

static bool requireApiToken(AsyncWebServerRequest* request) {
    String provided = readApiTokenHeader(request);
    if (provided.length() > 0 && String(apiToken) == provided) {
        return true;
    }
    request->send(401, "text/plain", "Unauthorized: missing or invalid API token");
    return false;
}

static bool isValidIPv4(const String& value) {
    int a, b, c, d;
    char dot1, dot2, dot3;
    if (sscanf(value.c_str(), "%d%c%d%c%d%c%d", &a, &dot1, &b, &dot2, &c, &dot3, &d) != 7) {
        return false;
    }
    if (dot1 != '.' || dot2 != '.' || dot3 != '.') {
        return false;
    }
    return a >= 0 && a <= 255 && b >= 0 && b <= 255 && c >= 0 && c <= 255 && d >= 0 && d <= 255;
}

// Constructor
WebServerManager::WebServerManager(int port)
    : _server(port) {
    // Subscribe current thread to TWDT
    esp_task_wdt_add(NULL);
}

// Initialize SPIFFS with better error handling
bool WebServerManager::initSPIFFS() {
    Serial.println("Initializing SPIFFS...");
    
    // Report free heap before SPIFFS init
    Serial.printf("Free heap before SPIFFS init: %d bytes\n", ESP.getFreeHeap());
    
    // Attempt to mount SPIFFS
    if (!ensureSpiffsMounted()) {
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

    // Reset watchdog before setting up routes
    esp_task_wdt_reset();
    
    Serial.println("Starting Web Server...");

    // Initialize mDNS so the device is reachable via esp32-led.local
    // Use a simple, consistent hostname; adjust if you want per-device uniqueness
    const char* mdnsHostname = "esp32-led";
    if (!MDNS.begin(mdnsHostname)) {
        Serial.println("mDNS start failed");
    } else {
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("telnet", "tcp", 23);
        Serial.printf("mDNS started: http://%s.local\n", mdnsHostname);
    }

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
            if (!requireApiToken(request)) {
                return;
            }
            bool error = Update.hasError();
            request->send(200, "text/plain", error ? "FAIL" : "OK");
            Serial.println("Firmware update complete, restarting in 1 second...");
            delay(1000); 
            ESP.restart();
        },
        [](AsyncWebServerRequest *request, const String &filename, size_t index,
           uint8_t *data, size_t len, bool final) 
        {
            if (!requireApiToken(request)) {
                return;
            }
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
            if (!requireApiToken(request)) {
                return;
            }
            bool error = Update.hasError();
            request->send(200, "text/plain", error ? "FS Update Failed" : "FS Update OK");
            Serial.println("SPIFFS update complete, restarting in 1 second...");
            delay(1000);
            ESP.restart();
        },
        [](AsyncWebServerRequest *request, const String &filename, size_t index,
           uint8_t *data, size_t len, bool final) 
        {
            if (!requireApiToken(request)) {
                return;
            }
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
    _server.on("/rebootNow", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        request->send(200, "text/plain", "Rebooting...");
        Serial.println("Manual reboot requested, restarting in 1 second...");
        delay(1000);
        ESP.restart();
    });

    /****************************************************
     * List Animations
     ****************************************************/
    _server.on("/api/listAnimations", HTTP_GET, [](AsyncWebServerRequest *request){
        size_t count = ledManager.getAnimationCount();
        String json = "{\"animations\":[";
        for(size_t i = 0; i < count; i++){
            json += "\"" + ledManager.getAnimationName(i) + "\"";
            if (i + 1 < count) {
                json += ",";
            }
        }
        json += "],\"current\":" + String(ledManager.getAnimation()) + "}";
        request->send(200, "application/json", json);
    });


     /****************************************************
     * Set Animations
     ****************************************************/
    _server.on("/api/setAnimation", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        // Reset watchdog timer at start of request
        esp_task_wdt_reset();
        
        if(!request->hasParam("val")){
            request->send(400, "text/plain", "Missing 'val' param");
            return;
        }
        int animIndex = request->getParam("val")->value().toInt();
        int totalAnimations = (int)ledManager.getAnimationCount();
        if(animIndex < 0 || animIndex >= totalAnimations){
            request->send(400, "text/plain","Invalid animation index");
            return;
        }
        
        // Reset watchdog before potentially long operation
        esp_task_wdt_reset();
        
        ledManager.setAnimation(animIndex);
        String msg = "Animation " + String(animIndex) + " ("
            + ledManager.getAnimationName(animIndex) + ") selected.";
        
        // Reset watchdog after completing operation
        esp_task_wdt_reset();
        
        request->send(200, "text/plain", msg);
        Serial.println(msg);
    });

    /****************************************************
     * Get Current Animation
     ****************************************************/
    _server.on("/api/getAnimation", HTTP_GET, [](AsyncWebServerRequest *request){
        int currentAnim = ledManager.getAnimation();
        request->send(200, "text/plain", String(currentAnim));
    });

    /****************************************************
     * API endpoints
     ****************************************************/
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
    _server.on("/api/setPalette", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
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
    _server.on("/api/setBrightness", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
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

    // 7) setTailLength => param "val" (1..30)
    _server.on("/api/setTailLength", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
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
        if(tLen<1 || tLen>30){
            releaseLEDManager();
            request->send(400, "text/plain","Tail length must be 1..30");
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
    _server.on("/api/setFadeAmount", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
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
    _server.on("/api/setSpawnRate", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
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

    // 13) setMaxFlakes => param "val" (10..500)
    _server.on("/api/setMaxFlakes", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
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
        if(maxF<10 || maxF>500){
            releaseLEDManager();
            request->send(400, "text/plain","Max flakes must be 10..500");
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
    _server.on("/api/swapPanels", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        ledManager.swapPanels();
        
        releaseLEDManager();
        request->send(200, "text/plain", "Panels swapped successfully.");
    });

    // 16) setPanelOrder => param "val" (left|right)
    _server.on("/api/setPanelOrder", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
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

    // 16.5) getPanelOrder
    _server.on("/api/getPanelOrder", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        int order = ledManager.getPanelOrder();
        String label = (order == 0) ? "left" : "right";
        releaseLEDManager();
        request->send(200, "text/plain", label);
    });

    // 17) rotatePanel1 => param "val" in {0,90,180,270}
    _server.on("/api/rotatePanel1", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
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
    _server.on("/api/rotatePanel2", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
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
    _server.on("/api/rotatePanel3", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
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

    // 18.8) getRotation => param "panel" (PANEL1/PANEL2/PANEL3)
    _server.on("/api/getRotation", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        if (!request->hasParam("panel")) {
            releaseLEDManager();
            request->send(400, "text/plain", "Missing panel param");
            return;
        }
        String panel = request->getParam("panel")->value();
        int angle = ledManager.getRotation(panel);
        if (angle < 0) {
            releaseLEDManager();
            request->send(400, "text/plain", "Invalid panel identifier");
            return;
        }
        releaseLEDManager();
        request->send(200, "text/plain", String(angle));
    });

    // 19) setSpeed => param "val" in milliseconds
    _server.on("/api/setSpeed", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
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
        if(speed < 3) speed = 3;  // Minimum 3ms
        if(speed > 1500) speed = 1500; // Max 1.5 seconds
        
        ledManager.setUpdateSpeed(speed);
        String msg = "Speed set to " + String(speed) + "ms";
        
        releaseLEDManager();
        request->send(200,"text/plain", msg);
        Serial.println(msg);
    });

    // 20) getSpeed => returns current speed in ms
    _server.on("/api/getSpeed", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!acquireLEDManager(500)) {
            request->send(503, "text/plain", "Server busy, try again later");
            return;
        }
        
        unsigned long speed = ledManager.getUpdateSpeed();
        
        releaseLEDManager();
        request->send(200,"text/plain", String(speed));
    });

    /****************************************************
     * Animation-specific settings
     ****************************************************/
    // Life-like rules list
    _server.on("/api/listLifeRules", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"rules\":[";
        size_t count = ledManager.getLifeRuleCount();
        for (size_t i = 0; i < count; i++) {
            json += "\"" + ledManager.getLifeRuleName(i) + "\"";
            if (i + 1 < count) json += ",";
        }
        json += "],\"current\":" + String(ledManager.getLifeRuleIndex()) + "}";
        request->send(200, "application/json", json);
    });

    _server.on("/api/setLifeRule", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int idx = request->getParam("val")->value().toInt();
        ledManager.setLifeRuleIndex(idx);
        request->send(200, "text/plain", "Life rule updated");
    });

    _server.on("/api/getLifeRule", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ledManager.getLifeRuleIndex()));
    });

    _server.on("/api/setLifeDensity", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int density = request->getParam("val")->value().toInt();
        if (density < 0) density = 0;
        if (density > 100) density = 100;
        ledManager.setLifeSeedDensity((uint8_t)density);
        request->send(200, "text/plain", "Life seed density updated");
    });

    _server.on("/api/getLifeDensity", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ledManager.getLifeSeedDensity()));
    });

    _server.on("/api/setLifeWrap", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        String val = request->getParam("val")->value();
        bool wrap = val.equalsIgnoreCase("1") || val.equalsIgnoreCase("true") || val.equalsIgnoreCase("on");
        ledManager.setLifeWrap(wrap);
        request->send(200, "text/plain", "Life wrap mode updated");
    });

    _server.on("/api/getLifeWrap", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", ledManager.getLifeWrap() ? "1" : "0");
    });

    _server.on("/api/setLifeStagnation", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int limit = request->getParam("val")->value().toInt();
        ledManager.setLifeStagnationLimit((uint16_t)limit);
        request->send(200, "text/plain", "Life stagnation limit updated");
    });

    _server.on("/api/getLifeStagnation", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ledManager.getLifeStagnationLimit()));
    });

    _server.on("/api/setLifeColorMode", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int mode = request->getParam("val")->value().toInt();
        ledManager.setLifeColorMode((uint8_t)mode);
        request->send(200, "text/plain", "Life color mode updated");
    });

    _server.on("/api/getLifeColorMode", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ledManager.getLifeColorMode()));
    });

    _server.on("/api/lifeReseed", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        ledManager.lifeReseed();
        request->send(200, "text/plain", "Life reseeded");
    });

    // Langton's Ant settings
    _server.on("/api/setAntRule", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        ledManager.setAntRule(request->getParam("val")->value());
        request->send(200, "text/plain", "Ant rule updated");
    });

    _server.on("/api/getAntRule", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", ledManager.getAntRule());
    });

    _server.on("/api/setAntCount", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int count = request->getParam("val")->value().toInt();
        ledManager.setAntCount((uint8_t)count);
        request->send(200, "text/plain", "Ant count updated");
    });

    _server.on("/api/getAntCount", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ledManager.getAntCount()));
    });

    _server.on("/api/setAntSteps", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int steps = request->getParam("val")->value().toInt();
        ledManager.setAntSteps((uint8_t)steps);
        request->send(200, "text/plain", "Ant steps updated");
    });

    _server.on("/api/getAntSteps", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ledManager.getAntSteps()));
    });

    _server.on("/api/setAntWrap", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        String val = request->getParam("val")->value();
        bool wrap = val.equalsIgnoreCase("1") || val.equalsIgnoreCase("true") || val.equalsIgnoreCase("on");
        ledManager.setAntWrap(wrap);
        request->send(200, "text/plain", "Ant wrap mode updated");
    });

    _server.on("/api/getAntWrap", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", ledManager.getAntWrap() ? "1" : "0");
    });

    // Sierpinski carpet settings
    _server.on("/api/setCarpetDepth", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int depth = request->getParam("val")->value().toInt();
        ledManager.setCarpetDepth((uint8_t)depth);
        request->send(200, "text/plain", "Carpet depth updated");
    });

    _server.on("/api/getCarpetDepth", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ledManager.getCarpetDepth()));
    });

    _server.on("/api/setCarpetInvert", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        String val = request->getParam("val")->value();
        bool invert = val.equalsIgnoreCase("1") || val.equalsIgnoreCase("true") || val.equalsIgnoreCase("on");
        ledManager.setCarpetInvert(invert);
        request->send(200, "text/plain", "Carpet invert updated");
    });

    _server.on("/api/getCarpetInvert", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", ledManager.getCarpetInvert() ? "1" : "0");
    });

    _server.on("/api/setCarpetShift", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int shift = request->getParam("val")->value().toInt();
        ledManager.setCarpetColorShift((uint8_t)shift);
        request->send(200, "text/plain", "Carpet color shift updated");
    });

    _server.on("/api/getCarpetShift", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ledManager.getCarpetColorShift()));
    });

    // Firework settings
    _server.on("/api/setFireworkMax", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int maxFw = request->getParam("val")->value().toInt();
        ledManager.setFireworkMax(maxFw);
        request->send(200, "text/plain", "Firework max updated");
    });

    _server.on("/api/getFireworkMax", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ledManager.getFireworkMax()));
    });

    _server.on("/api/setFireworkParticles", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int count = request->getParam("val")->value().toInt();
        ledManager.setFireworkParticles(count);
        request->send(200, "text/plain", "Firework particles updated");
    });

    _server.on("/api/getFireworkParticles", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ledManager.getFireworkParticles()));
    });

    _server.on("/api/setFireworkGravity", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        float gravity = request->getParam("val")->value().toFloat();
        ledManager.setFireworkGravity(gravity);
        request->send(200, "text/plain", "Firework gravity updated");
    });

    _server.on("/api/getFireworkGravity", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ledManager.getFireworkGravity(), 3));
    });

    _server.on("/api/setFireworkLaunch", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        float probability = request->getParam("val")->value().toFloat();
        ledManager.setFireworkLaunchProbability(probability);
        request->send(200, "text/plain", "Firework launch probability updated");
    });

    _server.on("/api/getFireworkLaunch", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ledManager.getFireworkLaunchProbability(), 2));
    });

    // Rainbow wave settings
    _server.on("/api/setRainbowHueScale", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if (!request->hasParam("val")) {
            request->send(400, "text/plain", "Missing val param");
            return;
        }
        int scale = request->getParam("val")->value().toInt();
        ledManager.setRainbowHueScale((uint8_t)scale);
        request->send(200, "text/plain", "Rainbow hue scale updated");
    });

    _server.on("/api/getRainbowHueScale", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ledManager.getRainbowHueScale()));
    });

    /****************************************************
     * Status endpoint (used by status.html)
     ****************************************************/
    _server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        bool connected = WiFi.status() == WL_CONNECTED;
        String ssid = connected ? WiFi.SSID() : String("");
        int rssi = connected ? WiFi.RSSI() : 0;

        IPAddress ip = WiFi.localIP();
        IPAddress subnet = WiFi.subnetMask();
        IPAddress gateway = WiFi.gatewayIP();

        unsigned long uptimeSeconds = millis() / 1000;
        unsigned long days = uptimeSeconds / 86400;
        unsigned long hours = (uptimeSeconds % 86400) / 3600;
        unsigned long minutes = (uptimeSeconds % 3600) / 60;
        unsigned long seconds = uptimeSeconds % 60;
        String uptimeStr = String(days) + "d " + String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";

        String json = "{";
        json += "\"wifi\":{\"connected\":" + String(connected ? "true" : "false");
        json += ",\"ssid\":\"" + ssid + "\",\"rssi\":" + String(rssi) + "},";
        json += "\"network\":{\"ip\":\"" + ip.toString() + "\",\"subnet\":\"" + subnet.toString() + "\",\"gateway\":\"" + gateway.toString() + "\"},";
        json += "\"system\":{\"uptime\":\"" + uptimeStr + "\",\"freeMemory\":" + String(ESP.getFreeHeap());
        json += ",\"version\":\"" + String(ESP.getSdkVersion()) + "\",\"buildDate\":\"" + String(__DATE__) + " " + String(__TIME__) + "\"}";
        json += "}";

        request->send(200, "application/json", json);
    });

    /****************************************************
     * Network settings
     ****************************************************/
    // Set network mode: dhcp or static
    _server.on("/api/setNetworkMode", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        if(!request->hasParam("val")){
            request->send(400, "text/plain", "Missing val param (dhcp|static)");
            return;
        }
        String mode = request->getParam("val")->value();
        if (!mode.equalsIgnoreCase("dhcp") && !mode.equalsIgnoreCase("static")) {
            request->send(400, "text/plain", "Invalid mode. Use dhcp or static");
            return;
        }
        if(!ensureSpiffsMounted()){
            request->send(500,"text/plain","SPIFFS mount failed");
            return;
        }
        // Read existing IP entries to preserve them
        String ip="192.168.2.38", gw="192.168.2.1", mask="255.255.255.0", dns="8.8.8.8";
        if(SPIFFS.exists("/net.cfg")){
            File fr = SPIFFS.open("/net.cfg","r");
            while(fr && fr.available()){
                String line = fr.readStringUntil('\n'); line.trim();
                int eq = line.indexOf('='); if(eq>0){
                    String k=line.substring(0,eq); String v=line.substring(eq+1); k.trim(); v.trim();
                    if(k.equalsIgnoreCase("ip")) ip=v;
                    else if(k.equalsIgnoreCase("gw")) gw=v;
                    else if(k.equalsIgnoreCase("mask")) mask=v;
                    else if(k.equalsIgnoreCase("dns")) dns=v;
                }
            }
            if(fr) fr.close();
        }
        File f = SPIFFS.open("/net.cfg","w");
        if(!f){ request->send(500,"text/plain","Failed to write net.cfg"); return; }
        f.printf("mode=%s\n", mode.equalsIgnoreCase("static")?"static":"dhcp");
        f.printf("ip=%s\n", ip.c_str());
        f.printf("gw=%s\n", gw.c_str());
        f.printf("mask=%s\n", mask.c_str());
        f.printf("dns=%s\n", dns.c_str());
        f.close();
        request->send(200, "text/plain", "Network mode updated. Reboot to apply.");
    });

    // Set static IP parameters
    _server.on("/api/setStaticIP", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
        auto getp=[&](const char* k)->String{ return request->hasParam(k)? request->getParam(k)->value():String(""); };
        String ip=getp("ip"), gw=getp("gw"), mask=getp("mask"), dns=getp("dns");
        if(ip==""||gw==""||mask==""||dns==""){
            request->send(400, "text/plain", "Missing params ip,gw,mask,dns");
            return;
        }
        if (!isValidIPv4(ip) || !isValidIPv4(gw) || !isValidIPv4(mask) || !isValidIPv4(dns)) {
            request->send(400, "text/plain", "Invalid IP format");
            return;
        }
        if(!ensureSpiffsMounted()){
            request->send(500,"text/plain","SPIFFS mount failed");
            return;
        }
        File f = SPIFFS.open("/net.cfg","w");
        if(!f){ request->send(500,"text/plain","Failed to write net.cfg"); return; }
        f.println("mode=static");
        f.printf("ip=%s\n", ip.c_str());
        f.printf("gw=%s\n", gw.c_str());
        f.printf("mask=%s\n", mask.c_str());
        f.printf("dns=%s\n", dns.c_str());
        f.close();
        request->send(200, "text/plain", "Static IP saved. Reboot to apply.");
    });

    // Get network config
    _server.on("/api/getNetwork", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!ensureSpiffsMounted()){
            request->send(500,"text/plain","SPIFFS mount failed");
            return;
        }
        String mode="dhcp", ip="", gw="", mask="", dns="";
        if(SPIFFS.exists("/net.cfg")){
            File fr = SPIFFS.open("/net.cfg","r");
            while(fr && fr.available()){
                String line = fr.readStringUntil('\n'); line.trim();
                int eq = line.indexOf('='); if(eq>0){
                    String k=line.substring(0,eq); String v=line.substring(eq+1); k.trim(); v.trim();
                    if(k.equalsIgnoreCase("mode")) mode=v;
                    else if(k.equalsIgnoreCase("ip")) ip=v;
                    else if(k.equalsIgnoreCase("gw")) gw=v;
                    else if(k.equalsIgnoreCase("mask")) mask=v;
                    else if(k.equalsIgnoreCase("dns")) dns=v;
                }
            }
            if(fr) fr.close();
        }
        String json = String("{\"mode\":\"")+mode+"\",\"ip\":\""+ip+"\",\"gw\":\""+gw+"\",\"mask\":\""+mask+"\",\"dns\":\""+dns+"\"}";
        request->send(200, "application/json", json);
    });

    // 21) setPanelCount => param "val" (1..8)
    _server.on("/api/setPanelCount", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
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

        if (ensureSpiffsMounted()) {
            File f = SPIFFS.open("/panel.cfg", "w");
            if (f) {
                f.printf("count=%d\n", count);
                f.close();
            }
        }

        String msg = "Panel count set to " + String(count);
        
        releaseLEDManager();
        request->send(200,"text/plain", msg);
        Serial.println(msg);
    });

    // 22) getPanelCount => returns current panel count
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

    // 23) identifyPanels => flash each panel in sequence
    _server.on("/api/identifyPanels", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!requireApiToken(request)) {
            return;
        }
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
    _server.on("/api/clearLogs", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!requireApiToken(request)) {
            return;
        }
        try {
            LogManager::getInstance().clearLogs();
            request->send(200, "text/plain", "Logs cleared successfully");
        } catch (...) {
            request->send(200, "text/plain", "Error clearing logs - see serial console");
            Serial.println("Error clearing logs in /api/clearLogs");
        }
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
