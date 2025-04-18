#include "LEDManager.h"
#include "config.h"
#include <Arduino.h>
#include <FastLED.h>
#include <typeinfo>
#include "LogManager.h"

// Include Animation header files
#include "animations/TrafficAnimation.h"
#include "animations/BlinkAnimation.h"
#include "animations/RainbowWaveAnimation.h"
#include "animations/FireworkAnimation.h"
#include "animations/GameOfLifeAnimation.h"

// Animations
#include "animations/BaseAnimation.h"

// Up to 8 panels of 16×16
CRGB leds[MAX_LEDS];

LEDManager::LEDManager()
    : _panelCount(2) // default
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
    , rotationAngle3(90)
    , ledUpdateInterval(38)
    , lastLedUpdate(0)
    , _isInitializing(true)
{
    createPalettes();
    _animationNames.push_back("Traffic");     // index=0
    _animationNames.push_back("Blink");       // index=1
    _animationNames.push_back("RainbowWave"); // index=2
    _animationNames.push_back("Firework");    // index=3
    _animationNames.push_back("GameOfLife");  // index=4
}

void LEDManager::begin() {
    reinitFastLED();
    showLoadingAnimation();
}

void LEDManager::showLoadingAnimation() {
    FastLED.clear();
    for (int i = 0; i < 5; i++) {
        int pos = (millis() / 200 + i * 3) % _numLeds;
        leds[pos] = CRGB::Blue;
    }
    uint8_t pulse = sin8(millis() / 10);
    for (int i = 0; i < _numLeds; i++) {
        if (leds[i]) {
            leds[i].fadeToBlackBy(255 - pulse);
        }
    }
    FastLED.show();
}

void LEDManager::finishInitialization() {
    Serial.println("Finishing initialization, switching to main animation");
    
    // Check memory before proceeding
    uint32_t freeHeap = ESP.getFreeHeap();
    Serial.printf("Free heap before switching from loading animation: %u bytes\n", freeHeap);
    
    if (freeHeap < 20000) {
        Serial.println("WARNING: Low memory detected, delaying animation switch");
        delay(500); // Give system time to free resources
    }
    
    // Set initialization flag to false to stop loading animation
    _isInitializing = false;
    
    // Create default animation (Traffic)
    setAnimation(0);
    
    Serial.printf("Free heap after animation switch: %u bytes\n", ESP.getFreeHeap());
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
    if (_isInitializing) {
        showLoadingAnimation();
        return;
    }
    if (_currentAnimation) {
        _currentAnimation->update();
    }
}

void LEDManager::show() {
    FastLED.show();
}

void LEDManager::configureCurrentAnimation() {
    if (!_currentAnimation) return;
    
    // Set common properties for all animations
    _currentAnimation->setBrightness(_brightness);
    
    // Set animation-specific properties
    if (_currentAnimationIndex == 0) { // Traffic
        TrafficAnimation* anim = static_cast<TrafficAnimation*>(_currentAnimation);
        anim->setRotationAngle1(rotationAngle1);
        anim->setRotationAngle2(rotationAngle2);
        anim->setRotationAngle3(rotationAngle3);
        anim->setPanelOrder(panelOrder);
        anim->setSpawnRate(spawnRate);
        anim->setMaxCars(maxFlakes);
        anim->setTailLength(tailLength);
        anim->setAllPalettes(&ALL_PALETTES);
        anim->setCurrentPalette(currentPalette);
    }
    else if (_currentAnimationIndex == 1) { // Blink
        BlinkAnimation* anim = static_cast<BlinkAnimation*>(_currentAnimation);
        anim->setInterval(ledUpdateInterval);
        anim->setPalette(&ALL_PALETTES[currentPalette]);
    }
    else if (_currentAnimationIndex == 2) { // RainbowWave
        RainbowWaveAnimation* anim = static_cast<RainbowWaveAnimation*>(_currentAnimation);
        // Calculate speed based on update interval
        float effectiveSpeed = 250.0f + (ledUpdateInterval - 10.0f) * (1250.0f / 1490.0f);
        float speedValue = constrain(effectiveSpeed, 250, 1500);
        float speedMultiplier = 3.0f - ((speedValue - 250) / 1250.0f * 2.5f);
        
        // Set animation parameters
        anim->setUpdateInterval(8);
        anim->setSpeedMultiplier(speedMultiplier);
        
        // Set panel configuration
        anim->setPanelOrder(panelOrder);
        anim->setRotationAngle1(rotationAngle1);
        anim->setRotationAngle2(rotationAngle2);
        anim->setRotationAngle3(rotationAngle3);
    }
    else if (_currentAnimationIndex == 3) { // Firework
        FireworkAnimation* anim = static_cast<FireworkAnimation*>(_currentAnimation);
        anim->setRotationAngle1(rotationAngle1);
        anim->setRotationAngle2(rotationAngle2);
        anim->setRotationAngle3(rotationAngle3);
        anim->setPanelOrder(panelOrder);
        anim->setUpdateInterval(15);
        anim->setMaxFireworks(10);
        anim->setParticleCount(40);
        anim->setGravity(0.15f);
        anim->setLaunchProbability(0.15f);
    }
    else if (_currentAnimationIndex == 4) { // GameOfLife
        GameOfLifeAnimation* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setRotationAngle1(rotationAngle1);
        anim->setRotationAngle2(rotationAngle2);
        anim->setRotationAngle3(rotationAngle3);
        anim->setPanelOrder(panelOrder);
        anim->setSpeed(map(ledUpdateInterval, 10, 1500, 0, 255));
        anim->setAllPalettes(&ALL_PALETTES);
        anim->setCurrentPalette(currentPalette);
    }
}

void LEDManager::cleanupAnimation() {
    if (_currentAnimation) {
        _currentAnimation->end();
        // Add a delay before deletion to ensure resources are properly cleaned up
        delay(5);
        delete _currentAnimation;
        _currentAnimation = nullptr;
        // Force a garbage collection moment
        ESP.getFreeHeap();
    }
}

void LEDManager::setAnimation(int index) {
    // Validate animation index
    if (index < 0 || index >= (int)_animationNames.size()) {
        Serial.printf("Invalid animation index: %d\n", index);
        systemWarning("Invalid animation index: " + String(index));
        return;
    }
    
    uint32_t heapBefore = ESP.getFreeHeap();
    systemInfo("Setting animation to: " + _animationNames[index] + " (index " + String(index) + ")");
    systemInfo("Free heap before animation change: " + String(heapBefore) + " bytes");

    // Clean up old animation
    cleanupAnimation();
    
    // Create new animation
    _currentAnimationIndex = index;
    
    try {
        switch (index) {
            case 0: // Traffic
                _currentAnimation = new TrafficAnimation(_numLeds, _brightness, _panelCount);
                break;
            case 1: // Blink
                _currentAnimation = new BlinkAnimation(_numLeds, _brightness, _panelCount);
                break;
            case 2: // RainbowWave
                _currentAnimation = new RainbowWaveAnimation(_numLeds, _brightness, _panelCount);
                break;
            case 3: // Firework
                _currentAnimation = new FireworkAnimation(_numLeds, _brightness, _panelCount);
                break;
            case 4: // GameOfLife
                _currentAnimation = new GameOfLifeAnimation(_numLeds, _brightness, _panelCount);
                break;
            default:
                systemError("Unknown animation index: " + String(index) + ", falling back to Traffic");
                _currentAnimation = new TrafficAnimation(_numLeds, _brightness, _panelCount);
                _currentAnimationIndex = 0;
        }
    } catch (const std::exception& e) {
        // Handle memory allocation failures
        systemError("Error creating animation '" + _animationNames[index] + "': " + String(e.what()));
        
        // Try to create a simpler animation
        try {
            _currentAnimation = new TrafficAnimation(_numLeds, _brightness, _panelCount);
            _currentAnimationIndex = 0;
        } catch (...) {
            systemCritical("CRITICAL: Failed to create even the fallback animation!");
            _currentAnimation = nullptr;
            return;
        }
    } catch (...) {
        systemError("Unknown error creating animation '" + _animationNames[index] + "'");
        
        // Try to create a simpler animation
        try {
            _currentAnimation = new TrafficAnimation(_numLeds, _brightness, _panelCount);
            _currentAnimationIndex = 0;
        } catch (...) {
            systemCritical("CRITICAL: Failed to create even the fallback animation!");
            _currentAnimation = nullptr;
            return;
        }
    }
    
    uint32_t heapAfter = ESP.getFreeHeap();
    uint32_t heapUsed = heapBefore > heapAfter ? heapBefore - heapAfter : 0;
    systemInfo("Animation created: " + _animationNames[_currentAnimationIndex]);
    systemInfo("Free heap after animation change: " + String(heapAfter) + " bytes (used: " + String(heapUsed) + " bytes)");
    
    // Configure the new animation
    if (_currentAnimation) {
        this->configureCurrentAnimation();
        _currentAnimation->begin();
    }
}

void LEDManager::setPanelCount(int count) {
    if(count<1) count=1;
    if(count>8) count=8;

    Serial.printf("Setting panel count to %d\n", count);
    systemInfo("Setting panel count to " + String(count));

    int oldCount = _panelCount;
    int oldNumLeds= _numLeds;
    int oldIdx = _currentAnimationIndex;

    _panelCount = count;
    _numLeds = _panelCount * 16 * 16;
    Serial.printf("Panel count set to %d, _numLeds=%d\n", _panelCount, _numLeds);
    systemInfo("Panel count set to " + String(_panelCount) + ", total LEDs=" + String(_numLeds));

    if(_numLeds < oldNumLeds){
        systemInfo("Clearing unused LEDs from previous configuration");
        for(int i=_numLeds; i<oldNumLeds && i<MAX_LEDS; i++){
            leds[i]=CRGB::Black;
        }
        FastLED.show();
    }
    
    systemInfo("Reinitializing FastLED with new panel count");
    reinitFastLED();

    systemInfo("Cleaning up animation for panel count change");
    cleanupAnimation();
    _currentAnimationIndex = -1;
    
    // Reset animation with safety checks
    if (oldIdx >= 0 && oldIdx < (int)_animationNames.size()) {
        systemInfo("Recreating animation: " + _animationNames[oldIdx]);
        
        try {
            setAnimation(oldIdx);
        } catch (const std::exception& e) {
            // Catch any memory allocation failures or other exceptions
            systemError("Error recreating animation: " + String(e.what()));
            
            // Try to fall back to Traffic animation (simplest)
            systemInfo("Falling back to Traffic animation");
            try {
                setAnimation(0); // Traffic animation
            } catch (...) {
                systemCritical("CRITICAL: Failed to create fallback animation!");
            }
        } catch (...) {
            systemError("Unknown error recreating animation");
            
            // Try to fall back to Traffic animation
            try {
                setAnimation(0); // Traffic animation
            } catch (...) {
                systemCritical("CRITICAL: Failed to create fallback animation!");
            }
        }
    } else {
        systemInfo("No valid previous animation, defaulting to Traffic");
        try {
            setAnimation(0); // Set to Traffic animation (first one)
        } catch (...) {
            systemCritical("CRITICAL: Failed to create any animation!");
        }
    }
}

int LEDManager::getPanelCount() const {
    return _panelCount;
}

void LEDManager::identifyPanels(){
    Serial.println("identifyPanels() invoked, blocking 10s...");
    int oldIdx = _currentAnimationIndex;
    cleanupAnimation();
    _currentAnimationIndex=-1; 

    FastLED.clear(true);

    for(int p=0; p<_panelCount; p++){
        int base = p*16*16; 
        drawUpArrow(base);
        drawLargeDigit(base, p+1); 
    }
    FastLED.show();

    delay(10000);

    if(oldIdx>=0){
        setAnimation(oldIdx);
    } else {
        setAnimation(0);
    }
}

void LEDManager::drawUpArrow(int baseIndex){
    int idx = baseIndex + (0*16)+8;
    if(idx<MAX_LEDS) leds[idx]=CRGB::Green;

    for(int x=7; x<=9; x++){
        idx= baseIndex+(1*16)+x;
        if(idx<MAX_LEDS) leds[idx]=CRGB::Green;
    }
    for(int x=6; x<=10; x++){
        idx= baseIndex+(2*16)+x;
        if(idx<MAX_LEDS) leds[idx]=CRGB::Green;
    }
    for(int x=5; x<=11; x++){
        idx= baseIndex+(3*16)+x;
        if(idx<MAX_LEDS) leds[idx]=CRGB::Green;
    }
}

void LEDManager::drawLargeDigit(int baseIndex, int digit){
    if(digit<1) digit=1;
    if(digit>8) digit=8;
    static const bool digits8x8[8][64] = {
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

void LEDManager::createPalettes() {
    ALL_PALETTES = {
        { CRGB(0,128,255), CRGB(255,128,0), CRGB(0,200,60), CRGB(64,0,128), CRGB(255,255,64) },
        { CRGB(255,100,0), CRGB(255,0,102), CRGB(128,0,128), CRGB(0,255,128), CRGB(255,255,128) },
        { CRGB(0,255,255), CRGB(255,0,255), CRGB(255,255,0), CRGB(0,255,0), CRGB(255,127,0) },
        { CRGB(0,0,128), CRGB(75,0,130), CRGB(128,0,128), CRGB(0,128,128), CRGB(255,0,128) },
        { CRGB(34,139,34), CRGB(255,69,0), CRGB(139,0,139), CRGB(205,133,63), CRGB(255,215,0) },
        { CRGB(255,182,193), CRGB(152,251,152), CRGB(135,206,250), CRGB(238,130,238), CRGB(255,160,122) },
        { CRGB(0,206,209), CRGB(127,255,212), CRGB(240,230,140), CRGB(255,160,122), CRGB(173,216,230) },
        { CRGB(255,0,0), CRGB(255,140,0), CRGB(255,69,0), CRGB(0,255,255), CRGB(0,128,255) },
        { CRGB(255,0,128), CRGB(128,0,255), CRGB(0,255,128), CRGB(255,255,0), CRGB(255,128,0) },
        { CRGB(139,0,0), CRGB(218,165,32), CRGB(255,0,255), CRGB(75,0,130), CRGB(0,100,140) }
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
        "royal_rainbow"
    };
}

void LEDManager::setBrightness(uint8_t b){
    _brightness=b;
    FastLED.setBrightness(_brightness);

    if(_currentAnimation) {
        _currentAnimation->setBrightness(_brightness);
    }
}

uint8_t LEDManager::getBrightness() const {
    return _brightness;
}

void LEDManager::setPalette(int idx){
    if(idx>=0 && idx<(int)ALL_PALETTES.size()){
        currentPalette= idx;
        Serial.printf("Palette %d (%s) selected.\n", idx, PALETTE_NAMES[idx].c_str());
        if(_currentAnimationIndex==0 && _currentAnimation){
            TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
            t->setCurrentPalette(currentPalette);
        }
        else if(_currentAnimationIndex==1 && _currentAnimation){
            BlinkAnimation* bA = static_cast<BlinkAnimation*>(_currentAnimation);
            bA->setPalette(&ALL_PALETTES[currentPalette]);
        }
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
        TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
        t->setSpawnRate(r);
    }
}
float LEDManager::getSpawnRate() const {
    return spawnRate;
}

void LEDManager::setMaxFlakes(int m){
    maxFlakes=m;
    if(_currentAnimationIndex==0 && _currentAnimation){
        TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
        t->setMaxCars(m);
    }
}
int LEDManager::getMaxFlakes() const {
    return maxFlakes;
}

void LEDManager::setTailLength(int l){
    tailLength=l;
    if(_currentAnimationIndex==0 && _currentAnimation){
        TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
        t->setTailLength(l);
    }
}
int LEDManager::getTailLength() const {
    return tailLength;
}

void LEDManager::setFadeAmount(uint8_t a){
    fadeAmount=a;
    if(_currentAnimationIndex==0 && _currentAnimation){
        TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
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
        TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
        t->setPanelOrder(panelOrder);
    }
}
void LEDManager::setPanelOrder(String order){
    if(order.equalsIgnoreCase("left")){
        panelOrder=0;
        Serial.println("Panel order set to LEFT first.");
    }
    else if(order.equalsIgnoreCase("right")){
        panelOrder=1;
        Serial.println("Panel order set to RIGHT first.");
    }
    else{
        return;
    }
    if(_currentAnimationIndex==0 && _currentAnimation){
        TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
        t->setPanelOrder(panelOrder);
    }
}

void LEDManager::rotatePanel(String panel, int angle){
    if(!(angle==0||angle==90||angle==180||angle==270)){
        Serial.printf("Invalid rotation angle: %d\n", angle);
        return;
    }
    if(panel.equalsIgnoreCase("panel1")){
        rotationAngle1=angle;
        Serial.printf("Panel1 angle set to %d\n", angle);
        if(_currentAnimationIndex==0 && _currentAnimation){
            TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
            t->setRotationAngle1(angle);
        }
    }
    else if(panel.equalsIgnoreCase("panel2")){
        rotationAngle2=angle;
        Serial.printf("Panel2 angle set to %d\n", angle);
        if(_currentAnimationIndex==0 && _currentAnimation){
            TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
            t->setRotationAngle2(angle);
        }
    }
    else if(panel.equalsIgnoreCase("panel3")){
        rotationAngle3=angle;
        Serial.printf("Panel3 angle set to %d\n", angle);
    }
}

int LEDManager::getRotation(String panel) const {
    if(panel.equalsIgnoreCase("panel1")){
        return rotationAngle1;
    }
    else if(panel.equalsIgnoreCase("panel2")){
        return rotationAngle2;
    }
    return -1;
}

void LEDManager::setUpdateSpeed(unsigned long speed){
    if(speed>=10 && speed<=1500){
        ledUpdateInterval=speed;
        Serial.printf("LED update speed set to %lu ms\n", speed);
        if(_currentAnimationIndex==0 && _currentAnimation){
            TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
            t->setUpdateInterval(speed);
        }
        else if(_currentAnimationIndex==1 && _currentAnimation){
            BlinkAnimation* bA = static_cast<BlinkAnimation*>(_currentAnimation);
            bA->setInterval(speed);
        }
        else if(_currentAnimationIndex==2 && _currentAnimation){
            RainbowWaveAnimation* wA = static_cast<RainbowWaveAnimation*>(_currentAnimation);
            
            float effectiveSpeed = 250.0f + (speed - 10.0f) * (1250.0f / 1490.0f);
            
            float speedValue = constrain(effectiveSpeed, 250, 1500);
            float speedMultiplier = 3.0f - ((speedValue - 250) / 1250.0f * 2.5f);
            
            wA->setSpeedMultiplier(speedMultiplier);
            
            Serial.printf("RainbowWave effective speed: %.0fms, multiplier: %.2f\n", 
                speedValue, speedMultiplier);
        }
    }
}
unsigned long LEDManager::getUpdateSpeed() const {
    return ledUpdateInterval;
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
