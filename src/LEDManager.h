#ifndef LEDMANAGER_H
#define LEDMANAGER_H

#include <FastLED.h>
#include <vector>
#include <Arduino.h>

// Up to 8 panels of 16×16
static const int MAX_LEDS = 16 * 16 * 8;
extern CRGB leds[MAX_LEDS];

class BaseAnimation;

class LEDManager {
public:
    LEDManager();

    void begin();
    void update();
    void show();

    // Panel count
    void setPanelCount(int count);
    int  getPanelCount() const;

    // Identify panels (stops animation, draws arrow+digit, waits 10s, restores)
    void identifyPanels();

    // Animations
    void setAnimation(int animIndex);
    int  getAnimation() const;
    size_t getAnimationCount() const;
    String getAnimationName(int animIndex) const;

    // Brightness
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const;

    // Speed is 0..100. 0 => slow, 100 => fast.
    void setSpeed(int val);  // We'll internally map to an ms update or something
    int  getSpeed() const;

    // Param1..4 in 0..255
    void setParam1(uint8_t p);
    uint8_t getParam1() const;

    void setParam2(uint8_t p);
    uint8_t getParam2() const;

    void setParam3(uint8_t p);
    uint8_t getParam3() const;

    void setParam4(uint8_t p);
    uint8_t getParam4() const;

    // Palette
    void setPalette(int paletteIndex);
    int  getCurrentPalette() const;
    size_t getPaletteCount() const;
    String getPaletteNameAt(int index) const;
    const std::vector<CRGB>& getCurrentPaletteColors() const;

    // Spawn
    void setSpawnRate(float rate);
    float getSpawnRate() const;

    // Max "cars"
    void setMaxFlakes(int max);
    int  getMaxFlakes() const;

    // Tail + fade
    void setTailLength(int length);
    int  getTailLength() const;
    void setFadeAmount(uint8_t amount);
    uint8_t getFadeAmount() const;

    // Panel (not currently used in HTML, but kept here)
    void swapPanels();
    void setPanelOrder(String order);
    void rotatePanel(String panel, int angle);
    int  getRotation(String panel) const;

    // Speed
    void setUpdateSpeed(unsigned long speed);
    unsigned long getUpdateSpeed() const;

private:
    // Re-init FastLED if panelCount changes
    void reinitFastLED();
    void cleanupAnimation();
    void createPalettes();

    // The bigger arrow + digit methods
    void drawUpArrow(int baseIndex);
    void drawLargeDigit(int baseIndex, int digit);

private:
    int      _panelCount; // default: 3
    uint16_t _numLeds;

    uint8_t  _brightness;
    std::vector<std::vector<CRGB>> ALL_PALETTES;
    std::vector<String>            PALETTE_NAMES;
    int                            currentPalette;

    BaseAnimation* _currentAnimation;
    int            _currentAnimationIndex;
    std::vector<String> _animationNames;

    float   spawnRate;
    int     maxFlakes;
    int     tailLength;
    uint8_t fadeAmount;

    int panelOrder;
    int rotationAngle1;
    int rotationAngle2;

    // Speed 
    int _speedVal; 

    //Generic parameters
    uint8_t _param1;
    uint8_t _param2;
    uint8_t _param3;
    uint8_t _param4;

    unsigned long ledUpdateInterval;
    unsigned long lastLedUpdate;
};

#endif // LEDMANAGER_H
