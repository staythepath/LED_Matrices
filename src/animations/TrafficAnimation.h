#ifndef TRAFFICANIMATION_H
#define TRAFFICANIMATION_H

#include "BaseAnimation.h"  // So we can inherit from BaseAnimation
#include <FastLED.h>
#include <vector>

// A struct representing one "car" in the Traffic animation
struct TrafficCar {
    int x;
    int y;
    int dx;
    int dy;
    float frac;
    CRGB startColor;
    CRGB endColor;
    bool bounce;
};

class TrafficAnimation : public BaseAnimation {
public:
    TrafficAnimation(uint16_t numLeds, uint8_t brightness);

    // BaseAnimation overrides
    void begin() override;  
    void update() override;

    // Setters
    void setBrightness(uint8_t b);
    void setSpawnRate(float rate);
    void setMaxCars(int max);
    void setTailLength(int length);
    void setFadeAmount(uint8_t amount);
    void setCurrentPalette(int index);
    void setAllPalettes(const std::vector<std::vector<CRGB>>* palettes);
    void setUpdateInterval(unsigned long interval);
    void setPanelOrder(int order);      // 0=left-first, 1=right-first
    void setRotationAngle1(int angle);  // 0,90,180,270 for panel1
    void setRotationAngle2(int angle);  // 0,90,180,270 for panel2

    // Getters
    float   getSpawnRate() const;
    int     getMaxCars() const;
    int     getTailLength() const;
    uint8_t getFadeAmount() const;

private:
    void performTrafficEffect();
    void spawnCar();
    CRGB calcColor(float frac, CRGB startC, CRGB endC, bool bounce);

    // Mapping (x,y)->LED index, with panel/rotation logic
    int  getLedIndex(int x, int y) const;
    void rotateCoordinates(int &x, int &y, int angle) const;

private:
    // Dimensions
    static const int WIDTH  = 32;
    static const int HEIGHT = 16;

    // For the global CRGB array
    uint16_t _numLeds;
    uint8_t  _brightness;

    // Palettes
    const std::vector<std::vector<CRGB>>* _allPalettes;
    int   _currentPalette;

    // Data structure for "cars"
    std::vector<TrafficCar> _cars;

    // Animation parameters
    float   _spawnRate;   
    int     _maxCars;
    int     _tailLength;
    uint8_t _fadeAmount;

    // Timing
    unsigned long _updateInterval;
    unsigned long _lastUpdate;

    // Panel geometry
    int _panelOrder;      
    int _rotationAngle1;  
    int _rotationAngle2;  
};

#endif // TRAFFICANIMATION_H
