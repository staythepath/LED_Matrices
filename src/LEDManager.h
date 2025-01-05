#ifndef LEDMANAGER_H
#define LEDMANAGER_H

#include <FastLED.h>
#include <vector>
#include <Arduino.h>

// Forward-declare the global LEDs array
extern CRGB leds[];

// Forward-declare the BaseAnimation class
class BaseAnimation;

// LED Configuration
#define LED_PIN             21
#define NUM_LEDS            512
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB
#define DEFAULT_BRIGHTNESS  30 // Initial brightness (0-255)

class LEDManager {
public:
    LEDManager();

    void begin();      // Initialize FastLED
    void update();     // Called in loop to run _currentAnimation->update()
    void show();       // FastLED.show()

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

    // ---------- Max "Cars" (was flakes) ----------
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
    uint16_t _numLeds;
    uint8_t  _brightness;

    // Palettes
    std::vector<std::vector<CRGB>> ALL_PALETTES;
    std::vector<String> PALETTE_NAMES;
    int  currentPalette;

    // _currentAnimation points to the active animation
    BaseAnimation* _currentAnimation;
    int            _currentAnimationIndex;
    std::vector<String> _animationNames; // e.g. {"Traffic"}

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
    // Creates initial palette data
    void createPalettes();

    // Clean up any existing animation
    void cleanupAnimation();
};

#endif // LEDMANAGER_H
