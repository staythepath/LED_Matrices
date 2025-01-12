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

// We'll define the global array with a max size of 16x16x8=2048
CRGB leds[MAX_LEDS];

LEDManager::LEDManager()
    : _panelCount(3) // default to 3
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
    , ledUpdateInterval(38)
    , lastLedUpdate(0)
{
    createPalettes();

    // Animations
    _animationNames.push_back("Traffic");     // index=0
    _animationNames.push_back("Blink");       // index=1
    _animationNames.push_back("RainbowWave"); // index=2
}

void LEDManager::begin() {
    reinitFastLED(); 
    // Optionally auto-select animation 0
    setAnimation(0);
}

void LEDManager::reinitFastLED() {
    // Clear any existing LED data
    FastLED.clear(true);
    FastLED.setBrightness(_brightness);

    // Now add the first _numLeds from the global leds[] array
    // Make sure we don't exceed MAX_LEDS
    if(_numLeds > MAX_LEDS) {
        Serial.println("Error: _numLeds exceeds MAX_LEDS, adjusting...");
        _numLeds = MAX_LEDS;
    }

    // Re-init FastLED. It's often good practice to remove existing controllers first:
    FastLED.clearData(); 
    // Some versions of FastLED also allow removeAllLeds() or similar, but not always needed.

    FastLED.addLeds<WS2812B, 21, GRB>(leds, _numLeds)
           .setCorrection(TypicalLEDStrip);
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

// ------------------------ Panel Count Control ------------------------
#include "LEDManager.h"
#include <Arduino.h>
#include <FastLED.h>
#include "animations/TrafficAnimation.h"
#include "animations/BlinkAnimation.h"
#include "animations/RainbowWaveAnimation.h"

extern CRGB leds[MAX_LEDS];

void LEDManager::setPanelCount(int count) {
    if(count < 1) {
        Serial.println("Panel count must be >= 1");
        return;
    }
    if(count > 8) {
        Serial.println("Panel count max is 8 for now!");
        count = 8;
    }

    int oldCount  = _panelCount;
    int oldNumLeds= _numLeds;

    _panelCount = count;
    _numLeds    = _panelCount * 16 * 16;
    Serial.printf("Panel count set to %d, _numLeds=%d\n", _panelCount, _numLeds);

    // If new count < old count, clear the leftover region
    if(_numLeds < oldNumLeds) {
        for(int i=_numLeds; i<oldNumLeds && i<MAX_LEDS; i++) {
            leds[i] = CRGB::Black; 
        }
        FastLED.show(); // force update
    }

    // Now re-init FastLED
    reinitFastLED();

    // Re-create the active animation with the new LED count
    cleanupAnimation();
    int oldIndex = _currentAnimationIndex;
    _currentAnimationIndex = -1; 
    setAnimation((oldIndex >= 0) ? oldIndex : 0);
}


int LEDManager::getPanelCount() const {
    return _panelCount;
}

// ------------------------ setAnimation(...) ------------------------
void LEDManager::setAnimation(int animIndex) {
    if (animIndex == _currentAnimationIndex) return;
    cleanupAnimation();

    if (animIndex < 0 || animIndex >= (int)getAnimationCount()) {
        Serial.println("Invalid animation index.");
        return;
    }
    _currentAnimationIndex = animIndex;

    if (animIndex == 0) {
        // Traffic
        // pass in the dynamic _numLeds, brightness, and panelCount
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
    }
    else if (animIndex == 1) {
        // Blink
        auto blink = new BlinkAnimation(_numLeds, _brightness, _panelCount);
        // setInterval, palette, etc.
        _currentAnimation = blink;
        _currentAnimation->begin();
    }
    else if (animIndex == 2) {
        // Rainbow
        auto rainbow = new RainbowWaveAnimation(_numLeds, _brightness, _panelCount);
        // setPanelOrder, angles, etc.
        _currentAnimation = rainbow;
        _currentAnimation->begin();
    }

}

// Accessors
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

// ---------- Brightness ----------
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
        wave->setBrightness(_brightness);
    }
}
uint8_t LEDManager::getBrightness() const {
    return _brightness;
}

// ---------- Palette ----------
void LEDManager::setPalette(int paletteIndex) {
    if (paletteIndex >= 0 && paletteIndex < (int)PALETTE_NAMES.size()) {
        currentPalette = paletteIndex;
        Serial.printf("Palette %d (%s) selected.\n",
                      currentPalette,
                      PALETTE_NAMES[currentPalette].c_str());

        if (_currentAnimationIndex == 0 && _currentAnimation) {
            auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
            traffic->setCurrentPalette(currentPalette);
        }
        else if (_currentAnimationIndex == 1 && _currentAnimation) {
            auto blink = static_cast<BlinkAnimation*>(_currentAnimation);
            blink->setPalette(&ALL_PALETTES[currentPalette]);
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

// ---------- Spawn Rate ----------
void LEDManager::setSpawnRate(float rate) {
    spawnRate = rate;
    if (_currentAnimationIndex == 0 && _currentAnimation) {
        auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
        traffic->setSpawnRate(rate);
    }
}
float LEDManager::getSpawnRate() const {
    return spawnRate;
}

// ---------- Max "Cars" ----------
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

// ---------- Tail + Fade ----------
void LEDManager::setTailLength(int length) {
    tailLength = length;
    if (_currentAnimationIndex == 0 && _currentAnimation) {
        auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
        traffic->setTailLength(length);
    }
}
int LEDManager::getTailLength() const {
    return tailLength;
}

void LEDManager::setFadeAmount(uint8_t amount) {
    fadeAmount = amount;
    if (_currentAnimationIndex == 0 && _currentAnimation) {
        auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
        traffic->setFadeAmount(amount);
    }
}
uint8_t LEDManager::getFadeAmount() const {
    return fadeAmount;
}

// ---------- Panel / Rotation ----------
void LEDManager::swapPanels() {
    panelOrder = 1 - panelOrder;
    Serial.println("Panels swapped successfully.");
    if (_currentAnimationIndex == 0 && _currentAnimation) {
        auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
        traffic->setPanelOrder(panelOrder);
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
    }
    else if (panel.equalsIgnoreCase("PANEL2")) {
        rotationAngle2 = angle;
        Serial.printf("Panel2 angle set to %d\n", rotationAngle2);
        if (_currentAnimationIndex == 0 && _currentAnimation) {
            auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
            traffic->setRotationAngle2(rotationAngle2);
        }
    }
    else if (panel.equalsIgnoreCase("PANEL3")) {
        // If you define a 3rd angle
        Serial.printf("Panel3 angle set to %d\n", angle);
        if (_currentAnimationIndex == 0 && _currentAnimation) {
            auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
            traffic->setRotationAngle3(angle);
        }
    }
    else {
        Serial.println("Unknown panel: use PANEL1, PANEL2, or PANEL3.");
    }
}
int LEDManager::getRotation(String panel) const {
    if (panel.equalsIgnoreCase("PANEL1")) {
        return rotationAngle1;
    }
    if (panel.equalsIgnoreCase("PANEL2")) {
        return rotationAngle2;
    }
    // etc. If you had panel3, handle that
    return -1;
}

// ---------- Update Speed ----------
void LEDManager::setUpdateSpeed(unsigned long speed) {
    if (speed >= 10 && speed <= 60000) {
        ledUpdateInterval = speed;
        Serial.printf("LED update speed set to %lu ms\n", ledUpdateInterval);

        if (_currentAnimationIndex == 0 && _currentAnimation) {
            auto traffic = static_cast<TrafficAnimation*>(_currentAnimation);
            traffic->setUpdateInterval(speed);
        }
        else if (_currentAnimationIndex == 1 && _currentAnimation) {
            auto blink = static_cast<BlinkAnimation*>(_currentAnimation);
            blink->setInterval(speed);
        }
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
