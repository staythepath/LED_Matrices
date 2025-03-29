#ifndef RAINBOWWAVEANIMATION_H
#define RAINBOWWAVEANIMATION_H

#include "BaseAnimation.h"
#include <FastLED.h>

/**
 * A "Rainbow Wave" animation that scrolls a rainbow horizontally across a variable
 * number of 16x16 panels. We'll store _panelCount and compute _width=16*_panelCount, 
 * _height=16, then do the coordinate transformations similarly to Traffic.
 */
class RainbowWaveAnimation : public BaseAnimation {
public:
    /**
     * @param numLeds     - total LED count (panelCount * 16 * 16)
     * @param brightness  - initial brightness
     * @param panelCount  - how many 16x16 panels
     */
    RainbowWaveAnimation(uint16_t numLeds, uint8_t brightness, int panelCount=3);

    void begin() override;
    void update() override;

    void setBrightness(uint8_t b) override;
    void setUpdateInterval(unsigned long intervalMs);
    void setSpeedMultiplier(float speedMultiplier);

    // Dynamic panel geometry
    void setPanelOrder(int order);
    void setRotationAngle1(int angle);
    void setRotationAngle2(int angle);
    void setRotationAngle3(int angle);
    // If you want more than 3 angle options, you can expand similarly.

private:
    // We'll compute these at construction based on panelCount.
    int _panelCount;
    int _width;   // = panelCount * 16
    int _height;  // = 16

    // Timing
    unsigned long _intervalMs;
    unsigned long _lastUpdate;

    // Phase offset in hue
    uint8_t       _phase;
    float         _speedMultiplier;  // Controls how fast colors change (1.0 = normal)

    // Panel geometry
    int _panelOrder; 
    int _rotationAngle1;
    int _rotationAngle2;
    int _rotationAngle3;

    // Internals
    void fillRainbowWave();
    int  getLedIndex(int x, int y) const;
    void rotateCoordinates(int &x, int &y, int angle) const;
};

#endif // RAINBOWWAVEANIMATION_H
