#include "LEDManager.h"
#include "config.h"
#include <Arduino.h>

// Global CRGB array
CRGB leds[NUM_LEDS];

// Constructor
LEDManager::LEDManager()
    : _numLeds(NUM_LEDS),
      _brightness(DEFAULT_BRIGHTNESS),
      currentPalette(0),
      spawnRate(0.6f),
      maxFlakes(200),
      tailLength(5),   // example default
      fadeAmount(80),  // example default
      panelOrder(0),
      rotationAngle1(90),
      rotationAngle2(90),
      ledUpdateInterval(80),
      lastLedUpdate(0)
{
    // Initialize your palettes
    ALL_PALETTES = {
        // 1: blu_orange_green
        { CRGB(0,128,255), CRGB(255,128,0), CRGB(0,200,60), CRGB(64,0,128), CRGB(255,255,64) },
        // 2: cool_sunset
        { CRGB(255,100,0), CRGB(255,0,102), CRGB(128,0,128), CRGB(0,255,128), CRGB(255,255,128) },
        // 3: neon_tropical
        { CRGB(0,255,255), CRGB(255,0,255), CRGB(255,255,0), CRGB(0,255,0), CRGB(255,127,0) },
        // 4: galaxy
        { CRGB(0,0,128), CRGB(75,0,130), CRGB(128,0,128), CRGB(0,128,128), CRGB(255,0,128) },
        // 5: forest_fire
        { CRGB(34,139,34), CRGB(255,69,0), CRGB(139,0,139), CRGB(205,133,63), CRGB(255,215,0) },
        // 6: cotton_candy
        { CRGB(255,182,193), CRGB(152,251,152), CRGB(135,206,250), CRGB(238,130,238), CRGB(255,160,122) },
        // 7: sea_shore
        { CRGB(0,206,209), CRGB(127,255,212), CRGB(240,230,140), CRGB(255,160,122), CRGB(173,216,230) },
        // 8: fire_and_ice
        { CRGB(255,0,0), CRGB(255,140,0), CRGB(255,69,0), CRGB(0,255,255), CRGB(0,128,255) },
        // 9: retro_arcade
        { CRGB(255,0,128), CRGB(128,0,255), CRGB(0,255,128), CRGB(255,255,0), CRGB(255,128,0) },
        // 10: royal_rainbow
        { CRGB(139,0,0), CRGB(218,165,32), CRGB(255,0,255), CRGB(75,0,130), CRGB(0,100,140) },
        // 11: red
        { CRGB(139,0,0), CRGB(139,0,0), CRGB(139,0,0), CRGB(139,0,0), CRGB(139,0,0) }
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

void LEDManager::begin(){
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, _numLeds).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(_brightness);
    FastLED.clear(true);
}

void LEDManager::update(){
    unsigned long now = millis();
    if(now - lastLedUpdate >= ledUpdateInterval){
        performSnowEffect();
        lastLedUpdate = now;
    }
}

void LEDManager::show(){
    FastLED.show();
}

// -------------------- Brightness --------------------
void LEDManager::setBrightness(uint8_t brightness){
    _brightness = brightness;
    FastLED.setBrightness(_brightness); 
}
uint8_t LEDManager::getBrightness() const{
    return _brightness;
}

// -------------------- Palette --------------------
void LEDManager::setPalette(int paletteIndex){
    if(paletteIndex >= 0 && paletteIndex < (int)PALETTE_NAMES.size()){
        currentPalette = paletteIndex;
        Serial.printf("Palette %d (%s) selected.\n", currentPalette+1, PALETTE_NAMES[currentPalette].c_str());
    }
}
int LEDManager::getCurrentPalette() const{
    return currentPalette;
}
size_t LEDManager::getPaletteCount() const{
    return PALETTE_NAMES.size();
}
String LEDManager::getPaletteNameAt(int index) const{
    if(index>=0 && index<(int)PALETTE_NAMES.size()){
        return PALETTE_NAMES[index];
    }
    return "Unknown";
}
const std::vector<CRGB>& LEDManager::getCurrentPaletteColors() const{
    return ALL_PALETTES[currentPalette];
}

// -------------------- Spawn Rate / Max Flakes --------------------
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

// -------------------- Tail + Fade --------------------
void LEDManager::setTailLength(int length){
    tailLength = length;
}
int LEDManager::getTailLength() const{
    return tailLength;
}
void LEDManager::setFadeAmount(uint8_t amount){
    fadeAmount = amount;
}
uint8_t LEDManager::getFadeAmount() const{
    return fadeAmount;
}

// -------------------- Panel / Rotation --------------------
void LEDManager::swapPanels(){
    panelOrder = 1 - panelOrder;
    Serial.println("Panels swapped successfully.");
}
void LEDManager::setPanelOrder(String order){
    if(order.equalsIgnoreCase("left")){
        panelOrder=0;
        Serial.println("Panel order set to left first.");
    }
    else if(order.equalsIgnoreCase("right")){
        panelOrder=1;
        Serial.println("Panel order set to right first.");
    }
    else{
        Serial.println("Invalid panel order. Use 'left' or 'right'.");
    }
}
void LEDManager::rotatePanel(String panel, int angle){
    if(!(angle==0 || angle==90||angle==180||angle==270)){
        Serial.printf("Invalid rotation angle: %d\n", angle);
        return;
    }
    if(panel.equalsIgnoreCase("PANEL1")){
        rotationAngle1=angle;
        Serial.printf("Panel1 angle set to %d\n", rotationAngle1);
    }
    else if(panel.equalsIgnoreCase("PANEL2")){
        rotationAngle2=angle;
        Serial.printf("Panel2 angle set to %d\n", rotationAngle2);
    }
    else {
        Serial.println("Unknown panel: use PANEL1 or PANEL2.");
    }
}
int LEDManager::getRotation(String panel) const{
    if(panel.equalsIgnoreCase("PANEL1")){
        return rotationAngle1;
    }
    else if(panel.equalsIgnoreCase("PANEL2")){
        return rotationAngle2;
    }
    Serial.println("Unknown panel: use PANEL1 or PANEL2.");
    return -1;
}

// -------------------- Update Speed --------------------
void LEDManager::setUpdateSpeed(unsigned long speed){
    if(speed>=10 && speed<=60000){
        ledUpdateInterval = speed;
        Serial.printf("LED update speed set to %lu ms\n", ledUpdateInterval);
    }
    else{
        Serial.println("Invalid speed. Must be 10..60000.");
    }
}
unsigned long LEDManager::getUpdateSpeed() const{
    return ledUpdateInterval;
}

// -------------------- Flake Logic --------------------
void LEDManager::spawnFlake(){
    Flake f;
    int edge = random(0,4);

    // assign random start/end colors from current palette
    f.startColor = ALL_PALETTES[currentPalette][ random( ALL_PALETTES[currentPalette].size()) ];
    f.endColor   = ALL_PALETTES[currentPalette][ random( ALL_PALETTES[currentPalette].size()) ];
    while(f.endColor == f.startColor){
        f.endColor = ALL_PALETTES[currentPalette][ random(ALL_PALETTES[currentPalette].size()) ];
    }
    f.bounce = false;
    f.frac  = 0.0f;

    switch(edge){
        case 0: // top
            f.x = random(0,32);
            f.y = 0;
            f.dx=0; f.dy=1;
            break;
        case 1: // bottom
            f.x = random(0,32);
            f.y = 15;
            f.dx=0; f.dy=-1;
            break;
        case 2: // left
            f.x=0; 
            f.y=random(0,16);
            f.dx=1; f.dy=0;
            break;
        case 3: // right
            f.x=31;
            f.y=random(0,16);
            f.dx=-1; f.dy=0;
            break;
    }
    flakes.push_back(f);
}

void LEDManager::performSnowEffect(){
    // 1) fade entire matrix by fadeAmount
    fadeToBlackBy(leds, _numLeds, fadeAmount);

    // 2) chance to spawn new flake
    if(random(1000) < (int)(spawnRate*1000) && (int)flakes.size()<maxFlakes){
        spawnFlake();
    }

    // 3) update flakes
    for(auto it=flakes.begin(); it!=flakes.end(); ){
        it->x += it->dx;
        it->y += it->dy;
        it->frac += 0.02f;

        // out of bounds -> remove
        if(it->x<0 || it->x>=32 || it->y<0 || it->y>=16){
            it = flakes.erase(it);
            continue;
        }
        if(it->frac>1.0f) it->frac=1.0f;

        CRGB mainC = calcColor(it->frac, it->startColor, it->endColor, it->bounce);
        // scale by overall brightness
        mainC.nscale8(_brightness);

        int idx = getLedIndex(it->x, it->y);
        if(idx>=0 && idx<_numLeds){
            leds[idx] += mainC;
        }

        // 4) draw tail behind flake
        // using tailLength to decide how many pixels
        for(int t=1; t<=tailLength; t++){
            int tx = it->x - t*it->dx;
            int ty = it->y - t*it->dy;
            if(tx<0 || tx>=32|| ty<0|| ty>=16) break;

            int tidx = getLedIndex(tx,ty);
            if(tidx<0||tidx>=_numLeds) break;

            float fracScale = 1.0f - (float)t/(float)(tailLength+1);
            uint8_t tailB   = (uint8_t)(_brightness * fracScale);

            if(tailB<10) tailB=10;  // optional min brightness
            CRGB tailCol = mainC;
            tailCol.nscale8(tailB);
            leds[tidx] += tailCol;
        }

        ++it;
    }
}

// blend helper
CRGB LEDManager::calcColor(float frac, CRGB startColor, CRGB endColor, bool bounce){
    if(!bounce){
        return blend(startColor, endColor, (uint8_t)(frac*255));
    }
    else{
        if(frac<=0.5f){
            return blend(startColor, endColor, (uint8_t)(frac*2*255));
        } else {
            return blend(endColor, startColor,(uint8_t)((frac-0.5f)*2*255));
        }
    }
}

// map (x,y)->led index with rotation + panel order
int LEDManager::getLedIndex(int x, int y) const{
    int panel = (x<16)?0:1;
    int localX= (x<16)? x : (x-16);
    int localY= y;

    // rotate
    rotateCoordinates(localX, localY, (panel==0) ? rotationAngle1:rotationAngle2);

    // zigzag row
    if(localY%2!=0){
        localX = 15 - localX;
    }

    // panel order
    int base = (panelOrder==0) ? panel*256 : (1-panel)*256;
    int idx  = base + localY*16 + localX;
    if(idx<0||idx>=_numLeds){
        // out-of-bounds
        return -1;
    }
    return idx;
}

// rotate coords by angle
void LEDManager::rotateCoordinates(int &x, int &y, int angle) const{
    int tmpX, tmpY;
    switch(angle){
        case 0:
            // no rotation
            break;
        case 90:
            tmpX=y;
            tmpY=15 - x;
            x=tmpX; y=tmpY;
            break;
        case 180:
            tmpX=15 - x;
            tmpY=15 - y;
            x=tmpX; y=tmpY;
            break;
        case 270:
            tmpX=15 - y;
            tmpY=x;
            x=tmpX; y=tmpY;
            break;
        default:
            // ignore
            break;
    }
}
