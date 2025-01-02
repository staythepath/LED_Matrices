// LEDManager.cpp
#include "LEDManager.h"
#include "config.h" // Include if needed
#include <Arduino.h>

// Define the leds array globally (only once)
CRGB leds[NUM_LEDS];

// Constructor: Initializes member variables
LEDManager::LEDManager()
    : _numLeds(NUM_LEDS),
      _brightness(DEFAULT_BRIGHTNESS),
      currentPalette(0),
      fadeValue(1400), // Initial fade value (controls tail length)
      spawnRate(0.6),
      maxFlakes(200),
      panelOrder(0), // 0 = Left first
      rotationAngle1(90),
      rotationAngle2(90),
      ledUpdateInterval(80), // Default: 80 ms
      lastLedUpdate(0)
{
    // Initialize palettes as per original code
    ALL_PALETTES = {
        // Palette 1: blu_orange_green
        { CRGB(0, 128, 255), CRGB(255, 128, 0), CRGB(0, 200, 60), CRGB(64, 0, 128), CRGB(255, 255, 64) },
        
        // Palette 2: cool_sunset
        { CRGB(255, 100, 0), CRGB(255, 0, 102), CRGB(128, 0, 128), CRGB(0, 255, 128), CRGB(255, 255, 128) },
        
        // Palette 3: neon_tropical
        { CRGB(0, 255, 255), CRGB(255, 0, 255), CRGB(255, 255, 0), CRGB(0, 255, 0), CRGB(255, 127, 0) },
        
        // Palette 4: galaxy
        { CRGB(0, 0, 128), CRGB(75, 0, 130), CRGB(128, 0, 128), CRGB(0, 128, 128), CRGB(255, 0, 128) },
        
        // Palette 5: forest_fire
        { CRGB(34, 139, 34), CRGB(255, 69, 0), CRGB(139, 0, 139), CRGB(205, 133, 63), CRGB(255, 215, 0) },
        
        // Palette 6: cotton_candy
        { CRGB(255, 182, 193), CRGB(152, 251, 152), CRGB(135, 206, 250), CRGB(238, 130, 238), CRGB(255, 160, 122) },
        
        // Palette 7: sea_shore
        { CRGB(0, 206, 209), CRGB(127, 255, 212), CRGB(240, 230, 140), CRGB(255, 160, 122), CRGB(173, 216, 230) },
        
        // Palette 8: fire_and_ice
        { CRGB(255, 0, 0), CRGB(255, 140, 0), CRGB(255, 69, 0), CRGB(0, 255, 255), CRGB(0, 128, 255) },
        
        // Palette 9: retro_arcade
        { CRGB(255, 0, 128), CRGB(128, 0, 255), CRGB(0, 255, 128), CRGB(255, 255, 0), CRGB(255, 128, 0) },
        
        // Palette 10: royal_rainbow
        { CRGB(139, 0, 0), CRGB(218, 165, 32), CRGB(255, 0, 255), CRGB(75, 0, 130), CRGB(0, 100, 140) },
        
        // Palette 11: red
        { CRGB(139, 0, 0), CRGB(139, 0, 0), CRGB(139, 0, 0), CRGB(139, 0, 0), CRGB(139, 0, 0) }
    };

    PALETTE_NAMES = {
        "blu_orange_green",
        "cool_sunset",
        "neon_tropical",
        "galaxy",
        "forest_fire",
        "cotton_candy",
        "sea_shore",
        "fire_and_ice",
        "retro_arcade",
        "royal_rainbow",
        "red"
    };
}

// Initialize LEDs
void LEDManager::begin(){
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, _numLeds).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(_brightness);
    FastLED.clear();
    FastLED.show();
}

// Update LEDs (e.g., perform effects)
void LEDManager::update(){
    unsigned long now = millis();
    if (now - lastLedUpdate >= ledUpdateInterval) {
        performSnowEffect();
        lastLedUpdate = now;
    }
}

// Brightness control
void LEDManager::setBrightness(uint8_t brightness){
    _brightness = brightness;
    FastLED.setBrightness(_brightness);
}

uint8_t LEDManager::getBrightness() const{
    return _brightness;
}

// Palette control
void LEDManager::setPalette(int paletteIndex){
    if(paletteIndex >= 0 && paletteIndex < PALETTE_NAMES.size()){
        currentPalette = paletteIndex;
        Serial.printf("Palette %d (%s) selected.\r\n", currentPalette +1, PALETTE_NAMES[currentPalette].c_str());
    }
}

int LEDManager::getCurrentPalette() const{
    return currentPalette;
}

const std::vector<CRGB>& LEDManager::getCurrentPaletteColors() const{
    return ALL_PALETTES[currentPalette];
}

// Tail and spawn rate control
void LEDManager::setFadeValue(int fadeVal){
    fadeValue = fadeVal;
}

int LEDManager::getFadeValue() const{
    return fadeValue;
}

void LEDManager::setSpawnRate(float rate){
    spawnRate = rate;
}

float LEDManager::getSpawnRate() const{
    return spawnRate;
}

void LEDManager::setMaxFlakes(int max){
    maxFlakes = max;
}

int LEDManager::getMaxFlakes() const{
    return maxFlakes;
}

// Panel order and rotation control
void LEDManager::swapPanels(){
    panelOrder = 1 - panelOrder; // Toggle between 0 and 1
    Serial.println("Panels swapped successfully.");
}

void LEDManager::setPanelOrder(String order){
    if(order.equalsIgnoreCase("left")){
        panelOrder = 0;
        Serial.println("Panel order set to left first.");
    }
    else if(order.equalsIgnoreCase("right")){
        panelOrder = 1;
        Serial.println("Panel order set to right first.");
    }
    else{
        Serial.println("Invalid panel order. Use 'left' or 'right'.");
    }
}

void LEDManager::rotatePanel(String panel, int angle){
    if(!(angle ==0 || angle ==90 || angle ==180 || angle ==270)){
        Serial.printf("Invalid rotation angle: %d\r\n", angle);
        return;
    }

    if(panel.equalsIgnoreCase("PANEL1")){
        rotationAngle1 = angle;
        Serial.printf("Rotation angle for Panel 1 set to %d degrees.\r\n", rotationAngle1);
    }
    else if(panel.equalsIgnoreCase("PANEL2")){
        rotationAngle2 = angle;
        Serial.printf("Rotation angle for Panel 2 set to %d degrees.\r\n", rotationAngle2);
    }
    else{
        Serial.println("Unknown panel identifier. Use PANEL1 or PANEL2.");
    }
}

int LEDManager::getRotation(String panel) const {
    if(panel.equalsIgnoreCase("PANEL1")){
        return rotationAngle1; // Return rotation angle for Panel 1
    }
    else if(panel.equalsIgnoreCase("PANEL2")){
        return rotationAngle2; // Return rotation angle for Panel 2
    }
    else{
        Serial.println("Unknown panel identifier. Use PANEL1 or PANEL2.");
        return -1; // Error code
    }
}


size_t LEDManager::getPaletteCount() const {
    return PALETTE_NAMES.size();
}

// Implement the getter for a specific palette name
String LEDManager::getPaletteNameAt(int index) const {
    if(index >= 0 && index < PALETTE_NAMES.size()){
        return PALETTE_NAMES[index];
    }
    return "Unknown";
}

// LED update speed control
void LEDManager::setUpdateSpeed(unsigned long speed){
    if(speed >=10 && speed <=60000){
        ledUpdateInterval = speed;
        Serial.printf("LED update speed set to %lu ms\r\n", ledUpdateInterval);
    }
    else{
        Serial.println("Invalid speed value. Enter a number between 10 and 60000.");
    }
}

unsigned long LEDManager::getUpdateSpeed() const{
    return ledUpdateInterval;
}

// Function to update the display (called from main loop)
void LEDManager::show(){
    FastLED.show();
}

// -------------------- Flake Functions --------------------

// Function to spawn a new flake
void LEDManager::spawnFlake(){
    Flake newFlake;
    
    // Decide spawn edge: 0=top, 1=bottom, 2=left, 3=right
    int edge = random(0,4);
    
    // Assign colors
    newFlake.startColor = ALL_PALETTES[currentPalette][random(0, ALL_PALETTES[currentPalette].size())];
    newFlake.endColor = ALL_PALETTES[currentPalette][random(0, ALL_PALETTES[currentPalette].size())];
    
    // Ensure endColor is different from startColor
    while(newFlake.endColor == newFlake.startColor){
        newFlake.endColor = ALL_PALETTES[currentPalette][random(0, ALL_PALETTES[currentPalette].size())];
    }
    
    newFlake.bounce = false; // Set to false unless you want flakes to bounce
    newFlake.frac = 0.0;
    
    // Initialize position and movement based on spawn edge
    switch(edge){
        case 0: // Top Edge
            newFlake.x = random(0,32);
            newFlake.y = 0;
            newFlake.dx = 0;
            newFlake.dy = 1;
            break;
        case 1: // Bottom Edge
            newFlake.x = random(0,32);
            newFlake.y = 15;
            newFlake.dx = 0;
            newFlake.dy = -1;
            break;
        case 2: // Left Edge
            newFlake.x = 0;
            newFlake.y = random(0,16);
            newFlake.dx = 1;
            newFlake.dy = 0;
            break;
        case 3: // Right Edge
            newFlake.x = 31;
            newFlake.y = random(0,16);
            newFlake.dx = -1;
            newFlake.dy = 0;
            break;
    }
    
    // Add to flakes vector
    flakes.push_back(newFlake);
    
    //Serial.printf("Spawned flake at (%d, %d) moving (%d, %d)\r\n", newFlake.x, newFlake.y, newFlake.dx, newFlake.dy);
}

// Function to perform the snow effect
void LEDManager::performSnowEffect(){
    // Fade all LEDs slightly to create a trailing effect
    fadeToBlackBy(leds, _numLeds, fadeValue); // Adjusted to use fadeValue
    
    // Spawn new flakes based on spawn chance and max flakes
    if(random(0, 1000) < (int)(spawnRate * 1000) && flakes.size() < maxFlakes){
        spawnFlake();
    }
    
    // Update existing flakes
    for(auto it = flakes.begin(); it != flakes.end(); ){
        // Update position
        it->x += it->dx;
        it->y += it->dy;
        it->frac += 0.02;
        
        // Remove flake if out of bounds
        if(it->x < 0 || it->x >=32 || it->y < 0 || it->y >=16){
            it = flakes.erase(it);
            continue;
        }
        
        // Clamp fraction
        if(it->frac >1.0){
            it->frac =1.0;
        }
        
        // Calculate main color
        CRGB mainColor = calcColor(it->frac, it->startColor, it->endColor, it->bounce);
        mainColor.nscale8(_brightness);
        
        // Set main flake color
        int ledIndex = getLedIndex(it->x, it->y);
        if(ledIndex >=0 && ledIndex < _numLeds){
            leds[ledIndex] += mainColor;
        }
        
        // Set tails based on fixed tailLength
        for(int t = 1; t <= fadeValue; t++){ // Fixed tailLength (e.g., 5 pixels)
            int tailX = it->x - t * it->dx;
            int tailY = it->y - t * it->dy;
            if(tailX >=0 && tailX <32 && tailY >=0 && tailY <16){
                int tailIndex = getLedIndex(tailX, tailY);
                if(tailIndex >=0 && tailIndex < _numLeds){
                    // Calculate brightness for the tail using linear scaling
                    float scale = 1.0 - ((float)t / (float)(fadeValue + 1));
                    uint8_t tailBrightness = (uint8_t)(_brightness * scale);

                    
                    // Ensure a minimum brightness for visibility
                    if(tailBrightness < 10) tailBrightness = 10;
                    
                    CRGB tailColor = mainColor;
                    tailColor.nscale8(tailBrightness);
                    leds[tailIndex] += tailColor; // Use additive blending
                    
                    // Serial.printf("Rendered tail pixel at (%d, %d) with brightness %d\r\n", tailX, tailY, tailBrightness);
                }
            }
        }
        
        ++it;
    }
}

// Function to calculate color based on fraction and bounce
CRGB LEDManager::calcColor(float frac, CRGB startColor, CRGB endColor, bool bounce){
    if(!bounce){
        return blend(startColor, endColor, frac * 255);
    }
    else{
        if(frac <= 0.5){
            return blend(startColor, endColor, frac * 2 * 255);
        }
        else{
            return blend(endColor, startColor, (frac - 0.5) * 2 * 255);
        }
    }
}

// Function to map (x,y) to LED index with rotation and panel order
int LEDManager::getLedIndex(int x, int y) const{
    // Determine which panel (0 or 1) based on x
    int panel = (x < 16) ? 0 : 1;
    
    // Determine local coordinates within the panel
    int localX = (x < 16) ? x : (x - 16);
    int localY = y;
    
    // Apply rotation based on panel
    rotateCoordinates(localX, localY, (panel == 0) ? rotationAngle1 : rotationAngle2);
    
    // Zigzag mapping: even rows left-to-right, odd rows right-to-left
    if(localY % 2 !=0){
        localX = 15 - localX;
    }
    
    // Calculate LED index based on panel order
    int ledIndex;
    if(panelOrder == 0){ // Left first
        ledIndex = panel * 256 + localY * 16 + localX;
    }
    else{ // Right first
        ledIndex = (1 - panel) * 256 + localY * 16 + localX;
    }
    
    // Ensure ledIndex is within bounds
    if(ledIndex < 0 || ledIndex >= _numLeds){
        Serial.printf("Invalid LED index calculated: %d\r\n", ledIndex);
        return -1;
    }
    
    return ledIndex;
}

// Function to rotate coordinates based on angle
void LEDManager::rotateCoordinates(int &x, int &y, int angle) const{
    int tempX, tempY;
    switch(angle){
        case 0:
            // No rotation
            break;
        case 90:
            tempX = y;
            tempY = 15 - x;
            x = tempX;
            y = tempY;
            break;
        case 180:
            tempX = 15 - x;
            tempY = 15 - y;
            x = tempX;
            y = tempY;
            break;
        case 270:
            tempX = 15 - y;
            tempY = x;
            x = tempX;
            y = tempY;
            break;
        default:
            // Invalid angle, do nothing
            Serial.printf("Invalid rotation angle: %d\r\n", angle);
            break;
    }
}
