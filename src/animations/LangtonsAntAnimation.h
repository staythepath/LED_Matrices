// File: LangtonsAntAnimation.h
// Langton's Ant cellular automaton for LED matrices

#ifndef LANGTONSANTANIMATION_H
#define LANGTONSANTANIMATION_H

#include "BaseAnimation.h"
#include <vector>

class LangtonsAntAnimation : public BaseAnimation {
public:
    LangtonsAntAnimation(uint16_t numLeds, uint8_t brightness, int panelCount = 2);
    virtual ~LangtonsAntAnimation();

    void begin() override;
    void update() override;
    void setBrightness(uint8_t b) override;

    void setUpdateInterval(unsigned long intervalMs) { _intervalMs = intervalMs; }
    void setPanelOrder(int order) { _panelOrder = order; }
    void setRotationAngle1(int angle) { _rotationAngle1 = angle; }
    void setRotationAngle2(int angle) { _rotationAngle2 = angle; }
    void setRotationAngle3(int angle) { _rotationAngle3 = angle; }

    void setRule(const String& rule);
    void setAntCount(uint8_t count);
    void setStepsPerFrame(uint8_t steps);
    void setWrapMode(bool wrap) { _wrapEdges = wrap; }
    void setPalette(const std::vector<CRGB>* palette) { _currentPalette = palette; }
    void resetSimulation();

private:
    struct Ant {
        int x;
        int y;
        uint8_t dir; // 0=up,1=right,2=down,3=left
    };

    void stepAnt(Ant& ant);
    void drawGrid();
    int mapXYtoLED(int x, int y) const;
    void rotateCoordinates(int& x, int& y, int angle) const;

private:
    int _panelCount;
    int _width;
    int _height;
    unsigned long _intervalMs;
    unsigned long _lastUpdate;
    int _panelOrder;
    int _rotationAngle1;
    int _rotationAngle2;
    int _rotationAngle3;

    String _rule;
    uint8_t _ruleLen;
    uint8_t _antCount;
    uint8_t _stepsPerFrame;
    bool _wrapEdges;

    uint8_t* _cells; // state per cell (0..ruleLen-1)
    std::vector<Ant> _ants;
    const std::vector<CRGB>* _currentPalette;
};

#endif // LANGTONSANTANIMATION_H
