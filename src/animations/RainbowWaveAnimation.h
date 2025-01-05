#ifndef RAINBOWWAVEANIMATION_H
#define RAINBOWWAVEANIMATION_H

#include "BaseAnimation.h"
#include <FastLED.h>

/**
 * A "Rainbow Wave" animation that scrolls a rainbow horizontally across the matrix.
 * We'll assume a 32x16 layout (Width=32, Height=16), but adapt if needed.
 */
class RainbowWaveAnimation : public BaseAnimation {
public:
    RainbowWaveAnimation(uint16_t numLeds, uint8_t brightness);

    // Called once when we select this animation
    void begin() override;

    // Called repeatedly in loop
    void update() override;

    // Setters
    void setBrightness(uint8_t b);
    void setUpdateInterval(unsigned long intervalMs);

private:
    // Internal logic
    void fillRainbowWave();

    // For indexing
    int getLedIndex(int x, int y) const;
    void rotateCoordinates(int &x, int &y, int angle, bool panel0) const;

private:
    // Basic info
    static const int WIDTH = 32;   // If your matrix is 32 wide
    static const int HEIGHT = 16;  // 16 tall

    uint16_t _numLeds;
    uint8_t  _brightness;

    // Timing
    unsigned long _intervalMs; 
    unsigned long _lastUpdate;

    // "phase" scroll offset
    uint16_t _phase; // increments to move rainbow horizontally

    // Panel geometry (like your traffic logic)
    int _panelOrder;
    int _rotationAngle1;
    int _rotationAngle2;
};

#endif // RAINBOWWAVEANIMATION_H
