#ifndef LEDMANAGER_H
#define LEDMANAGER_H

#include <FastLED.h>
#include <vector>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

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
    int  getPanelOrder() const;
    void rotatePanel(String panel, int angle);
    int  getRotation(String panel) const;

    // Speed
    void setUpdateSpeed(unsigned long speed);
    unsigned long getUpdateSpeed() const;

    // Life-like settings
    void setLifeSeedDensity(uint8_t density);
    uint8_t getLifeSeedDensity() const;
    void setLifeRuleIndex(int index);
    int getLifeRuleIndex() const;
    size_t getLifeRuleCount() const;
    String getLifeRuleName(int index) const;
    void setLifeWrap(bool wrap);
    bool getLifeWrap() const;
    void setLifeStagnationLimit(uint16_t limit);
    uint16_t getLifeStagnationLimit() const;
    void setLifeColorMode(uint8_t mode);
    uint8_t getLifeColorMode() const;
    void lifeReseed();

    // Langton's Ant settings
    void setAntRule(const String& rule);
    String getAntRule() const;
    void setAntCount(uint8_t count);
    uint8_t getAntCount() const;
    void setAntSteps(uint8_t steps);
    uint8_t getAntSteps() const;
    void setAntWrap(bool wrap);
    bool getAntWrap() const;

    // Sierpinski carpet settings
    void setCarpetDepth(uint8_t depth);
    uint8_t getCarpetDepth() const;
    void setCarpetInvert(bool invert);
    bool getCarpetInvert() const;
    void setCarpetColorShift(uint8_t shift);
    uint8_t getCarpetColorShift() const;

    // Firework settings
    void setFireworkMax(int maxFireworks);
    int getFireworkMax() const;
    void setFireworkParticles(int count);
    int getFireworkParticles() const;
    void setFireworkGravity(float gravity);
    float getFireworkGravity() const;
    void setFireworkLaunchProbability(float probability);
    float getFireworkLaunchProbability() const;

    // Rainbow wave settings
    void setRainbowHueScale(uint8_t scale);
    uint8_t getRainbowHueScale() const;

    bool beginExclusiveAccess(uint32_t timeoutMs = 1000) const;
    void endExclusiveAccess() const;

    class LockGuard {
    public:
        LockGuard(LEDManager& manager, uint32_t timeoutMs = 1000);
        ~LockGuard();
        bool locked() const;
    private:
        LEDManager& _manager;
        bool _locked;
    };

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

    // Life-like settings
    uint8_t lifeSeedDensity;
    int lifeRuleIndex;
    bool lifeWrapEdges;
    uint16_t lifeStagnationLimit;
    uint8_t lifeColorMode;

    // Langton's Ant settings
    String antRule;
    uint8_t antCount;
    uint8_t antSteps;
    bool antWrapEdges;

    // Sierpinski carpet settings
    uint8_t carpetDepth;
    bool carpetInvert;
    uint8_t carpetColorShift;

    // Firework settings
    int fireworkMax;
    int fireworkParticles;
    float fireworkGravity;
    float fireworkLaunchProbability;

    // Rainbow wave settings
    uint8_t rainbowHueScale;

    mutable SemaphoreHandle_t _stateMutex;
};

#endif // LEDMANAGER_H
