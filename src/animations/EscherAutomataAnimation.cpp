#include "EscherAutomataAnimation.h"

#include <Arduino.h>
#include <algorithm>
#include <cmath>

extern CRGB leds[];

static constexpr int PANEL_SIZE = 16;
static constexpr float PI_F = 3.14159265f;
static constexpr uint8_t RULE_TABLE[] = { 30, 45, 73, 90, 102, 110, 129, 165, 225 };

EscherAutomataAnimation::EscherAutomataAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
    : BaseAnimation(numLeds, brightness, panelCount)
    , _mode(Mode::LangtonsAnt)
    , _primaryControl(50)
    , _secondaryControl(55)
    , _updateIntervalMs(42)
    , _lastUpdateMs(0)
    , _width(PANEL_SIZE * panelCount)
    , _height(PANEL_SIZE)
    , _panelOrder(1)
    , _rotation1(0)
    , _rotation2(0)
    , _rotation3(0)
    , _allPalettes(nullptr)
    , _currentPalette(nullptr)
    , _usePalette(true)
    , _paletteIndex(0)
    , _cellStates(static_cast<size_t>(_width) * _height, 0)
    , _cellColors(static_cast<size_t>(_width) * _height, CRGB::Black)
    , _ruleCurrent(static_cast<size_t>(_width), 0)
    , _ruleNext(static_cast<size_t>(_width), 0)
    , _elementaryGeneration(0)
    , _moirePhase(0.0f)
    , _moireSpeed(0.45f) {
}

void EscherAutomataAnimation::begin() {
    clearBuffers();
    _lastUpdateMs = 0;
    resetForMode();
}

void EscherAutomataAnimation::update() {
    const uint32_t now = millis();
    if ((now - _lastUpdateMs) < _updateIntervalMs) {
        return;
    }
    _lastUpdateMs = now;

    switch (_mode) {
        case Mode::LangtonsAnt:
            stepLangtonsAnt();
            renderLangtonsAnt();
            break;
        case Mode::Elementary:
            stepElementary();
            renderElementary();
            break;
        case Mode::RecursiveMoire:
            renderMoire();
            break;
    }
    FastLED.show();
}

void EscherAutomataAnimation::setUpdateInterval(uint32_t intervalMs) {
    _updateIntervalMs = std::max<uint32_t>(intervalMs, 1);
}

void EscherAutomataAnimation::setMode(Mode mode) {
    if (_mode == mode) {
        return;
    }
    _mode = mode;
    resetForMode();
}

void EscherAutomataAnimation::setPrimaryControl(uint8_t value) {
    if (value > 100) {
        value = 100;
    }
    const bool changed = (_primaryControl != value);
    _primaryControl = value;
    if (_mode == Mode::LangtonsAnt && changed) {
        resetLangtonsAnt();
    } else if (_mode == Mode::Elementary && changed) {
        resetElementary();
    }
}

void EscherAutomataAnimation::setSecondaryControl(uint8_t value) {
    if (value > 100) {
        value = 100;
    }
    if (_secondaryControl == value) {
        return;
    }
    _secondaryControl = value;
    if (_mode == Mode::RecursiveMoire) {
        updateMoireSpeed();
    } else if (_mode == Mode::LangtonsAnt && _ants.empty()) {
        resetLangtonsAnt();
    }
}

void EscherAutomataAnimation::setAllPalettes(const std::vector<std::vector<CRGB>>* palettes) {
    _allPalettes = palettes;
    setCurrentPalette(_paletteIndex);
}

void EscherAutomataAnimation::setCurrentPalette(int index) {
    _paletteIndex = index;
    if (_allPalettes && index >= 0 && index < static_cast<int>(_allPalettes->size())) {
        _currentPalette = &(_allPalettes->at(index));
    } else {
        _currentPalette = nullptr;
    }
    if (_mode == Mode::LangtonsAnt) {
        resetLangtonsAnt();
    }
}

void EscherAutomataAnimation::setUsePalette(bool usePalette) {
    _usePalette = usePalette;
}

void EscherAutomataAnimation::setPanelOrder(int order) {
    _panelOrder = order;
}

void EscherAutomataAnimation::setRotationAngle1(int angle) { _rotation1 = angle; }
void EscherAutomataAnimation::setRotationAngle2(int angle) { _rotation2 = angle; }
void EscherAutomataAnimation::setRotationAngle3(int angle) { _rotation3 = angle; }

int EscherAutomataAnimation::mapXYtoLED(int x, int y) const {
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

void EscherAutomataAnimation::clearBuffers() {
    std::fill(_cellStates.begin(), _cellStates.end(), 0);
    std::fill(_cellColors.begin(), _cellColors.end(), CRGB::Black);
}

void EscherAutomataAnimation::resetForMode() {
    clearBuffers();
    _ants.clear();
    _elementaryGeneration = 0;
    _moirePhase = 0.0f;
    updateMoireSpeed();

    switch (_mode) {
        case Mode::LangtonsAnt:
            resetLangtonsAnt();
            break;
        case Mode::Elementary:
            resetElementary();
            break;
        case Mode::RecursiveMoire:
            FastLED.clear();
            break;
    }
}

void EscherAutomataAnimation::resetLangtonsAnt() {
    clearBuffers();
    _ants.clear();
    const uint8_t count = antCountFromPrimary();
    _ants.reserve(count);

    for (uint8_t i = 0; i < count; ++i) {
        Ant ant;
        ant.x = random16(_width);
        ant.y = random16(_height);
        ant.direction = random8(4);
        ant.color = pickPaletteColor(static_cast<uint16_t>(i * 53));
        _ants.push_back(ant);
    }
}

void EscherAutomataAnimation::resetElementary() {
    std::fill(_ruleCurrent.begin(), _ruleCurrent.end(), 0);
    std::fill(_ruleNext.begin(), _ruleNext.end(), 0);
    clearBuffers();
    if (!_ruleCurrent.empty()) {
        _ruleCurrent[_width / 2] = 1;
    }
    _elementaryGeneration = 0;
}

CRGB EscherAutomataAnimation::pickPaletteColor(uint16_t seed) const {
    if (_usePalette && _currentPalette && !_currentPalette->empty()) {
        const auto& palette = *_currentPalette;
        return palette[seed % palette.size()];
    }
    return CHSV(static_cast<uint8_t>(seed), 220, 255);
}

uint8_t EscherAutomataAnimation::antCountFromPrimary() const {
    return static_cast<uint8_t>(1 + (_primaryControl * 7) / 100); // 1..8
}

uint8_t EscherAutomataAnimation::langtonStepsPerFrame() const {
    return static_cast<uint8_t>(1 + (_secondaryControl * 63) / 100); // 1..64
}

uint8_t EscherAutomataAnimation::ruleIndexFromPrimary() const {
    const uint8_t maxIndex = static_cast<uint8_t>(sizeof(RULE_TABLE) / sizeof(RULE_TABLE[0]) - 1);
    const uint8_t idx = static_cast<uint8_t>((_primaryControl * maxIndex + 50) / 100);
    return std::min<uint8_t>(idx, maxIndex);
}

uint8_t EscherAutomataAnimation::ruleIterationsPerFrame() const {
    return static_cast<uint8_t>(1 + (_secondaryControl * 19) / 100); // 1..20
}

void EscherAutomataAnimation::updateMoireSpeed() {
    const float norm = static_cast<float>(_secondaryControl) / 100.0f;
    _moireSpeed = 0.35f + norm * 2.3f;
}

void EscherAutomataAnimation::stepLangtonsAnt() {
    if (_ants.empty()) {
        resetLangtonsAnt();
    }
    const uint8_t steps = langtonStepsPerFrame();
    for (uint8_t step = 0; step < steps; ++step) {
        for (auto& ant : _ants) {
            const int idx = index(ant.x, ant.y);
            uint8_t& cell = _cellStates[idx];
            if (cell) {
                ant.direction = (ant.direction + 3) & 0x03;
                cell = 0;
                _cellColors[idx].fadeToBlackBy(180);
            } else {
                ant.direction = (ant.direction + 1) & 0x03;
                cell = 1;
                _cellColors[idx] = ant.color;
            }

            switch (ant.direction & 0x03) {
                case 0: ant.y = (ant.y - 1 + _height) % _height; break;
                case 1: ant.x = (ant.x + 1) % _width; break;
                case 2: ant.y = (ant.y + 1) % _height; break;
                case 3: ant.x = (ant.x - 1 + _width) % _width; break;
            }
        }
    }
}

void EscherAutomataAnimation::renderLangtonsAnt() {
    const size_t total = _cellStates.size();
    for (size_t i = 0; i < total; ++i) {
        if (_cellStates[i] == 0) {
            _cellColors[i].fadeToBlackBy(24);
        }
    }

    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            const int idx = index(x, y);
            CRGB color = _cellColors[idx];
            color.nscale8_video(_brightness);
            leds[mapXYtoLED(x, y)] = color;
        }
    }
}

void EscherAutomataAnimation::stepElementary() {
    if (_ruleCurrent.empty()) {
        return;
    }
    const uint8_t ruleValue = RULE_TABLE[ruleIndexFromPrimary()];
    const uint8_t iterations = ruleIterationsPerFrame();

    for (uint8_t iter = 0; iter < iterations; ++iter) {
        for (int x = 0; x < _width; ++x) {
            const uint8_t left = _ruleCurrent[(x - 1 + _width) % _width];
            const uint8_t mid = _ruleCurrent[x];
            const uint8_t right = _ruleCurrent[(x + 1) % _width];
            const uint8_t pattern = static_cast<uint8_t>((left << 2) | (mid << 1) | right);
            _ruleNext[x] = (ruleValue >> pattern) & 0x01;
        }

        for (int y = _height - 1; y > 0; --y) {
            for (int x = 0; x < _width; ++x) {
                const int dst = index(x, y);
                const int src = index(x, y - 1);
                _cellStates[dst] = _cellStates[src];
                CRGB c = _cellColors[src];
                c.fadeToBlackBy(18);
                _cellColors[dst] = c;
            }
        }

        for (int x = 0; x < _width; ++x) {
            const uint8_t state = _ruleNext[x];
            _ruleCurrent[x] = state;
            const int idx = index(x, 0);
            _cellStates[idx] = state;
            if (state) {
                _cellColors[idx] = pickPaletteColor(static_cast<uint16_t>(_elementaryGeneration * 5 + x * 9));
            } else {
                _cellColors[idx] = CRGB::Black;
            }
        }

        ++_elementaryGeneration;
    }
}

void EscherAutomataAnimation::renderElementary() {
    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            const int idx = index(x, y);
            CRGB color = _cellColors[idx];
            if (_cellStates[idx] == 0) {
                color.fadeToBlackBy(25);
                _cellColors[idx] = color;
            }
            color.nscale8_video(_brightness);
            leds[mapXYtoLED(x, y)] = color;
        }
    }
}

void EscherAutomataAnimation::renderMoire() {
    _moirePhase += _moireSpeed;
    if (_moirePhase > 10000.0f) {
        _moirePhase -= 10000.0f;
    }

    const float normalizedPrimary = static_cast<float>(_primaryControl) / 100.0f;
    const float density = 0.8f + normalizedPrimary * 7.0f;
    const int layers = 2 + static_cast<int>(normalizedPrimary * 4.0f);
    const float cx = (_width - 1) * 0.5f;
    const float cy = (_height - 1) * 0.5f;
    const float invWidth = 1.0f / std::max(1, _width);
    const float invHeight = 1.0f / std::max(1, _height);

    const bool hasPalette = _usePalette && _currentPalette && !_currentPalette->empty();

    for (int y = 0; y < _height; ++y) {
        for (int x = 0; x < _width; ++x) {
            const float dx = (static_cast<float>(x) - cx) * invWidth;
            const float dy = (static_cast<float>(y) - cy) * invHeight;
            const float radius = sqrtf(dx * dx + dy * dy);
            const float angle = atan2f(dy, dx);

            float composite = sinf(radius * density * 4.1f + _moirePhase * 0.05f)
                            + cosf((dx + dy) * density * 3.3f - _moirePhase * 0.03f);
            float localRadius = radius * density * 2.2f + 0.3f;
            for (int layer = 1; layer <= layers; ++layer) {
                composite += sinf(localRadius + angle * layer * 0.6f + _moirePhase * 0.02f * layer);
                localRadius *= 1.14f;
            }
            composite /= (2.0f + layers);
            composite = composite * 0.5f + 0.5f;
            composite = constrain(composite, 0.0f, 1.0f);
            const uint8_t brightness = static_cast<uint8_t>(composite * 255.0f);

            CRGB color;
            if (hasPalette) {
                const auto& palette = *_currentPalette;
                const size_t paletteSize = palette.size();
                const size_t paletteIndex = std::min(paletteSize - 1,
                    (static_cast<size_t>(brightness) * paletteSize) >> 8);
                color = palette[paletteIndex];
                color.nscale8_video(brightness);
            } else {
                const float hueNorm = fmodf((angle / (2.0f * PI_F)) + 1.0f, 1.0f);
                color = CHSV(static_cast<uint8_t>(hueNorm * 255.0f), 200, brightness);
            }
            color.nscale8_video(_brightness);
            leds[mapXYtoLED(x, y)] = color;
        }
    }
}
