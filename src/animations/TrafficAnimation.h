#ifndef TRAFFIC_ANIMATION_H
#define TRAFFIC_ANIMATION_H

#include "BaseAnimation.h"
#include <Arduino.h>
#include <FastLED.h>
#include <vector>

/**
 * TrafficAnimation that can handle a variable number of 16×16 panels.
 */
class TrafficAnimation : public BaseAnimation {
public:
    /**
     * @param totalLeds  total LED count = panelCount * 16*16
     * @param brightness initial brightness
     * @param panelCount how many 16x16 panels
     */
    TrafficAnimation(uint16_t totalLeds, uint8_t brightness, int panelCount = 2);

    void begin() override;
    void update() override;

    // Overridden brightness
    void setBrightness(uint8_t b) override;

    // Additional setters
    void setUpdateInterval(unsigned long interval);
    void setPanelOrder(int order);
    void setRotationAngle1(int angle);
    void setRotationAngle2(int angle);
    void setRotationAngle3(int angle);

    void setSpawnRate(float rate);
    void setMaxCars(int max);
    void setTailLength(int length);
    void setFadeAmount(uint8_t amount);
    void setCurrentPalette(int index);
    void setAllPalettes(const std::vector<std::vector<CRGB>>* palettes);

private:
    void performTrafficEffect();
    void spawnCar();
    CRGB calcColor(float frac, CRGB startC, CRGB endC, bool bounce);
    int  getLedIndex(int x, int y) const;
    void rotateCoordinates(int &x, int &y, int angle) const;

private:
    // For dynamic panel count
    int _panelCount;
    int _width;   // = panelCount * 16
    int _height;  // = 16

    // Palettes
    const std::vector<std::vector<CRGB>>* _allPalettes;
    int   _currentPalette;

    // Animation state
    float      _spawnRate;
    int        _maxCars;
    int        _tailLength;
    uint8_t    _fadeAmount;
    unsigned long _updateInterval;
    unsigned long _lastUpdate;

    // Panel/rotation config
    int _panelOrder;     
    int _rotationAngle1;
    int _rotationAngle2;
    int _rotationAngle3;

    struct TrafficCar {
        int  x, y, dx, dy;
        CRGB startColor, endColor;
        bool bounce;
        float frac;
    };
    std::vector<TrafficCar> _cars;
};

#endif // TRAFFIC_ANIMATION_H
