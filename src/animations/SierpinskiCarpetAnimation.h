// File: SierpinskiCarpetAnimation.h
// Recursive Sierpinski carpet fractal for LED matrices

#ifndef SIERPINSKICARPETANIMATION_H
#define SIERPINSKICARPETANIMATION_H

#include "BaseAnimation.h"
#include <vector>

class SierpinskiCarpetAnimation : public BaseAnimation {
public:
    SierpinskiCarpetAnimation(uint16_t numLeds, uint8_t brightness, int panelCount = 2);
    virtual ~SierpinskiCarpetAnimation() {}

    void begin() override;
    void update() override;
    void setBrightness(uint8_t b) override;

    void setUpdateInterval(unsigned long intervalMs) { _intervalMs = intervalMs; }
    void setPanelOrder(int order) { _panelOrder = order; }
    void setRotationAngle1(int angle) { _rotationAngle1 = angle; }
    void setRotationAngle2(int angle) { _rotationAngle2 = angle; }
    void setRotationAngle3(int angle) { _rotationAngle3 = angle; }

    void setDepth(uint8_t depth);
    void setInvert(bool invert) { _invert = invert; }
    void setColorShift(uint8_t shift) { _colorShift = shift; }
    void setPalette(const std::vector<CRGB>* palette) { _currentPalette = palette; }

private:
    void drawCarpet();
    bool isCarpetHole(int x, int y, int size, int depth) const;
    int mapXYtoLED(int x, int y) const;
    void rotateCoordinates(int& x, int& y, int angle) const;

private:
    int _panelCount;
    int _width;
    int _height;
    unsigned long _intervalMs;
    unsigned long _lastUpdate;
    uint8_t _phase;
    uint8_t _depth;
    bool _invert;
    uint8_t _colorShift;
    int _panelOrder;
    int _rotationAngle1;
    int _rotationAngle2;
    int _rotationAngle3;
    const std::vector<CRGB>* _currentPalette;
};

#endif // SIERPINSKICARPETANIMATION_H
