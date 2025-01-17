// File: BlinkAnimation.cpp

#include "BlinkAnimation.h"
#include <Arduino.h>
#include <FastLED.h>

// Declared in LEDManager.cpp
extern CRGB leds[];

BlinkAnimation::BlinkAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
  : BaseAnimation(numLeds, brightness) // call the base class constructor
  , _panelCount(panelCount)
  , _intervalMs(500)
  , _lastToggle(0)
  , _isOn(false)
  , _palette(nullptr)
  , _paletteIndex(0)
{
}

void BlinkAnimation::begin() {
    // Turn off to start
    FastLED.clear(true);
    _isOn = false;
    _lastToggle = millis();
    _paletteIndex = 0;
}

void BlinkAnimation::update() {
    unsigned long now = millis();
    if ((now - _lastToggle) >= _intervalMs) {
        _lastToggle = now;
        // Toggle state
        _isOn = !_isOn;

        if (_isOn) {
            // If we have a palette, pick the next color
            CRGB color = CRGB::White;
            if (_palette && !_palette->empty()) {
                color = (*_palette)[_paletteIndex % _palette->size()];
                _paletteIndex++;
            }
            // Fill all
            for (int i = 0; i < _numLeds; i++) {
                leds[i] = color;
            }
            FastLED.setBrightness(_brightness);
        } else {
            // Turn all LEDs off
            FastLED.clear(true);
        }
        FastLED.show();
    }
}

void BlinkAnimation::setBrightness(uint8_t b) {
    _brightness = b;
}

void BlinkAnimation::setInterval(unsigned long intervalMs) {
    _intervalMs = intervalMs;
}

void BlinkAnimation::setPalette(const std::vector<CRGB>* palette) {
    _palette = palette;
    _paletteIndex = 0;
}
