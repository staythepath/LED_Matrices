#include "TrafficAnimation.h"
#include <Arduino.h> // for millis(), random(), etc.
#include <FastLED.h> // for fadeToBlackBy, blend, etc.

// Externally defined in your main or LEDManager
extern CRGB leds[];

// ---------------- Constructor ----------------
TrafficAnimation::TrafficAnimation(uint16_t numLeds, uint8_t brightness)
    : _numLeds(numLeds),
      _brightness(brightness),
      _allPalettes(nullptr),
      _currentPalette(0),
      _spawnRate(0.6f),
      _maxCars(200),
      _tailLength(5),
      _fadeAmount(80),
      _updateInterval(80),
      _lastUpdate(0),
      _panelOrder(0),
      _rotationAngle1(90),
      _rotationAngle2(90)
{
}

// ---------------- begin() ----------------
void TrafficAnimation::begin() {
    // Clear the vector of cars
    _cars.clear();
    // Clear the LEDs
    FastLED.clear(true);
}

// ---------------- update() ----------------
void TrafficAnimation::update() {
    unsigned long now = millis();
    if((now - _lastUpdate) >= _updateInterval) {
        performTrafficEffect();
        _lastUpdate = now;
    }
}

// ---------------- performTrafficEffect() ----------------
void TrafficAnimation::performTrafficEffect() {
    // 1) Fade
    fadeToBlackBy(leds, _numLeds, _fadeAmount);

    // 2) Possibly spawn a new car
    if(random(1000) < (int)(_spawnRate*1000) && (int)_cars.size() < _maxCars) {
        spawnCar();
    }

    // 3) Move each car + draw
    for(auto it = _cars.begin(); it != _cars.end(); ) {
        it->x += it->dx;
        it->y += it->dy;
        it->frac += 0.02f;

        // Out of bounds => remove
        if(it->x < 0 || it->x >= WIDTH || it->y < 0 || it->y >= HEIGHT) {
            it = _cars.erase(it);
            continue;
        }
        if(it->frac > 1.0f) {
            it->frac = 1.0f;
        }

        // Blend color
        CRGB mainC = calcColor(it->frac, it->startColor, it->endColor, it->bounce);
        mainC.nscale8(_brightness);

        // Draw main pixel
        int idx = getLedIndex(it->x, it->y);
        if(idx >= 0 && idx < (int)_numLeds) {
            leds[idx] += mainC;
        }

        // Tail
        for(int t = 1; t <= _tailLength; t++) {
            int tx = it->x - t*it->dx;
            int ty = it->y - t*it->dy;
            if(tx < 0 || tx >= WIDTH || ty < 0 || ty >= HEIGHT) {
                break;
            }
            int tidx = getLedIndex(tx, ty);
            if(tidx < 0 || tidx >= (int)_numLeds) {
                break;
            }
            float fracScale = 1.0f - (float)t / (float)(_tailLength + 1);
            uint8_t tailB   = (uint8_t)(_brightness * fracScale);
            if(tailB < 10) {
                tailB = 10;
            }
            CRGB tailCol = mainC;
            tailCol.nscale8(tailB);
            leds[tidx] += tailCol;
        }

        ++it;
    }
}

// ---------------- spawnCar() ----------------
void TrafficAnimation::spawnCar() {
    TrafficCar c;
    int edge = random(0,4);

    // pick random start/end from current palette
    if(_allPalettes) {
        const auto& pal = (*_allPalettes)[_currentPalette];
        c.startColor = pal[random(pal.size())];
        c.endColor   = pal[random(pal.size())];
        while(c.endColor == c.startColor) {
            c.endColor = pal[random(pal.size())];
        }
    } else {
        c.startColor = CRGB::Red;
        c.endColor   = CRGB::Blue;
    }
    c.bounce = false;
    c.frac   = 0.0f;

    // random edge
    switch(edge){
        case 0: // top
            c.x = random(0, WIDTH);
            c.y = 0;
            c.dx=0; 
            c.dy=1;
            break;
        case 1: // bottom
            c.x = random(0, WIDTH);
            c.y = HEIGHT - 1;
            c.dx=0; 
            c.dy=-1;
            break;
        case 2: // left
            c.x=0; 
            c.y=random(0, HEIGHT);
            c.dx=1; 
            c.dy=0;
            break;
        case 3: // right
            c.x=WIDTH - 1;
            c.y=random(0, HEIGHT);
            c.dx=-1; 
            c.dy=0;
            break;
    }

    _cars.push_back(c);
}

// ---------------- calcColor() ----------------
CRGB TrafficAnimation::calcColor(float frac, CRGB startC, CRGB endC, bool bounce) {
    if(!bounce) {
        return blend(startC, endC, (uint8_t)(frac*255));
    } else {
        // optional bounce
        if(frac <= 0.5f) {
            return blend(startC, endC, (uint8_t)(frac*2*255));
        } else {
            return blend(endC, startC, (uint8_t)((frac-0.5f)*2*255));
        }
    }
}

// ---------------- getLedIndex() ----------------
int TrafficAnimation::getLedIndex(int x, int y) const {
    // same logic as your old code
    int panel = (x < 16) ? 0 : 1;
    int localX = (x < 16) ? x : (x - 16);
    int localY = y;

    // rotation
    if(panel == 0) {
        rotateCoordinates(localX, localY, _rotationAngle1);
    } else {
        rotateCoordinates(localX, localY, _rotationAngle2);
    }

    // zigzag
    if(localY % 2 != 0) {
        localX = 15 - localX;
    }

    // panel order
    int base = (_panelOrder == 0) ? panel*256 : (1 - panel)*256;
    int idx  = base + localY*16 + localX;
    if(idx < 0 || idx >= (int)_numLeds) {
        return -1;
    }
    return idx;
}

// TrafficAnimation.cpp (somewhere near the other setters)
void TrafficAnimation::setUpdateInterval(unsigned long interval) {
    _updateInterval = interval;
}

// ---------------- rotateCoordinates() ----------------
void TrafficAnimation::rotateCoordinates(int &x, int &y, int angle) const {
    int tmpX, tmpY;
    switch(angle){
        case 0:
            // no rotation
            break;
        case 90:
            tmpX = y;
            tmpY = 15 - x;
            x = tmpX; 
            y = tmpY;
            break;
        case 180:
            tmpX = 15 - x;
            tmpY = 15 - y;
            x = tmpX; 
            y = tmpY;
            break;
        case 270:
            tmpX = 15 - y;
            tmpY = x;
            x = tmpX; 
            y = tmpY;
            break;
        default:
            break;
    }
}

// ---------------- Setters / Getters ----------------
void TrafficAnimation::setPanelOrder(int order)         { _panelOrder     = order; }
void TrafficAnimation::setRotationAngle1(int angle)     { _rotationAngle1 = angle; }
void TrafficAnimation::setRotationAngle2(int angle)     { _rotationAngle2 = angle; }

void TrafficAnimation::setBrightness(uint8_t b)         { _brightness     = b; }
void TrafficAnimation::setSpawnRate(float rate)         { _spawnRate      = rate; }
void TrafficAnimation::setMaxCars(int max)              { _maxCars        = max; }
void TrafficAnimation::setTailLength(int length)        { _tailLength     = length; }
void TrafficAnimation::setFadeAmount(uint8_t amount)    { _fadeAmount     = amount; }
void TrafficAnimation::setCurrentPalette(int index)     { _currentPalette = index; }
void TrafficAnimation::setAllPalettes(const std::vector<std::vector<CRGB>>* palettes){
    _allPalettes = palettes;
}

float   TrafficAnimation::getSpawnRate() const          { return _spawnRate; }
int     TrafficAnimation::getMaxCars() const            { return _maxCars; }
int     TrafficAnimation::getTailLength() const         { return _tailLength; }
uint8_t TrafficAnimation::getFadeAmount() const         { return _fadeAmount; }
