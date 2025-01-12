#ifndef LEDMANAGER_H
#define LEDMANAGER_H

#include <FastLED.h>
#include <vector>
#include <Arduino.h>

// We still declare a global CRGB array, but we will define its maximum size:
static const int MAX_LEDS = 16 * 16 * 8; // up to 8 panels of 16x16 = 2048
extern CRGB leds[MAX_LEDS];

// Forward-declare BaseAnimation
class BaseAnimation;

/**
 * The LEDManager now manages a dynamic panel count.
 * Each panel is 16x16, so total LED count = panelCount * 16 * 16.
 */
class LEDManager {
public:
    LEDManager();

    void begin();
    void update();
    void show();

    // ---------- Panel Count ----------
    void setPanelCount(int count);  // e.g. 1..8
    int  getPanelCount() const;

    // ---------- Brightness ----------
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const;

    // ---------- Palette ----------
    void setPalette(int paletteIndex);
    int  getCurrentPalette() const;
    size_t getPaletteCount() const;
    String getPaletteNameAt(int index) const;
    const std::vector<CRGB>& getCurrentPaletteColors() const;

    // ---------- Spawn Rate ----------
    void setSpawnRate(float rate);
    float getSpawnRate() const;

    // ---------- Max "Cars" ----------
    void setMaxFlakes(int max);
    int  getMaxFlakes() const;

    // ---------- Tail + Fade ----------
    void setTailLength(int length);
    int  getTailLength() const;
    void setFadeAmount(uint8_t amount);
    uint8_t getFadeAmount() const;

    // ---------- Panel / Rotation ----------
    void swapPanels();
    void setPanelOrder(String order);
    void rotatePanel(String panel, int angle);
    int  getRotation(String panel) const;

    // ---------- Update Speed ----------
    void setUpdateSpeed(unsigned long speed);
    unsigned long getUpdateSpeed() const;

    // ---------- Animation Control ----------
    void setAnimation(int animIndex);
    int  getAnimation() const;
    size_t getAnimationCount() const;
    String getAnimationName(int animIndex) const;

private:
    // The current number of 16×16 panels
    int _panelCount;     // default: 3

    // The actual LED count (panelCount * 16 * 16)
    uint16_t _numLeds;

    // Brightness
    uint8_t  _brightness;

    // Palettes
    std::vector<std::vector<CRGB>> ALL_PALETTES;
    std::vector<String>            PALETTE_NAMES;
    int                            currentPalette;

    // The active animation
    BaseAnimation* _currentAnimation;
    int            _currentAnimationIndex;
    std::vector<String> _animationNames;

    // Common parameters
    float   spawnRate;
    int     maxFlakes;
    int     tailLength;
    uint8_t fadeAmount;

    // Panel
    int panelOrder;
    int rotationAngle1;
    int rotationAngle2;

    // Update timing
    unsigned long ledUpdateInterval;
    unsigned long lastLedUpdate;

private:
    void createPalettes();
    void cleanupAnimation();

    // We can re-initialize FastLED if user changes panel count
    void reinitFastLED();
};

#endif // LEDMANAGER_H
