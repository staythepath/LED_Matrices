// File: SierpinskiCarpetAnimation.cpp
// Recursive Sierpinski carpet fractal for LED matrices

#include "SierpinskiCarpetAnimation.h"
#include <Arduino.h>
#include <FastLED.h>

extern CRGB leds[];

SierpinskiCarpetAnimation::SierpinskiCarpetAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
    : BaseAnimation(numLeds, brightness, panelCount)
    , _panelCount(panelCount)
    , _width(panelCount * 16)
    , _height(16)
    , _intervalMs(120)
    , _lastUpdate(0)
    , _phase(0)
    , _depth(4)
    , _invert(false)
    , _colorShift(2)
    , _panelOrder(0)
    , _rotationAngle1(0)
    , _rotationAngle2(0)
    , _rotationAngle3(0)
    , _currentPalette(nullptr)
{
}

void SierpinskiCarpetAnimation::begin() {
    FastLED.clear(true);
    _lastUpdate = millis();
}

void SierpinskiCarpetAnimation::setBrightness(uint8_t b) {
    _brightness = b;
    FastLED.setBrightness(b);
}

void SierpinskiCarpetAnimation::setDepth(uint8_t depth) {
    if (depth < 1) depth = 1;
    if (depth > 6) depth = 6;
    _depth = depth;
}

void SierpinskiCarpetAnimation::update() {
    unsigned long now = millis();
    if (now - _lastUpdate < _intervalMs) {
        return;
    }
    _lastUpdate = now;
    _phase += _colorShift;
    drawCarpet();
    FastLED.show();
}

void SierpinskiCarpetAnimation::drawCarpet() {
    int size = _width < _height ? _width : _height;
    if (size <= 0) return;

    int offsetX = (_width - size) / 2;
    int offsetY = (_height - size) / 2;

    bool hasPalette = _currentPalette && !_currentPalette->empty();
    size_t paletteSize = hasPalette ? _currentPalette->size() : 0;

    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            CRGB color = CRGB::Black;

            if (x >= offsetX && x < offsetX + size && y >= offsetY && y < offsetY + size) {
                int localX = x - offsetX;
                int localY = y - offsetY;
                bool hole = isCarpetHole(localX, localY, size, _depth);
                bool draw = _invert ? hole : !hole;
                if (draw) {
                    if (hasPalette) {
                        size_t idx = (localX + localY + _phase) % paletteSize;
                        color = (*_currentPalette)[idx];
                    } else {
                        color = CHSV((uint8_t)(localX * 4 + localY * 3 + _phase), 255, 255);
                    }
                }
            }

            int ledIndex = mapXYtoLED(x, y);
            if (ledIndex >= 0 && ledIndex < _numLeds) {
                leds[ledIndex] = color;
            }
        }
    }
    FastLED.setBrightness(_brightness);
}

bool SierpinskiCarpetAnimation::isCarpetHole(int x, int y, int size, int depth) const {
    if (depth <= 0 || size < 3) {
        return false;
    }
    int third = size / 3;
    int xThird = x / third;
    int yThird = y / third;
    if (xThird == 1 && yThird == 1) {
        return true;
    }
    return isCarpetHole(x % third, y % third, third, depth - 1);
}

int SierpinskiCarpetAnimation::mapXYtoLED(int x, int y) const {
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

void SierpinskiCarpetAnimation::rotateCoordinates(int& x, int& y, int angle) const {
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
