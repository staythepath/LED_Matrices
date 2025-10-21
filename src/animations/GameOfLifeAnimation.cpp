#include "GameOfLifeAnimation.h"

#include <Arduino.h>
#include <FastLED.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>

extern CRGB leds[];

static constexpr int PANEL_SIZE = 16;
static constexpr uint8_t DEFAULT_DENSITY = 33;

GameOfLifeAnimation::GameOfLifeAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
    : BaseAnimation(numLeds, brightness, panelCount),
      _gridA(nullptr),
      _gridB(nullptr),
      _intensity(nullptr),
      _colors(nullptr),
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
      _lastHash(0),
      _hashRepeatCount(0),
      _stepsSinceChange(0),
      _allPalettes(nullptr),
      _currentPalette(nullptr),
      _usePalette(true),
      _paletteIndex(0),
      _highlightBoost(80),
      _fadeInStep(DEFAULT_FADE_IN_STEP),
      _fadeOutStep(DEFAULT_FADE_OUT_STEP),
      _panelOrder(1),
      _rotation1(0),
      _rotation2(0),
      _rotation3(0) {

    const size_t gridSize = static_cast<size_t>(_width) * _height;

    _gridA = static_cast<uint8_t*>(calloc(_gridBytes, 1));
    _gridB = static_cast<uint8_t*>(calloc(_gridBytes, 1));
    _intensity = static_cast<uint8_t*>(calloc(gridSize, 1));
    _colors = static_cast<CRGB*>(calloc(gridSize, sizeof(CRGB)));

    if (!_gridA || !_gridB || !_intensity || !_colors) {
        Serial.println("GameOfLife: memory allocation failed");
    }
}

GameOfLifeAnimation::~GameOfLifeAnimation() {
    free(_gridA);
    free(_gridB);
    free(_intensity);
    free(_colors);
}

void GameOfLifeAnimation::setUpdateInterval(unsigned long intervalMs) {
    _baseIntervalMs = std::max<unsigned long>(intervalMs, 6UL);
    recalculateSweepInterval();
}

void GameOfLifeAnimation::setSpeedMultiplier(float multiplier) {
    _speedMultiplier = std::max(0.25f, std::min(multiplier, 6.0f));
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
    // Repurpose "column skip" control as sweep speed preference (1-10)
    _sweepSpeedSetting = std::max(1, std::min(skip, 10));
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

void GameOfLifeAnimation::setRotationAngle1(int angle) { _rotation1 = angle; }
void GameOfLifeAnimation::setRotationAngle2(int angle) { _rotation2 = angle; }
void GameOfLifeAnimation::setRotationAngle3(int angle) { _rotation3 = angle; }
void GameOfLifeAnimation::setPanelOrder(int order) { _panelOrder = order; }

void GameOfLifeAnimation::recalculateSweepInterval() {
    float base = std::max(6.0f, static_cast<float>(_baseIntervalMs));
    float speedFactor = 0.5f + (_sweepSpeedSetting - 1) * 0.25f; // range approx 0.5 .. 2.75
    float interval = base / speedFactor;
    if (interval < 4.0f) {
        interval = 4.0f;
    }
    _sweepIntervalMs = static_cast<uint32_t>(interval);
}
void GameOfLifeAnimation::begin() {
    if (!_gridA || !_gridB) return;
    randomize(DEFAULT_DENSITY);
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

    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            if (random(100) < density) {
                setCell(_gridA, x, y, true);
                const int idx = index(x, y);
                _colors[idx] = chooseColor();
                _intensity[idx] = random8(40, 120);
            }
        }
    }

    _generationReady = false;
    _anyChangeThisPass = false;
    _sweepDirection = 1;
    _sweepColumn = 0;
    _hashRepeatCount = 0;
    _stepsSinceChange = 0;
    _lastHash = hashGrid(_gridA);
    const uint32_t now = millis();
    _lastSweepMs = now;
    _lastRenderMs = now;
    recalculateSweepInterval();
}

void GameOfLifeAnimation::update() {
    if (!_gridA || !_gridB) return;

    const uint32_t now = millis();

    if (!_generationReady) {
        prepareNextGeneration();
        _generationReady = true;
        _anyChangeThisPass = false;
        _sweepColumn = (_sweepDirection > 0) ? 0 : (_width - 1);
        _lastSweepMs = now;
    }

    const uint32_t sweepInterval = std::max<uint32_t>(4, static_cast<uint32_t>(_sweepIntervalMs / _speedMultiplier));
    if (now - _lastSweepMs >= sweepInterval) {
        applyColumn(_sweepColumn);
        _lastSweepMs = now;

        _sweepColumn += _sweepDirection;
        bool completedPass = false;
        if (_sweepColumn >= _width) {
            _sweepColumn = _width - 1;
            completedPass = true;
        } else if (_sweepColumn < 0) {
            _sweepColumn = 0;
            completedPass = true;
        }
        if (completedPass) {
            const bool restarted = detectStagnation(_anyChangeThisPass);
            if (!restarted) {
                _sweepDirection = -_sweepDirection;
            }
            _generationReady = false;
        }
    }

    if (now - _lastRenderMs >= _renderIntervalMs) {
        renderFrame();
        _lastRenderMs = now;
        FastLED.show();
    }
}

void GameOfLifeAnimation::prepareNextGeneration() {
    bool anyAlive = false;

    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            const bool alive = getCell(_gridA, x, y);
            const int neighbors = countNeighbors(_gridA, x, y);
            bool nextAlive = false;
            if (alive) {
                nextAlive = (neighbors == 2 || neighbors == 3);
            } else {
                nextAlive = (neighbors == 3);
            }

            setCell(_gridB, x, y, nextAlive);
            if (nextAlive) {
                anyAlive = true;
            }
        }
    }

    // Avoid blank board for long periods
    if (!anyAlive) {
        randomize(DEFAULT_DENSITY);
        prepareNextGeneration();
    }
}

void GameOfLifeAnimation::applyColumn(int column) {
    if (column < 0 || column >= _width) return;

    for (int y = 0; y < _height; ++y) {
        const bool currentAlive = getCell(_gridA, column, y);
        const bool nextAlive = getCell(_gridB, column, y);
        if (currentAlive == nextAlive) continue;

        const int idx = index(column, y);
        if (nextAlive) {
            applyBirth(idx);
        } else {
            applyDeath(idx);
        }

        setCell(_gridA, column, y, nextAlive);
        _anyChangeThisPass = true;
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
            const int distance = abs(x - highlightColumn);
            if (distance <= 1) {
                const uint8_t boost = (distance == 0) ? _highlightBoost : (_highlightBoost / 2);
                pixel.r = qadd8(pixel.r, boost);
                pixel.g = qadd8(pixel.g, boost);
                pixel.b = qadd8(pixel.b, boost);
            }

            pixel.nscale8_video(_brightness);
            leds[ledIdx] = pixel;
        }
    }
}

void GameOfLifeAnimation::applyBirth(int idx) {
    _colors[idx] = chooseColor();
    _intensity[idx] = 0;
}

void GameOfLifeAnimation::applyDeath(int idx) {
    // Keep current colour for fade-out; brightness will decay in renderFrame
}

bool GameOfLifeAnimation::detectStagnation(bool anyChange) {
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
        randomize(DEFAULT_DENSITY);
        restarted = true;
    } else {
        const uint32_t hash = hashGrid(_gridA);
        if (hash == _lastHash) {
            if (++_hashRepeatCount >= MAX_REPEAT_HASH) {
                Serial.println("GameOfLife: repeating pattern detected, restarting");
                randomize(DEFAULT_DENSITY);
                restarted = true;
            }
        } else {
            _hashRepeatCount = 0;
            _lastHash = hash;
        }
    }

    if (restarted) {
        _generationReady = false;
    }

    return restarted;
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
    x = (x + _width) % _width;
    y = (y + _height) % _height;
    const int bitIndex = index(x, y);
    return (grid[bitIndex >> 3] >> (bitIndex & 7)) & 0x01;
}

void GameOfLifeAnimation::setCell(uint8_t* grid, int x, int y, bool value) {
    x = (x + _width) % _width;
    y = (y + _height) % _height;
    const int bitIndex = index(x, y);
    uint8_t& byte = grid[bitIndex >> 3];
    const uint8_t mask = static_cast<uint8_t>(1U << (bitIndex & 7));
    if (value) {
        byte |= mask;
    } else {
        byte &= ~mask;
    }
}

int GameOfLifeAnimation::countNeighbors(const uint8_t* grid, int x, int y) const {
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            if (getCell(grid, x + dx, y + dy)) {
                ++count;
            }
        }
    }
    return count;
}
