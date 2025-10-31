#pragma once

#include "BaseAnimation.h"
#include <FastLED.h>
#include <vector>

class EscherAutomataAnimation : public BaseAnimation {
public:
    enum class Mode : uint8_t {
        LangtonsAnt = 0,
        Elementary = 1,
        RecursiveMoire = 2
    };

    EscherAutomataAnimation(uint16_t numLeds, uint8_t brightness, int panelCount = 2);
    ~EscherAutomataAnimation() override = default;

    void begin() override;
    void update() override;

    void setBrightness(uint8_t b) override { _brightness = b; }

    void setUpdateInterval(uint32_t intervalMs);
    void setMode(Mode mode);
    void setPrimaryControl(uint8_t value);
    void setSecondaryControl(uint8_t value);
    void setAllPalettes(const std::vector<std::vector<CRGB>>* palettes);
    void setCurrentPalette(int index);
    void setUsePalette(bool usePalette);
    void setPanelOrder(int order);
    void setRotationAngle1(int angle);
    void setRotationAngle2(int angle);
    void setRotationAngle3(int angle);

    bool isAutomata() const override { return true; }

private:
    int mapXYtoLED(int x, int y) const;
    inline int index(int x, int y) const { return y * _width + x; }

    void clearBuffers();
    void resetForMode();
    void resetLangtonsAnt();
    void resetElementary();

    void stepLangtonsAnt();
    void renderLangtonsAnt();

    void stepElementary();
    void renderElementary();

    void renderMoire();

    CRGB pickPaletteColor(uint16_t seed) const;
    void updateMoireSpeed();

    uint8_t antCountFromPrimary() const;
    uint8_t langtonStepsPerFrame() const;
    uint8_t ruleIndexFromPrimary() const;
    uint8_t ruleIterationsPerFrame() const;

private:
    Mode _mode;
    uint8_t _primaryControl;
    uint8_t _secondaryControl;

    uint32_t _updateIntervalMs;
    uint32_t _lastUpdateMs;

    int _width;
    int _height;

    int _panelOrder;
    int _rotation1;
    int _rotation2;
    int _rotation3;

    const std::vector<std::vector<CRGB>>* _allPalettes;
    const std::vector<CRGB>* _currentPalette;
    bool _usePalette;
    int _paletteIndex;

    std::vector<uint8_t> _cellStates;
    std::vector<CRGB> _cellColors;

    struct Ant {
        int x;
        int y;
        uint8_t direction;
        CRGB color;
    };
    std::vector<Ant> _ants;

    std::vector<uint8_t> _ruleCurrent;
    std::vector<uint8_t> _ruleNext;
    uint32_t _elementaryGeneration;

    float _moirePhase;
    float _moireSpeed;
};

