// LEDManager.h
#ifndef LEDMANAGER_H
#define LEDMANAGER_H

#include <FastLED.h>
#include <vector>
#include <Arduino.h>

// Forward declaration for Flake struct
struct Flake;

// Declare leds as extern
extern CRGB leds[];

// LED Configuration Constants
#define LED_PIN     21
#define NUM_LEDS    512
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define DEFAULT_BRIGHTNESS 30 // Initial brightness (0-255)

// Flake Structure
struct Flake {
    int x; // X-coordinate
    int y; // Y-coordinate
    int dx; // Movement in X
    int dy; // Movement in Y
    float frac; // Fraction for color interpolation
    CRGB startColor;
    CRGB endColor;
    bool bounce; // Whether the flake should bounce
};

class LEDManager {
public:
    // Constructor
    LEDManager();

    // Initialize LEDs
    void begin();

    // Update LEDs (e.g., perform effects)
    void update();

    // Brightness control
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const;

    // Palette control
    void setPalette(int paletteIndex);
    int getCurrentPalette() const;
    size_t getPaletteCount() const;
    String getPaletteNameAt(int index) const;
    const std::vector<CRGB>& getCurrentPaletteColors() const;

    // Tail and spawn rate control
    void setFadeValue(int fadeValue);
    int getFadeValue() const;

    void setSpawnRate(float rate);
    float getSpawnRate() const;

    void setMaxFlakes(int max);
    int getMaxFlakes() const;

    // Panel order and rotation control
    void swapPanels();
    void setPanelOrder(String order);
    void rotatePanel(String panel, int angle);
    int getRotation(String panel) const;

    // LED update speed control
    void setUpdateSpeed(unsigned long speed);
    unsigned long getUpdateSpeed() const;

    // Function to update the display (called from main loop)
    void show();

private:
    // LED Properties
    uint16_t _numLeds;
    uint8_t _brightness;

    // Palette Properties
    std::vector<std::vector<CRGB>> ALL_PALETTES;
    std::vector<String> PALETTE_NAMES;
    int currentPalette;

    // Flake Properties
    std::vector<Flake> flakes;
    int fadeValue;
    float spawnRate;
    int maxFlakes;

    // Panel Properties
    int panelOrder; // 0 = Left first, 1 = Right first
    int rotationAngle1; // Rotation for Panel 1
    int rotationAngle2; // Rotation for Panel 2

    // LED Update Speed
    unsigned long ledUpdateInterval; // in milliseconds
    unsigned long lastLedUpdate;

    // Helper Functions
    void spawnFlake();
    void performSnowEffect();
    CRGB calcColor(float frac, CRGB startColor, CRGB endColor, bool bounce);
    int getLedIndex(int x, int y) const;
    void rotateCoordinates(int &x, int &y, int angle) const;
};

#endif // LEDMANAGER_H
