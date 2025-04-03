#ifndef FIREWORKANIMATION_H
#define FIREWORKANIMATION_H

#include "BaseAnimation.h"
#include <vector>
#include <FastLED.h>

struct Particle {
    float x, y;       // Position
    float vx, vy;     // Velocity
    float gravity;    // Gravity effect
    uint8_t hue;      // Color
    uint8_t brightness; // Brightness
    uint8_t life;     // Remaining life
};

struct Firework {
    float x, y;       // Launch position
    float vy;         // Launch velocity
    uint8_t hue;      // Color
    bool exploded;    // Whether it has exploded
    std::vector<Particle> particles; // Particles after explosion
};

class FireworkAnimation : public BaseAnimation {
public:
    FireworkAnimation(uint16_t numLeds, uint8_t brightness, int panelCount = 2);
    virtual ~FireworkAnimation() {}

    // Required BaseAnimation methods
    virtual void begin() override;
    virtual void update() override;
    virtual void setBrightness(uint8_t b) override;

    // Specific methods for this animation
    void setUpdateInterval(unsigned long intervalMs);
    void setPanelOrder(int order);
    void setRotationAngle1(int angle);
    void setRotationAngle2(int angle);
    void setRotationAngle3(int angle);
    void setMaxFireworks(int max);
    void setParticleCount(int count);
    void setGravity(float gravity);
    void setLaunchProbability(float prob);

private:
    void updateFireworks();
    void launchFirework();
    void explodeFirework(Firework& firework);
    void drawFireworks();
    int getLedIndex(int x, int y);
    void rotateCoordinates(int &x, int &y, int angle);

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
    int _maxFireworks;
    int _particleCount;
    float _gravity;
    float _launchProbability;
    std::vector<Firework> _fireworks;
};

#endif // FIREWORKANIMATION_H