#pragma once

#include "BaseAnimation.h"
#include <vector>

class GameOfLifeAnimation : public BaseAnimation {
public:
    enum class UpdateMode : uint8_t {
        Sweep = 0,
        Simultaneous = 1,
        FadeOnly = 2
    };

    enum class NeighborMode : uint8_t {
        Four = 0,
        Eight = 1
    };

    enum class ClusterColorMode : uint8_t {
        Off = 0,
        Size = 1,
        Age = 2,    
        Shape = 3,
        Stable = 4
    };

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
    void setColorPulseMode(bool enabled);
    void setUpdateMode(UpdateMode mode);
    void setNeighborMode(NeighborMode mode);
    void setWrapEdges(bool wrap);
    void setUniformBirthColorEnabled(bool enabled);
    void setUniformBirthColor(const CRGB& color);
    void setRules(uint16_t birthMask, uint16_t surviveMask);
    void setClusterColorMode(ClusterColorMode mode);
    void setSeedDensity(uint8_t percent);
    void setMutationChance(uint8_t percent);

    bool isGameOfLife() const override { return true; }

    void randomize(uint8_t density = 33);

private:
    int mapXYtoLED(int x, int y) const;
    inline int index(int x, int y) const { return y * _width + x; }

    bool getCell(const uint8_t* grid, int x, int y) const;
    void setCell(uint8_t* grid, int x, int y, bool value);
    int countNeighbors(const uint8_t* grid, int x, int y) const;
    bool isNeighborAlive(const uint8_t* grid, int x, int y) const;

    void prepareNextGeneration();
    void applyColumn(int column);
    void applyFullFrame();
    void finalizeGeneration();
    void updateClusterColors();
    bool mutateGrid(uint8_t* grid);
    void renderFrame();
    void startPulseGroup();
    CRGB computePulseColor(int idx) const;
    void ensurePulseStorage();
    void applyBirth(int idx);
    void applyDeath(int idx);
    bool detectStagnation(bool anyChange);
    bool detectExtendedLoop(uint32_t hash, uint32_t generation);
    void initializeHashHistory(uint32_t initialHash);
    uint32_t hashGrid(const uint8_t* grid) const;
    CRGB chooseColor() const;
    uint8_t* _gridA;
    uint8_t* _gridB;
    uint8_t* _intensity;   // per-cell brightness 0-255
    CRGB* _colors;         // stored colour for fade-out
    uint8_t* _age;
    uint16_t* _componentIds;
    uint16_t* _pulseGroup;
    uint8_t _seedDensity;
    uint8_t _mutationChance;

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
    UpdateMode _updateMode;

    // stagnation detection
    static constexpr uint8_t MAX_REPEAT_HASH = 10;
    static constexpr uint32_t MAX_STEPS_WITHOUT_CHANGE = 80;
    static constexpr uint8_t HASH_HISTORY = 32;
    static constexpr uint8_t LOOP_REPEAT_THRESHOLD = 10;

    uint32_t _lastHash;
    uint32_t _prevHashSecondary;
    uint8_t _hashRepeatCount;
    uint8_t _twoStateRepeatCount;
    uint32_t _stepsSinceChange;
    uint32_t _generationCounter;
    uint32_t _hashHistory[HASH_HISTORY];
    uint32_t _hashGeneration[HASH_HISTORY];
    uint8_t _hashHistorySize;
    uint8_t _hashHistoryIndex;
    uint8_t _loopPeriod;
    uint8_t _loopPeriodStreak;
    bool _mutationsAppliedThisGeneration;

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
    bool _colorPulseMode;
    uint16_t _pulseGroupCounter;
    uint16_t _currentPulseGroup;
    size_t _pulseCapacity;
    static constexpr uint8_t PULSE_HISTORY = 32;
    uint32_t _pulseStartMs[PULSE_HISTORY];
    uint8_t _pulsePrimary[PULSE_HISTORY];
    uint8_t _pulseSecondary[PULSE_HISTORY];
    uint8_t _pulseTertiary[PULSE_HISTORY];
    uint16_t _pulsePeriodMs[PULSE_HISTORY];
    bool _uniformBirthColorEnabled;
    CRGB _uniformBirthColor;
    NeighborMode _neighborMode;
    bool _wrapEdges;
    ClusterColorMode _clusterColorMode;
    uint16_t _birthMask;
    uint16_t _surviveMask;

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

