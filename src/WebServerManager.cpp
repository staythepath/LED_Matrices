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

// Format SPIFFS if mount fails
#define FORMAT_SPIFFS_IF_FAILED true

// If you have a global LEDManager instance in main.cpp:
//   LEDManager ledManager;
// then declare it as extern so we can use it here
extern LEDManager ledManager;

// Create a global mutex for thread-safe access to LEDManager
static SemaphoreHandle_t ledManagerMutex = NULL;

// Constructor
WebServerManager::WebServerManager(int port)
    : _server(port) {
    // Create mutex for thread-safe access to LEDManager
    ledManagerMutex = xSemaphoreCreateMutex();
    
    // Subscribe current thread to TWDT
    esp_task_wdt_add(NULL);
}

// Helper function to safely access LEDManager with improved timeout and retry logic
static bool acquireLEDManager(uint32_t timeout = 1000) { // Increased default timeout to 1000ms
    if (ledManagerMutex == NULL) return true; // No mutex, just proceed
    
    // Reset watchdog timer
    esp_task_wdt_reset();
    
    // Try to acquire mutex with timeout
    if (xSemaphoreTake(ledManagerMutex, pdMS_TO_TICKS(timeout)) == pdTRUE) {
        return true;
    }
    
    // If we reach here, initial acquisition failed - try one more time after yielding
    Serial.println("Warning: First mutex acquisition failed, retrying after yield");
    vTaskDelay(pdMS_TO_TICKS(50)); // Short delay to let other tasks run
    
    // Reset watchdog again before second attempt
    esp_task_wdt_reset();
    
    bool acquired = xSemaphoreTake(ledManagerMutex, pdMS_TO_TICKS(timeout)) == pdTRUE;
    if (!acquired) {
        Serial.println("Critical: Failed to acquire mutex after retry - possible deadlock");
        // Force release the mutex in extreme cases (might cause other issues but prevents permanent lock)
        xSemaphoreGive(ledManagerMutex);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return acquired;
}

static void releaseLEDManager() {
    if (ledManagerMutex != NULL) {
        xSemaphoreGive(ledManagerMutex);
        // Add small delay to prevent immediate reacquisition
        vTaskDelay(pdMS_TO_TICKS(5)); 
        
        // Reset watchdog after releasing mutex
        esp_task_wdt_reset();
    }
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

    // Reset watchdog before setting up routes
    esp_task_wdt_reset();
    
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
        // Reset watchdog timer at start of request
        esp_task_wdt_reset();
        
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
        
        // Reset watchdog before potentially long operation
        esp_task_wdt_reset();
        
        ledManager.setAnimation(animIndex);
        String msg = "Animation " + String(animIndex) + " ("
            + ledManager.getAnimationName(animIndex) + ") selected.";
        
        releaseLEDManager();
        
        // Reset watchdog after completing operation
        esp_task_wdt_reset();
        
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
