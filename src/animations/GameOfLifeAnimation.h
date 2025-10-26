#pragma once

#include "BaseAnimation.h"
#include <vector>

class GameOfLifeAnimation : public BaseAnimation {
public:
    GameOfLifeAnimation(uint16_t numLeds, uint8_t brightness, int panelCount = 2);
    ~GameOfLifeAnimation() override;

    void begin() override;
    void update() override;

    void setUpdateInterval(unsigned long intervalMs);
    void setSpeedMultiplier(float multiplier);
    void setUsePalette(bool usePalette);
    void setAllPalettes(const std::vector<std::vector<CRGB>>* palettes);
    void setCurrentPalette(int index);
    void setColumnSkip(int skip);
    void setWipeBarBrightness(uint8_t value);
    void setFadeStepControl(uint8_t value);
    void setRotationAngle1(int angle);
    void setRotationAngle2(int angle);
    void setRotationAngle3(int angle);
    void setPanelOrder(int order);
    void setHighlightWidth(uint8_t width);
    void setHighlightColor(const CRGB& color);

    bool isGameOfLife() const override { return true; }

    void randomize(uint8_t density = 33);

private:
    int mapXYtoLED(int x, int y) const;
    inline int index(int x, int y) const { return y * _width + x; }

    bool getCell(const uint8_t* grid, int x, int y) const;
    void setCell(uint8_t* grid, int x, int y, bool value);
    int countNeighbors(const uint8_t* grid, int x, int y) const;

    void prepareNextGeneration();
    void applyColumn(int column);
    void renderFrame();
    void applyBirth(int idx);
    void applyDeath(int idx);
    bool detectStagnation(bool anyChange);
    uint32_t hashGrid(const uint8_t* grid) const;
    CRGB chooseColor() const;
    uint8_t* _gridA;
    uint8_t* _gridB;
    uint8_t* _intensity;   // per-cell brightness 0-255
    CRGB* _colors;         // stored colour for fade-out

    int _width;
    int _height;
    int _gridBytes;

    uint32_t _baseIntervalMs;
    float _speedMultiplier;
    uint32_t _lastRenderMs;
    uint32_t _renderIntervalMs;

    // sweep highlight
    int _sweepColumn;
    int _sweepDirection; // +1 or -1
    uint32_t _lastSweepMs;
    uint32_t _sweepIntervalMs;
    int _sweepSpeedSetting;
    void recalculateSweepInterval();
    bool _generationReady;
    bool _anyChangeThisPass;

    // stagnation detection
    uint32_t _lastHash;
    uint32_t _prevHashSecondary;
    uint8_t _hashRepeatCount;
    uint8_t _twoStateRepeatCount;
    uint32_t _stepsSinceChange;
    static constexpr uint8_t MAX_REPEAT_HASH = 10;
    static constexpr uint32_t MAX_STEPS_WITHOUT_CHANGE = 80;

    // palette
    const std::vector<std::vector<CRGB>>* _allPalettes;
    const std::vector<CRGB>* _currentPalette;
   bool _usePalette;
    int _paletteIndex;
    uint8_t _highlightBoost;
    uint8_t _fadeInStep;
    uint8_t _fadeOutStep;
    uint8_t _highlightWidth;
    CRGB _highlightColor;

    // panel configuration
    int _panelOrder;
    int _rotation1;
    int _rotation2;
    int _rotation3;

    static constexpr uint8_t MAX_INTENSITY = 255;
    static constexpr uint8_t DEFAULT_FADE_IN_STEP = 28;
    static constexpr uint8_t DEFAULT_FADE_OUT_STEP = 20;
    static constexpr uint8_t MIN_FADE_IN_STEP = 8;
    static constexpr uint8_t MAX_FADE_IN_STEP = 44;
    static constexpr uint8_t MIN_FADE_OUT_STEP = 6;
    static constexpr uint8_t MAX_FADE_OUT_STEP = 32;
};
