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
    BaseAnimation(uint16_t numLeds, uint8_t brightness, int panelCount = 2)
        : _numLeds(numLeds)
        , _brightness(brightness)
        , _panelCount(panelCount)
    {
    }

    virtual ~BaseAnimation() {}

    // Called once at startup or when selected
    virtual void begin() = 0;
    
    // Called when animation is being cleaned up
    virtual void end() { /* Default empty implementation */ }

    // Called repeatedly in loop
    virtual void update() = 0;

    // A virtual setter for brightness (can be overridden)
    virtual void setBrightness(uint8_t b) { _brightness = b; }

    // Type checking methods
    virtual bool isBlink() const { return false; }
    virtual bool isTraffic() const { return false; }
    virtual bool isRainbowWave() const { return false; }
    virtual bool isGameOfLife() const { return false; }
    virtual bool isFirework() const { return false; }

protected:
    // Shared with derived classes
    uint16_t _numLeds;
    uint8_t  _brightness;
    int _panelCount;
};

#endif // BASEANIMATION_H
