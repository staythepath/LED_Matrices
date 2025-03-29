#include "LEDManager.h"
#include "config.h"
#include <Arduino.h>
#include <FastLED.h>
#include <typeinfo>

// Animations
#include "animations/BaseAnimation.h"
#include "animations/TrafficAnimation.h"
#include "animations/BlinkAnimation.h"
#include "animations/RainbowWaveAnimation.h"
#include "animations/FireworkAnimation.h"

// Up to 8 panels of 16×16
CRGB leds[MAX_LEDS];

LEDManager::LEDManager()
    : _panelCount(3) // default
    , _numLeds(_panelCount * 16 * 16)
    , _brightness(32)
    , currentPalette(0)
    , _currentAnimation(nullptr)
    , _currentAnimationIndex(-1)
    , spawnRate(1.0f)
    , maxFlakes(500)
    , tailLength(3)
    , fadeAmount(39)
    , panelOrder(1)  // Start with right panel first instead of left
    , rotationAngle1(90)
    , rotationAngle2(90)
    , ledUpdateInterval(38)
    , lastLedUpdate(0)
{
    createPalettes();
    _animationNames.push_back("Traffic");     // index=0
    _animationNames.push_back("Blink");       // index=1
    _animationNames.push_back("RainbowWave"); // index=2
    _animationNames.push_back("Firework");    // index=3
}

void LEDManager::begin() {
    reinitFastLED();
    // default to anim 0
    setAnimation(0);
}

void LEDManager::reinitFastLED() {
    FastLED.clear(true);
    FastLED.setBrightness(_brightness);

    if(_numLeds > MAX_LEDS) {
        Serial.println("Error: _numLeds > MAX_LEDS, adjusting.");
        _numLeds = MAX_LEDS;
    }
    FastLED.clearData();
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, _numLeds).setCorrection(TypicalLEDStrip);
    FastLED.show();
}

void LEDManager::update() {
    if (_currentAnimation) {
        _currentAnimation->update();
    }
}

void LEDManager::show() {
    FastLED.show();
}

void LEDManager::cleanupAnimation() {
    if (_currentAnimation) {
        delete _currentAnimation;
        _currentAnimation = nullptr;
    }
}

void LEDManager::createPalettes() {
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

void LEDManager::setPanelCount(int count) {
    if(count<1) count=1;
    if(count>8) count=8;

    int oldCount = _panelCount;
    int oldNumLeds= _numLeds;

    _panelCount= count;
    _numLeds = _panelCount * 16 * 16;
    Serial.printf("Panel count set to %d, _numLeds=%d\n", _panelCount, _numLeds);

    if(_numLeds < oldNumLeds){
        for(int i=_numLeds; i<oldNumLeds && i<MAX_LEDS; i++){
            leds[i]=CRGB::Black;
        }
        FastLED.show();
    }
    reinitFastLED();

    cleanupAnimation();
    int oldIdx= _currentAnimationIndex;
    _currentAnimationIndex=-1;
    setAnimation( (oldIdx>=0) ? oldIdx : 0 );
}
int LEDManager::getPanelCount() const {
    return _panelCount;
}

// Identify Panels
// Blocks 10s, draws big arrow + big digit
void LEDManager::identifyPanels(){
    Serial.println("identifyPanels() invoked, blocking 10s...");
    int oldIdx = _currentAnimationIndex;
    cleanupAnimation();
    _currentAnimationIndex=-1; 

    // Clear
    FastLED.clear(true);

    // For each panel p => draw arrow + digit
    for(int p=0; p<_panelCount; p++){
        int base = p*16*16; 
        drawUpArrow(base);
        drawLargeDigit(base, p+1); 
    }
    FastLED.show();

    // Wait 10s
    delay(10000);

    // Restore old anim
    if(oldIdx>=0){
        setAnimation(oldIdx);
    } else {
        setAnimation(0);
    }
}

// A bigger arrow at the top
// Let's do 4 rows: row=0..3
// row=0 => x=8 = tip
// row=1 => x=7..9
// row=2 => x=6..10
// row=3 => x=5..11
void LEDManager::drawUpArrow(int baseIndex){
    // row=0 => x=8
    int idx = baseIndex + (0*16)+8;
    if(idx<MAX_LEDS) leds[idx]=CRGB::Green;

    // row=1 => x=7..9
    for(int x=7; x<=9; x++){
        idx= baseIndex+(1*16)+x;
        if(idx<MAX_LEDS) leds[idx]=CRGB::Green;
    }
    // row=2 => x=6..10
    for(int x=6; x<=10; x++){
        idx= baseIndex+(2*16)+x;
        if(idx<MAX_LEDS) leds[idx]=CRGB::Green;
    }
    // row=3 => x=5..11
    for(int x=5; x<=11; x++){
        idx= baseIndex+(3*16)+x;
        if(idx<MAX_LEDS) leds[idx]=CRGB::Green;
    }
}

/**
 * We'll define an 8×8 pattern for digits 1..8,
 * placing top-left at (4,6), so it's somewhat in the center 
 * (x=4..11, y=6..13).
 *
 * We'll store them in a big array "digits8x8[digit-1][64]"
 * 'X' = True => color pixel, '.'=False => skip
 */
void LEDManager::drawLargeDigit(int baseIndex, int digit){
    // clamp 1..8
    if(digit<1) digit=1;
    if(digit>8) digit=8;
    static const bool digits8x8[8][64] = {
        // 1 => a tall line at x=3 maybe
        {
        // row=0..7
        // We'll do a minimal shape that roughly looks like "1"
        // 'X' => true, '.' => false
        // We'll do 8 columns => col=0..7
          // row0
          false,false,true,false,false,false,false,false,
          false,false,true,false,false,false,false,false,
          false,false,true,false,false,false,false,false,
          false,false,true,false,false,false,false,false,
          false,false,true,false,false,false,false,false,
          false,false,true,false,false,false,false,false,
          false,false,true,false,false,false,false,false,
          false,false,true,false,false,false,false,false
        },
        // 2 => let's do a shape
        {
          true,true,true,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          true,true,true,true,false,false,false,false,
          true,false,false,false,false,false,false,false,
          true,false,false,false,false,false,false,false,
          true,true,true,true,false,false,false,false,
          false,false,false,false,false,false,false,false
        },
        // 3
        {
          true,true,true,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          false,true,true,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          true,true,true,true,false,false,false,false,
          false,false,false,false,false,false,false,false
        },
        // 4
        {
          true,false,false,true,false,false,false,false,
          true,false,false,true,false,false,false,false,
          true,false,false,true,false,false,false,false,
          true,true,true,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          false,false,false,false,false,false,false,false
        },
        // 5
        {
          true,true,true,true,false,false,false,false,
          true,false,false,false,false,false,false,false,
          true,false,false,false,false,false,false,false,
          true,true,true,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          true,true,true,true,false,false,false,false,
          false,false,false,false,false,false,false,false
        },
        // 6
        {
          false,true,true,true,false,false,false,false,
          true,false,false,false,false,false,false,false,
          true,false,false,false,false,false,false,false,
          true,true,true,true,false,false,false,false,
          true,false,false,true,false,false,false,false,
          true,false,false,true,false,false,false,false,
          false,true,true,true,false,false,false,false,
          false,false,false,false,false,false,false,false
        },
        // 7
        {
          true,true,true,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          false,false,false,true,false,false,false,false,
          false,false,false,false,false,false,false,false
        },
        // 8
        {
          false,true,true,true,false,false,false,false,
          true,false,false,true,false,false,false,false,
          true,false,false,true,false,false,false,false,
          false,true,true,true,false,false,false,false,
          true,false,false,true,false,false,false,false,
          true,false,false,true,false,false,false,false,
          false,true,true,true,false,false,false,false,
          false,false,false,false,false,false,false,false
        }
    };

    // pick color
    CRGB color = CHSV((digit*32) & 255, 255, 255);

    // top-left corner of digit => (4,6)
    // so it occupies x=4..11, y=6..13
    int startX=4;
    int startY=6;
    int indexDigit= digit-1;

    for(int row=0; row<8; row++){
        for(int col=0; col<8; col++){
            bool on = digits8x8[indexDigit][row*8 + col];
            if(on){
                int x= startX+ col;
                int y= startY+ row;
                int idx= baseIndex + y*16 + x;
                if(idx<MAX_LEDS){
                    leds[idx]= color;
                }
            }
        }
    }
}

void LEDManager::setAnimation(int animIndex) {
    // Clean up old animation
    cleanupAnimation();

    // Bounds check
    if(animIndex < 0 || animIndex >= (int)_animationNames.size()) {
        Serial.println("Invalid animation index");
        return;
    }

    _currentAnimationIndex = animIndex;
    
    // Create new animation
    if(animIndex == 0) {
        TrafficAnimation* anim = new TrafficAnimation(_numLeds, _brightness, _panelCount);
        anim->setAllPalettes(&ALL_PALETTES);
        anim->setCurrentPalette(currentPalette);
        anim->setSpawnRate(spawnRate);
        anim->setMaxCars(maxFlakes);
        anim->setTailLength(tailLength);
        anim->setFadeAmount(fadeAmount);
        anim->setPanelOrder(panelOrder);
        anim->setRotationAngle1(rotationAngle1);
        anim->setRotationAngle2(rotationAngle2);
        anim->setRotationAngle3(rotationAngle1); // 3rd panel same as 1st
        anim->setUpdateInterval(ledUpdateInterval);
        _currentAnimation = anim;
    }
    else if(animIndex == 1) {
        BlinkAnimation* anim = new BlinkAnimation(_numLeds, _brightness);
        _currentAnimation = anim;
    }
    else if(animIndex == 2) {
        RainbowWaveAnimation* anim = new RainbowWaveAnimation(_numLeds, _brightness, _panelCount);
        anim->setPanelOrder(panelOrder);
        anim->setRotationAngle1(rotationAngle1);
        anim->setRotationAngle2(rotationAngle2);
        anim->setRotationAngle3(rotationAngle1); // 3rd panel same as 1st
        
        // Always use a fast update interval for smooth animation
        anim->setUpdateInterval(8);
        
        // For Rainbow Wave, we want to limit the minimum effective speed to 250ms
        // while keeping the slider range from 10-1500ms
        // Map from range [10, 1500] to [250, 1500] for effective speed calculation
        float effectiveSpeed = 250.0f + (ledUpdateInterval - 10.0f) * (1250.0f / 1490.0f);
        
        // Now map this effective speed to the multiplier range [3.0, 0.5]
        float speedValue = constrain(effectiveSpeed, 250, 1500);
        float speedMultiplier = 3.0f - ((speedValue - 250) / 1250.0f * 2.5f);
        anim->setSpeedMultiplier(speedMultiplier);
        
        _currentAnimation = anim;
    }
    else if(animIndex == 3) {
        FireworkAnimation* anim = new FireworkAnimation(_numLeds, _brightness, _panelCount);
        anim->setPanelOrder(panelOrder);
        anim->setRotationAngle1(0); // Fixed rotation for fireworks
        anim->setRotationAngle2(0); // Fixed rotation for fireworks
        anim->setRotationAngle3(0); // Fixed rotation for fireworks
        anim->setUpdateInterval(15); // Faster update for fireworks
        anim->setMaxFireworks(10);
        anim->setParticleCount(40);
        anim->setGravity(0.15f);
        anim->setLaunchProbability(0.15f);
        _currentAnimation = anim;
    }

    if(_currentAnimation) {
        _currentAnimation->begin();
    }
}

int LEDManager::getAnimation() const {
    return _currentAnimationIndex;
}
size_t LEDManager::getAnimationCount() const {
    return _animationNames.size();
}
String LEDManager::getAnimationName(int animIndex) const {
    if (animIndex>=0 && animIndex<(int)_animationNames.size()){
        return _animationNames[animIndex];
    }
    return "Unknown";
}

void LEDManager::setBrightness(uint8_t b){
    _brightness=b;
    FastLED.setBrightness(_brightness);

    if(_currentAnimationIndex==0 && _currentAnimation){
        auto t=(TrafficAnimation*)_currentAnimation;
        t->setBrightness(_brightness);
    } else if(_currentAnimationIndex==1 && _currentAnimation){
        auto bA=(BlinkAnimation*)_currentAnimation;
        bA->setBrightness(_brightness);
    } else if(_currentAnimationIndex==2 && _currentAnimation){
        auto wA=(RainbowWaveAnimation*)_currentAnimation;
        wA->setBrightness(_brightness);
    }
}
uint8_t LEDManager::getBrightness() const {
    return _brightness;
}

void LEDManager::setPalette(int idx){
    if(idx>=0 && idx<(int)PALETTE_NAMES.size()){
        currentPalette= idx;
        Serial.printf("Palette %d (%s) selected.\n", idx, PALETTE_NAMES[idx].c_str());
        if(_currentAnimationIndex==0 && _currentAnimation){
            auto t=(TrafficAnimation*)_currentAnimation;
            t->setCurrentPalette(currentPalette);
        }
        else if(_currentAnimationIndex==1 && _currentAnimation){
            auto bA=(BlinkAnimation*)_currentAnimation;
            bA->setPalette(&ALL_PALETTES[currentPalette]);
        }
        // etc.
    }
}
int LEDManager::getCurrentPalette() const {
    return currentPalette;
}
size_t LEDManager::getPaletteCount() const {
    return PALETTE_NAMES.size();
}
String LEDManager::getPaletteNameAt(int i) const {
    if(i>=0 && i<(int)PALETTE_NAMES.size()){
        return PALETTE_NAMES[i];
    }
    return "Unknown";
}
const std::vector<CRGB>& LEDManager::getCurrentPaletteColors() const {
    if (currentPalette<0 || currentPalette>=(int)ALL_PALETTES.size()){
        static std::vector<CRGB> dummy;
        return dummy;
    }
    return ALL_PALETTES[currentPalette];
}

void LEDManager::setSpawnRate(float r){
    spawnRate=r;
    if(_currentAnimationIndex==0 && _currentAnimation){
        auto t=(TrafficAnimation*)_currentAnimation;
        t->setSpawnRate(r);
    }
}
float LEDManager::getSpawnRate() const {
    return spawnRate;
}

void LEDManager::setMaxFlakes(int m){
    maxFlakes=m;
    if(_currentAnimationIndex==0 && _currentAnimation){
        auto t=(TrafficAnimation*)_currentAnimation;
        t->setMaxCars(m);
    }
}
int LEDManager::getMaxFlakes() const {
    return maxFlakes;
}

void LEDManager::setTailLength(int l){
    tailLength=l;
    if(_currentAnimationIndex==0 && _currentAnimation){
        auto t=(TrafficAnimation*)_currentAnimation;
        t->setTailLength(l);
    }
}
int LEDManager::getTailLength() const {
    return tailLength;
}

void LEDManager::setFadeAmount(uint8_t a){
    fadeAmount=a;
    if(_currentAnimationIndex==0 && _currentAnimation){
        auto t=(TrafficAnimation*)_currentAnimation;
        t->setFadeAmount(a);
    }
}
uint8_t LEDManager::getFadeAmount() const {
    return fadeAmount;
}

void LEDManager::swapPanels(){
    panelOrder=1-panelOrder;
    Serial.println("Panels swapped successfully.");
    if(_currentAnimationIndex==0 && _currentAnimation){
        auto t=(TrafficAnimation*)_currentAnimation;
        t->setPanelOrder(panelOrder);
    }
}
void LEDManager::setPanelOrder(String order){
    if(order.equalsIgnoreCase("left")){
        panelOrder=0;
        Serial.println("Panel order set to left first.");
    }
    else if(order.equalsIgnoreCase("right")){
        panelOrder=1;
        Serial.println("Panel order set to right first.");
    } else {
        Serial.println("Invalid panel order. Use 'left' or 'right'.");
        return;
    }
    if(_currentAnimationIndex==0 && _currentAnimation){
        auto t=(TrafficAnimation*)_currentAnimation;
        t->setPanelOrder(panelOrder);
    }
}
void LEDManager::rotatePanel(String panel, int angle){
    if(!(angle==0||angle==90||angle==180||angle==270)){
        Serial.printf("Invalid rotation angle: %d\n", angle);
        return;
    }
    if(panel.equalsIgnoreCase("PANEL1")){
        rotationAngle1=angle;
        Serial.printf("Panel1 angle set to %d\n", angle);
        if(_currentAnimationIndex==0 && _currentAnimation){
            auto t=(TrafficAnimation*)_currentAnimation;
            t->setRotationAngle1(angle);
        }
    }
    else if(panel.equalsIgnoreCase("PANEL2")){
        rotationAngle2=angle;
        Serial.printf("Panel2 angle set to %d\n", angle);
        if(_currentAnimationIndex==0 && _currentAnimation){
            auto t=(TrafficAnimation*)_currentAnimation;
            t->setRotationAngle2(angle);
        }
    }
    else {
        Serial.printf("Unknown panel: %s\n", panel.c_str());
    }
}
int LEDManager::getRotation(String panel) const {
    if(panel.equalsIgnoreCase("PANEL1")){
        return rotationAngle1;
    }
    else if(panel.equalsIgnoreCase("PANEL2")){
        return rotationAngle2;
    }
    return -1;
}

void LEDManager::setUpdateSpeed(unsigned long speed){
    if(speed>=10 && speed<=60000){
        ledUpdateInterval=speed;
        Serial.printf("LED update speed set to %lu ms\n", speed);
        if(_currentAnimationIndex==0 && _currentAnimation){
            auto t=(TrafficAnimation*)_currentAnimation;
            t->setUpdateInterval(speed);
        }
        else if(_currentAnimationIndex==1 && _currentAnimation){
            auto bA=(BlinkAnimation*)_currentAnimation;
            bA->setInterval(speed);
        }
        else if(_currentAnimationIndex==2 && _currentAnimation){
            auto wA=(RainbowWaveAnimation*)_currentAnimation;
            
            // For Rainbow Wave, we want to limit the minimum effective speed to 250ms
            // while keeping the slider range from 10-1500ms
            // Map from range [10, 1500] to [250, 1500] for effective speed calculation
            float effectiveSpeed = 250.0f + (speed - 10.0f) * (1250.0f / 1490.0f);
            
            // Now map this effective speed to the multiplier range [3.0, 0.5]
            float speedValue = constrain(effectiveSpeed, 250, 1500);
            float speedMultiplier = 3.0f - ((speedValue - 250) / 1250.0f * 2.5f);
            
            // Keep the update interval constant for smooth animation
            wA->setUpdateInterval(8);
            wA->setSpeedMultiplier(speedMultiplier);
            
            Serial.printf("RainbowWave effective speed: %.0fms, multiplier: %.2f\n", 
                          effectiveSpeed, speedMultiplier);
        }
        else if(_currentAnimationIndex==3 && _currentAnimation){
            auto fA=(FireworkAnimation*)_currentAnimation;
            fA->setUpdateInterval(speed);
        }
    } else {
        Serial.println("Invalid speed. Must be 10..60000");
    }
}
unsigned long LEDManager::getUpdateSpeed() const {
    return ledUpdateInterval;
}
