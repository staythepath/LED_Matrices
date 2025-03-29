// File: RainbowWaveAnimation.cpp

#include "RainbowWaveAnimation.h"
#include <Arduino.h>
#include <FastLED.h>

extern CRGB leds[];

RainbowWaveAnimation::RainbowWaveAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
    : BaseAnimation(numLeds, brightness)
    , _panelCount(panelCount)
    , _width(panelCount * 16)
    , _height(16)
    , _intervalMs(8)  // Consistent fast frame rate for smooth animation
    , _lastUpdate(0)
    , _phase(0)
    , _speedMultiplier(1.0f)  // Default speed
    , _panelOrder(1)
    , _rotationAngle1(90)
    , _rotationAngle2(90)
    , _rotationAngle3(90)
{
}

void RainbowWaveAnimation::begin() {
    FastLED.clear(true);
    _phase = 0;
    _lastUpdate = millis();
}

void RainbowWaveAnimation::update() {
    unsigned long now = millis();
    if ((now - _lastUpdate) >= _intervalMs) {
        _lastUpdate = now;
        
        // Apply speed multiplier to phase increment
        _phase += (uint8_t)(8 * _speedMultiplier);
        
        fillRainbowWave();
        FastLED.show();
    }
}

void RainbowWaveAnimation::fillRainbowWave() {
    // We'll produce a horizontal rainbow that scrolls over time.
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            // compute hue based on x + _phase
            uint8_t hue = (x * 4 + _phase) & 255; 
            CHSV colorHSV(hue, 255, 255);
            CRGB colorRGB;
            hsv2rgb_rainbow(colorHSV, colorRGB);

            // get index for (x,y) 
            int index = getLedIndex(x, y);
            if (index >= 0 && index < _numLeds) {
                leds[index] = colorRGB;
            }
        }
    }
    // Scale overall brightness
    FastLED.setBrightness(_brightness);
}

void RainbowWaveAnimation::setBrightness(uint8_t b) {
    _brightness = b;
    FastLED.setBrightness(b);
}

void RainbowWaveAnimation::setUpdateInterval(unsigned long intervalMs) {
    _intervalMs = intervalMs;
}

void RainbowWaveAnimation::setSpeedMultiplier(float speedMultiplier) {
    // Limit to reasonable range (0.1 to 5.0)
    if (speedMultiplier < 0.1f) speedMultiplier = 0.1f;
    if (speedMultiplier > 5.0f) speedMultiplier = 5.0f;
    _speedMultiplier = speedMultiplier;
}

void RainbowWaveAnimation::setPanelOrder(int order)         { _panelOrder     = order; }
void RainbowWaveAnimation::setRotationAngle1(int angle)     { _rotationAngle1 = angle; }
void RainbowWaveAnimation::setRotationAngle2(int angle)     { _rotationAngle2 = angle; }
void RainbowWaveAnimation::setRotationAngle3(int angle)     { _rotationAngle3 = angle; }

// Similar logic as in Traffic
int RainbowWaveAnimation::getLedIndex(int x, int y) const {
    // which 16x16 panel index
    int panel = x / 16;
    if (panel < 0 || panel >= _panelCount) {
        return -1;
    }
    // local coords
    int localX = x % 16;
    int localY = y;

    // rotate
    // (If the user can have up to 8 panels, you'll either store
    // angles in an array or handle it more systematically. For now,
    // we handle up to 3 angles. Panels beyond index=2 just won't rotate.)
    switch (panel) {
        case 0: rotateCoordinates(localX, localY, _rotationAngle1); break;
        case 1: rotateCoordinates(localX, localY, _rotationAngle2); break;
        case 2: rotateCoordinates(localX, localY, _rotationAngle3); break;
        default: break; // no rotation if we have >3 panels
    }

    // zigzag
    if (localY % 2 != 0) {
        localX = 15 - localX;
    }

    // base offset
    int base = 0;
    if (_panelOrder == 0) {
        // normal
        base = panel * 16 * 16;
    } else {
        // reversed
        base = (_panelCount - 1 - panel) * 16 * 16;
    }
    int idx = base + localY * 16 + localX;
    if (idx < 0 || idx >= (int)_numLeds) {
        return -1;
    }
    return idx;
}

void RainbowWaveAnimation::rotateCoordinates(int &x, int &y, int angle) const {
    int tmpX, tmpY;
    switch(angle) {
        case 0:
            // no rotation
            break;
        case 90:
            tmpX = y;
            tmpY = 15 - x;
            x = tmpX;
            y = tmpY;
            break;
        case 180:
            tmpX = 15 - x;
            tmpY = 15 - y;
            x = tmpX;
            y = tmpY;
            break;
        case 270:
            tmpX = 15 - y;
            tmpY = x;
            x = tmpX;
            y = tmpY;
            break;
        default:
            break;
    }
}
