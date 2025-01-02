#ifndef LEDMANAGER_H
#define LEDMANAGER_H

#include <FastLED.h>
#include <vector>
#include <Arduino.h>

// Forward declaration for Flake struct
struct Flake;

// Extern declaration for the global LEDs array
extern CRGB leds[];

// LED Configuration
#define LED_PIN             21
#define NUM_LEDS            512
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB
#define DEFAULT_BRIGHTNESS  30 // Initial brightness (0-255)

// Flake Structure
struct Flake {
    int x;
    int y;
    int dx;
    int dy;
    float frac;
    CRGB startColor;
    CRGB endColor;
    bool bounce;
};

class LEDManager {
public:
    LEDManager();

    void begin();            // Initialize FastLED
    void update();           // Called in loop to run flake effect
    void show();             // Call FastLED.show()

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

    // ---------- Max Flakes ----------
    void setMaxFlakes(int max);
    int  getMaxFlakes() const;

    // ---------- Tail + Fade ----------
    // tailLength = # of trailing LEDs behind each flake
    // fadeAmount = how strongly the matrix is faded each frame (0..255)
    void setTailLength(int length);
    int  getTailLength() const;

    void setFadeAmount(uint8_t amount);
    uint8_t getFadeAmount() const;

    // ---------- Panel Order / Rotation ----------
    void swapPanels();
    void setPanelOrder(String order);
    void rotatePanel(String panel, int angle);
    int  getRotation(String panel) const;

    // ---------- Update Speed ----------
    void setUpdateSpeed(unsigned long speed);
    unsigned long getUpdateSpeed() const;

private:
    // LED stuff
    uint16_t _numLeds;
    uint8_t  _brightness;

    // Palettes
    std::vector<std::vector<CRGB>> ALL_PALETTES;
    std::vector<String> PALETTE_NAMES;
    int  currentPalette;

    // Flake parameters
    std::vector<Flake> flakes;
    float spawnRate;   // 0..1
    int   maxFlakes;

    // Tail + fade
    int    tailLength;   // # trailing LEDs
    uint8_t fadeAmount;  // fadeToBlackBy( fadeAmount )

    // Panel
    int panelOrder;      // 0 = left first, 1 = right first
    int rotationAngle1;  // 0,90,180,270 for panel1
    int rotationAngle2;  // 0,90,180,270 for panel2

    // Update timing
    unsigned long ledUpdateInterval;
    unsigned long lastLedUpdate;

    // Internal methods
    void spawnFlake();
    void performSnowEffect();
    CRGB calcColor(float frac, CRGB startColor, CRGB endColor, bool bounce);
    int getLedIndex(int x, int y) const;
    void rotateCoordinates(int &x, int &y, int angle) const;
};

#endif // LEDMANAGER_H
