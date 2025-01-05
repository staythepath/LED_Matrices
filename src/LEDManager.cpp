#include "LEDManager.h"
#include "config.h"
#include <Arduino.h>
#include <FastLED.h>

// Animation headers
#include "animations/BaseAnimation.h"
#include "animations/TrafficAnimation.h"
#include "animations/BlinkAnimation.h"
#include "animations/RainbowWaveAnimation.h"  
#include "animations/MultiSpawnWaveAnimation.h"

// Global CRGB array
CRGB leds[NUM_LEDS];

// Constructor
LEDManager::LEDManager()
    : _numLeds(NUM_LEDS),
      _brightness(DEFAULT_BRIGHTNESS),
      currentPalette(0),
      _currentAnimation(nullptr),
      _currentAnimationIndex(-1),
      spawnRate(0.6f),
      maxFlakes(200),
      tailLength(5),
      fadeAmount(80),
      panelOrder(0),
      rotationAngle1(90),
      rotationAngle2(90),
      ledUpdateInterval(80),
      lastLedUpdate(0)
{
    // Create palettes
    createPalettes();

    // Add multiple animations to _animationNames
    _animationNames.push_back("Traffic");     // index = 0
    _animationNames.push_back("Blink");       // index = 1
    _animationNames.push_back("RainbowWave"); // index = 2
    _animationNames.push_back("MultiSpawnWave");// index=3 (NEW)
}

// Initialize
void LEDManager::begin() {
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, _numLeds)
           .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(_brightness);
    FastLED.clear(true);

    // Optionally auto-select animation 0 at startup:
    setAnimation(0);
}

// Update
void LEDManager::update() {
    if (_currentAnimation) {
        _currentAnimation->update();
    }
}

// Show
void LEDManager::show() {
    FastLED.show();
}

// Cleanup old animation
void LEDManager::cleanupAnimation() {
    if (_currentAnimation) {
        delete _currentAnimation;
        _currentAnimation = nullptr;
    }
}

// Create palettes
void LEDManager::createPalettes() {
    ALL_PALETTES = {
        // 1: blu_orange_green
        { CRGB(0,128,255), CRGB(255,128,0), CRGB(0,200,60), CRGB(64,0,128), CRGB(255,255,64), CRGB(0,0,0) },
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
        { CRGB(139,0,0), CRGB(139,0,0), CRGB(139,0,0), CRGB(139,0,0), CRGB(139,0,0) },
        // 12:Rainbow
        { CRGB::Red, CRGB::Orange, CRGB::Yellow, CRGB::Green, CRGB::Blue, CRGB::Indigo, CRGB::Violet }
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
        "red",
        "rainbow"
    };
}

// setAnimation(...)
void LEDManager::setAnimation(int animIndex) {
    if (animIndex == _currentAnimationIndex) {
        return;
    }
    cleanupAnimation();

    if (animIndex < 0 || animIndex >= (int)getAnimationCount()) {
        Serial.println("Invalid animation index.");
        return;
    }
    _currentAnimationIndex = animIndex;

    // 0 => Traffic
    if (animIndex == 0) {
        auto traffic = new TrafficAnimation(_numLeds, _brightness);
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
    }
    // 1 => Blink
    else if (animIndex == 1) {
        auto blink = new BlinkAnimation(_numLeds, _brightness);
        // blink->setInterval(200); // optional
        _currentAnimation = blink;
        _currentAnimation->begin();
        Serial.println("Blink animation selected.");
    }
    // 2 => RainbowWave
    else if (animIndex == 2) {
        auto wave = new RainbowWaveAnimation(_numLeds, _brightness);
        wave->setAllPalettes(&ALL_PALETTES);
        wave->setCurrentPalette(currentPalette);
        wave->setUpdateInterval(ledUpdateInterval);
        wave->setPanelOrder(panelOrder);
        wave->setRotationAngle1(rotationAngle1);
        wave->setRotationAngle2(rotationAngle2);

        _currentAnimation = wave;
        _currentAnimation->begin();
        Serial.println("RainbowWave animation selected.");
    }
    // 3 => MultiSpawnWave
    else if (animIndex == 3) {
        auto wave = new MultiSpawnWaveAnimation(_numLeds, _brightness);
        wave->setAllPalettes(&ALL_PALETTES);
        wave->setCurrentPalette(currentPalette);
        wave->setUpdateInterval(ledUpdateInterval);
        wave->setPanelOrder(panelOrder);
        wave->setRotationAngle1(rotationAngle1);
        wave->setRotationAngle2(rotationAngle2);

        // Possibly set wave->setWaveSpeed(spawnRate), wave->setWaveAmplitude(fadeAmount), wave->setWaveFrequency(tailLength)
        // if you want those initial values to apply immediately

        _currentAnimation = wave;
        _currentAnimation->begin();
        Serial.println("MultiSpawnWave animation selected.");
    }
}

int LEDManager::getAnimation() const {
    return _currentAnimationIndex;
}
size_t LEDManager::getAnimationCount() const {
    return _animationNames.size();
}
String LEDManager::getAnimationName(int animIndex) const {
    if (animIndex >= 0 && animIndex < (int)_animationNames.size()) {
        return _animationNames[animIndex];
    }
    return "Unknown";
}

// -------------------- BRIGHTNESS --------------------
void LEDManager::setBrightness(uint8_t brightness) {
    _brightness = brightness;
    FastLED.setBrightness(_brightness);

    if (_currentAnimationIndex == 0 && _currentAnimation) {
        auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
        traffic->setBrightness(_brightness);
    }
    else if (_currentAnimationIndex == 1 && _currentAnimation) {
        auto blink = static_cast<BlinkAnimation*>(_currentAnimation);
        blink->setBrightness(_brightness);
    }
    else if (_currentAnimationIndex == 2 && _currentAnimation) {
        auto wave = static_cast<RainbowWaveAnimation*>(_currentAnimation);
        // The wave also can set brightness:
        wave->setBrightness(_brightness);
        // (Removed calls to setWaveMorphSpeed / setWaveAmplitude / setWaveFrequency here!)
    }
}
uint8_t LEDManager::getBrightness() const {
    return _brightness;
}

// -------------------- PALETTE --------------------
void LEDManager::setPalette(int paletteIndex) {
    if (paletteIndex >= 0 && paletteIndex < (int)PALETTE_NAMES.size()) {
        currentPalette = paletteIndex;
        Serial.printf("Palette %d (%s) selected.\n",
                      currentPalette, PALETTE_NAMES[currentPalette].c_str());
        
        // For Traffic (idx=0)
        if (_currentAnimationIndex == 0 && _currentAnimation) {
            auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
            traffic->setCurrentPalette(currentPalette);
        }
        // For RainbowWave (idx=2)
        else if (_currentAnimationIndex == 2 && _currentAnimation) {
            auto wave = static_cast<RainbowWaveAnimation*>(_currentAnimation);
            wave->setCurrentPalette(currentPalette);
        }
        // ADD THIS for MultiSpawnWave (idx=3)
        else if (_currentAnimationIndex == 3 && _currentAnimation) {
            auto multiWave = static_cast<MultiSpawnWaveAnimation*>(_currentAnimation);
            multiWave->setCurrentPalette(currentPalette);
        }
    }
}

int LEDManager::getCurrentPalette() const {
    return currentPalette;
}
size_t LEDManager::getPaletteCount() const {
    return PALETTE_NAMES.size();
}
String LEDManager::getPaletteNameAt(int index) const {
    if (index >= 0 && index < (int)PALETTE_NAMES.size()) {
        return PALETTE_NAMES[index];
    }
    return "Unknown";
}
const std::vector<CRGB>& LEDManager::getCurrentPaletteColors() const {
    if (currentPalette < 0 || currentPalette >= (int)ALL_PALETTES.size()) {
        static std::vector<CRGB> dummy;
        return dummy;
    }
    return ALL_PALETTES[currentPalette];
}

// -------------------- SPAWN RATE --------------------
void LEDManager::setSpawnRate(float rate) 
{
    // 1) Store globally
    spawnRate = rate;

    // 2) If Traffic (index=0)
    if (_currentAnimationIndex == 0 && _currentAnimation) {
        auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
        traffic->setSpawnRate(rate);
    }
    // 3) RainbowWave (index=2)
    else if (_currentAnimationIndex == 2 && _currentAnimation) {
        auto wave = static_cast<RainbowWaveAnimation*>(_currentAnimation);
        wave->setScrollSpeed(rate);
    }
    // 4) MultiSpawnWave (index=3)
    else if (_currentAnimationIndex == 3 && _currentAnimation) {
        auto multiWave = static_cast<MultiSpawnWaveAnimation*>(_currentAnimation);
        multiWave->setWaveSpeed(rate);

        // Force immediate re-init if you want new spawn speeds to show
        multiWave->begin(); 
    }

    // If Blink (index=1) doesn't use spawnRate, do nothing
}

float LEDManager::getSpawnRate() const {
    return spawnRate;
}

// -------------------- MAX "Cars" --------------------
void LEDManager::setMaxFlakes(int max) {
    maxFlakes = max;
    if (_currentAnimationIndex == 0 && _currentAnimation) {
        auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
        traffic->setMaxCars(max);
    }
}
int LEDManager::getMaxFlakes() const {
    return maxFlakes;
}

// -------------------- TAIL + FADE --------------------
void LEDManager::setTailLength(int length) {
    // 1) Store the new tail length
    tailLength = length;

    // 2) If we're on Traffic (index=0)
    if (_currentAnimationIndex == 0 && _currentAnimation) {
        auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
        traffic->setTailLength(length);
    }
    // 3) If we're on RainbowWave (index=2)
    else if (_currentAnimationIndex == 2 && _currentAnimation) {
        auto wave = static_cast<RainbowWaveAnimation*>(_currentAnimation);
        // e.g. wave->setHorizontalFreq(length);
        wave->setHorizontalFreq(length);
    }
    // 4) If we're on MultiSpawnWave (index=3)
    else if (_currentAnimationIndex == 3 && _currentAnimation) {
        auto multiWave = static_cast<MultiSpawnWaveAnimation*>(_currentAnimation);
        multiWave->setWaveFrequency(length);

        // Re-init so the new freq is used
        multiWave->begin();
    }
}


void LEDManager::setFadeAmount(uint8_t amount)
{
    // 1) store it
    fadeAmount = amount;

    // 2) If Traffic
    if (_currentAnimationIndex == 0 && _currentAnimation) {
        auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
        traffic->setFadeAmount(amount);
    }
    // 3) RainbowWave
    else if (_currentAnimationIndex == 2 && _currentAnimation) {
        auto wave = static_cast<RainbowWaveAnimation*>(_currentAnimation);
        wave->setVerticalAmplitude(amount);
    }
    // 4) MultiSpawnWave
    else if (_currentAnimationIndex == 3 && _currentAnimation) {
        auto multiWave = static_cast<MultiSpawnWaveAnimation*>(_currentAnimation);
        multiWave->setWaveAmplitude(amount);

        // Re-randomize / re-init so the new amplitude is used
        multiWave->begin();
    }
}



// -------------------- PANEL / ROTATION --------------------
void LEDManager::swapPanels() {
    panelOrder = 1 - panelOrder;
    Serial.println("Panels swapped successfully.");
    if (_currentAnimationIndex == 0 && _currentAnimation) {
        auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
        traffic->setPanelOrder(panelOrder);
    }
    else if(_currentAnimationIndex == 2 && _currentAnimation){
        auto wave = static_cast<RainbowWaveAnimation*>(_currentAnimation);
        wave->setPanelOrder(panelOrder);
    }
}

void LEDManager::setPanelOrder(String order) {
    if (order.equalsIgnoreCase("left")) {
        panelOrder = 0;
        Serial.println("Panel order set to left first.");
    }
    else if (order.equalsIgnoreCase("right")) {
        panelOrder = 1;
        Serial.println("Panel order set to right first.");
    }
    else {
        Serial.println("Invalid panel order. Use 'left' or 'right'.");
        return;
    }
    if (_currentAnimationIndex == 0 && _currentAnimation) {
        auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
        traffic->setPanelOrder(panelOrder);
    }
    else if(_currentAnimationIndex == 2 && _currentAnimation){
        auto wave = static_cast<RainbowWaveAnimation*>(_currentAnimation);
        wave->setPanelOrder(panelOrder);
    }
}

void LEDManager::rotatePanel(String panel, int angle) {
    if (!(angle == 0 || angle == 90 || angle == 180 || angle == 270)) {
        Serial.printf("Invalid rotation angle: %d\n", angle);
        return;
    }
    if (panel.equalsIgnoreCase("PANEL1")) {
        rotationAngle1 = angle;
        Serial.printf("Panel1 angle set to %d\n", rotationAngle1);
        if (_currentAnimationIndex == 0 && _currentAnimation) {
            auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
            traffic->setRotationAngle1(rotationAngle1);
        }
        else if(_currentAnimationIndex == 2 && _currentAnimation){
            auto wave = static_cast<RainbowWaveAnimation*>(_currentAnimation);
            wave->setRotationAngle1(rotationAngle1);
        }
    }
    else if (panel.equalsIgnoreCase("PANEL2")) {
        rotationAngle2 = angle;
        Serial.printf("Panel2 angle set to %d\n", rotationAngle2);
        if (_currentAnimationIndex == 0 && _currentAnimation) {
            auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
            traffic->setRotationAngle2(rotationAngle2);
        }
        else if(_currentAnimationIndex == 2 && _currentAnimation){
            auto wave = static_cast<RainbowWaveAnimation*>(_currentAnimation);
            wave->setRotationAngle2(rotationAngle2);
        }
    }
    else {
        Serial.println("Unknown panel: use PANEL1 or PANEL2.");
    }
}
int LEDManager::getRotation(String panel) const {
    if (panel.equalsIgnoreCase("PANEL1")) {
        return rotationAngle1;
    }
    else if (panel.equalsIgnoreCase("PANEL2")) {
        return rotationAngle2;
    }
    Serial.println("Unknown panel: use PANEL1 or PANEL2.");
    return -1;
}

int LEDManager::getTailLength() const {
    return tailLength;
}
uint8_t LEDManager::getFadeAmount() const {
    return fadeAmount;
}


// -------------------- UPDATE SPEED --------------------
void LEDManager::setUpdateSpeed(unsigned long speed) {
    if (speed >= 10 && speed <= 60000) {
        ledUpdateInterval = speed;
        Serial.printf("LED update speed set to %lu ms\n", ledUpdateInterval);

        // Traffic
        if (_currentAnimationIndex == 0 && _currentAnimation) {
            auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
            traffic->setUpdateInterval(speed);
        }
        // Blink
        else if (_currentAnimationIndex == 1 && _currentAnimation) {
            auto blink = static_cast<BlinkAnimation*>(_currentAnimation);
            blink->setInterval(speed);
        }
        // RainbowWave
        else if (_currentAnimationIndex == 2 && _currentAnimation) {
            auto wave = static_cast<RainbowWaveAnimation*>(_currentAnimation);
            wave->setUpdateInterval(speed);
        }
    }
    else {
        Serial.println("Invalid speed. Must be 10..60000.");
    }
}

unsigned long LEDManager::getUpdateSpeed() const {
    return ledUpdateInterval;
}
