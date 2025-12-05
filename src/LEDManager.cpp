#include "LEDManager.h"
#include "config.h"
#include <Arduino.h>
#include <FastLED.h>
#include <typeinfo>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cctype>
#include <freertos/semphr.h>
#include "LogManager.h"

// Include Animation header files
#include "animations/TrafficAnimation.h"
#include "animations/BlinkAnimation.h"
#include "animations/RainbowWaveAnimation.h"
#include "animations/FireworkAnimation.h"
#include "animations/GameOfLifeAnimation.h"
#include "animations/EscherAutomataAnimation.h"
#include "animations/TextScrollAnimation.h"

// Animations
#include "animations/BaseAnimation.h"

// Up to 8 panels of 16x16
CRGB leds[MAX_LEDS];

LEDManager::LEDManager()
    : _panelCount(DEFAULT_PANEL_COUNT)
    , _numLeds(_panelCount * 16 * 16)
    , _brightness(DEFAULT_BRIGHTNESS)
    , currentPalette(0)
    , _customPaletteIndex(-1)
    , _currentAnimation(nullptr)
    , _currentAnimationIndex(-1)
    , spawnRate(1.0f)
    , maxFlakes(500)
    , tailLength(3)
    , fadeAmount(39)
    , panelOrder(1)  // Start with right panel first instead of left
    , rotationAngle1(270)
    , rotationAngle2(270)
    , rotationAngle3(270)
    , ledUpdateInterval(38)
    , lastLedUpdate(0)
    , _isInitializing(true)
    , _speed(30)
    , _columnSkip(6) // Midpoint sweep speed by default
    , _golHighlightWidth(2)
    , _golHighlightColor(CRGB::White)
    , _automataMode(0)
    , _automataPrimary(50)
    , _automataSecondary(55)
    , _textScrollMode(0)
    , _textScrollSpeed(60)
    , _textScrollDirection(0)
    , _textMirrorGlyphs(false)
    , _textCompactHeight(false)
    , _textReverseOrder(false)
    , _textPrimary("[TIME]")
    , _textLeft("LEFT")
    , _textRight("[DATE]")
    , _mutex(nullptr)
    , _prefsReady(false)
{
    createPalettes();
    _animationNames.push_back("Traffic");     // index=0
    _animationNames.push_back("Blink");       // index=1
    _animationNames.push_back("RainbowWave"); // index=2
    _animationNames.push_back("Firework");    // index=3
    _animationNames.push_back("GameOfLife");  // index=4
    _animationNames.push_back("StrangeLoop"); // index=5
    _animationNames.push_back("TextTicker");  // index=6

    _golUpdateMode = 0;
    _golNeighborMode = 1; // default to 8-connected
    _golWrapEdges = true;
    _golClusterColorMode = 0;
    _golBirthMask = (1 << 3);
    _golSurviveMask = (1 << 2) | (1 << 3);
    _golRuleString = "B3/S23";
    _golSeedDensity = 33;
    _golMutationChance = 0;
    _golUniformBirths = false;
    _golBirthColor = CRGB::White;

    if (_mutex == nullptr) {
        _mutex = xSemaphoreCreateRecursiveMutex();
    }
}

void LEDManager::begin() {
    if (!_prefsReady) {
        _prefsReady = _prefs.begin("ledmgr", false);
        if (_prefsReady) {
            uint8_t savedPanels = _prefs.getUChar("panelCount", static_cast<uint8_t>(_panelCount));
            if (savedPanels < 1) {
                savedPanels = 1;
            } else if (savedPanels > 8) {
                savedPanels = 8;
            }
            _panelCount = savedPanels;
            _numLeds = _panelCount * 16 * 16;
            loadCustomPaletteFromPrefs();
            loadUserPalettesFromPrefs();
        }
    }

    reinitFastLED();
    showBootPattern();
    showLoadingAnimation();
}

void LEDManager::showBootPattern() {
    FastLED.clear(true);

    static const CRGB bootColors[] = {
        CRGB::Red,
        CRGB::Green,
        CRGB::Blue,
        CRGB::Yellow
    };

    const int ledsPerPanel = 16 * 16;
    for (int panel = 0; panel < _panelCount; ++panel) {
        const CRGB color = bootColors[panel % (sizeof(bootColors) / sizeof(bootColors[0]))];
        const int startIndex = panel * ledsPerPanel;
        const int endIndex = std::min(startIndex + ledsPerPanel, static_cast<int>(_numLeds));
        for (int i = startIndex; i < endIndex; ++i) {
            leds[i] = color;
        }
    }
    FastLED.show();

    for (int step = 0; step < 3; ++step) {
        fadeToBlackBy(leds, _numLeds, 90);
        FastLED.show();
        delay(120);
    }

    FastLED.clear(true);
    FastLED.show();
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
    
    int desiredAnim = 4; // fallback to GoL if nothing saved
    if (_prefsReady || ensurePrefsReady()) {
        int stored = _prefs.getUChar("lastAnim", 4);
        if (stored >= 0 && stored < (int)_animationNames.size()) {
            desiredAnim = stored;
        }
    }

    setAnimation(desiredAnim);
    
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

bool LEDManager::lock(uint32_t timeoutMs) {
    if (_mutex == nullptr) {
        _mutex = xSemaphoreCreateRecursiveMutex();
        if (_mutex == nullptr) {
            return true; // Best effort if allocation fails
        }
    }
    return xSemaphoreTakeRecursive(_mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void LEDManager::unlock() {
    if (_mutex) {
        xSemaphoreGiveRecursive(_mutex);
    }
}

void LEDManager::configureCurrentAnimation() {
    if (!_currentAnimation) return;
    
    // Set common properties for all animations
    _currentAnimation->setBrightness(_brightness);
    
    // Configure the animation based on its type
    if (_currentAnimation->isBlink()) {
        BlinkAnimation* anim = static_cast<BlinkAnimation*>(_currentAnimation);
        // Direct setting for faster animations, especially for Blink
        anim->setInterval(ledUpdateInterval);
        Serial.printf("Blink animation speed set to interval: %lu ms\n", ledUpdateInterval);
    } 
    else if (_currentAnimation->isTraffic()) {
        TrafficAnimation* anim = static_cast<TrafficAnimation*>(_currentAnimation);
        anim->setUpdateInterval(ledUpdateInterval);
        Serial.printf("Traffic animation speed set to interval: %lu ms\n", ledUpdateInterval);
    } 
    else if (_currentAnimation->isRainbowWave()) {
        RainbowWaveAnimation* anim = static_cast<RainbowWaveAnimation*>(_currentAnimation);
        // RainbowWave needs two parameters - update interval AND speed multiplier
        anim->setUpdateInterval(8); // Base update rate stays constant
        
        // Convert to speed multiplier (higher speed = higher multiplier)
        float speedMultiplier = map(100 - (_speed), 0, 100, 300, 50) / 100.0f;
        anim->setSpeedMultiplier(speedMultiplier);
        Serial.printf("Rainbow animation speed set with multiplier: %.2f\n", speedMultiplier);
    } 
    else if (_currentAnimation->isGameOfLife()) {
        GameOfLifeAnimation* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setAllPalettes(&ALL_PALETTES);
        anim->setCurrentPalette(currentPalette);
        anim->setRotationAngle1(rotationAngle1);
        anim->setRotationAngle2(rotationAngle2);
        anim->setRotationAngle3(rotationAngle3);
        anim->setPanelOrder(panelOrder);
        anim->setWipeBarBrightness(fadeAmount);
        anim->setFadeStepControl(static_cast<uint8_t>(tailLength));
        anim->setHighlightWidth(_golHighlightWidth);
        anim->setHighlightColor(_golHighlightColor);
        anim->setUniformBirthColorEnabled(_golUniformBirths);
        anim->setUniformBirthColor(_golBirthColor);
        const auto mode = static_cast<GameOfLifeAnimation::UpdateMode>(_golUpdateMode);
        anim->setUpdateMode(mode);
        anim->setNeighborMode(static_cast<GameOfLifeAnimation::NeighborMode>(_golNeighborMode));
        anim->setWrapEdges(_golWrapEdges);
        anim->setRules(_golBirthMask, _golSurviveMask);
        anim->setClusterColorMode(static_cast<GameOfLifeAnimation::ClusterColorMode>(_golClusterColorMode));
        anim->setSeedDensity(_golSeedDensity);
        anim->setMutationChance(_golMutationChance);
        anim->setColorPulseMode(_currentAnimationIndex == 5);

        unsigned long baseInterval = ledUpdateInterval;
        if (baseInterval < 1UL) {
            baseInterval = 1UL;
        }

        const int minSetting = 1;
        const int maxSetting = 20;
        int sweepSetting = constrain(_columnSkip, minSetting, maxSetting);
        anim->setColumnSkip(sweepSetting);
        anim->setUpdateInterval(baseInterval);

        anim->setSpeedPercent(static_cast<uint8_t>(_speed));

        Serial.printf("Game of Life configured mode=%d baseInterval=%lu speed=%u%% sweepSetting=%d\n",
                      static_cast<int>(mode), baseInterval, _speed, sweepSetting);
    }
    else if (_currentAnimation->isAutomata()) {
        auto* anim = static_cast<EscherAutomataAnimation*>(_currentAnimation);
        anim->setAllPalettes(&ALL_PALETTES);
        anim->setCurrentPalette(currentPalette);
        anim->setUsePalette(true);
        anim->setPanelOrder(panelOrder);
        anim->setRotationAngle1(rotationAngle1);
        anim->setRotationAngle2(rotationAngle2);
        anim->setRotationAngle3(rotationAngle3);
        anim->setMode(static_cast<EscherAutomataAnimation::Mode>(_automataMode));
        anim->setPrimaryControl(_automataPrimary);
        anim->setSecondaryControl(_automataSecondary);

        unsigned long interval = ledUpdateInterval;
        if (interval < 12UL) {
            interval = 12UL;
        }
        anim->setUpdateInterval(interval);

        Serial.printf("StrangeLoop configured mode=%u primary=%u secondary=%u interval=%lu\n",
                      _automataMode, _automataPrimary, _automataSecondary, interval);
    }
    else if (_currentAnimation->isTextScroller()) {
        auto* anim = static_cast<TextScrollAnimation*>(_currentAnimation);
        anim->setPanelOrder(panelOrder);
        anim->setRotationAngle1(rotationAngle1);
        anim->setRotationAngle2(rotationAngle2);
        anim->setRotationAngle3(rotationAngle3);
        anim->setMode(static_cast<TextScrollAnimation::Mode>(_textScrollMode));
        anim->setScrollDirection(_textScrollDirection != 0);
        anim->setMirrorGlyphs(_textMirrorGlyphs);
        anim->setCompactHeight(_textCompactHeight);
        anim->setReverseTextOrder(_textReverseOrder);

        const uint16_t slowest = 480;
        const uint16_t fastest = 40;
        float normalized = static_cast<float>(_textScrollSpeed) / 100.0f;
        uint16_t interval = slowest - static_cast<uint16_t>((slowest - fastest) * normalized);
        if (interval < fastest) interval = fastest;
        anim->setScrollSpeed(interval);

        CRGB textColor = CRGB::White;
        if (!ALL_PALETTES.empty()) {
            const auto& palette = ALL_PALETTES[currentPalette % ALL_PALETTES.size()];
            if (!palette.empty()) {
                textColor = palette[0];
            }
        }
        anim->setTextColor(textColor);
        anim->setMessage(TextScrollAnimation::Slot::Primary, _textPrimary);
        anim->setMessage(TextScrollAnimation::Slot::Left, _textLeft);
        anim->setMessage(TextScrollAnimation::Slot::Right, _textRight);
    }
    else if (_currentAnimation->isFirework()) {
        FireworkAnimation* anim = static_cast<FireworkAnimation*>(_currentAnimation);
        anim->setUpdateInterval(ledUpdateInterval);
        Serial.printf("Firework animation speed set to interval: %lu ms\n", ledUpdateInterval);
    }
    
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
        uint16_t onMs = map(fadeAmount, 0, 200, 60, 1200);
        uint16_t offMs = map(tailLength, 0, 20, 60, 1200);
        anim->setOnDuration(onMs);
        anim->setOffDuration(offMs);
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
        anim->setSaturation(fadeAmount);
        long strideRaw = map(tailLength, 0, 20, 1, 12);
        if (strideRaw < 1) strideRaw = 1;
        if (strideRaw > 16) strideRaw = 16;
        anim->setHueStride(static_cast<uint8_t>(strideRaw));
    }
    else if (_currentAnimationIndex == 3) { // Firework
        FireworkAnimation* anim = static_cast<FireworkAnimation*>(_currentAnimation);
        anim->setRotationAngle1(rotationAngle1);
        anim->setRotationAngle2(rotationAngle2);
        anim->setRotationAngle3(rotationAngle3);
        anim->setPanelOrder(panelOrder);
        anim->setUpdateInterval(15);
        float gravity = 0.05f + (static_cast<float>(fadeAmount) / 255.0f) * 0.35f;
        anim->setGravity(gravity);
        long particleRaw = map(tailLength, 0, 20, 20, 200);
        if (particleRaw < 10) particleRaw = 10;
        if (particleRaw > 300) particleRaw = 300;
        anim->setParticleCount(static_cast<int>(particleRaw));
        int maxFireworks = maxFlakes / 20;
        if (maxFireworks < 1) maxFireworks = 1;
        if (maxFireworks > 30) maxFireworks = 30;
        anim->setMaxFireworks(maxFireworks);
        float probability = constrain(spawnRate, 0.01f, 1.0f);
        anim->setLaunchProbability(probability);
    }
    else if (_currentAnimationIndex == 4) { // GameOfLife
        GameOfLifeAnimation* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setRotationAngle1(rotationAngle1);
        anim->setRotationAngle2(rotationAngle2);
        anim->setRotationAngle3(rotationAngle3);
        anim->setPanelOrder(panelOrder);
        anim->setAllPalettes(&ALL_PALETTES);
        anim->setCurrentPalette(currentPalette);
        anim->setWipeBarBrightness(fadeAmount);
        anim->setFadeStepControl(static_cast<uint8_t>(tailLength));
        anim->setHighlightWidth(_golHighlightWidth);
        anim->setHighlightColor(_golHighlightColor);
        anim->setUniformBirthColorEnabled(_golUniformBirths);
        anim->setUniformBirthColor(_golBirthColor);
        anim->setColumnSkip(_columnSkip);
        anim->setUpdateMode(static_cast<GameOfLifeAnimation::UpdateMode>(_golUpdateMode));
        anim->setNeighborMode(static_cast<GameOfLifeAnimation::NeighborMode>(_golNeighborMode));
        anim->setWrapEdges(_golWrapEdges);
        anim->setRules(_golBirthMask, _golSurviveMask);
        anim->setClusterColorMode(static_cast<GameOfLifeAnimation::ClusterColorMode>(_golClusterColorMode));
        anim->setSeedDensity(_golSeedDensity);
        anim->setMutationChance(_golMutationChance);
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
        case 5: // StrangeLoop / automata showcase
            _currentAnimation = new EscherAutomataAnimation(_numLeds, _brightness, _panelCount);
            break;
        case 6: // Text ticker
            _currentAnimation = new TextScrollAnimation(_numLeds, _brightness, _panelCount);
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

    if (_prefsReady || ensurePrefsReady()) {
        _prefs.putUChar("lastAnim", static_cast<uint8_t>(_currentAnimationIndex));
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
    if (_prefsReady) {
        _prefs.putUChar("panelCount", static_cast<uint8_t>(_panelCount));
    }
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
            systemError("Unknown error recreating animation");
            
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
        { CRGB(18,110,246), CRGB(255,110,32), CRGB(240,45,45), CRGB(255,188,80), CRGB(0,190,120) },
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
        "BOG",
        "BOGR",
        "Cool Sunset",
        "Neon Tropical",
        "Galaxy",
        "Forest Fire",
        "Cotton Candy",
        "Sea Shore",
        "Fire and Ice",
        "Retro Arcade",
        "Royal Rainbow"
    };

    _customPalette = {
        CRGB::Red,
        CRGB::Green,
        CRGB::Blue,
        CRGB::White,
        CRGB(255, 128, 0)
    };

    ALL_PALETTES.push_back(_customPalette);
    PALETTE_NAMES.push_back("Custom");
    _customPaletteIndex = static_cast<int>(ALL_PALETTES.size()) - 1;
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
    if (idx < 0 || idx >= static_cast<int>(ALL_PALETTES.size())) {
        return;
    }

    currentPalette = idx;
    Serial.printf("Palette %d (%s) selected.\n", idx, PALETTE_NAMES[idx].c_str());

    // Reconfigure the active animation so palette-driven rendering updates immediately.
    if (_currentAnimation) {
        configureCurrentAnimation();
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
    if (r < 0.0f) r = 0.0f;
    if (r > 1.0f) r = 1.0f;
    spawnRate = r;
    if (_currentAnimationIndex == 0 && _currentAnimation){
        TrafficAnimation* t = static_cast<TrafficAnimation*>(_currentAnimation);
        t->setSpawnRate(r);
    }
    else if (_currentAnimationIndex == 3 && _currentAnimation) {
        FireworkAnimation* f = static_cast<FireworkAnimation*>(_currentAnimation);
        float probability = r;
        if (probability < 0.01f) probability = 0.01f;
        if (probability > 1.0f) probability = 1.0f;
        f->setLaunchProbability(probability);
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
    else if (_currentAnimationIndex==3 && _currentAnimation) {
        FireworkAnimation* f = static_cast<FireworkAnimation*>(_currentAnimation);
        int maxFireworks = m / 20;
        if (maxFireworks < 1) maxFireworks = 1;
        if (maxFireworks > 30) maxFireworks = 30;
        f->setMaxFireworks(maxFireworks);
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
    else if (_currentAnimationIndex==1 && _currentAnimation) {
        BlinkAnimation* b = static_cast<BlinkAnimation*>(_currentAnimation);
        uint16_t offMs = map(l, 0, 20, 60, 1200);
        b->setOffDuration(offMs);
    }
    else if (_currentAnimationIndex==2 && _currentAnimation) {
        RainbowWaveAnimation* r = static_cast<RainbowWaveAnimation*>(_currentAnimation);
        long strideRaw = map(l, 0, 20, 1, 12);
        if (strideRaw < 1) strideRaw = 1;
        if (strideRaw > 16) strideRaw = 16;
        r->setHueStride(static_cast<uint8_t>(strideRaw));
    }
    else if (_currentAnimationIndex==3 && _currentAnimation) {
        FireworkAnimation* f = static_cast<FireworkAnimation*>(_currentAnimation);
        long particleRaw = map(l, 0, 20, 20, 200);
        if (particleRaw < 10) particleRaw = 10;
        if (particleRaw > 300) particleRaw = 300;
        f->setParticleCount(static_cast<int>(particleRaw));
    }
    else if (_currentAnimationIndex==4 && _currentAnimation) {
        GameOfLifeAnimation* g = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        g->setFadeStepControl(static_cast<uint8_t>(l));
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
    else if (_currentAnimationIndex==1 && _currentAnimation) {
        BlinkAnimation* b = static_cast<BlinkAnimation*>(_currentAnimation);
        uint16_t onMs = map(a, 0, 200, 60, 1200);
        b->setOnDuration(onMs);
    }
    else if (_currentAnimationIndex==2 && _currentAnimation) {
        RainbowWaveAnimation* r = static_cast<RainbowWaveAnimation*>(_currentAnimation);
        r->setSaturation(a);
    }
    else if (_currentAnimationIndex==3 && _currentAnimation) {
        FireworkAnimation* f = static_cast<FireworkAnimation*>(_currentAnimation);
        float gravity = 0.05f + (static_cast<float>(a) / 255.0f) * 0.35f;
        f->setGravity(gravity);
    }
    else if(_currentAnimationIndex==4 && _currentAnimation){
        GameOfLifeAnimation* g = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        g->setWipeBarBrightness(a);
        Serial.printf("Game of Life wipe bar brightness set to %d\n", a);
    }
}
uint8_t LEDManager::getFadeAmount() const {
    return fadeAmount;
}

void LEDManager::swapPanels(){
    panelOrder=1-panelOrder;
    Serial.println("Panels swapped successfully.");
    if (_currentAnimation) {
        configureCurrentAnimation();
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
    if (_currentAnimation) {
        configureCurrentAnimation();
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
    }
    else if(panel.equalsIgnoreCase("panel2")){
        rotationAngle2=angle;
        Serial.printf("Panel2 angle set to %d\n", angle);
    }
    else if(panel.equalsIgnoreCase("panel3")){
        rotationAngle3=angle;
        Serial.printf("Panel3 angle set to %d\n", angle);
    }

    if (_currentAnimation) {
        configureCurrentAnimation();
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
    if(speed>= 10 && speed<=1500){
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
        else if(_currentAnimationIndex == 4 && _currentAnimation){
            GameOfLifeAnimation* gA = static_cast<GameOfLifeAnimation*>(_currentAnimation);
            gA->setUpdateInterval(speed);
            gA->setSpeedPercent(static_cast<uint8_t>(_speed));
            Serial.printf("Game of Life: update interval=%lu ms, ui speed=%u%%\n",
                          speed, _speed);
        }
    }
}

unsigned long LEDManager::getUpdateSpeed() const {
    return ledUpdateInterval;
}

// Game of Life column skip getter/setter
void LEDManager::setColumnSkip(int skip) {
    if (skip < 1) skip = 1;
    if (skip > 20) skip = 20;
    
    _columnSkip = skip;
    LogManager::getInstance().debug("Game of Life sweep speed set to " + String(_columnSkip));
    
    // If Game of Life is the current animation, update its column skip value
    if (_currentAnimation && _currentAnimationIndex == 4) { // 4 is GameOfLife
        GameOfLifeAnimation* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setColumnSkip(_columnSkip);
        anim->setUniformBirthColorEnabled(_golUniformBirths);
        anim->setUniformBirthColor(_golBirthColor);
    }
}

int LEDManager::getColumnSkip() const {
    return _columnSkip;
}

void LEDManager::setGoLHighlightWidth(uint8_t width) {
    if (width > 8) {
        width = 8;
    }
    _golHighlightWidth = width;
    if (_currentAnimation && _currentAnimationIndex == 4) {
        auto* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setHighlightWidth(_golHighlightWidth);
    }
}

uint8_t LEDManager::getGoLHighlightWidth() const {
    return _golHighlightWidth;
}

void LEDManager::setGoLHighlightColorRGB(uint8_t r, uint8_t g, uint8_t b) {
    _golHighlightColor = CRGB(r, g, b);
    if (_currentAnimation && _currentAnimationIndex == 4) {
        auto* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setHighlightColor(_golHighlightColor);
    }
}

bool LEDManager::setGoLHighlightColorHex(const String& hex) {
    String cleaned = hex;
    cleaned.trim();
    if (cleaned.startsWith("#")) {
        cleaned.remove(0, 1);
    }
    if (cleaned.length() != 6) {
        return false;
    }

    char* endPtr = nullptr;
    long value = strtol(cleaned.c_str(), &endPtr, 16);
    if (endPtr == cleaned.c_str() || value < 0 || value > 0xFFFFFF) {
        return false;
    }

    uint8_t r = static_cast<uint8_t>((value >> 16) & 0xFF);
    uint8_t g = static_cast<uint8_t>((value >> 8) & 0xFF);
    uint8_t b = static_cast<uint8_t>(value & 0xFF);
    setGoLHighlightColorRGB(r, g, b);
    return true;
}

String LEDManager::getGoLHighlightColorHex() const {
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", _golHighlightColor.r, _golHighlightColor.g, _golHighlightColor.b);
    return String(buffer);
}

void LEDManager::setGoLUniformBirths(bool enabled) {
    _golUniformBirths = enabled;
    LogManager::getInstance().info(String("Game of Life uniform births ") + (enabled ? "enabled" : "disabled"));
    if (_currentAnimation && _currentAnimationIndex == 4) {
        auto* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setUniformBirthColorEnabled(_golUniformBirths);
    }
}

bool LEDManager::getGoLUniformBirths() const {
    return _golUniformBirths;
}

void LEDManager::setGoLBirthColorRGB(uint8_t r, uint8_t g, uint8_t b) {
    _golBirthColor = CRGB(r, g, b);
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", _golBirthColor.r, _golBirthColor.g, _golBirthColor.b);
    LogManager::getInstance().info(String("Game of Life birth color set to ") + buffer);
    if (_currentAnimation && _currentAnimationIndex == 4) {
        auto* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setUniformBirthColor(_golBirthColor);
    }
}

bool LEDManager::setGoLBirthColorHex(const String& hex) {
    String cleaned = hex;
    cleaned.trim();
    if (cleaned.startsWith("#")) {
        cleaned.remove(0, 1);
    }
    if (cleaned.length() != 6) {
        return false;
    }

    char* endPtr = nullptr;
    long value = strtol(cleaned.c_str(), &endPtr, 16);
    if (endPtr == cleaned.c_str() || value < 0 || value > 0xFFFFFF) {
        return false;
    }

    uint8_t r = static_cast<uint8_t>((value >> 16) & 0xFF);
    uint8_t g = static_cast<uint8_t>((value >> 8) & 0xFF);
    uint8_t b = static_cast<uint8_t>(value & 0xFF);
    setGoLBirthColorRGB(r, g, b);
    return true;
}

String LEDManager::getGoLBirthColorHex() const {
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", _golBirthColor.r, _golBirthColor.g, _golBirthColor.b);
    return String(buffer);
}

void LEDManager::setAutomataMode(uint8_t mode) {
    if (mode > 2) {
        mode = 2;
    }
    _automataMode = mode;
    LogManager::getInstance().info("StrangeLoop mode set to " + String(_automataMode));
    if (_currentAnimation && _currentAnimation->isAutomata()) {
        auto* anim = static_cast<EscherAutomataAnimation*>(_currentAnimation);
        anim->setMode(static_cast<EscherAutomataAnimation::Mode>(_automataMode));
        configureCurrentAnimation();
    }
}

uint8_t LEDManager::getAutomataMode() const {
    return _automataMode;
}

void LEDManager::setAutomataPrimary(uint8_t value) {
    if (value > 100) {
        value = 100;
    }
    _automataPrimary = value;
    if (_currentAnimation && _currentAnimation->isAutomata()) {
        auto* anim = static_cast<EscherAutomataAnimation*>(_currentAnimation);
        anim->setPrimaryControl(_automataPrimary);
    }
}

uint8_t LEDManager::getAutomataPrimary() const {
    return _automataPrimary;
}

void LEDManager::setAutomataSecondary(uint8_t value) {
    if (value > 100) {
        value = 100;
    }
    _automataSecondary = value;
    if (_currentAnimation && _currentAnimation->isAutomata()) {
        auto* anim = static_cast<EscherAutomataAnimation*>(_currentAnimation);
        anim->setSecondaryControl(_automataSecondary);
    }
}

uint8_t LEDManager::getAutomataSecondary() const {
    return _automataSecondary;
}

void LEDManager::resetAutomataPattern() {
    if (_currentAnimation && _currentAnimation->isAutomata()) {
        auto* anim = static_cast<EscherAutomataAnimation*>(_currentAnimation);
        anim->begin();
        configureCurrentAnimation();
    }
}

void LEDManager::setTextScrollMode(uint8_t mode) {
    if (mode > 1) mode = 0;
    _textScrollMode = mode;
    if (_currentAnimation && _currentAnimation->isTextScroller()) {
        auto* anim = static_cast<TextScrollAnimation*>(_currentAnimation);
        anim->setMode(static_cast<TextScrollAnimation::Mode>(_textScrollMode));
    }
}

uint8_t LEDManager::getTextScrollMode() const {
    return _textScrollMode;
}

void LEDManager::setTextScrollDirection(uint8_t direction) {
    _textScrollDirection = direction ? 1 : 0;
    if (_currentAnimation && _currentAnimation->isTextScroller()) {
        auto* anim = static_cast<TextScrollAnimation*>(_currentAnimation);
        anim->setScrollDirection(_textScrollDirection != 0);
    }
}

uint8_t LEDManager::getTextScrollDirection() const {
    return _textScrollDirection;
}

void LEDManager::setTextMirrorGlyphs(bool mirror) {
    _textMirrorGlyphs = mirror;
    if (_currentAnimation && _currentAnimation->isTextScroller()) {
        auto* anim = static_cast<TextScrollAnimation*>(_currentAnimation);
        anim->setMirrorGlyphs(_textMirrorGlyphs);
    }
}

bool LEDManager::getTextMirrorGlyphs() const {
    return _textMirrorGlyphs;
}

void LEDManager::setTextCompactHeight(bool compact) {
    _textCompactHeight = compact;
    if (_currentAnimation && _currentAnimation->isTextScroller()) {
        auto* anim = static_cast<TextScrollAnimation*>(_currentAnimation);
        anim->setCompactHeight(_textCompactHeight);
    }
}

bool LEDManager::getTextCompactHeight() const {
    return _textCompactHeight;
}

void LEDManager::setTextReverseOrder(bool reverse) {
    _textReverseOrder = reverse;
    if (_currentAnimation && _currentAnimation->isTextScroller()) {
        auto* anim = static_cast<TextScrollAnimation*>(_currentAnimation);
        anim->setReverseTextOrder(_textReverseOrder);
    }
}

bool LEDManager::getTextReverseOrder() const {
    return _textReverseOrder;
}

void LEDManager::setTextScrollSpeed(uint8_t percent) {
    if (percent > 100) percent = 100;
    _textScrollSpeed = percent;
    if (_currentAnimation && _currentAnimation->isTextScroller()) {
        auto* anim = static_cast<TextScrollAnimation*>(_currentAnimation);
        const uint16_t slowest = 480;
        const uint16_t fastest = 40;
        float normalized = static_cast<float>(_textScrollSpeed) / 100.0f;
        uint16_t interval = slowest - static_cast<uint16_t>((slowest - fastest) * normalized);
        if (interval < fastest) interval = fastest;
        anim->setScrollSpeed(interval);
    }
}

uint8_t LEDManager::getTextScrollSpeed() const {
    return _textScrollSpeed;
}

void LEDManager::setTextScrollMessage(uint8_t slot, const String& text) {
    String sanitized = text;
    sanitized.trim();
    sanitized.replace("\r", "");
    sanitized.replace("\n", " ");
    if (sanitized.length() > 96) {
        sanitized = sanitized.substring(0, 96);
    }

    switch (slot) {
        case 0: _textPrimary = sanitized; break;
        case 1: _textLeft = sanitized; break;
        case 2: _textRight = sanitized; break;
        default: return;
    }

    if (_currentAnimation && _currentAnimation->isTextScroller()) {
        auto* anim = static_cast<TextScrollAnimation*>(_currentAnimation);
        auto textSlot = static_cast<TextScrollAnimation::Slot>(std::min<uint8_t>(slot, 2));
        anim->setMessage(textSlot, sanitized);
    }
}

String LEDManager::getTextScrollMessage(uint8_t slot) const {
    switch (slot) {
        case 0: return _textPrimary;
        case 1: return _textLeft;
        case 2: return _textRight;
        default: return "";
    }
}

void LEDManager::setGoLUpdateMode(uint8_t mode) {
    if (mode > 2) mode = 0;
    _golUpdateMode = mode;
    if (_currentAnimation && _currentAnimationIndex == 4) {
        auto* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setUpdateMode(static_cast<GameOfLifeAnimation::UpdateMode>(_golUpdateMode));
    }
}

uint8_t LEDManager::getGoLUpdateMode() const {
    return _golUpdateMode;
}

void LEDManager::setGoLNeighborMode(uint8_t mode) {
    if (mode > 1) mode = 1;
    _golNeighborMode = mode;
    if (_currentAnimation && _currentAnimationIndex == 4) {
        auto* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setNeighborMode(static_cast<GameOfLifeAnimation::NeighborMode>(_golNeighborMode));
    }
}

uint8_t LEDManager::getGoLNeighborMode() const {
    return _golNeighborMode;
}

void LEDManager::setGoLWrapEdges(bool wrap) {
    _golWrapEdges = wrap;
    if (_currentAnimation && _currentAnimationIndex == 4) {
        auto* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setWrapEdges(_golWrapEdges);
    }
}

bool LEDManager::getGoLWrapEdges() const {
    return _golWrapEdges;
}

static bool parseRulePart(const String& part, char prefix, uint16_t& mask) {
    if (part.length() == 0) return false;
    int idx = 0;
    if (std::toupper(static_cast<unsigned char>(part[idx])) == prefix) {
        ++idx;
    }
    mask = 0;
    for (; idx < part.length(); ++idx) {
        char c = part[idx];
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        int value = c - '0';
        if (value < 0 || value > 8) return false;
        mask |= (1 << value);
    }
    return true;
}

bool LEDManager::setGoLRules(const String& rule) {
    String cleaned = rule;
    cleaned.trim();
    cleaned.toUpperCase();
    cleaned.replace(" ", "");
    if (cleaned.isEmpty()) {
        return false;
    }

    int slash = cleaned.indexOf('/');
    String birthPart;
    String survivePart;
    if (slash >= 0) {
        birthPart = cleaned.substring(0, slash);
        survivePart = cleaned.substring(slash + 1);
    } else {
        birthPart = cleaned;
        survivePart = "";
    }

    uint16_t birthMask = _golBirthMask;
    uint16_t surviveMask = _golSurviveMask;

    if (!birthPart.isEmpty()) {
        if (!parseRulePart(birthPart, 'B', birthMask)) return false;
    }
    if (!survivePart.isEmpty()) {
        if (!parseRulePart(survivePart, 'S', surviveMask)) return false;
    }

    _golBirthMask = birthMask;
    _golSurviveMask = surviveMask;
    _golRuleString = "B";
    for (int i = 0; i <= 8; ++i) {
        if (birthMask & (1 << i)) {
            _golRuleString += String(i);
        }
    }
    _golRuleString += "/S";
    for (int i = 0; i <= 8; ++i) {
        if (surviveMask & (1 << i)) {
            _golRuleString += String(i);
        }
    }

    LogManager::getInstance().info("Game of Life rules set to " + _golRuleString);

    if (_currentAnimation && _currentAnimationIndex == 4) {
        auto* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setRules(_golBirthMask, _golSurviveMask);
    }
    return true;
}

String LEDManager::getGoLRules() const {
    return _golRuleString;
}

void LEDManager::setGoLClusterColorMode(uint8_t mode) {
    if (mode > 4) mode = 0;
    _golClusterColorMode = mode;
    if (_currentAnimation && _currentAnimationIndex == 4) {
        auto* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setClusterColorMode(static_cast<GameOfLifeAnimation::ClusterColorMode>(_golClusterColorMode));
    }
}

uint8_t LEDManager::getGoLClusterColorMode() const {
    return _golClusterColorMode;
}

void LEDManager::setGoLSeedDensity(uint8_t percent) {
    if (percent < 1) percent = 1;
    if (percent > 100) percent = 100;
    _golSeedDensity = percent;
    LogManager::getInstance().info("Game of Life starting fill set to " + String(_golSeedDensity) + "%");
    if (_currentAnimation && _currentAnimationIndex == 4) {
        auto* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setSeedDensity(_golSeedDensity);
    }
}

uint8_t LEDManager::getGoLSeedDensity() const {
    return _golSeedDensity;
}

void LEDManager::setGoLMutationChance(uint8_t percent) {
    if (percent > 100) percent = 100;
    _golMutationChance = percent;
    LogManager::getInstance().info("Game of Life mutation nudges set to " + String(_golMutationChance) + "%");
    if (_currentAnimation && _currentAnimationIndex == 4) {
        auto* anim = static_cast<GameOfLifeAnimation*>(_currentAnimation);
        anim->setMutationChance(_golMutationChance);
    }
}

uint8_t LEDManager::getGoLMutationChance() const {
    return _golMutationChance;
}

void LEDManager::setSpeed(int speed) {
    if (speed < 0 || speed > 100) return;
    
    // Validate and constrain input
    speed = constrain(speed, 0, 100);
    _speed = speed;

    const float minInterval = 0.3f;     // fastest allowed update (ms)
    const float maxInterval = 9000.0f;  // allow very slow sweeps
    const float percent = static_cast<float>(_speed) / 100.0f;
    const float ratio = minInterval / maxInterval;
  
    float interval = maxInterval * powf(ratio, percent);
    interval = constrain(interval, minInterval, maxInterval);
  
    ledUpdateInterval = static_cast<unsigned long>(interval);
    if (ledUpdateInterval < 1) {
        ledUpdateInterval = 1;
    }
    
    Serial.printf("Speed: %d%%, Interval: %.2fms\n", speed, interval);
    
    configureCurrentAnimation();
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

bool LEDManager::ensurePrefsReady() {
    if (_prefsReady) {
        return true;
    }
    _prefsReady = _prefs.begin("ledmgr", false);
    if (!_prefsReady) {
        LogManager::getInstance().error("Failed to open preferences namespace 'ledmgr'");
    }
    return _prefsReady;
}

String LEDManager::sanitizePresetName(const String& name) const {
    String trimmed = name;
    trimmed.trim();
    String sanitized;
    sanitized.reserve(trimmed.length());
    for (size_t i = 0; i < trimmed.length(); ++i) {
        char c = trimmed.charAt(i);
        if (std::isalnum(static_cast<unsigned char>(c)) ||
            c == ' ' || c == '_' || c == '-') {
            sanitized += c;
        }
    }
    sanitized.trim();
    if (sanitized.length() > 24) {
        sanitized = sanitized.substring(0, 24);
    }
    // Collapse consecutive spaces
    while (sanitized.indexOf("  ") != -1) {
        sanitized.replace("  ", " ");
    }
    return sanitized;
}

String LEDManager::makePresetStorageKey(const String& name) const {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < name.length(); ++i) {
        hash ^= static_cast<uint8_t>(name.charAt(i));
        hash *= 16777619u;
    }
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "pr_%08lX", static_cast<unsigned long>(hash));
    return String(buffer);
}

String LEDManager::legacyPresetStorageKey(const String& name) const {
    return "preset:" + name;
}

std::vector<String> LEDManager::loadPresetNameList() {
    std::vector<String> names;
    if (!ensurePrefsReady()) {
        return names;
    }
    String stored = _prefs.getString("presetNames", "");
    stored.trim();
    if (stored.isEmpty()) {
        return names;
    }
    int start = 0;
    while (start < stored.length()) {
        int sep = stored.indexOf(',', start);
        if (sep < 0) {
            sep = stored.length();
        }
        String entry = stored.substring(start, sep);
        entry.trim();
        if (!entry.isEmpty()) {
            names.push_back(entry);
        }
        start = sep + 1;
    }
    return names;
}

void LEDManager::storePresetNameList(const std::vector<String>& names) {
    if (!ensurePrefsReady()) {
        return;
    }
    String combined;
    for (size_t i = 0; i < names.size(); ++i) {
        combined += names[i];
        if (i + 1 < names.size()) {
            combined += ",";
        }
    }
    _prefs.putString("presetNames", combined);
}

LEDManager::PresetSnapshot LEDManager::captureCurrentPreset() const {
    PresetSnapshot snapshot;
    snapshot.panelCount = _panelCount;
    snapshot.animationIndex = _currentAnimationIndex;
    snapshot.paletteIndex = currentPalette;
    snapshot.brightness = _brightness;
    snapshot.fadeAmount = fadeAmount;
    snapshot.tailLength = tailLength;
    snapshot.speed = _speed;
    snapshot.spawnRate = spawnRate;
    snapshot.maxFlakes = maxFlakes;
    snapshot.columnSkip = _columnSkip;
    snapshot.highlightWidth = _golHighlightWidth;
    snapshot.highlightColorHex = getGoLHighlightColorHex();
    snapshot.uniformBirths = _golUniformBirths;
    snapshot.birthColorHex = getGoLBirthColorHex();
    snapshot.golUpdateMode = _golUpdateMode;
    snapshot.golNeighborMode = _golNeighborMode;
    snapshot.golWrapEdges = _golWrapEdges;
    snapshot.golClusterColorMode = _golClusterColorMode;
    snapshot.golSeedDensity = _golSeedDensity;
    snapshot.golMutationChance = _golMutationChance;
    snapshot.golRuleString = _golRuleString;
    snapshot.panelOrder = panelOrder;
    snapshot.rotation1 = rotationAngle1;
    snapshot.rotation2 = rotationAngle2;
    snapshot.rotation3 = rotationAngle3;
    snapshot.automataMode = _automataMode;
    snapshot.automataPrimary = _automataPrimary;
    snapshot.automataSecondary = _automataSecondary;
    snapshot.textMode = _textScrollMode;
    snapshot.textSpeed = _textScrollSpeed;
    snapshot.textDirection = _textScrollDirection;
    snapshot.textMirror = _textMirrorGlyphs;
    snapshot.textCompact = _textCompactHeight;
    snapshot.textReverse = _textReverseOrder;
    snapshot.textPrimary = _textPrimary;
    snapshot.textLeft = _textLeft;
    snapshot.textRight = _textRight;
    return snapshot;
}

String LEDManager::serializePreset(const PresetSnapshot& snapshot) const {
    String data;
    data.reserve(256);
    data += "panelCount=" + String(snapshot.panelCount) + "\n";
    data += "animation=" + String(snapshot.animationIndex) + "\n";
    data += "palette=" + String(snapshot.paletteIndex) + "\n";
    data += "brightness=" + String(snapshot.brightness) + "\n";
    data += "fade=" + String(snapshot.fadeAmount) + "\n";
    data += "tail=" + String(snapshot.tailLength) + "\n";
    data += "speed=" + String(snapshot.speed) + "\n";
    data += "spawnRate=" + String(snapshot.spawnRate, 4) + "\n";
    data += "maxFlakes=" + String(snapshot.maxFlakes) + "\n";
    data += "columnSkip=" + String(snapshot.columnSkip) + "\n";
    data += "highlightWidth=" + String(snapshot.highlightWidth) + "\n";
    data += "highlightColor=" + snapshot.highlightColorHex + "\n";
    data += "uniformBirths=" + String(snapshot.uniformBirths ? 1 : 0) + "\n";
    data += "uniformBirthColor=" + snapshot.birthColorHex + "\n";
    data += "golUpdateMode=" + String(snapshot.golUpdateMode) + "\n";
    data += "golNeighborMode=" + String(snapshot.golNeighborMode) + "\n";
    data += "golWrapEdges=" + String(snapshot.golWrapEdges ? 1 : 0) + "\n";
    data += "golClusterMode=" + String(snapshot.golClusterColorMode) + "\n";
    data += "golSeedDensity=" + String(snapshot.golSeedDensity) + "\n";
    data += "golMutationChance=" + String(snapshot.golMutationChance) + "\n";
   data += "golRule=" + snapshot.golRuleString + "\n";
   data += "panelOrder=" + String(snapshot.panelOrder) + "\n";
   data += "rotation1=" + String(snapshot.rotation1) + "\n";
   data += "rotation2=" + String(snapshot.rotation2) + "\n";
    data += "rotation3=" + String(snapshot.rotation3) + "\n";
    data += "automataMode=" + String(snapshot.automataMode) + "\n";
    data += "automataPrimary=" + String(snapshot.automataPrimary) + "\n";
    data += "automataSecondary=" + String(snapshot.automataSecondary) + "\n";
    data += "textMode=" + String(snapshot.textMode) + "\n";
    data += "textSpeed=" + String(snapshot.textSpeed) + "\n";
    data += "textDirection=" + String(snapshot.textDirection) + "\n";
    data += "textMirror=" + String(snapshot.textMirror ? 1 : 0) + "\n";
    data += "textCompact=" + String(snapshot.textCompact ? 1 : 0) + "\n";
    data += "textReverse=" + String(snapshot.textReverse ? 1 : 0) + "\n";
    data += "textPrimary=" + snapshot.textPrimary + "\n";
    data += "textLeft=" + snapshot.textLeft + "\n";
    data += "textRight=" + snapshot.textRight + "\n";
    return data;
}

static bool parseBoolValue(const String& value, bool defaultValue) {
    if (value.equalsIgnoreCase("true") || value == "1") return true;
    if (value.equalsIgnoreCase("false") || value == "0") return false;
    return defaultValue;
}

bool LEDManager::deserializePreset(const String& payload, PresetSnapshot& snapshot) const {
    PresetSnapshot parsed = captureCurrentPreset();
    int start = 0;
    while (start < payload.length()) {
        int end = payload.indexOf('\n', start);
        if (end < 0) end = payload.length();
        String line = payload.substring(start, end);
        start = end + 1;
        line.trim();
        if (line.isEmpty()) continue;
        int eq = line.indexOf('=');
        if (eq < 0) continue;
        String key = line.substring(0, eq);
        String value = line.substring(eq + 1);
        key.trim();
        value.trim();

        if (key.equalsIgnoreCase("panelCount")) parsed.panelCount = value.toInt();
        else if (key.equalsIgnoreCase("animation")) parsed.animationIndex = value.toInt();
        else if (key.equalsIgnoreCase("palette")) parsed.paletteIndex = value.toInt();
        else if (key.equalsIgnoreCase("brightness")) parsed.brightness = static_cast<uint8_t>(value.toInt());
        else if (key.equalsIgnoreCase("fade")) parsed.fadeAmount = static_cast<uint8_t>(value.toInt());
        else if (key.equalsIgnoreCase("tail")) parsed.tailLength = value.toInt();
        else if (key.equalsIgnoreCase("speed")) parsed.speed = value.toInt();
        else if (key.equalsIgnoreCase("spawnRate")) parsed.spawnRate = value.toFloat();
        else if (key.equalsIgnoreCase("maxFlakes")) parsed.maxFlakes = value.toInt();
        else if (key.equalsIgnoreCase("columnSkip")) parsed.columnSkip = value.toInt();
        else if (key.equalsIgnoreCase("highlightWidth")) parsed.highlightWidth = static_cast<uint8_t>(value.toInt());
        else if (key.equalsIgnoreCase("highlightColor")) parsed.highlightColorHex = value;
        else if (key.equalsIgnoreCase("uniformBirths")) parsed.uniformBirths = parseBoolValue(value, parsed.uniformBirths);
        else if (key.equalsIgnoreCase("uniformBirthColor")) parsed.birthColorHex = value;
        else if (key.equalsIgnoreCase("golUpdateMode")) parsed.golUpdateMode = static_cast<uint8_t>(value.toInt());
        else if (key.equalsIgnoreCase("golNeighborMode")) parsed.golNeighborMode = static_cast<uint8_t>(value.toInt());
        else if (key.equalsIgnoreCase("golWrapEdges")) parsed.golWrapEdges = parseBoolValue(value, parsed.golWrapEdges);
        else if (key.equalsIgnoreCase("golClusterMode")) parsed.golClusterColorMode = static_cast<uint8_t>(value.toInt());
        else if (key.equalsIgnoreCase("golSeedDensity")) parsed.golSeedDensity = static_cast<uint8_t>(value.toInt());
        else if (key.equalsIgnoreCase("golMutationChance")) parsed.golMutationChance = static_cast<uint8_t>(value.toInt());
        else if (key.equalsIgnoreCase("golRule")) parsed.golRuleString = value;
        else if (key.equalsIgnoreCase("panelOrder")) parsed.panelOrder = value.toInt();
        else if (key.equalsIgnoreCase("rotation1")) parsed.rotation1 = value.toInt();
        else if (key.equalsIgnoreCase("rotation2")) parsed.rotation2 = value.toInt();
        else if (key.equalsIgnoreCase("rotation3")) parsed.rotation3 = value.toInt();
        else if (key.equalsIgnoreCase("automataMode")) parsed.automataMode = static_cast<uint8_t>(value.toInt());
        else if (key.equalsIgnoreCase("automataPrimary")) parsed.automataPrimary = static_cast<uint8_t>(value.toInt());
        else if (key.equalsIgnoreCase("automataSecondary")) parsed.automataSecondary = static_cast<uint8_t>(value.toInt());
        else if (key.equalsIgnoreCase("textMode")) parsed.textMode = static_cast<uint8_t>(value.toInt());
        else if (key.equalsIgnoreCase("textSpeed")) parsed.textSpeed = static_cast<uint8_t>(value.toInt());
        else if (key.equalsIgnoreCase("textDirection")) parsed.textDirection = static_cast<uint8_t>(value.toInt());
        else if (key.equalsIgnoreCase("textMirror")) parsed.textMirror = parseBoolValue(value, parsed.textMirror);
        else if (key.equalsIgnoreCase("textCompact")) parsed.textCompact = parseBoolValue(value, parsed.textCompact);
        else if (key.equalsIgnoreCase("textReverse")) parsed.textReverse = parseBoolValue(value, parsed.textReverse);
        else if (key.equalsIgnoreCase("textPrimary")) parsed.textPrimary = value;
        else if (key.equalsIgnoreCase("textLeft")) parsed.textLeft = value;
        else if (key.equalsIgnoreCase("textRight")) parsed.textRight = value;
    }
    snapshot = parsed;
    return true;
}

void LEDManager::applyPreset(const PresetSnapshot& snapshot) {
    if (snapshot.panelCount != _panelCount) {
        setPanelCount(snapshot.panelCount);
    }

    if (snapshot.animationIndex != _currentAnimationIndex) {
        setAnimation(snapshot.animationIndex);
    }

    setPalette(snapshot.paletteIndex);
    setBrightness(snapshot.brightness);
    setFadeAmount(snapshot.fadeAmount);
    setTailLength(snapshot.tailLength);
    setSpeed(snapshot.speed);
    setSpawnRate(snapshot.spawnRate);
    setMaxFlakes(snapshot.maxFlakes);
    setColumnSkip(snapshot.columnSkip);
    setGoLHighlightWidth(snapshot.highlightWidth);
    setGoLHighlightColorHex(snapshot.highlightColorHex);
    setGoLUniformBirths(snapshot.uniformBirths);
    setGoLBirthColorHex(snapshot.birthColorHex);
    setGoLUpdateMode(snapshot.golUpdateMode);
    setGoLNeighborMode(snapshot.golNeighborMode);
    setGoLWrapEdges(snapshot.golWrapEdges);
    setGoLClusterColorMode(snapshot.golClusterColorMode);
    setGoLSeedDensity(snapshot.golSeedDensity);
    setGoLMutationChance(snapshot.golMutationChance);
    setGoLRules(snapshot.golRuleString);
    setAutomataMode(snapshot.automataMode);
    setAutomataPrimary(snapshot.automataPrimary);
    setAutomataSecondary(snapshot.automataSecondary);
    setTextScrollMode(snapshot.textMode);
    setTextScrollSpeed(snapshot.textSpeed);
    setTextScrollDirection(snapshot.textDirection);
    setTextMirrorGlyphs(snapshot.textMirror);
    setTextCompactHeight(snapshot.textCompact);
    setTextReverseOrder(snapshot.textReverse);
    setTextScrollMessage(0, snapshot.textPrimary);
    setTextScrollMessage(1, snapshot.textLeft);
    setTextScrollMessage(2, snapshot.textRight);

    if (snapshot.panelOrder == 0) {
        setPanelOrder("left");
    } else if (snapshot.panelOrder == 1) {
        setPanelOrder("right");
    }

    rotatePanel("panel1", snapshot.rotation1);
    rotatePanel("panel2", snapshot.rotation2);
    rotatePanel("panel3", snapshot.rotation3);
}

std::vector<String> LEDManager::listPresets() {
    return loadPresetNameList();
}

bool LEDManager::savePreset(const String& rawName, String& errorMessage) {
    String name = sanitizePresetName(rawName);
    if (name.isEmpty()) {
        errorMessage = "Preset name cannot be empty.";
        return false;
    }
    if (!ensurePrefsReady()) {
        errorMessage = "Preferences not available.";
        return false;
    }

    PresetSnapshot snapshot = captureCurrentPreset();
    String serialized = serializePreset(snapshot);
    const String key = makePresetStorageKey(name);
    size_t written = _prefs.putString(key.c_str(), serialized);
    if (written == 0) {
        errorMessage = "Unable to store preset.";
        return false;
    }
    const String legacyKey = legacyPresetStorageKey(name);
    if (!legacyKey.equals(key)) {
        _prefs.remove(legacyKey.c_str());
    }

    auto names = loadPresetNameList();
    bool found = false;
    for (auto& existing : names) {
        if (existing.equalsIgnoreCase(name)) {
            existing = name;
            found = true;
            break;
        }
    }
    if (!found) {
        names.push_back(name);
    }
    storePresetNameList(names);
    LogManager::getInstance().info("Preset '" + name + "' saved.");
    return true;
}

bool LEDManager::loadPreset(const String& rawName, String& errorMessage) {
    String name = sanitizePresetName(rawName);
    if (name.isEmpty()) {
        errorMessage = "Preset name cannot be empty.";
        return false;
    }
    if (!ensurePrefsReady()) {
        errorMessage = "Preferences not available.";
        return false;
    }
    const String key = makePresetStorageKey(name);
    String data = _prefs.getString(key.c_str(), "");
    if (data.isEmpty()) {
        const String legacyKey = legacyPresetStorageKey(name);
        data = _prefs.getString(legacyKey.c_str(), "");
        if (!data.isEmpty()) {
            _prefs.remove(legacyKey.c_str());
            _prefs.putString(key.c_str(), data);
        }
    }
    if (data.isEmpty()) {
        errorMessage = "Preset not found.";
        return false;
    }
    PresetSnapshot snapshot = captureCurrentPreset();
    if (!deserializePreset(data, snapshot)) {
        errorMessage = "Preset data is corrupted.";
        return false;
    }
    applyPreset(snapshot);
    LogManager::getInstance().info("Preset '" + name + "' loaded.");
    return true;
}

bool LEDManager::deletePreset(const String& rawName, String& errorMessage) {
    String name = sanitizePresetName(rawName);
    if (name.isEmpty()) {
        errorMessage = "Preset name cannot be empty.";
        return false;
    }
    if (!ensurePrefsReady()) {
        errorMessage = "Preferences not available.";
        return false;
    }
    const String key = makePresetStorageKey(name);
    bool removed = _prefs.remove(key.c_str());
    if (!removed) {
        const String legacyKey = legacyPresetStorageKey(name);
        removed = _prefs.remove(legacyKey.c_str());
    }
    if (!removed) {
        errorMessage = "Preset not found.";
        return false;
    }
    auto names = loadPresetNameList();
    names.erase(std::remove_if(names.begin(), names.end(), [&](const String& value){
        return value.equalsIgnoreCase(name);
    }), names.end());
    storePresetNameList(names);
    LogManager::getInstance().info("Preset '" + name + "' deleted.");
    return true;
}

bool LEDManager::setCustomPaletteHex(const std::vector<String>& hexColors) {
    if (hexColors.empty()) {
        return false;
    }

    std::vector<CRGB> parsed;
    parsed.reserve(hexColors.size());

    for (const auto& hex : hexColors) {
        if (parsed.size() >= MAX_CUSTOM_PALETTE_COLORS) {
            break;
        }
        CRGB color;
        if (!parseHexColor(hex, color)) {
            return false;
        }
        parsed.push_back(color);
    }

    if (parsed.empty()) {
        return false;
    }

    _customPalette = parsed;

    if (_customPaletteIndex < 0 || _customPaletteIndex >= static_cast<int>(ALL_PALETTES.size())) {
        ALL_PALETTES.push_back(_customPalette);
        PALETTE_NAMES.push_back("Custom");
        _customPaletteIndex = static_cast<int>(ALL_PALETTES.size()) - 1;
    } else {
        ALL_PALETTES[_customPaletteIndex] = _customPalette;
    }

    if (_currentAnimation && currentPalette == _customPaletteIndex) {
        configureCurrentAnimation();
    }

    LogManager::getInstance().info("Custom palette updated with " + String(_customPalette.size()) + " colours.");
    saveCustomPaletteToPrefs();
    return true;
}

std::vector<String> LEDManager::getCustomPaletteHex() const {
    std::vector<String> output;
    output.reserve(_customPalette.size());
    for (const auto& color : _customPalette) {
        output.push_back(colorToHex(color));
    }
    return output;
}

int LEDManager::getCustomPaletteIndex() const {
    return _customPaletteIndex;
}

bool LEDManager::addUserPalette(const String& name, const std::vector<String>& hexColors, int& newIndexOut) {
    newIndexOut = -1;
    if (hexColors.empty()) {
        return false;
    }
    if (!ensurePrefsReady()) {
        return false;
    }

    loadUserPalettesFromPrefs();

    String sanitizedName = sanitizePaletteName(name);
    if (sanitizedName.isEmpty()) {
        return false;
    }

    for (const auto& existing : PALETTE_NAMES) {
        if (existing.equalsIgnoreCase(sanitizedName)) {
            return false;
        }
    }

    if (_userPaletteNames.size() >= MAX_USER_PALETTES) {
        LogManager::getInstance().warning("Maximum user palettes reached.");
        return false;
    }

    std::vector<CRGB> parsed;
    parsed.reserve(hexColors.size());
    for (const auto& hex : hexColors) {
        if (parsed.size() >= MAX_CUSTOM_PALETTE_COLORS) {
            break;
        }
        CRGB color;
        if (!parseHexColor(hex, color)) {
            return false;
        }
        parsed.push_back(color);
    }

    if (parsed.empty()) {
        return false;
    }

    _userPaletteNames.push_back(sanitizedName);
    _userPalettes.push_back(parsed);
    ALL_PALETTES.push_back(parsed);
    PALETTE_NAMES.push_back(sanitizedName);
    newIndexOut = static_cast<int>(ALL_PALETTES.size()) - 1;

    saveUserPalettesToPrefs();
    LogManager::getInstance().info("Saved user palette '" + sanitizedName + "' with " + String(parsed.size()) + " colours.");
    return true;
}

void LEDManager::loadCustomPaletteFromPrefs() {
    if (!_prefsReady) {
        return;
    }

    String stored = _prefs.getString("customPalette", "");
    stored.trim();
    if (stored.isEmpty()) {
        return;
    }

    std::vector<String> hexValues;
    int start = 0;
    while (start < stored.length()) {
        int comma = stored.indexOf(',', start);
        if (comma < 0) {
            comma = stored.length();
        }
        String token = stored.substring(start, comma);
        token.trim();
        if (!token.isEmpty()) {
            hexValues.push_back(token);
        }
        start = comma + 1;
    }

    if (!hexValues.empty()) {
        setCustomPaletteHex(hexValues);
    }
}

void LEDManager::saveCustomPaletteToPrefs() {
    if (!_prefsReady) {
        return;
    }

    String encoded;
    for (size_t i = 0; i < _customPalette.size(); ++i) {
        if (i > 0) {
            encoded += ",";
        }
        encoded += colorToHex(_customPalette[i]);
    }
    _prefs.putString("customPalette", encoded);
}

void LEDManager::loadUserPalettesFromPrefs() {
    if (!_prefsReady) {
        return;
    }
    if (!_userPaletteNames.empty()) {
        return;
    }

    String stored = _prefs.getString("userPalettes", "");
    stored.trim();
    if (stored.isEmpty()) {
        return;
    }

    int start = 0;
    while (start < stored.length()) {
        int delim = stored.indexOf(';', start);
        if (delim < 0) {
            delim = stored.length();
        }
        String entry = stored.substring(start, delim);
        entry.trim();
        start = delim + 1;
        if (entry.isEmpty()) {
            continue;
        }

        int split = entry.indexOf('|');
        if (split <= 0 || split >= entry.length() - 1) {
            continue;
        }

        String rawName = entry.substring(0, split);
        String coloursPart = entry.substring(split + 1);
        rawName.trim();
        coloursPart.trim();
        if (rawName.isEmpty() || coloursPart.isEmpty()) {
            continue;
        }

        String sanitizedName = sanitizePaletteName(rawName);
        if (sanitizedName.isEmpty()) {
            continue;
        }

        bool duplicate = false;
        for (const auto& existing : PALETTE_NAMES) {
            if (existing.equalsIgnoreCase(sanitizedName)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        std::vector<CRGB> parsed;
        parsed.reserve(MAX_CUSTOM_PALETTE_COLORS);

        int colorStart = 0;
        while (colorStart < coloursPart.length()) {
            int comma = coloursPart.indexOf(',', colorStart);
            if (comma < 0) {
                comma = coloursPart.length();
            }
            String token = coloursPart.substring(colorStart, comma);
            token.trim();
            colorStart = comma + 1;
            if (token.isEmpty()) {
                continue;
            }
            CRGB color;
            if (!parseHexColor(token, color)) {
                parsed.clear();
                break;
            }
            parsed.push_back(color);
            if (parsed.size() >= MAX_CUSTOM_PALETTE_COLORS) {
                break;
            }
        }

        if (parsed.empty()) {
            continue;
        }

        _userPaletteNames.push_back(sanitizedName);
        _userPalettes.push_back(parsed);
        ALL_PALETTES.push_back(parsed);
        PALETTE_NAMES.push_back(sanitizedName);
    }
}

void LEDManager::saveUserPalettesToPrefs() {
    if (!ensurePrefsReady()) {
        return;
    }

    String encoded;
    for (size_t i = 0; i < _userPalettes.size() && i < _userPaletteNames.size(); ++i) {
        if (i > 0) {
            encoded += ";";
        }
        encoded += _userPaletteNames[i];
        encoded += "|";
        for (size_t j = 0; j < _userPalettes[i].size(); ++j) {
            if (j > 0) {
                encoded += ",";
            }
            encoded += colorToHex(_userPalettes[i][j]);
        }
    }
    _prefs.putString("userPalettes", encoded);
}

bool LEDManager::parseHexColor(const String& hex, CRGB& outColor) const {
    String value = hex;
    value.trim();
    if (value.isEmpty()) {
        return false;
    }
    if (value.charAt(0) == '#') {
        value = value.substring(1);
    }
    if (value.length() != 6) {
        return false;
    }
    for (int i = 0; i < 6; ++i) {
        if (!isxdigit(static_cast<unsigned char>(value.charAt(i)))) {
            return false;
        }
    }
    long parsed = strtol(value.c_str(), nullptr, 16);
    if (parsed < 0 || parsed > 0xFFFFFF) {
        return false;
    }
    outColor = CRGB(
        static_cast<uint8_t>((parsed >> 16) & 0xFF),
        static_cast<uint8_t>((parsed >> 8) & 0xFF),
        static_cast<uint8_t>(parsed & 0xFF)
    );
    return true;
}

String LEDManager::colorToHex(const CRGB& color) const {
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", color.r, color.g, color.b);
    return String(buffer);
}

String LEDManager::sanitizePaletteName(const String& name) const {
    String sanitized = name;
    sanitized.trim();
    if (sanitized.isEmpty()) {
        return "";
    }

    String output;
    output.reserve(sanitized.length());
    for (size_t i = 0; i < sanitized.length(); ++i) {
        char c = sanitized.charAt(i);
        if (c == '|' || c == ';' || c == ',' || c == '\r' || c == '\n') {
            c = '-';
        }
        if (static_cast<unsigned char>(c) < 32) {
            continue;
        }
        output += c;
    }
    output.trim();
    if (output.length() > 32) {
        output = output.substring(0, 32);
    }
    return output;
}


