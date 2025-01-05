#ifndef MULTISPAWNWAVEANIMATION_H
#define MULTISPAWNWAVEANIMATION_H

#include "BaseAnimation.h"
#include <FastLED.h>
#include <vector>

// We'll define a small struct for each wave spawn
struct WaveSpawn {
    float cx;        // center x
    float cy;        // center y
    float freq;      // how quickly the ripple cycles in radial distance
    float speed;     // how fast the ripple moves outward
    float phase;     // initial phase offset
    float amplitude; // overall wave strength
};

class MultiSpawnWaveAnimation : public BaseAnimation {
public:
    MultiSpawnWaveAnimation(uint16_t numLeds, uint8_t brightness);

    // BaseAnimation overrides
    void begin() override;
    void update() override;

    // Setters from LEDManager
    void setBrightness(uint8_t b);
    void setUpdateInterval(unsigned long intervalMs);
    void setAllPalettes(const std::vector<std::vector<CRGB>>* allPalettes);
    void setCurrentPalette(int paletteIndex);
    void setPanelOrder(int order);
    void setRotationAngle1(int angle);
    void setRotationAngle2(int angle);

    // Mapped from your flake settings
    // e.g. spawnRate => wave speed
    // fadeAmount => wave amplitude
    // tailLength => wave frequency
    void setWaveSpeed(float spawnRate);
    void setWaveAmplitude(uint8_t fadeAmount);
    void setWaveFrequency(int tailLength);

private:
    void fillMultiSpawnWave(); // main method combining multiple spawns
    int  getLedIndex(int x, int y) const;
    void rotateCoordinates(int &x, int &y, int angle) const;

private:
    static const int WIDTH  = 32;
    static const int HEIGHT = 16;
    static const int MAX_SPAWNS = 4; 
    // number of wave centers (4 is arbitrary—try 2..8)

    uint16_t _numLeds;
    uint8_t  _brightness;

    unsigned long _intervalMs;
    unsigned long _lastUpdate;
    unsigned long _frameCounter; // increments each update

    const std::vector<std::vector<CRGB>>* _allPalettes;
    int _currentPalette;

    int _panelOrder;
    int _rotationAngle1;
    int _rotationAngle2;

    // wave parameters
    float _waveSpeed;     // from spawnRate
    float _waveAmplitude; // from fadeAmount
    float _waveFreq;      // from tailLength

    // We'll store a few wave spawns
    WaveSpawn _spawns[MAX_SPAWNS];
};

#endif
