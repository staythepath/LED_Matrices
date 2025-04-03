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

    // Loading animation
    void showLoadingAnimation();
    void finishInitialization();

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
    void configureCurrentAnimation();

    // The bigger arrow + digit methods
    void drawUpArrow(int baseIndex);
    void drawLargeDigit(int baseIndex, int digit);

private:
    bool _isInitializing;  // Flag to indicate system is still initializing
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
    int rotationAngle3;

    unsigned long ledUpdateInterval;
    unsigned long lastLedUpdate;
};

#endif // LEDMANAGER_H
