// File: BaseAnimation.h

#ifndef BASEANIMATION_H
#define BASEANIMATION_H

#include <Arduino.h>
#include <FastLED.h>

/**
 * A generic base class for animations. 
 * Each derived class must implement begin() and update().
 */
class BaseAnimation {
public:
    // We require constructor arguments so there's NO default constructor.
    BaseAnimation(uint16_t numLeds, uint8_t brightness)
        : _numLeds(numLeds)
        , _brightness(brightness)
    {
    }

    virtual ~BaseAnimation() {}

    // Called once at startup or when selected
    virtual void begin() = 0;

    // Called repeatedly in loop
    virtual void update() = 0;

    // A virtual setter for brightness (can be overridden)
    virtual void setBrightness(uint8_t b) { _brightness = b; }

protected:
    // Shared with derived classes
    uint16_t _numLeds;
    uint8_t  _brightness;
};

#endif // BASEANIMATION_H
