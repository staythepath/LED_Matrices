// File: LangtonsAntAnimation.cpp
// Langton's Ant cellular automaton for LED matrices

#include "LangtonsAntAnimation.h"
#include <Arduino.h>
#include <FastLED.h>
#include <cstring>
#include <ctype.h>
#include <new>

extern CRGB leds[];

LangtonsAntAnimation::LangtonsAntAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
    : BaseAnimation(numLeds, brightness, panelCount)
    , _panelCount(panelCount)
    , _width(panelCount * 16)
    , _height(16)
    , _intervalMs(60)
    , _lastUpdate(0)
    , _panelOrder(0)
    , _rotationAngle1(0)
    , _rotationAngle2(0)
    , _rotationAngle3(0)
    , _rule("LR")
    , _ruleLen(2)
    , _antCount(1)
    , _stepsPerFrame(8)
    , _wrapEdges(true)
    , _cells(nullptr)
    , _currentPalette(nullptr)
{
    size_t cellCount = _width * _height;
    try {
        _cells = new uint8_t[cellCount];
        memset(_cells, 0, cellCount);
    } catch (std::bad_alloc& e) {
        Serial.println("LangtonsAnt: Failed to allocate cell grid");
        _cells = nullptr;
    }
}

LangtonsAntAnimation::~LangtonsAntAnimation() {
    if (_cells) {
        delete[] _cells;
        _cells = nullptr;
    }
}

void LangtonsAntAnimation::begin() {
    FastLED.clear(true);
    _lastUpdate = millis();
    resetSimulation();
}

void LangtonsAntAnimation::setBrightness(uint8_t b) {
    _brightness = b;
    FastLED.setBrightness(b);
}

void LangtonsAntAnimation::setRule(const String& rule) {
    String cleaned;
    cleaned.reserve(rule.length());
    for (size_t i = 0; i < rule.length(); i++) {
        char c = rule.charAt(i);
        c = (char)toupper(c);
        if (c == 'L' || c == 'R') {
            cleaned += c;
        }
    }
    if (cleaned.length() > 8) {
        cleaned = cleaned.substring(0, 8);
    }
    if (cleaned.length() == 0) {
        cleaned = "LR";
    }
    _rule = cleaned;
    _ruleLen = (uint8_t)_rule.length();
    resetSimulation();
}

void LangtonsAntAnimation::setAntCount(uint8_t count) {
    if (count < 1) count = 1;
    if (count > 6) count = 6;
    _antCount = count;
    resetSimulation();
}

void LangtonsAntAnimation::setStepsPerFrame(uint8_t steps) {
    if (steps < 1) steps = 1;
    if (steps > 50) steps = 50;
    _stepsPerFrame = steps;
}

void LangtonsAntAnimation::resetSimulation() {
    if (_cells) {
        memset(_cells, 0, _width * _height);
    }
    _ants.clear();
    _ants.reserve(_antCount);

    int centerX = _width / 2;
    int centerY = _height / 2;
    for (uint8_t i = 0; i < _antCount; i++) {
        Ant ant;
        ant.x = (centerX + (int)i) % _width;
        ant.y = (centerY + (int)i) % _height;
        ant.dir = i % 4;
        _ants.push_back(ant);
    }
}

void LangtonsAntAnimation::update() {
    if (!_cells) {
        return;
    }
    unsigned long now = millis();
    if (now - _lastUpdate < _intervalMs) {
        return;
    }
    _lastUpdate = now;

    for (uint8_t step = 0; step < _stepsPerFrame; step++) {
        for (size_t i = 0; i < _ants.size(); i++) {
            stepAnt(_ants[i]);
        }
    }
    drawGrid();
    FastLED.show();
}

void LangtonsAntAnimation::stepAnt(Ant& ant) {
    if (!_cells || _ruleLen == 0) return;

    int idx = ant.y * _width + ant.x;
    uint8_t state = _cells[idx];
    char turn = _rule.charAt(state % _ruleLen);

    if (turn == 'L') {
        ant.dir = (ant.dir + 3) % 4;
    } else {
        ant.dir = (ant.dir + 1) % 4;
    }

    _cells[idx] = (state + 1) % _ruleLen;

    int nx = ant.x;
    int ny = ant.y;
    switch (ant.dir) {
        case 0: ny -= 1; break;
        case 1: nx += 1; break;
        case 2: ny += 1; break;
        case 3: nx -= 1; break;
        default: break;
    }

    if (_wrapEdges) {
        nx = (nx + _width) % _width;
        ny = (ny + _height) % _height;
    } else {
        if (nx < 0) { nx = 0; ant.dir = 1; }
        if (nx >= _width) { nx = _width - 1; ant.dir = 3; }
        if (ny < 0) { ny = 0; ant.dir = 2; }
        if (ny >= _height) { ny = _height - 1; ant.dir = 0; }
    }

    ant.x = nx;
    ant.y = ny;
}

void LangtonsAntAnimation::drawGrid() {
    if (!_cells) return;

    bool hasPalette = _currentPalette && !_currentPalette->empty();
    size_t paletteSize = hasPalette ? _currentPalette->size() : 0;

    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            int cellIndex = y * _width + x;
            uint8_t state = _cells[cellIndex];
            CRGB color = CRGB::Black;
            if (state > 0) {
                if (hasPalette) {
                    color = (*_currentPalette)[state % paletteSize];
                } else {
                    color = CHSV(state * (255 / _ruleLen), 255, 255);
                }
            }

            int ledIndex = mapXYtoLED(x, y);
            if (ledIndex >= 0 && ledIndex < _numLeds) {
                leds[ledIndex] = color;
            }
        }
    }

    // Draw ants on top
    for (size_t i = 0; i < _ants.size(); i++) {
        int ledIndex = mapXYtoLED(_ants[i].x, _ants[i].y);
        if (ledIndex >= 0 && ledIndex < _numLeds) {
            leds[ledIndex] = CRGB::White;
        }
    }

    FastLED.setBrightness(_brightness);
}

int LangtonsAntAnimation::mapXYtoLED(int x, int y) const {
    const int panelSize = 16;
    if (x < 0 || y < 0 || x >= _width || y >= _height) {
        return -1;
    }

    int panel = x / panelSize;
    if (panel < 0 || panel >= _panelCount) {
        return -1;
    }

    int localX = x % panelSize;
    int localY = y;

    int rotation = 0;
    if (panel == 0) rotation = _rotationAngle1;
    else if (panel == 1) rotation = _rotationAngle2;
    else if (panel == 2) rotation = _rotationAngle3;
    rotateCoordinates(localX, localY, rotation);

    if (localY % 2 != 0) {
        localX = (panelSize - 1) - localX;
    }

    int effectivePanel = (_panelOrder == 0) ? panel : (_panelCount - 1 - panel);
    int index = effectivePanel * panelSize * panelSize + localY * panelSize + localX;
    if (index < 0 || index >= (int)_numLeds) {
        return -1;
    }
    return index;
}

void LangtonsAntAnimation::rotateCoordinates(int& x, int& y, int angle) const {
    const int panelSize = 16;
    int normalizedAngle = angle % 360;
    if (normalizedAngle < 0) {
        normalizedAngle += 360;
    }

    int tmpX, tmpY;
    switch (normalizedAngle) {
        case 0:
            break;
        case 90:
            tmpX = y;
            tmpY = panelSize - 1 - x;
            x = tmpX;
            y = tmpY;
            break;
        case 180:
            tmpX = panelSize - 1 - x;
            tmpY = panelSize - 1 - y;
            x = tmpX;
            y = tmpY;
            break;
        case 270:
            tmpX = panelSize - 1 - y;
            tmpY = x;
            x = tmpX;
            y = tmpY;
            break;
        default:
            break;
    }
}
