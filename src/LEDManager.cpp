#include "LEDManager.h"
#include "config.h"
#include <Arduino.h>
#include <FastLED.h>
#include <typeinfo>
#include <ctype.h>
#include "LogManager.h"

// Include Animation header files
#include "animations/TrafficAnimation.h"
#include "animations/BlinkAnimation.h"
#include "animations/RainbowWaveAnimation.h"
#include "animations/FireworkAnimation.h"
#include "animations/GameOfLifeAnimation.h"
#include "animations/LangtonsAntAnimation.h"
#include "animations/SierpinskiCarpetAnimation.h"

// Animations
#include "animations/BaseAnimation.h"

// Up to 8 panels of 16×16
CRGB leds[MAX_LEDS];

#define LEDMANAGER_LOCK_OR_RETURN(timeoutMs)                         \
    LockGuard lock(*this, timeoutMs);                                \
    if (!lock.locked()) {                                            \
        Serial.printf("%s failed to obtain LEDManager mutex\n", __func__); \
        return;                                                      \
    }

#define LEDMANAGER_LOCK_OR_RETURN_VALUE(timeoutMs, defaultValue)     \
    LockGuard lock(*this, timeoutMs);                                \
    if (!lock.locked()) {                                            \
        Serial.printf("%s failed to obtain LEDManager mutex\n", __func__); \
        return defaultValue;                                         \
    }

#define LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(timeoutMs, defaultValue)               \
    LockGuard lock(const_cast<LEDManager&>(*this), timeoutMs);                       \
    if (!lock.locked()) {                                                            \
        Serial.printf("%s failed to obtain LEDManager mutex\n", __func__);           \
        return defaultValue;                                                         \
    }

struct LifeRulePreset {
    const char* name;
    uint16_t birthMask;
    uint16_t surviveMask;
};

static const LifeRulePreset LIFE_RULES[] = {
    { "Life (B3/S23)", (1 << 3), (1 << 2) | (1 << 3) },
    { "HighLife (B36/S23)", (1 << 3) | (1 << 6), (1 << 2) | (1 << 3) },
    { "Seeds (B2/S)", (1 << 2), 0 },
    { "Maze (B3/S12345)", (1 << 3), (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) },
    { "Day & Night (B3678/S34678)", (1 << 3) | (1 << 6) | (1 << 7) | (1 << 8), (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 8) },
    { "Anneal (B4678/S35678)", (1 << 4) | (1 << 6) | (1 << 7) | (1 << 8), (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8) }
};

static const size_t LIFE_RULE_COUNT = sizeof(LIFE_RULES) / sizeof(LIFE_RULES[0]);

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
    , lifeSeedDensity(33)
    , lifeRuleIndex(0)
    , lifeWrapEdges(true)
    , lifeStagnationLimit(45)
    , lifeColorMode(0)
    , antRule("LR")
    , antCount(1)
    , antSteps(8)
    , antWrapEdges(true)
    , carpetDepth(4)
    , carpetInvert(false)
    , carpetColorShift(2)
    , fireworkMax(10)
    , fireworkParticles(40)
    , fireworkGravity(0.15f)
    , fireworkLaunchProbability(0.15f)
    , rainbowHueScale(4)
    , _isInitializing(true)
    , _stateMutex(xSemaphoreCreateRecursiveMutex())
{
    if (_stateMutex == nullptr) {
        Serial.println("LEDManager: Failed to create state mutex!");
    }

    createPalettes();
    _animationNames.push_back("Traffic");     // index=0
    _animationNames.push_back("Blink");       // index=1
    _animationNames.push_back("RainbowWave"); // index=2
    _animationNames.push_back("Firework");    // index=3
    _animationNames.push_back("GameOfLife");  // index=4
    _animationNames.push_back("LangtonsAnt"); // index=5
    _animationNames.push_back("SierpinskiCarpet"); // index=6
}

bool LEDManager::beginExclusiveAccess(uint32_t timeoutMs) const {
    if (_stateMutex == nullptr) {
        return true;
    }
    return xSemaphoreTakeRecursive(_stateMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void LEDManager::endExclusiveAccess() const {
    if (_stateMutex != nullptr) {
        xSemaphoreGiveRecursive(_stateMutex);
    }
}

LEDManager::LockGuard::LockGuard(LEDManager& manager, uint32_t timeoutMs)
    : _manager(manager)
    , _locked(manager.beginExclusiveAccess(timeoutMs)) {
}

LEDManager::LockGuard::~LockGuard() {
    if (_locked) {
        _manager.endExclusiveAccess();
    }
}

bool LEDManager::LockGuard::locked() const {
    return _locked;
}

void LEDManager::begin() {
    LEDMANAGER_LOCK_OR_RETURN(1000);
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
    LEDMANAGER_LOCK_OR_RETURN(1000);
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
    LEDMANAGER_LOCK_OR_RETURN(1000);
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
    LockGuard lock(*this, 0);
    if (!lock.locked()) {
        return;
    }

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
        anim->setUpdateInterval(ledUpdateInterval);
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
        anim->setHueScale(rainbowHueScale);
        
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
        anim->setUpdateInterval(ledUpdateInterval);
        anim->setMaxFireworks(fireworkMax);
        anim->setParticleCount(fireworkParticles);
        anim->setGravity(fireworkGravity);
        anim->setLaunchProbability(fireworkLaunchProbability);
    }
    else if (_currentAnimationIndex == 4) { // GameOfLife
        GameOfLifeAnimation* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setRotationAngle1(rotationAngle1);
        anim->setRotationAngle2(rotationAngle2);
        anim->setRotationAngle3(rotationAngle3);
        anim->setPanelOrder(panelOrder);
        anim->setSpeed(map(ledUpdateInterval, 3, 1500, 0, 255));
        anim->setAllPalettes(&ALL_PALETTES);
        anim->setCurrentPalette(currentPalette);
        if (lifeRuleIndex >= 0 && lifeRuleIndex < (int)LIFE_RULE_COUNT) {
            anim->setRuleMasks(LIFE_RULES[lifeRuleIndex].birthMask, LIFE_RULES[lifeRuleIndex].surviveMask);
        }
        anim->setSeedDensity(lifeSeedDensity);
        anim->setWrapMode(lifeWrapEdges);
        anim->setStagnationLimit(lifeStagnationLimit);
        anim->setColorMode(lifeColorMode);
    }
    else if (_currentAnimationIndex == 5) { // LangtonsAnt
        LangtonsAntAnimation* anim = static_cast<LangtonsAntAnimation*>(_currentAnimation);
        anim->setRotationAngle1(rotationAngle1);
        anim->setRotationAngle2(rotationAngle2);
        anim->setRotationAngle3(rotationAngle3);
        anim->setPanelOrder(panelOrder);
        anim->setUpdateInterval(ledUpdateInterval);
        anim->setRule(antRule);
        anim->setAntCount(antCount);
        anim->setStepsPerFrame(antSteps);
        anim->setWrapMode(antWrapEdges);
        anim->setPalette(&ALL_PALETTES[currentPalette]);
    }
    else if (_currentAnimationIndex == 6) { // SierpinskiCarpet
        SierpinskiCarpetAnimation* anim = static_cast<SierpinskiCarpetAnimation*>(_currentAnimation);
        anim->setRotationAngle1(rotationAngle1);
        anim->setRotationAngle2(rotationAngle2);
        anim->setRotationAngle3(rotationAngle3);
        anim->setPanelOrder(panelOrder);
        anim->setUpdateInterval(ledUpdateInterval);
        anim->setDepth(carpetDepth);
        anim->setInvert(carpetInvert);
        anim->setColorShift(carpetColorShift);
        anim->setPalette(&ALL_PALETTES[currentPalette]);
    }
}

void LEDManager::cleanupAnimation() {
    LEDMANAGER_LOCK_OR_RETURN(1000);
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
    LEDMANAGER_LOCK_OR_RETURN(1000);
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
            case 5: // LangtonsAnt
                _currentAnimation = new LangtonsAntAnimation(_numLeds, _brightness, _panelCount);
                break;
            case 6: // SierpinskiCarpet
                _currentAnimation = new SierpinskiCarpetAnimation(_numLeds, _brightness, _panelCount);
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
    LEDMANAGER_LOCK_OR_RETURN(1000);
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
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, _panelCount);
    return _panelCount;
}

void LEDManager::identifyPanels(){
    LEDMANAGER_LOCK_OR_RETURN(1000);
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
    LEDMANAGER_LOCK_OR_RETURN(1000);
    _brightness=b;
    FastLED.setBrightness(_brightness);

    if(_currentAnimation) {
        _currentAnimation->setBrightness(_brightness);
    }
}

uint8_t LEDManager::getBrightness() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, _brightness);
    return _brightness;
}

void LEDManager::setPalette(int idx){
    LEDMANAGER_LOCK_OR_RETURN(1000);
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
        else if(_currentAnimationIndex==4 && _currentAnimation){
            auto* g = static_cast<GameOfLifeAnimation*>(_currentAnimation);
            g->setCurrentPalette(currentPalette);
        }
        else if(_currentAnimationIndex==5 && _currentAnimation){
            auto* a = static_cast<LangtonsAntAnimation*>(_currentAnimation);
            a->setPalette(&ALL_PALETTES[currentPalette]);
        }
        else if(_currentAnimationIndex==6 && _currentAnimation){
            auto* s = static_cast<SierpinskiCarpetAnimation*>(_currentAnimation);
            s->setPalette(&ALL_PALETTES[currentPalette]);
        }
    }
}

int LEDManager::getCurrentPalette() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, currentPalette);
    return currentPalette;
}
size_t LEDManager::getPaletteCount() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, 0);
    return PALETTE_NAMES.size();
}
String LEDManager::getPaletteNameAt(int i) const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, String("Unknown"));
    if(i>=0 && i<(int)PALETTE_NAMES.size()){
        return PALETTE_NAMES[i];
    }
    return "Unknown";
}
const std::vector<CRGB>& LEDManager::getCurrentPaletteColors() const {
    static std::vector<CRGB> dummy;
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, dummy);
    if (currentPalette<0 || currentPalette>=(int)ALL_PALETTES.size()){
        return dummy;
    }
    return ALL_PALETTES[currentPalette];
}

void LEDManager::setSpawnRate(float r){
    LEDMANAGER_LOCK_OR_RETURN(1000);
    spawnRate=r;
    if(_currentAnimationIndex==0 && _currentAnimation){
        TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
        t->setSpawnRate(r);
    }
}
float LEDManager::getSpawnRate() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, spawnRate);
    return spawnRate;
}

void LEDManager::setMaxFlakes(int m){
    LEDMANAGER_LOCK_OR_RETURN(1000);
    maxFlakes=m;
    if(_currentAnimationIndex==0 && _currentAnimation){
        TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
        t->setMaxCars(m);
    }
}
int LEDManager::getMaxFlakes() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, maxFlakes);
    return maxFlakes;
}

void LEDManager::setTailLength(int l){
    LEDMANAGER_LOCK_OR_RETURN(1000);
    tailLength=l;
    if(_currentAnimationIndex==0 && _currentAnimation){
        TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
        t->setTailLength(l);
    }
}
int LEDManager::getTailLength() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, tailLength);
    return tailLength;
}

void LEDManager::setFadeAmount(uint8_t a){
    LEDMANAGER_LOCK_OR_RETURN(1000);
    fadeAmount=a;
    if(_currentAnimationIndex==0 && _currentAnimation){
        TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
        t->setFadeAmount(a);
    }
}
uint8_t LEDManager::getFadeAmount() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, fadeAmount);
    return fadeAmount;
}

void LEDManager::swapPanels(){
    LEDMANAGER_LOCK_OR_RETURN(1000);
    panelOrder=1-panelOrder;
    Serial.println("Panels swapped successfully.");
    if(!_currentAnimation) {
        return;
    }

    if(_currentAnimationIndex==0){
        auto* t = static_cast<TrafficAnimation*>(_currentAnimation);
        t->setPanelOrder(panelOrder);
    } else if(_currentAnimationIndex==2){
        auto* w = static_cast<RainbowWaveAnimation*>(_currentAnimation);
        w->setPanelOrder(panelOrder);
    } else if(_currentAnimationIndex==3){
        auto* f = static_cast<FireworkAnimation*>(_currentAnimation);
        f->setPanelOrder(panelOrder);
    } else if(_currentAnimationIndex==4){
        auto* g = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        g->setPanelOrder(panelOrder);
    } else if(_currentAnimationIndex==5){
        auto* a = static_cast<LangtonsAntAnimation*>(_currentAnimation);
        a->setPanelOrder(panelOrder);
    } else if(_currentAnimationIndex==6){
        auto* s = static_cast<SierpinskiCarpetAnimation*>(_currentAnimation);
        s->setPanelOrder(panelOrder);
    }
}
void LEDManager::setPanelOrder(String order){
    LEDMANAGER_LOCK_OR_RETURN(1000);
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
    if(!_currentAnimation) {
        return;
    }

    if(_currentAnimationIndex==0){
        auto* t = static_cast<TrafficAnimation*>(_currentAnimation);
        t->setPanelOrder(panelOrder);
    } else if(_currentAnimationIndex==2){
        auto* w = static_cast<RainbowWaveAnimation*>(_currentAnimation);
        w->setPanelOrder(panelOrder);
    } else if(_currentAnimationIndex==3){
        auto* f = static_cast<FireworkAnimation*>(_currentAnimation);
        f->setPanelOrder(panelOrder);
    } else if(_currentAnimationIndex==4){
        auto* g = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        g->setPanelOrder(panelOrder);
    } else if(_currentAnimationIndex==5){
        auto* a = static_cast<LangtonsAntAnimation*>(_currentAnimation);
        a->setPanelOrder(panelOrder);
    } else if(_currentAnimationIndex==6){
        auto* s = static_cast<SierpinskiCarpetAnimation*>(_currentAnimation);
        s->setPanelOrder(panelOrder);
    }
}

int LEDManager::getPanelOrder() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, panelOrder);
    return panelOrder;
}

void LEDManager::rotatePanel(String panel, int angle){
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if(!(angle==0||angle==90||angle==180||angle==270)){
        Serial.printf("Invalid rotation angle: %d\n", angle);
        return;
    }
    if(panel.equalsIgnoreCase("panel1")){
        rotationAngle1=angle;
        Serial.printf("Panel1 angle set to %d\n", angle);
        if(_currentAnimation){
            if(_currentAnimationIndex==0){
                static_cast<TrafficAnimation*>(_currentAnimation)->setRotationAngle1(angle);
            } else if(_currentAnimationIndex==2){
                static_cast<RainbowWaveAnimation*>(_currentAnimation)->setRotationAngle1(angle);
            } else if(_currentAnimationIndex==3){
                static_cast<FireworkAnimation*>(_currentAnimation)->setRotationAngle1(angle);
            } else if(_currentAnimationIndex==4){
                static_cast<GameOfLifeAnimation*>(_currentAnimation)->setRotationAngle1(angle);
            } else if(_currentAnimationIndex==5){
                static_cast<LangtonsAntAnimation*>(_currentAnimation)->setRotationAngle1(angle);
            } else if(_currentAnimationIndex==6){
                static_cast<SierpinskiCarpetAnimation*>(_currentAnimation)->setRotationAngle1(angle);
            }
        }
    }
    else if(panel.equalsIgnoreCase("panel2")){
        rotationAngle2=angle;
        Serial.printf("Panel2 angle set to %d\n", angle);
        if(_currentAnimation){
            if(_currentAnimationIndex==0){
                static_cast<TrafficAnimation*>(_currentAnimation)->setRotationAngle2(angle);
            } else if(_currentAnimationIndex==2){
                static_cast<RainbowWaveAnimation*>(_currentAnimation)->setRotationAngle2(angle);
            } else if(_currentAnimationIndex==3){
                static_cast<FireworkAnimation*>(_currentAnimation)->setRotationAngle2(angle);
            } else if(_currentAnimationIndex==4){
                static_cast<GameOfLifeAnimation*>(_currentAnimation)->setRotationAngle2(angle);
            } else if(_currentAnimationIndex==5){
                static_cast<LangtonsAntAnimation*>(_currentAnimation)->setRotationAngle2(angle);
            } else if(_currentAnimationIndex==6){
                static_cast<SierpinskiCarpetAnimation*>(_currentAnimation)->setRotationAngle2(angle);
            }
        }
    }
    else if(panel.equalsIgnoreCase("panel3")){
        rotationAngle3=angle;
        Serial.printf("Panel3 angle set to %d\n", angle);
        if(_currentAnimation){
            if(_currentAnimationIndex==0){
                static_cast<TrafficAnimation*>(_currentAnimation)->setRotationAngle3(angle);
            } else if(_currentAnimationIndex==2){
                static_cast<RainbowWaveAnimation*>(_currentAnimation)->setRotationAngle3(angle);
            } else if(_currentAnimationIndex==3){
                static_cast<FireworkAnimation*>(_currentAnimation)->setRotationAngle3(angle);
            } else if(_currentAnimationIndex==4){
                static_cast<GameOfLifeAnimation*>(_currentAnimation)->setRotationAngle3(angle);
            } else if(_currentAnimationIndex==5){
                static_cast<LangtonsAntAnimation*>(_currentAnimation)->setRotationAngle3(angle);
            } else if(_currentAnimationIndex==6){
                static_cast<SierpinskiCarpetAnimation*>(_currentAnimation)->setRotationAngle3(angle);
            }
        }
    }
}

int LEDManager::getRotation(String panel) const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, -1);
    if(panel.equalsIgnoreCase("panel1")){
        return rotationAngle1;
    }
    else if(panel.equalsIgnoreCase("panel2")){
        return rotationAngle2;
    }
    else if(panel.equalsIgnoreCase("panel3")){
        return rotationAngle3;
    }
    return -1;
}

void LEDManager::setUpdateSpeed(unsigned long speed){
    if(speed>=3 && speed<=1500){
        LEDMANAGER_LOCK_OR_RETURN(1000);
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
        else if(_currentAnimationIndex==3 && _currentAnimation){
            auto* f = static_cast<FireworkAnimation*>(_currentAnimation);
            f->setUpdateInterval(speed);
        }
        else if(_currentAnimationIndex==4 && _currentAnimation){
            auto* g = static_cast<GameOfLifeAnimation*>(_currentAnimation);
            uint8_t mapped = map((int)speed, 3, 1500, 0, 255);
            g->setSpeed(mapped);
        }
        else if(_currentAnimationIndex==5 && _currentAnimation){
            auto* a = static_cast<LangtonsAntAnimation*>(_currentAnimation);
            a->setUpdateInterval(speed);
        }
        else if(_currentAnimationIndex==6 && _currentAnimation){
            auto* s = static_cast<SierpinskiCarpetAnimation*>(_currentAnimation);
            s->setUpdateInterval(speed);
        }
    }
}
unsigned long LEDManager::getUpdateSpeed() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, ledUpdateInterval);
    return ledUpdateInterval;
}

void LEDManager::setLifeSeedDensity(uint8_t density) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if (density > 100) density = 100;
    lifeSeedDensity = density;
    if (_currentAnimationIndex == 4 && _currentAnimation) {
        auto* g = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        g->setSeedDensity(lifeSeedDensity);
        g->reseed();
    }
}

uint8_t LEDManager::getLifeSeedDensity() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, lifeSeedDensity);
    return lifeSeedDensity;
}

void LEDManager::setLifeRuleIndex(int index) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if (index < 0 || index >= (int)LIFE_RULE_COUNT) {
        return;
    }
    lifeRuleIndex = index;
    if (_currentAnimationIndex == 4 && _currentAnimation) {
        auto* g = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        g->setRuleMasks(LIFE_RULES[index].birthMask, LIFE_RULES[index].surviveMask);
    }
}

int LEDManager::getLifeRuleIndex() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, lifeRuleIndex);
    return lifeRuleIndex;
}

size_t LEDManager::getLifeRuleCount() const {
    return LIFE_RULE_COUNT;
}

String LEDManager::getLifeRuleName(int index) const {
    if (index < 0 || index >= (int)LIFE_RULE_COUNT) {
        return "Unknown";
    }
    return String(LIFE_RULES[index].name);
}

void LEDManager::setLifeWrap(bool wrap) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    lifeWrapEdges = wrap;
    if (_currentAnimationIndex == 4 && _currentAnimation) {
        auto* g = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        g->setWrapMode(lifeWrapEdges);
    }
}

bool LEDManager::getLifeWrap() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, lifeWrapEdges);
    return lifeWrapEdges;
}

void LEDManager::setLifeStagnationLimit(uint16_t limit) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if (limit < 5) limit = 5;
    if (limit > 250) limit = 250;
    lifeStagnationLimit = limit;
    if (_currentAnimationIndex == 4 && _currentAnimation) {
        auto* g = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        g->setStagnationLimit(lifeStagnationLimit);
    }
}

uint16_t LEDManager::getLifeStagnationLimit() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, lifeStagnationLimit);
    return lifeStagnationLimit;
}

void LEDManager::setLifeColorMode(uint8_t mode) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if (mode > 2) mode = 0;
    lifeColorMode = mode;
    if (_currentAnimationIndex == 4 && _currentAnimation) {
        auto* g = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        g->setColorMode(lifeColorMode);
    }
}

uint8_t LEDManager::getLifeColorMode() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, lifeColorMode);
    return lifeColorMode;
}

void LEDManager::lifeReseed() {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if (_currentAnimationIndex == 4 && _currentAnimation) {
        auto* g = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        g->reseed();
    }
}

void LEDManager::setAntRule(const String& rule) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    String cleaned;
    cleaned.reserve(rule.length());
    for (size_t i = 0; i < rule.length(); i++) {
        char c = (char)toupper(rule.charAt(i));
        if (c == 'L' || c == 'R') {
            cleaned += c;
        }
        if (cleaned.length() >= 8) {
            break;
        }
    }
    if (cleaned.length() == 0) {
        cleaned = "LR";
    }
    antRule = cleaned;
    if (_currentAnimationIndex == 5 && _currentAnimation) {
        auto* a = static_cast<LangtonsAntAnimation*>(_currentAnimation);
        a->setRule(antRule);
    }
}

String LEDManager::getAntRule() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, antRule);
    return antRule;
}

void LEDManager::setAntCount(uint8_t count) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if (count < 1) count = 1;
    if (count > 6) count = 6;
    antCount = count;
    if (_currentAnimationIndex == 5 && _currentAnimation) {
        auto* a = static_cast<LangtonsAntAnimation*>(_currentAnimation);
        a->setAntCount(antCount);
    }
}

uint8_t LEDManager::getAntCount() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, antCount);
    return antCount;
}

void LEDManager::setAntSteps(uint8_t steps) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if (steps < 1) steps = 1;
    if (steps > 50) steps = 50;
    antSteps = steps;
    if (_currentAnimationIndex == 5 && _currentAnimation) {
        auto* a = static_cast<LangtonsAntAnimation*>(_currentAnimation);
        a->setStepsPerFrame(antSteps);
    }
}

uint8_t LEDManager::getAntSteps() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, antSteps);
    return antSteps;
}

void LEDManager::setAntWrap(bool wrap) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    antWrapEdges = wrap;
    if (_currentAnimationIndex == 5 && _currentAnimation) {
        auto* a = static_cast<LangtonsAntAnimation*>(_currentAnimation);
        a->setWrapMode(antWrapEdges);
    }
}

bool LEDManager::getAntWrap() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, antWrapEdges);
    return antWrapEdges;
}

void LEDManager::setCarpetDepth(uint8_t depth) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if (depth < 1) depth = 1;
    if (depth > 6) depth = 6;
    carpetDepth = depth;
    if (_currentAnimationIndex == 6 && _currentAnimation) {
        auto* s = static_cast<SierpinskiCarpetAnimation*>(_currentAnimation);
        s->setDepth(carpetDepth);
    }
}

uint8_t LEDManager::getCarpetDepth() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, carpetDepth);
    return carpetDepth;
}

void LEDManager::setCarpetInvert(bool invert) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    carpetInvert = invert;
    if (_currentAnimationIndex == 6 && _currentAnimation) {
        auto* s = static_cast<SierpinskiCarpetAnimation*>(_currentAnimation);
        s->setInvert(carpetInvert);
    }
}

bool LEDManager::getCarpetInvert() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, carpetInvert);
    return carpetInvert;
}

void LEDManager::setCarpetColorShift(uint8_t shift) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if (shift < 1) shift = 1;
    if (shift > 20) shift = 20;
    carpetColorShift = shift;
    if (_currentAnimationIndex == 6 && _currentAnimation) {
        auto* s = static_cast<SierpinskiCarpetAnimation*>(_currentAnimation);
        s->setColorShift(carpetColorShift);
    }
}

uint8_t LEDManager::getCarpetColorShift() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, carpetColorShift);
    return carpetColorShift;
}

void LEDManager::setFireworkMax(int maxFireworks) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if (maxFireworks < 1) maxFireworks = 1;
    if (maxFireworks > 25) maxFireworks = 25;
    fireworkMax = maxFireworks;
    if (_currentAnimationIndex == 3 && _currentAnimation) {
        auto* f = static_cast<FireworkAnimation*>(_currentAnimation);
        f->setMaxFireworks(fireworkMax);
    }
}

int LEDManager::getFireworkMax() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, fireworkMax);
    return fireworkMax;
}

void LEDManager::setFireworkParticles(int count) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if (count < 10) count = 10;
    if (count > 120) count = 120;
    fireworkParticles = count;
    if (_currentAnimationIndex == 3 && _currentAnimation) {
        auto* f = static_cast<FireworkAnimation*>(_currentAnimation);
        f->setParticleCount(fireworkParticles);
    }
}

int LEDManager::getFireworkParticles() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, fireworkParticles);
    return fireworkParticles;
}

void LEDManager::setFireworkGravity(float gravity) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if (gravity < 0.01f) gravity = 0.01f;
    if (gravity > 0.5f) gravity = 0.5f;
    fireworkGravity = gravity;
    if (_currentAnimationIndex == 3 && _currentAnimation) {
        auto* f = static_cast<FireworkAnimation*>(_currentAnimation);
        f->setGravity(fireworkGravity);
    }
}

float LEDManager::getFireworkGravity() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, fireworkGravity);
    return fireworkGravity;
}

void LEDManager::setFireworkLaunchProbability(float probability) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if (probability < 0.01f) probability = 0.01f;
    if (probability > 1.0f) probability = 1.0f;
    fireworkLaunchProbability = probability;
    if (_currentAnimationIndex == 3 && _currentAnimation) {
        auto* f = static_cast<FireworkAnimation*>(_currentAnimation);
        f->setLaunchProbability(fireworkLaunchProbability);
    }
}

float LEDManager::getFireworkLaunchProbability() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, fireworkLaunchProbability);
    return fireworkLaunchProbability;
}

void LEDManager::setRainbowHueScale(uint8_t scale) {
    LEDMANAGER_LOCK_OR_RETURN(1000);
    if (scale < 1) scale = 1;
    if (scale > 12) scale = 12;
    rainbowHueScale = scale;
    if (_currentAnimationIndex == 2 && _currentAnimation) {
        auto* w = static_cast<RainbowWaveAnimation*>(_currentAnimation);
        w->setHueScale(rainbowHueScale);
    }
}

uint8_t LEDManager::getRainbowHueScale() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, rainbowHueScale);
    return rainbowHueScale;
}

int LEDManager::getAnimation() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, _currentAnimationIndex);
    return _currentAnimationIndex;
}
size_t LEDManager::getAnimationCount() const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, 0);
    return _animationNames.size();
}
String LEDManager::getAnimationName(int animIndex) const {
    LEDMANAGER_LOCK_CONST_OR_RETURN_VALUE(1000, String("Unknown"));
    if (animIndex>=0 && animIndex<(int)_animationNames.size()){
        return _animationNames[animIndex];
    }
    return "Unknown";
}
