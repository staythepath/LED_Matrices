#include "RainbowWaveAnimation.h"
#include <Arduino.h> // for millis()
#include <FastLED.h>

// We'll assume the global CRGB leds[] is declared in main or LEDManager
extern CRGB leds[];

// ---------------- Constructor ----------------
RainbowWaveAnimation::RainbowWaveAnimation(uint16_t numLeds, uint8_t brightness)
    : _numLeds(numLeds),
      _brightness(brightness),
      _intervalMs(50),   // default 50 ms between updates
      _lastUpdate(0),
      _phase(0),
      _panelOrder(0),
      _rotationAngle1(90),
      _rotationAngle2(90)
{
}

// ---------------- begin() ----------------
void RainbowWaveAnimation::begin() {
    FastLED.clear(true);
    _phase = 0;
    _lastUpdate = millis();
}

// ---------------- update() ----------------
void RainbowWaveAnimation::update() {
    unsigned long now = millis();
    if((now - _lastUpdate) >= _intervalMs) {
        _lastUpdate = now;
        _phase++;  // increment the wave offset
        fillRainbowWave();
    }
}

// ---------------- fillRainbowWave() ----------------
// We'll create a horizontal rainbow that scrolls left->right.
void RainbowWaveAnimation::fillRainbowWave() {
    // For each pixel (x,y), we compute a color from the wave
    // We'll use "CHSV" to generate a rainbow. Hue is determined by x + _phase, 
    // while we do a simple offset for y to vary brightness or whatever you'd like.

    // For a simple wave: Hue = x*4 + _phase
    // S=255, V=255.
    for(int y=0; y<HEIGHT; y++){
        for(int x=0; x<WIDTH; x++){
            // compute hue
            uint8_t hue = (uint8_t)( (x*4) + _phase );
            CHSV colorHSV(hue, 255, 255);
            CRGB colorRGB;
            hsv2rgb_rainbow(colorHSV, colorRGB);

            // find the final LED index with your 2-panel rotation logic
            int index = getLedIndex(x, y);
            if(index >= 0 && index < (int)_numLeds){
                leds[index] = colorRGB;
            }
        }
    }

    // Scale brightness
    FastLED.setBrightness(_brightness);
    FastLED.show();
}

// ---------------- setBrightness(...) ----------------
void RainbowWaveAnimation::setBrightness(uint8_t b) {
    _brightness = b;
}

// ---------------- setUpdateInterval(...) ----------------
void RainbowWaveAnimation::setUpdateInterval(unsigned long intervalMs) {
    _intervalMs = intervalMs;
}

// -------------- getLedIndex(...) -----------------
// Copy or adapt from your traffic logic
int RainbowWaveAnimation::getLedIndex(int x, int y) const {
    // which 16-wide panel
    int panel = (x < 16) ? 0 : 1;
    int localX = (x < 16) ? x : (x - 16);
    int localY = y;

    // rotate
    if(panel == 0) {
        // rotateCoordinates(localX, localY, _rotationAngle1, true);
        rotateCoordinates(localX, localY, _rotationAngle1, true);
    } else {
        // rotateCoordinates(localX, localY, _rotationAngle2, false);
        rotateCoordinates(localX, localY, _rotationAngle2, false);
    }

    // zigzag row
    if(localY % 2 != 0){
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

// ---------------- rotateCoordinates(...) ----------------
// Similar approach to traffic. We pass bool panel0 or not if you want
void RainbowWaveAnimation::rotateCoordinates(int &x, int &y, int angle, bool panel0) const {
    // same logic used before
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

// You might also want setters for panel order & angles if you want that dynamic:
