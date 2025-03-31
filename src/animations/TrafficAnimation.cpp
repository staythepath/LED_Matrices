#include "TrafficAnimation.h"
#include <Arduino.h>
#include <FastLED.h>

extern CRGB leds[]; // from LEDManager

TrafficAnimation::TrafficAnimation(uint16_t totalLeds, uint8_t brightness, int panelCount)
    : BaseAnimation(totalLeds, brightness)
    , _panelCount(panelCount <= 0 || panelCount > 2 ? 2 : panelCount) // Ensure panel count is valid, max 2
    , _width(_panelCount * 16)
    , _height(16)
    , _allPalettes(nullptr)
    , _currentPalette(0)
    , _spawnRate(1.0f)
    , _maxCars(200)
    , _tailLength(5)
    , _fadeAmount(80)
    , _updateInterval(37)
    , _lastUpdate(0)
    , _panelOrder(0)
    , _rotationAngle1(90)
    , _rotationAngle2(90)
    , _rotationAngle3(90)
{
    // Additional safety initialization
    Serial.printf("TrafficAnimation created with panel count: %d (Width: %d, Height: %d, LEDs: %d)\n", 
                 _panelCount, _width, _height, _numLeds);
    
    // Ensure the panel count is valid and matches our expectations
    if (panelCount != _panelCount) {
        Serial.printf("WARNING: Requested panel count %d was adjusted to %d\n", panelCount, _panelCount);
    }
    
    // Verify our dimensions
    if (_width <= 0 || _height <= 0 || _numLeds <= 0) {
        Serial.println("ERROR: Invalid dimensions in TrafficAnimation constructor!");
    }
}

void TrafficAnimation::begin() {
    _cars.clear();
    FastLED.clear(true);
}

void TrafficAnimation::update() {
    unsigned long now = millis();
    if ((now - _lastUpdate) >= _updateInterval) {
        performTrafficEffect();
        _lastUpdate = now;
    }
}

void TrafficAnimation::performTrafficEffect() {
    // Safety check - ensure we have valid dimensions
    if (_width <= 0 || _height <= 0 || _numLeds <= 0) {
        return;
    }

    fadeToBlackBy(leds, _numLeds, _fadeAmount);

    if (random(1000) < (int)(_spawnRate * 1000) &&
        (int)_cars.size() < _maxCars)
    {
        spawnCar();
    }

    auto it = _cars.begin();
    while (it != _cars.end()) {
        it->x += it->dx;
        it->y += it->dy;
        it->frac += 0.02f;

        // remove if out of bounds
        if (it->x < 0 || it->x >= _width ||
            it->y < 0 || it->y >= _height)
        {
            it = _cars.erase(it);
            continue;
        }
        if (it->frac > 1.0f) {
            it->frac = 1.0f;
        }

        CRGB mainC = calcColor(it->frac, it->startColor, it->endColor, it->bounce);
        mainC.nscale8(_brightness);

        int idx = getLedIndex(it->x, it->y);
        if (idx >= 0 && idx < (int)_numLeds) {
            leds[idx] += mainC;
        }

        // tail - with additional safety checks
        for(int t=1; t<=_tailLength && t <= 10; t++) { // Add max limit to tail length
            int tx = it->x - t*it->dx;
            int ty = it->y - t*it->dy;
            
            // Skip out-of-bounds coordinates
            if(tx<0 || tx>=_width || ty<0 || ty>=_height) {
                break; // Exit tail loop for this car
            }
            
            int tidx = getLedIndex(tx, ty);
            
            // Validate LED index strictly
            if(tidx<0 || tidx>=(int)_numLeds) {
                break; // Exit tail loop for this car
            }
            
            // Safety check before accessing leds array
            if (tidx >= 0 && tidx < (int)_numLeds) {
                float fracScale = 1.0f - (float)t / (float)(_tailLength + 1);
                uint8_t tailB = (uint8_t)(_brightness * fracScale);
                if(tailB < 10) tailB=10;
                CRGB tailCol = mainC;
                tailCol.nscale8(tailB);
                leds[tidx] += tailCol;
            }
        }

        ++it;
    }
}

void TrafficAnimation::spawnCar() {
    // Ensure we have valid dimensions before spawning
    if (_width <= 0 || _height <= 0 || _panelCount <= 0) {
        return;
    }
    
    // Force panel count to be at most 2 for safety
    int safePanelCount = _panelCount > 2 ? 2 : _panelCount;
    int safeWidth = safePanelCount * 16;
    
    TrafficCar c;
    int edge = random(0,4);

    // pick random start/end from current palette
    if(_allPalettes) {
        const auto& pal = (*_allPalettes)[_currentPalette]; // Use const reference here
        if(pal.size() >= 2) {
            c.startColor = pal[random(pal.size())];
            c.endColor = pal[random(pal.size())];
        }
    } else {
        c.startColor = CRGB::Red;
        c.endColor   = CRGB::Blue;
    }
    c.bounce = false;
    c.frac   = 0.0f;

    switch(edge){
        case 0: // top
            c.x = random(0, safeWidth);
            c.y = 0;
            c.dx = 0; c.dy = 1;
            break;
        case 1: // bottom
            c.x = random(0, safeWidth);
            c.y = _height - 1;
            c.dx = 0; c.dy = -1;
            break;
        case 2: // left
            c.x = 0;
            c.y = random(0, _height);
            c.dx = 1; c.dy = 0;
            break;
        case 3: // right
            c.x = safeWidth - 1;
            c.y = random(0, _height);
            c.dx = -1; c.dy = 0;
            break;
    }
    
    // Final validation - make sure coordinates are in bounds
    if (c.x >= 0 && c.x < safeWidth && c.y >= 0 && c.y < _height) {
        _cars.push_back(c);
    }
}

CRGB TrafficAnimation::calcColor(float frac, CRGB startC, CRGB endC, bool bounce) {
    if(!bounce){
        return blend(startC, endC, (uint8_t)(frac * 255));
    } else {
        if(frac <= 0.5f){
            return blend(startC, endC, (uint8_t)(frac*2*255));
        } else {
            return blend(endC, startC, (uint8_t)((frac-0.5f)*2*255));
        }
    }
}

int TrafficAnimation::getLedIndex(int x, int y) const {
    // Validate input coordinates
    if (x < 0 || y < 0 || x >= _width || y >= _height) {
        return -1;
    }

    // Ensure panel count is valid and clamped to 2
    int safetyPanelCount = (_panelCount <= 0 || _panelCount > 2) ? 2 : _panelCount;
    
    // which panel?
    int panel = x / 16;
    if (panel < 0 || panel >= safetyPanelCount) {
        return -1;
    }
    
    int localX = x % 16;
    int localY = y;

    // rotate - only handle panels 0 and 1 since we're enforcing max of 2 panels
    switch(panel) {
        case 0: rotateCoordinates(localX, localY, _rotationAngle1); break;
        case 1: rotateCoordinates(localX, localY, _rotationAngle2); break;
        default: break; // No need to handle more than 2 panels
    }

    // zigzag
    if (localY % 2 != 0) {
        localX = (16 - 1) - localX;
    }

    // base index - only handle values 0 and 1 for _panelOrder
    int base;
    if (_panelOrder == 0) {
        base = panel * 16 * 16;
    } else {
        base = (safetyPanelCount - 1 - panel) * 16 * 16;
    }
    
    int idx = base + localY * 16 + localX;
    
    // Validate final index
    if (idx < 0 || idx >= (int)_numLeds) {
        return -1;
    }
    
    return idx;
}

void TrafficAnimation::rotateCoordinates(int &x, int &y, int angle) const {
    int tmpX, tmpY;
    switch(angle){
        case 0: break;
        case 90:
            tmpX=y;
            tmpY=15 - x;
            x=tmpX; y=tmpY;
            break;
        case 180:
            tmpX=15 - x;
            tmpY=15 - y;
            x=tmpX; y=tmpY;
            break;
        case 270:
            tmpX=15 - y;
            tmpY=x;
            x=tmpX; y=tmpY;
            break;
        default:
            break;
    }
}

// ------- Setters -------
void TrafficAnimation::setBrightness(uint8_t b) {
    _brightness = b;
}
void TrafficAnimation::setUpdateInterval(unsigned long interval) {
    _updateInterval = interval;
}
void TrafficAnimation::setPanelOrder(int order)         { _panelOrder     = order; }
void TrafficAnimation::setRotationAngle1(int angle)     { _rotationAngle1 = angle; }
void TrafficAnimation::setRotationAngle2(int angle)     { _rotationAngle2 = angle; }
void TrafficAnimation::setRotationAngle3(int angle)     { _rotationAngle3 = angle; }

void TrafficAnimation::setSpawnRate(float rate)         { _spawnRate      = rate; }
void TrafficAnimation::setMaxCars(int max)              { _maxCars        = max; }
void TrafficAnimation::setTailLength(int length)        { _tailLength     = length; }
void TrafficAnimation::setFadeAmount(uint8_t amount)    { _fadeAmount     = amount; }
void TrafficAnimation::setCurrentPalette(int index)     { _currentPalette = index; }
void TrafficAnimation::setAllPalettes(const std::vector<std::vector<CRGB>>* palettes){
    _allPalettes = palettes;
}
