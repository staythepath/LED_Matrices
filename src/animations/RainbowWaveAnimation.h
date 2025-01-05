#ifndef RAINBOWWAVEANIMATION_H
#define RAINBOWWAVEANIMATION_H

#include "BaseAnimation.h"
#include <FastLED.h>
#include <vector>

class RainbowWaveAnimation : public BaseAnimation {
public:
    RainbowWaveAnimation(uint16_t numLeds, uint8_t brightness);

    void begin() override; 
    void update() override;

    // Setters from LEDManager
    void setBrightness(uint8_t b);
    void setAllPalettes(const std::vector<std::vector<CRGB>>* allPalettes);
    void setCurrentPalette(int paletteIndex);
    void setUpdateInterval(unsigned long interval);
    void setPanelOrder(int order);
    void setRotationAngle1(int angle);
    void setRotationAngle2(int angle);


    // The 3 wave parameters: spawnRate, fadeAmount, tailLength
    void setScrollSpeed(float spawnRate);    // controls horizontal scrolling speed
    void setVerticalAmplitude(uint8_t fadeAmount); // how much the wave bobs up/down
    void setHorizontalFreq(int tailLength);  // how “tight” the wave is horizontally

private:
    void fillScrollingRainbow(); // main wave logic
    int  getLedIndex(int x, int y) const;
    void rotateCoordinates(int &x, int &y, int angle) const;

private:
    static const int WIDTH  = 32;
    static const int HEIGHT = 16;

    uint16_t _numLeds;
    uint8_t  _brightness;

    // timing
    unsigned long _intervalMs;
    unsigned long _lastUpdate;
    unsigned long _frameCounter; // simple counter each frame

    // palette
    const std::vector<std::vector<CRGB>>* _allPalettes;
    int   _currentPalette;

    // panel
    int _panelOrder;
    int _rotationAngle1;
    int _rotationAngle2;

    // wave parameters mapped from flake settings
    float _scrollSpeed;     // from spawnRate
    float _verticalAmp;     // from fadeAmount
    float _horizontalFreq;  // from tailLength
};

#endif
