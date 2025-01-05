#ifndef BLINKANIMATION_H
#define BLINKANIMATION_H

#include "BaseAnimation.h"
#include <FastLED.h>

// A very simple "Blink" animation that toggles all LEDs on/off
class BlinkAnimation : public BaseAnimation {
public:
    BlinkAnimation(uint16_t numLeds, uint8_t brightness);

    void begin() override; // Called once when animation starts
    void update() override; // Called periodically

    // Setters
    void setBrightness(uint8_t b);
    void setInterval(unsigned long intervalMs);

private:
    uint16_t _numLeds;
    uint8_t  _brightness;

    // Timing
    unsigned long _intervalMs;  
    unsigned long _lastToggle;
    bool _isOn;
};

#endif // BLINKANIMATION_H
