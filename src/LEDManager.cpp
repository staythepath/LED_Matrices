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

// Up to 8 panels of 16×16
CRGB leds[MAX_LEDS];

/**
 * LEDManager Constructor
 * 
 * We keep your existing fields (brightness, spawnRate, etc.)
 * and add new ones for "speedVal" in [0..100], plus param1..4 in [0..255].
 */
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
    , panelOrder(0)
    , rotationAngle1(90)
    , rotationAngle2(90)
    , ledUpdateInterval(38) // in ms
    , lastLedUpdate(0)
    // NEW FIELDS:
    , _speedVal(50)  // 0..100, default ~50 => moderate speed
    , _param1(0)
    , _param2(0)
    , _param3(0)
    , _param4(0)
{
    createPalettes();
    _animationNames.push_back("Traffic");     // index=0
    _animationNames.push_back("Blink");       // index=1
    _animationNames.push_back("RainbowWave"); // index=2
}

/**
 * begin()
 *   - reinit the LEDs and default to animation 0.
 */
void LEDManager::begin() {
    reinitFastLED();
    // default to anim 0
    setAnimation(0);
}

/**
 * reinitFastLED()
 *   - Clears LED data, sets brightness, 
 *     and configures your strip on pin 45.
 */
void LEDManager::reinitFastLED() {
    FastLED.clear(true);
    FastLED.setBrightness(_brightness);

    if(_numLeds > MAX_LEDS) {
        Serial.println("Error: _numLeds > MAX_LEDS, adjusting.");
        _numLeds = MAX_LEDS;
    }
    FastLED.clearData();
    // Pin 45 for data
    FastLED.addLeds<WS2812B, 4, GRB>(leds, _numLeds).setCorrection(TypicalLEDStrip);
    FastLED.show();
}

/**
 * update()
 *   - Called in loop() to let the current animation update.
 */
void LEDManager::update() {
    if (_currentAnimation) {
        _currentAnimation->update();
    }
}

/**
 * show()
 *   - Forces a FastLED.show()
 */
void LEDManager::show() {
    FastLED.show();
}

/**
 * cleanupAnimation()
 *   - Deletes the current animation object if any.
 */
void LEDManager::cleanupAnimation() {
    if (_currentAnimation) {
        delete _currentAnimation;
        _currentAnimation = nullptr;
    }
}

/**
 * createPalettes()
 *   - Fills ALL_PALETTES and PALETTE_NAMES with your 11 examples.
 */
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

/**
 * setPanelCount()
 *   - Changes panel count [1..8].
 *   - Reinitializes and restarts the animation.
 */
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

/**
 * identifyPanels()
 *   - Clears the strip, draws an arrow+digit for each panel, 
 *     waits 10s, then restores the old animation.
 */
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

/**
 * drawUpArrow()
 *   - Renders a bigger arrow at the top rows (0..3)
 */
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
 * drawLargeDigit()
 *   - Places an 8×8 pattern in the panel, digit in [1..8]
 */
void LEDManager::drawLargeDigit(int baseIndex, int digit){
    if(digit<1) digit=1;
    if(digit>8) digit=8;
    static const bool digits8x8[8][64] = {
        // digit 1
        {
          false,false,true,false,false,false,false,false,
          false,false,true,false,false,false,false,false,
          false,false,true,false,false,false,false,false,
          false,false,true,false,false,false,false,false,
          false,false,true,false,false,false,false,false,
          false,false,true,false,false,false,false,false,
          false,false,true,false,false,false,false,false,
          false,false,true,false,false,false,false,false
        },
        // digit 2
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
        // digit 3
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
        // digit 4
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
        // digit 5
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
        // digit 6
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
        // digit 7
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
        // digit 8
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

    CRGB color = CHSV((digit*32) & 255, 255, 255);
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

/**
 * setAnimation()
 *   - Switches animations: Traffic, Blink, or RainbowWave
 */
void LEDManager::setAnimation(int animIndex) {
    if (animIndex == _currentAnimationIndex) return;
    cleanupAnimation();

    if (animIndex < 0 || animIndex >= (int)getAnimationCount()) {
        Serial.println("Invalid animation index.");
        return;
    }
    _currentAnimationIndex = animIndex;

    if (animIndex == 0) {
        auto traffic = new TrafficAnimation(_numLeds, _brightness, _panelCount);
        traffic->setAllPalettes(&ALL_PALETTES);
        traffic->setCurrentPalette(currentPalette);
        traffic->setSpawnRate(spawnRate);
        traffic->setMaxCars(maxFlakes);
        traffic->setTailLength(tailLength);
        traffic->setFadeAmount(fadeAmount);
        traffic->setPanelOrder(panelOrder);
        traffic->setRotationAngle1(rotationAngle1);
        traffic->setRotationAngle2(rotationAngle2);
        _currentAnimation = traffic;
        _currentAnimation->begin();
        Serial.println("Traffic animation selected.");
    } else if (animIndex==1){
        auto blink= new BlinkAnimation(_numLeds, _brightness, _panelCount);
        _currentAnimation=blink;
        _currentAnimation->begin();
        Serial.println("Blink animation selected.");
    } else if(animIndex==2){
        auto wave= new RainbowWaveAnimation(_numLeds, _brightness, _panelCount);
        _currentAnimation= wave;
        _currentAnimation->begin();
        Serial.println("RainbowWave animation selected.");
    }
    else {
        Serial.println("Unknown anim index.");
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

/**
 * setBrightness() / getBrightness()
 *   - 0..255
 */
void LEDManager::setBrightness(uint8_t b){
    _brightness=b;
    FastLED.setBrightness(_brightness);

    // If an animation is running, pass it along
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

/**
 * setPalette(), getPaletteNameAt(), etc.
 *   - picks from your 11 sample palettes
 */
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
        // etc. if needed
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

/**
 * setSpawnRate() / getSpawnRate()
 *   - 0..(some max?)
 */
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

/**
 * setMaxFlakes() / getMaxFlakes()
 *   - 0..500 typically
 */
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

/**
 * setTailLength() / getTailLength()
 *   - 0..30
 */
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

/**
 * setFadeAmount() / getFadeAmount()
 *   - 0..255
 */
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

/**
 * swapPanels(), setPanelOrder(), rotatePanel(), getRotation()
 *   - Additional panel manipulation
 */
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

/**
 * setUpdateSpeed() / getUpdateSpeed()
 *   - The old approach: 10..60000 ms
 *   - We'll keep it for backward compatibility, but we can still call it internally
 *     from setSpeed() if you want.
 */
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
            wA->setUpdateInterval(speed);
        }
    } else {
        Serial.println("Invalid speed. Must be 10..60000");
    }
}
unsigned long LEDManager::getUpdateSpeed() const {
    return ledUpdateInterval;
}

/**
 * NEW: setSpeed(int val) / getSpeed()
 *   - Speed = 0..100 => internally map to ledUpdateInterval from ~3000 ms (speed=0) down to 10 ms (speed=100).
 *   - If you want a narrower range, adjust the formula.
 */
void LEDManager::setSpeed(int val){
    if(val<0) val=0;
    if(val>100) val=100;
    _speedVal= val;

    // We'll map 0 => 3000 ms, 100 => 10 ms
    //   range = 2990
    //   ledUpdateInterval = 3000 - (val * 2990/100)
    int range = 2990;
    unsigned long mapped = 3000 - ((range * val) / 100);
    if(mapped<10) mapped=10; // clamp

    setUpdateSpeed(mapped); // Reuse your existing logic
    // Now your animations get the new interval
    // done
}
int LEDManager::getSpeed() const {
    return _speedVal;
}

/**
 * NEW: param1..param4 in [0..255]
 *   - Generic parameters for your animations or anything else
 */
void LEDManager::setParam1(uint8_t p){
    _param1= p;
    Serial.printf("Param1 set to %u\n", p);
    // If your animation needs it, pass it there
}
uint8_t LEDManager::getParam1() const {
    return _param1;
}

void LEDManager::setParam2(uint8_t p){
    _param2= p;
    Serial.printf("Param2 set to %u\n", p);
}
uint8_t LEDManager::getParam2() const {
    return _param2;
}

void LEDManager::setParam3(uint8_t p){
    _param3= p;
    Serial.printf("Param3 set to %u\n", p);
}
uint8_t LEDManager::getParam3() const {
    return _param3;
}

void LEDManager::setParam4(uint8_t p){
    _param4= p;
    Serial.printf("Param4 set to %u\n", p);
}
uint8_t LEDManager::getParam4() const {
    return _param4;
}
