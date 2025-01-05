#include "BlinkAnimation.h"
#include <Arduino.h> // for millis()
#include <FastLED.h>

extern CRGB leds[]; // from your main or LEDManager

BlinkAnimation::BlinkAnimation(uint16_t numLeds, uint8_t brightness)
  : _numLeds(numLeds),
    _brightness(brightness),
    _intervalMs(500),    // default half-second blink
    _lastToggle(0),
    _isOn(false)
{
}

void BlinkAnimation::begin() {
    // Turn off to start
    FastLED.clear(true);
    _isOn = false;
    _lastToggle = millis();
}

void BlinkAnimation::update() {
    unsigned long now = millis();
    if((now - _lastToggle) >= _intervalMs) {
        _lastToggle = now;
        _isOn = !_isOn;

        if(_isOn) {
            // Turn all LEDs on (white, scaled by brightness)
            for(int i=0; i<_numLeds; i++){
                leds[i] = CRGB::White;
            }
            FastLED.setBrightness(_brightness);
        } else {
            // Turn all LEDs off
            FastLED.clear(true);
        }
    }
}

// Optional setter for brightness
void BlinkAnimation::setBrightness(uint8_t b) {
    _brightness = b;
}

// If you want a way to change blink speed
void BlinkAnimation::setInterval(unsigned long intervalMs){
    _intervalMs = intervalMs;
}
