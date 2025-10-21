// File: BlinkAnimation.cpp

#include "BlinkAnimation.h"
#include <Arduino.h>
#include <FastLED.h>

// Declared in LEDManager.cpp
extern CRGB leds[];

BlinkAnimation::BlinkAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
  : BaseAnimation(numLeds, brightness) // call the base class constructor
  , _panelCount(panelCount)
  , _intervalMs(400)
  , _lastToggle(0)
  , _isOn(false)
  , _onDurationMs(300)
  , _offDurationMs(300)
  , _palette(nullptr)
  , _paletteIndex(0)
{}

void BlinkAnimation::begin() {
    // Turn off to start
    FastLED.clear(true);
    _isOn = false;
    _lastToggle = millis();
    _paletteIndex = 0;
}

void BlinkAnimation::update() {
    unsigned long now = millis();
    const uint16_t period = _isOn ? _onDurationMs : _offDurationMs;
    if ((now - _lastToggle) >= period) {
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
    _onDurationMs = static_cast<uint16_t>(intervalMs);
    _offDurationMs = static_cast<uint16_t>(intervalMs);
}

void BlinkAnimation::setOnDuration(uint16_t ms) {
    if (ms < 40) ms = 40;
    if (ms > 2000) ms = 2000;
    _onDurationMs = ms;
}

void BlinkAnimation::setOffDuration(uint16_t ms) {
    if (ms < 40) ms = 40;
    if (ms > 2000) ms = 2000;
    _offDurationMs = ms;
}

void BlinkAnimation::setPalette(const std::vector<CRGB>* palette) {
    _palette = palette;
    _paletteIndex = 0;
}
