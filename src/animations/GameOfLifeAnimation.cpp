#include "GameOfLifeAnimation.h"

#include <Arduino.h>
#include <FastLED.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <vector>

extern CRGB leds[];

static constexpr int PANEL_SIZE = 16;
static constexpr uint8_t DEFAULT_DENSITY = 33;
static constexpr int NEIGHBOR_OFFSETS_4[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
static constexpr int NEIGHBOR_OFFSETS_8[8][2] = {
    {1,0}, {-1,0}, {0,1}, {0,-1},
    {1,1}, {1,-1}, {-1,1}, {-1,-1}
};

GameOfLifeAnimation::GameOfLifeAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
    : BaseAnimation(numLeds, brightness, panelCount),
      _gridA(nullptr),
      _gridB(nullptr),
      _intensity(nullptr),
      _colors(nullptr),
      _age(nullptr),
      _componentIds(nullptr),
      _pulseGroup(nullptr),
      _width(PANEL_SIZE * panelCount),
      _height(PANEL_SIZE),
      _gridBytes(((_width * _height) + 7) / 8),
      _baseIntervalMs(40),
      _speedMultiplier(1.0f),
      _lastRenderMs(0),
      _renderIntervalMs(16),
      _sweepColumn(0),
      _sweepDirection(1),
      _lastSweepMs(0),
      _sweepIntervalMs(18),
      _sweepSpeedSetting(6),
      _generationReady(false),
      _anyChangeThisPass(false),
      _updateMode(UpdateMode::Sweep),
      _lastHash(0),
      _prevHashSecondary(0),
      _hashRepeatCount(0),
      _twoStateRepeatCount(0),
      _stepsSinceChange(0),
      _generationCounter(0),
      _hashHistorySize(0),
      _hashHistoryIndex(0),
      _loopPeriod(0),
      _loopPeriodStreak(0),
      _mutationsAppliedThisGeneration(false),
      _allPalettes(nullptr),
      _currentPalette(nullptr),
      _usePalette(true),
      _paletteIndex(0),
      _highlightBoost(80),
      _fadeInStep(DEFAULT_FADE_IN_STEP),
      _fadeOutStep(DEFAULT_FADE_OUT_STEP),
      _highlightWidth(2),
      _highlightColor(CRGB::White),
      _colorPulseMode(false),
      _pulseGroupCounter(1),
      _currentPulseGroup(0),
      _pulseCapacity(0),
      _uniformBirthColorEnabled(false),
      _uniformBirthColor(CRGB::White),
      _neighborMode(NeighborMode::Eight),
      _wrapEdges(true),
      _clusterColorMode(ClusterColorMode::Off),
      _birthMask(0),
      _surviveMask(0),
      _seedDensity(DEFAULT_DENSITY),
      _mutationChance(0),
      _panelOrder(1),
      _rotation1(0),
      _rotation2(0),
      _rotation3(0) {

    const size_t gridSize = static_cast<size_t>(_width) * _height;

    _gridA = static_cast<uint8_t*>(calloc(_gridBytes, 1));
    _gridB = static_cast<uint8_t*>(calloc(_gridBytes, 1));
    _intensity = static_cast<uint8_t*>(calloc(gridSize, 1));
    _colors = static_cast<CRGB*>(calloc(gridSize, sizeof(CRGB)));
    _age = static_cast<uint8_t*>(calloc(gridSize, 1));
    _componentIds = static_cast<uint16_t*>(calloc(gridSize, sizeof(uint16_t)));

    if (!_gridA || !_gridB || !_intensity || !_colors || !_age || !_componentIds) {
        Serial.println("GameOfLife: memory allocation failed");
    }

    memset(_hashHistory, 0, sizeof(_hashHistory));
    memset(_hashGeneration, 0, sizeof(_hashGeneration));
    memset(_pulseStartMs, 0, sizeof(_pulseStartMs));
    memset(_pulsePrimary, 0, sizeof(_pulsePrimary));
    memset(_pulseSecondary, 0, sizeof(_pulseSecondary));
    memset(_pulseTertiary, 0, sizeof(_pulseTertiary));
    memset(_pulsePeriodMs, 0, sizeof(_pulsePeriodMs));

    _birthMask = (1 << 3);            // B3
    _surviveMask = (1 << 2) | (1 << 3); // S23
}

GameOfLifeAnimation::~GameOfLifeAnimation() {
    free(_gridA);
    free(_gridB);
    free(_intensity);
    free(_colors);
    free(_age);
    free(_componentIds);
    free(_pulseGroup);
}

void GameOfLifeAnimation::setUpdateInterval(unsigned long intervalMs) {
    _baseIntervalMs = std::max<unsigned long>(intervalMs, 1UL);
    recalculateSweepInterval();
}

void GameOfLifeAnimation::setSpeedMultiplier(float multiplier) {
    _speedMultiplier = std::max(0.05f, std::min(multiplier, 30.0f));
}

void GameOfLifeAnimation::setUsePalette(bool usePalette) {
    _usePalette = usePalette;
}

void GameOfLifeAnimation::setAllPalettes(const std::vector<std::vector<CRGB>>* palettes) {
    _allPalettes = palettes;
    if (_allPalettes && !_allPalettes->empty()) {
        _currentPalette = &(*_allPalettes)[_paletteIndex % _allPalettes->size()];
    }
}

void GameOfLifeAnimation::setCurrentPalette(int index) {
    _paletteIndex = index;
    if (_allPalettes && !_allPalettes->empty()) {
        _currentPalette = &(*_allPalettes)[(_paletteIndex % _allPalettes->size() + _allPalettes->size()) % _allPalettes->size()];
    }
}

void GameOfLifeAnimation::setColumnSkip(int skip) {
    // Repurpose "column skip" control as sweep speed preference (1-20)
    _sweepSpeedSetting = std::max(1, std::min(skip, 20));
    recalculateSweepInterval();
}

void GameOfLifeAnimation::setWipeBarBrightness(uint8_t value) {
    _highlightBoost = map(value, 0, 255, 30, 120);
}

void GameOfLifeAnimation::setFadeStepControl(uint8_t value) {
    uint8_t clamped = std::min<uint8_t>(value, 20);
    _fadeInStep = map(clamped, 0, 20, MIN_FADE_IN_STEP, MAX_FADE_IN_STEP);
    _fadeOutStep = map(clamped, 0, 20, MIN_FADE_OUT_STEP, MAX_FADE_OUT_STEP);
}

void GameOfLifeAnimation::setHighlightWidth(uint8_t width) {
    if (width > 8) {
        width = 8;
    }
    _highlightWidth = width;
}

void GameOfLifeAnimation::setHighlightColor(const CRGB& color) {
    _highlightColor = color;
}

void GameOfLifeAnimation::setColorPulseMode(bool enabled) {
    if (_colorPulseMode == enabled) {
        return;
    }
    _colorPulseMode = enabled;
    if (!enabled) {
        free(_pulseGroup);
        _pulseGroup = nullptr;
        _pulseCapacity = 0;
        return;
    }

    ensurePulseStorage();
    if (_pulseGroup) {
        memset(_pulseGroup, 0, sizeof(uint16_t) * _pulseCapacity);
    }
    _pulseGroupCounter = 1;
    startPulseGroup();
}

void GameOfLifeAnimation::setUniformBirthColorEnabled(bool enabled) {
    _uniformBirthColorEnabled = enabled;
}

void GameOfLifeAnimation::setUniformBirthColor(const CRGB& color) {
    _uniformBirthColor = color;
}

void GameOfLifeAnimation::setUpdateMode(UpdateMode mode) {
    if (_updateMode == mode) {
        return;
    }
    _updateMode = mode;
    _generationReady = false;
    _sweepDirection = 1;
    _sweepColumn = 0;
    _lastSweepMs = millis();
}

void GameOfLifeAnimation::setNeighborMode(NeighborMode mode) {
    _neighborMode = mode;
}

void GameOfLifeAnimation::setWrapEdges(bool wrap) {
    _wrapEdges = wrap;
}

void GameOfLifeAnimation::setRules(uint16_t birthMask, uint16_t surviveMask) {
    _birthMask = birthMask & 0x1FF;
    _surviveMask = surviveMask & 0x1FF;
}

void GameOfLifeAnimation::setClusterColorMode(ClusterColorMode mode) {
    _clusterColorMode = mode;
    if (_clusterColorMode != ClusterColorMode::Off) {
        updateClusterColors();
    }
}

void GameOfLifeAnimation::setSeedDensity(uint8_t percent) {
    if (percent > 100) percent = 100;
    _seedDensity = percent;
}

void GameOfLifeAnimation::setMutationChance(uint8_t percent) {
    if (percent > 100) percent = 100;
    _mutationChance = percent;
}

void GameOfLifeAnimation::setRotationAngle1(int angle) { _rotation1 = angle; }
void GameOfLifeAnimation::setRotationAngle2(int angle) { _rotation2 = angle; }
void GameOfLifeAnimation::setRotationAngle3(int angle) { _rotation3 = angle; }
void GameOfLifeAnimation::setPanelOrder(int order) { _panelOrder = order; }

void GameOfLifeAnimation::recalculateSweepInterval() {
    const float minFactor = 0.6f;
    const float maxFactor = 48.0f;
    const float normalized = static_cast<float>(_sweepSpeedSetting - 1) / 19.0f;
    const float speedFactor = minFactor + (maxFactor - minFactor) * normalized;
    float interval = static_cast<float>(_baseIntervalMs) / speedFactor;
    if (interval < 1.0f) {
        interval = 1.0f;
    }
    _sweepIntervalMs = static_cast<uint32_t>(interval);
}
void GameOfLifeAnimation::begin() {
    if (!_gridA || !_gridB) return;
    randomize(_seedDensity);
    _sweepDirection = 1;
    _sweepColumn = 0;
    _generationReady = false;
    _lastRenderMs = millis();
    _lastSweepMs = _lastRenderMs;
}

void GameOfLifeAnimation::randomize(uint8_t density) {
    if (!_gridA || !_gridB) return;

    memset(_gridA, 0, _gridBytes);
    memset(_gridB, 0, _gridBytes);

    const int gridSize = _width * _height;
    for (int i = 0; i < gridSize; ++i) {
        _intensity[i] = 0;
        _colors[i] = CRGB::Black;
    }

    uint8_t densityValue = (density == 0) ? _seedDensity : density;
    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            if (random(100) < densityValue) {
                setCell(_gridA, x, y, true);
                const int idx = index(x, y);
                const CRGB birthColor = _uniformBirthColorEnabled ? _uniformBirthColor : chooseColor();
                _colors[idx] = birthColor;
                _intensity[idx] = random8(40, 120);
                if (_age) {
                    _age[idx] = 1;
                }
            } else if (_age) {
                _age[index(x, y)] = 0;
            }
        }
    }

    if (_componentIds) {
        memset(_componentIds, 0, sizeof(uint16_t) * _width * _height);
    }

    _generationReady = false;
    _anyChangeThisPass = false;
    _sweepDirection = 1;
    _sweepColumn = 0;
    const uint32_t initialHash = hashGrid(_gridA);
    initializeHashHistory(initialHash);
    const uint32_t now = millis();
    _lastSweepMs = now;
    _lastRenderMs = now;
    recalculateSweepInterval();
    _mutationsAppliedThisGeneration = false;
}

void GameOfLifeAnimation::initializeHashHistory(uint32_t initialHash) {
    _hashRepeatCount = 0;
    _twoStateRepeatCount = 0;
    _prevHashSecondary = 0;
    _stepsSinceChange = 0;
    _generationCounter = 0;
    _loopPeriod = 0;
    _loopPeriodStreak = 0;
    _hashHistorySize = 0;
    _hashHistoryIndex = 0;
    std::fill_n(_hashHistory, HASH_HISTORY, 0);
    std::fill_n(_hashGeneration, HASH_HISTORY, 0);
    _hashHistory[_hashHistoryIndex] = initialHash;
    _hashGeneration[_hashHistoryIndex] = 0;
    _hashHistorySize = 1;
    _hashHistoryIndex = (_hashHistoryIndex + 1) % HASH_HISTORY;
    _lastHash = initialHash;
    if (_colorPulseMode) {
        _pulseGroupCounter = 1;
        startPulseGroup();
    }
}

void GameOfLifeAnimation::update() {
    if (!_gridA || !_gridB) return;

    const uint32_t now = millis();
    const uint32_t sourceInterval = (_updateMode == UpdateMode::Sweep) ? _sweepIntervalMs : _baseIntervalMs;
    const uint32_t generationInterval = std::max<uint32_t>(
        1, static_cast<uint32_t>(static_cast<float>(sourceInterval) / _speedMultiplier)
    );

    if (_updateMode == UpdateMode::Sweep) {
        if (!_generationReady) {
            prepareNextGeneration();
            _generationReady = true;
            _anyChangeThisPass = false;
            _sweepColumn = (_sweepDirection > 0) ? 0 : (_width - 1);
            _lastSweepMs = now;
        }

        if (now - _lastSweepMs >= generationInterval) {
            applyColumn(_sweepColumn);
            _lastSweepMs = now;

            _sweepColumn += _sweepDirection;
            bool completedPass = false;
            if (_sweepColumn >= _width) {
                _sweepColumn = _width - 2;
                _sweepDirection = -1;
                completedPass = true;
            } else if (_sweepColumn < 0) {
                _sweepColumn = 1;
                _sweepDirection = 1;
                completedPass = true;
            }
            if (completedPass) {
                finalizeGeneration();
            }
        }
    } else {
        if ((now - _lastSweepMs) >= generationInterval || !_generationReady) {
            prepareNextGeneration();
            applyFullFrame();
            finalizeGeneration();
            _lastSweepMs = now;
        }
    }

    if (now - _lastRenderMs >= _renderIntervalMs) {
        renderFrame();
        _lastRenderMs = now;
        FastLED.show();
    }
}

void GameOfLifeAnimation::prepareNextGeneration() {
    if (_colorPulseMode) {
        startPulseGroup();
    }

    bool anyAlive = false;

    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            const bool alive = getCell(_gridA, x, y);
            const int neighbors = countNeighbors(_gridA, x, y);
            const bool nextAlive = alive ? ((_surviveMask >> neighbors) & 0x1) : ((_birthMask >> neighbors) & 0x1);

            setCell(_gridB, x, y, nextAlive);
            if (nextAlive) {
                anyAlive = true;
            }
        }
    }

    bool mutated = false;
    if (_mutationChance > 0) {
        mutated = mutateGrid(_gridB);
    }
    _mutationsAppliedThisGeneration = mutated;

    bool anyAliveAfter = anyAlive;
    if (mutated || !anyAlive) {
        anyAliveAfter = false;
        for (int y = 0; y < _height && !anyAliveAfter; ++y) {
            for (int x = 0; x < _width; ++x) {
                if (getCell(_gridB, x, y)) {
                    anyAliveAfter = true;
                    break;
                }
            }
        }
    }

    // Avoid blank board for long periods
    if (!anyAliveAfter) {
        randomize(_seedDensity);
        prepareNextGeneration();
    }
}

void GameOfLifeAnimation::applyColumn(int column) {
    if (column < 0 || column >= _width) return;

    for (int y = 0; y < _height; ++y) {
        const bool currentAlive = getCell(_gridA, column, y);
        const bool nextAlive = getCell(_gridB, column, y);

        const int idx = index(column, y);

        if (nextAlive) {
            if (currentAlive) {
                if (_age) {
                    if (_age[idx] < 255) {
                        ++_age[idx];
                    }
                }
            } else {
                applyBirth(idx);
                if (_age) {
                    _age[idx] = 1;
                }
            }
        } else {
            if (currentAlive) {
                applyDeath(idx);
            }
            if (_age) {
                _age[idx] = 0;
            }
        }

        if (currentAlive != nextAlive) {
            setCell(_gridA, column, y, nextAlive);
            _anyChangeThisPass = true;
        }
    }
}

void GameOfLifeAnimation::applyFullFrame() {
    _anyChangeThisPass = false;
    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            const bool currentAlive = getCell(_gridA, x, y);
            const bool nextAlive = getCell(_gridB, x, y);
            const int idx = index(x, y);

            if (nextAlive) {
                if (currentAlive) {
                    if (_age && _age[idx] < 255) {
                        ++_age[idx];
                    }
                } else {
                    applyBirth(idx);
                    if (_age) {
                        _age[idx] = 1;
                    }
                }
            } else {
                if (currentAlive) {
                    applyDeath(idx);
                }
                if (_age) {
                    _age[idx] = 0;
                }
            }

            if (currentAlive != nextAlive) {
                setCell(_gridA, x, y, nextAlive);
                _anyChangeThisPass = true;
            }
        }
    }
}

void GameOfLifeAnimation::finalizeGeneration() {
    const bool restarted = detectStagnation(_anyChangeThisPass);
    if (!restarted) {
        if (_clusterColorMode != ClusterColorMode::Off || _mutationsAppliedThisGeneration) {
            updateClusterColors();
        }
    }
    _generationReady = false;
    _anyChangeThisPass = false;
    _mutationsAppliedThisGeneration = false;
    if (_colorPulseMode) {
        ++_pulseGroupCounter;
    }
}

bool GameOfLifeAnimation::mutateGrid(uint8_t* grid) {
    if (!grid || _mutationChance == 0) {
        return false;
    }

    bool mutated = false;
    const int threshold = static_cast<int>(_mutationChance) * 10;
    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            if (random(1000) >= threshold) {
                continue;
            }
            const bool currentlyAlive = getCell(grid, x, y);
            setCell(grid, x, y, !currentlyAlive);
            mutated = true;
        }
    }
    return mutated;
}


void GameOfLifeAnimation::ensurePulseStorage() {
    if (!_colorPulseMode) {
        return;
    }
    size_t required = static_cast<size_t>(_width) * static_cast<size_t>(_height);
    if (required == 0) {
        return;
    }
    if (_pulseGroup && _pulseCapacity == required) {
        return;
    }
    free(_pulseGroup);
    _pulseGroup = nullptr;
    _pulseCapacity = 0;
    _pulseGroup = static_cast<uint16_t*>(calloc(required, sizeof(uint16_t)));
    if (_pulseGroup) {
        _pulseCapacity = required;
    }
}

void GameOfLifeAnimation::startPulseGroup() {
    if (!_colorPulseMode) {
        return;
    }
    ensurePulseStorage();
    if (!_pulseGroup || _pulseCapacity == 0) {
        return;
    }

    _currentPulseGroup = _pulseGroupCounter;
    const uint8_t slot = static_cast<uint8_t>(_currentPulseGroup % PULSE_HISTORY);
    const uint32_t now = millis();
    _pulseStartMs[slot] = now;

    if (_currentPalette && !_currentPalette->empty()) {
        const auto& palette = *_currentPalette;
        const uint8_t paletteSize = static_cast<uint8_t>(palette.size());
        uint8_t base = (_pulseGroupCounter + _paletteIndex) % paletteSize;
        uint8_t secondary = (base + 1) % paletteSize;
        uint8_t tertiary = (base + 2) % paletteSize;

        uint8_t mostRed = base;
        uint16_t bestScore = 0;
        for (uint8_t i = 0; i < paletteSize; ++i) {
            const CRGB& c = palette[i];
            uint16_t score = static_cast<uint16_t>(c.r) * 3 + static_cast<uint16_t>(c.g);
            if (score > bestScore) {
                bestScore = score;
                mostRed = i;
            }
        }

        _pulsePrimary[slot] = base;
        _pulseSecondary[slot] = secondary;
        _pulseTertiary[slot] = mostRed;
    } else {
        _pulsePrimary[slot] = 0;
        _pulseSecondary[slot] = 0;
        _pulseTertiary[slot] = 0;
    }

    _pulsePeriodMs[slot] = 1600 + ((_pulseGroupCounter * 37U) % 7U) * 360U;
}

CRGB GameOfLifeAnimation::computePulseColor(int idx) const {
    CRGB base = _colors[idx];
    if (!_colorPulseMode || !_pulseGroup || _pulseCapacity == 0 || !_currentPalette || _currentPalette->empty()) {
        return base;
    }
    const auto& palette = *_currentPalette;
    if (palette.empty()) {
        return base;
    }

    const uint16_t group = _pulseGroup[idx];
    const uint8_t slot = static_cast<uint8_t>(group % PULSE_HISTORY);
    const uint8_t paletteSize = static_cast<uint8_t>(palette.size());
    CRGB primary = palette[_pulsePrimary[slot] % paletteSize];
    CRGB secondary = palette[_pulseSecondary[slot] % paletteSize];
    CRGB tertiary = palette[_pulseTertiary[slot] % paletteSize];

    uint32_t period = _pulsePeriodMs[slot];
    if (period < 200U) {
        period = 200U;
    }
    const uint32_t elapsed = millis() - _pulseStartMs[slot];
    const uint32_t progress = elapsed % period;
    const float phase = static_cast<float>(progress) / static_cast<float>(period);
    const float blendFactor = (phase < 0.5f) ? (phase * 2.0f) : ((1.0f - phase) * 2.0f);
    const uint8_t blendAB = static_cast<uint8_t>(blendFactor * 255.0f);

    CRGB color = blend(primary, secondary, blendAB);
    const uint8_t tertiaryMix = sin8((elapsed / 4U) & 0xFFU) / 2;
    color = blend(color, tertiary, tertiaryMix);

    if (_age) {
        const uint8_t ageValue = _age[idx];
        const uint8_t wave = sin8(ageValue * 16 + slot * 21);
        color.nscale8_video(180 + (wave >> 1));
    }

    return color;
}


void GameOfLifeAnimation::updateClusterColors() {
    if (_clusterColorMode == ClusterColorMode::Off || !_componentIds) {
        return;
    }

    const int totalCells = _width * _height;
    memset(_componentIds, 0, sizeof(uint16_t) * totalCells);

    struct ClusterInfo {
        uint16_t id;
        uint16_t size;
        uint32_t ageSum;
        uint16_t perimeter;
        uint16_t minX;
        uint16_t maxX;
        uint16_t minY;
        uint16_t maxY;
        uint32_t hash;
    };

    std::vector<ClusterInfo> clusters;
    clusters.reserve(32);
    std::vector<int> stack;
    stack.reserve(totalCells);

    uint16_t nextId = 1;

    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            const int idx = index(x, y);
            if (!getCell(_gridA, x, y) || _componentIds[idx] != 0) {
                continue;
            }

            ClusterInfo info{
                nextId,
                0,
                0,
                0,
                static_cast<uint16_t>(x),
                static_cast<uint16_t>(x),
                static_cast<uint16_t>(y),
                static_cast<uint16_t>(y),
                2166136261u
            };

            stack.push_back(idx);
            _componentIds[idx] = nextId;

            while (!stack.empty()) {
                const int current = stack.back();
                stack.pop_back();

                const int cx = current % _width;
                const int cy = current / _width;

                info.size++;
                if (_age) {
                    info.ageSum += _age[current];
                }
                info.minX = std::min<uint16_t>(info.minX, static_cast<uint16_t>(cx));
                info.maxX = std::max<uint16_t>(info.maxX, static_cast<uint16_t>(cx));
                info.minY = std::min<uint16_t>(info.minY, static_cast<uint16_t>(cy));
                info.maxY = std::max<uint16_t>(info.maxY, static_cast<uint16_t>(cy));

                info.hash ^= static_cast<uint32_t>(current);
                info.hash *= 16777619u;

                for (const auto& offset : NEIGHBOR_OFFSETS_4) {
                    const int nx = cx + offset[0];
                    const int ny = cy + offset[1];
                    if (!isNeighborAlive(_gridA, nx, ny)) {
                        ++info.perimeter;
                    }
                }

                const int neighborCount = (_neighborMode == NeighborMode::Eight) ? 8 : 4;
                const int (*offsets)[2] = (_neighborMode == NeighborMode::Eight) ? NEIGHBOR_OFFSETS_8 : NEIGHBOR_OFFSETS_4;
                for (int i = 0; i < neighborCount; ++i) {
                    int nx = cx + offsets[i][0];
                    int ny = cy + offsets[i][1];
                    if (!_wrapEdges && (nx < 0 || nx >= _width || ny < 0 || ny >= _height)) {
                        continue;
                    }
                    const int wrappedX = _wrapEdges ? ((nx % _width + _width) % _width) : nx;
                    const int wrappedY = _wrapEdges ? ((ny % _height + _height) % _height) : ny;
                    const int nIdx = index(wrappedX, wrappedY);
                    if (getCell(_gridA, nx, ny) && _componentIds[nIdx] == 0) {
                        _componentIds[nIdx] = nextId;
                        stack.push_back(nIdx);
                    }
                }
            }

            clusters.push_back(info);
            ++nextId;
        }
    }

    if (clusters.empty()) {
        return;
    }

    std::array<CRGB, 4> palette{
        CRGB(255, 64, 96),
        CRGB(64, 255, 160),
        CRGB(64, 160, 255),
        CRGB(255, 196, 64)
    };

    if (_currentPalette && !_currentPalette->empty()) {
        for (size_t i = 0; i < palette.size(); ++i) {
            if (i < _currentPalette->size()) {
                palette[i] = (*_currentPalette)[i];
            }
        }
    }

    uint16_t maxSize = 0;
    uint32_t maxAvgAge = 0;
    float maxCompactness = 0.0f;
    for (const auto& c : clusters) {
        maxSize = std::max<uint16_t>(maxSize, c.size);
        const uint32_t avgAge = c.size ? (c.ageSum / c.size) : 0;
        maxAvgAge = std::max(maxAvgAge, avgAge);
        const float boundingArea = static_cast<float>((c.maxX - c.minX + 1) * (c.maxY - c.minY + 1));
        float compactness = 0.0f;
        if (c.perimeter > 0) {
            compactness = static_cast<float>(c.size) / static_cast<float>(c.perimeter);
        }
        maxCompactness = std::max(maxCompactness, compactness);
    }

    std::vector<CRGB> clusterColors(nextId, palette[0]);

    for (const auto& c : clusters) {
        int colorIndex = 0;
        switch (_clusterColorMode) {
            case ClusterColorMode::Size: {
                if (maxSize > 0) {
                    colorIndex = static_cast<int>((static_cast<uint32_t>(c.size) * palette.size()) / (maxSize + 1));
                }
                break;
            }
            case ClusterColorMode::Age: {
                if (c.size > 0 && maxAvgAge > 0) {
                    const uint32_t avgAge = c.ageSum / c.size;
                    colorIndex = static_cast<int>((avgAge * palette.size()) / (maxAvgAge + 1));
                }
                break;
            }
            case ClusterColorMode::Shape: {
                float compactness = 0.0f;
                if (c.perimeter > 0) {
                    compactness = static_cast<float>(c.size) / static_cast<float>(c.perimeter);
                }
                if (maxCompactness > 0.0f) {
                    colorIndex = static_cast<int>((compactness / maxCompactness) * palette.size());
                }
                break;
            }
            case ClusterColorMode::Stable: {
                colorIndex = static_cast<int>(c.hash % palette.size());
                break;
            }
            case ClusterColorMode::Off:
            default:
                break;
        }

        if (colorIndex < 0) colorIndex = 0;
        if (colorIndex >= static_cast<int>(palette.size())) {
            colorIndex = static_cast<int>(palette.size()) - 1;
        }
        clusterColors[c.id] = palette[colorIndex];
    }

    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            const int idx = index(x, y);
            const uint16_t component = _componentIds ? _componentIds[idx] : 0;
            if (component > 0 && component < clusterColors.size()) {
                _colors[idx] = clusterColors[component];
            }
        }
    }
}

void GameOfLifeAnimation::renderFrame() {
    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            const int idx = index(x, y);
            const int ledIdx = mapXYtoLED(x, y);
            if (ledIdx < 0 || ledIdx >= _numLeds) continue;

            const bool alive = getCell(_gridA, x, y);
            const uint8_t target = alive ? MAX_INTENSITY : 0;
            const uint8_t step = alive ? _fadeInStep : _fadeOutStep;

            if (_intensity[idx] != target) {
                if (alive) {
                    _intensity[idx] = qadd8(_intensity[idx], step);
                } else {
                    _intensity[idx] = qsub8(_intensity[idx], step);
                    if (_intensity[idx] == 0) {
                        _colors[idx] = CRGB::Black;
                    }
                }
            }

            CRGB pixel = _colors[idx];
            pixel.nscale8_video(_intensity[idx]);

            int highlightColumn = _sweepColumn;
            if (highlightColumn < 0) {
                highlightColumn = 0;
            } else if (highlightColumn >= _width) {
                highlightColumn = _width - 1;
            }

            if (_updateMode == UpdateMode::Sweep && _highlightWidth > 0) {
                const int distance = abs(x - highlightColumn);
                if (distance < _highlightWidth) {
                    const uint8_t falloff = static_cast<uint8_t>(
                        ((static_cast<int>(_highlightWidth) - distance) * 255) / static_cast<int>(_highlightWidth)
                    );
                    uint8_t boost = scale8(_highlightBoost, falloff);
                    CRGB highlight = _highlightColor;
                    highlight.nscale8_video(boost);
                    nblend(pixel, highlight, boost);
                }
            }

            pixel.nscale8_video(_brightness);
            leds[ledIdx] = pixel;
        }
    }
}

void GameOfLifeAnimation::applyBirth(int idx) {
    if (_colorPulseMode) {
        ensurePulseStorage();
        if (_pulseGroup && static_cast<size_t>(idx) < _pulseCapacity) {
            _pulseGroup[idx] = _currentPulseGroup;
        }
    }

    if (_uniformBirthColorEnabled) {
        _colors[idx] = _uniformBirthColor;
    } else if (_colorPulseMode && _currentPalette && !_currentPalette->empty()) {
        uint8_t slot = static_cast<uint8_t>(_currentPulseGroup % PULSE_HISTORY);
        const auto& palette = *_currentPalette;
        _colors[idx] = palette[_pulsePrimary[slot] % palette.size()];
    } else {
        _colors[idx] = chooseColor();
    }
    _intensity[idx] = 0;
}

void GameOfLifeAnimation::applyDeath(int idx) {
    // Keep current colour for fade-out; brightness will decay in renderFrame
}

bool GameOfLifeAnimation::detectStagnation(bool anyChange) {
    const uint32_t generation = ++_generationCounter;

    if (!anyChange) {
        ++_stepsSinceChange;
    } else {
        _stepsSinceChange = 0;
    }

    int aliveCount = 0;
    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            if (getCell(_gridA, x, y)) {
                ++aliveCount;
            }
        }
    }

    bool restarted = false;

    if (aliveCount == 0 || _stepsSinceChange >= MAX_STEPS_WITHOUT_CHANGE) {
        Serial.println("GameOfLife: restarting due to stagnation/empty grid");
        randomize(_seedDensity);
        restarted = true;
    } else {
        const uint32_t hash = hashGrid(_gridA);
        bool repeatDetected = false;
        const bool extendedLoop = detectExtendedLoop(hash, generation);

        if (hash == _lastHash) {
            if (++_hashRepeatCount >= MAX_REPEAT_HASH) {
                repeatDetected = true;
                Serial.println("GameOfLife: steady-state pattern detected, restarting");
            }
        } else if (_prevHashSecondary != 0 && hash == _prevHashSecondary) {
            ++_twoStateRepeatCount;
            _hashRepeatCount = 0;
            if (_twoStateRepeatCount >= MAX_REPEAT_HASH) {
                repeatDetected = true;
                Serial.println("GameOfLife: oscillating pattern detected, restarting");
            }
        } else {
            _hashRepeatCount = 0;
            _twoStateRepeatCount = 0;
        }

        if (!repeatDetected && extendedLoop) {
            repeatDetected = true;
            Serial.println("GameOfLife: repeating loop detected, restarting");
        }

        _prevHashSecondary = _lastHash;
        _lastHash = hash;

        if (repeatDetected) {
            randomize(_seedDensity);
            restarted = true;
        }
    }

    if (restarted) {
        _generationReady = false;
    }

    return restarted;
}

bool GameOfLifeAnimation::detectExtendedLoop(uint32_t hash, uint32_t generation) {
    bool matched = false;

    for (uint8_t i = 0; i < _hashHistorySize; ++i) {
        const uint8_t idx = (_hashHistoryIndex + HASH_HISTORY - 1 - i) % HASH_HISTORY;
        if (_hashHistory[idx] == hash) {
            const uint32_t previousGeneration = _hashGeneration[idx];
            const uint32_t period = generation - previousGeneration;
            if (period > 0 && period <= 255) {
                matched = true;
                if (_loopPeriod == period) {
                    if (_loopPeriodStreak < 255) {
                        ++_loopPeriodStreak;
                    }
                } else {
                    _loopPeriod = static_cast<uint8_t>(period);
                    _loopPeriodStreak = 1;
                }
                if (_loopPeriodStreak >= LOOP_REPEAT_THRESHOLD) {
                    return true;
                }
            }
            break;
        }
    }

    if (!matched) {
        _loopPeriod = 0;
        _loopPeriodStreak = 0;
    }

    _hashHistory[_hashHistoryIndex] = hash;
    _hashGeneration[_hashHistoryIndex] = generation;
    if (_hashHistorySize < HASH_HISTORY) {
        ++_hashHistorySize;
    }
    _hashHistoryIndex = (_hashHistoryIndex + 1) % HASH_HISTORY;
    return false;
}

uint32_t GameOfLifeAnimation::hashGrid(const uint8_t* grid) const {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < _gridBytes; ++i) {
        hash ^= grid[i];
        hash *= 16777619u;
    }
    return hash;
}

CRGB GameOfLifeAnimation::chooseColor() const {
    if (_usePalette && _currentPalette && !_currentPalette->empty()) {
        return (*_currentPalette)[random16(_currentPalette->size())];
    }

    CHSV hsv(random8(), 200, 255);
    CRGB rgb;
    hsv2rgb_rainbow(hsv, rgb);
    return rgb;
}

int GameOfLifeAnimation::mapXYtoLED(int x, int y) const {
    int panel = x / PANEL_SIZE;
    int localX = x % PANEL_SIZE;
    int localY = y;

    int angle = 0;
    switch (panel) {
        case 0: angle = _rotation1; break;
        case 1: angle = _rotation2; break;
        case 2: angle = _rotation3; break;
        default: angle = 0; break;
    }

    angle = ((angle % 360) + 360) % 360;
    int rx = localX;
    int ry = localY;
    switch (angle) {
        case 90:
            rx = PANEL_SIZE - 1 - localY;
            ry = localX;
            break;
        case 180:
            rx = PANEL_SIZE - 1 - localX;
            ry = PANEL_SIZE - 1 - localY;
            break;
        case 270:
            rx = localY;
            ry = PANEL_SIZE - 1 - localX;
            break;
        default:
            break;
    }

    int mappedPanel = panel;
    if (_panelOrder != 0) {
        mappedPanel = (_panelCount - 1) - panel;
    }

    const bool reverse = (ry & 1);
    const int serpX = reverse ? (PANEL_SIZE - 1 - rx) : rx;
    return mappedPanel * PANEL_SIZE * PANEL_SIZE + ry * PANEL_SIZE + serpX;
}

bool GameOfLifeAnimation::getCell(const uint8_t* grid, int x, int y) const {
    int wrappedX = x;
    int wrappedY = y;
    if (_wrapEdges) {
        wrappedX = (x % _width + _width) % _width;
        wrappedY = (y % _height + _height) % _height;
    } else {
        if (x < 0 || x >= _width || y < 0 || y >= _height) {
            return false;
        }
    }
    const int bitIndex = index((wrappedX + _width) % _width, (wrappedY + _height) % _height);
    return (grid[bitIndex >> 3] >> (bitIndex & 7)) & 0x01;
}

void GameOfLifeAnimation::setCell(uint8_t* grid, int x, int y, bool value) {
    int wrappedX = x;
    int wrappedY = y;
    if (_wrapEdges) {
        wrappedX = (x % _width + _width) % _width;
        wrappedY = (y % _height + _height) % _height;
    } else {
        if (x < 0 || x >= _width || y < 0 || y >= _height) {
            return;
        }
    }
    const int bitIndex = index((wrappedX + _width) % _width, (wrappedY + _height) % _height);
    uint8_t& byte = grid[bitIndex >> 3];
    const uint8_t mask = static_cast<uint8_t>(1U << (bitIndex & 7));
    if (value) {
        byte |= mask;
    } else {
        byte &= ~mask;
    }
}

bool GameOfLifeAnimation::isNeighborAlive(const uint8_t* grid, int x, int y) const {
    if (!_wrapEdges && (x < 0 || x >= _width || y < 0 || y >= _height)) {
        return false;
    }
    return getCell(grid, x, y);
}

int GameOfLifeAnimation::countNeighbors(const uint8_t* grid, int x, int y) const {
    int count = 0;
    const int neighborCount = (_neighborMode == NeighborMode::Eight) ? 8 : 4;
    const int (*offsets)[2] = (_neighborMode == NeighborMode::Eight) ? NEIGHBOR_OFFSETS_8 : NEIGHBOR_OFFSETS_4;
    for (int i = 0; i < neighborCount; ++i) {
        const int nx = x + offsets[i][0];
        const int ny = y + offsets[i][1];
        if (isNeighborAlive(grid, nx, ny)) {
            ++count;
        }
    }
    return count;
}






