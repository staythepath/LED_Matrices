#include "RainbowWaveAnimation.h"
#include <Arduino.h>
#include <FastLED.h>
#include <math.h> // sinf, fmodf

extern CRGB leds[]; // from LEDManager or main

RainbowWaveAnimation::RainbowWaveAnimation(uint16_t numLeds, uint8_t brightness)
  : _numLeds(numLeds),
    _brightness(brightness),
    _intervalMs(50),
    _lastUpdate(0),
    _frameCounter(0),
    _allPalettes(nullptr),
    _currentPalette(0),
    _panelOrder(0),
    _rotationAngle1(90),
    _rotationAngle2(90),
    // default wave parameters
    _scrollSpeed(0.5f),    // spawnRate=0.5 => moderate scroll
    _verticalAmp(1.0f),    // fadeAmount => 1.0 => minimal vertical offset
    _horizontalFreq(0.1f)  // tailLength => default freq
{
}

void RainbowWaveAnimation::begin(){
    FastLED.clear(true);
    _lastUpdate = millis();
    _frameCounter = 0;
}

void RainbowWaveAnimation::update(){
    unsigned long now = millis();
    if((now - _lastUpdate) >= _intervalMs){
        _lastUpdate = now;

        // increment frame counter
        _frameCounter++;
        
        // fill the scrolling wave
        fillScrollingRainbow();
        FastLED.show();
    }
}

// ---------- Setters ----------
void RainbowWaveAnimation::setBrightness(uint8_t b){
    _brightness = b;
}
void RainbowWaveAnimation::setUpdateInterval(unsigned long intervalMs){
    _intervalMs = intervalMs;
}
void RainbowWaveAnimation::setAllPalettes(const std::vector<std::vector<CRGB>>* allPalettes){
    _allPalettes = allPalettes;
}
void RainbowWaveAnimation::setCurrentPalette(int paletteIndex){
    _currentPalette = paletteIndex;
}
void RainbowWaveAnimation::setPanelOrder(int order){
    _panelOrder = order;
}
void RainbowWaveAnimation::setRotationAngle1(int angle){
    _rotationAngle1 = angle;
}
void RainbowWaveAnimation::setRotationAngle2(int angle){
    _rotationAngle2 = angle;
}

// Let the user control wave parameters:
void RainbowWaveAnimation::setScrollSpeed(float spawnRate){
    // spawnRate in [0..1]. Let’s map that => scroll speed [0..2]
    _scrollSpeed = 2.0f * spawnRate; 
}
void RainbowWaveAnimation::setVerticalAmplitude(uint8_t fadeAmount){
    // fadeAmount in [0..255]. Map that => [0..2.0]
    _verticalAmp = (float)fadeAmount / 128.0f;
}
void RainbowWaveAnimation::setHorizontalFreq(int tailLength){
    // tailLength in [1..30]. Map => [0.05..0.35] 
    // or do any range that looks good
    _horizontalFreq = 0.05f + (float)tailLength * 0.01f;
}

// ---------- fillScrollingRainbow() ----------
void RainbowWaveAnimation::fillScrollingRainbow(){
    if(!_allPalettes || _currentPalette<0 
       || _currentPalette>=(int)_allPalettes->size()){
        // fallback
        for(int i=0; i<(int)_numLeds; i++){
            leds[i] = CRGB::Black;
        }
        return;
    }
    const auto& paletteColors = (*_allPalettes)[_currentPalette];
    int paletteSize = (int)paletteColors.size();
    // quick guard
    if(paletteSize<2){
        // single color => fill
        for(int i=0; i<(int)_numLeds; i++){
            leds[i]= paletteColors[0];
        }
        FastLED.setBrightness(_brightness);
        return;
    }

    // We'll do a simple horizontal scrolling wave:
    // - colorIndex = (x + offset) => from _frameCounter * _scrollSpeed
    // - a slight vertical offset => y * _verticalAmp
    // - Then we blend across the palette
    float offset = (float)_frameCounter * _scrollSpeed; // horizontal scroll offset

    for(int y=0; y<HEIGHT; y++){
        for(int x=0; x<WIDTH; x++){
            // 1) base color index from horizontal
            float baseIndex = (float)x * _horizontalFreq + offset;

            // 2) add vertical wave
            // e.g. let sin( y * freq + offset ) 
            // but let's keep it simpler:
            float vWave = sinf((float)y * 0.3f) * _verticalAmp;
            baseIndex += vWave; 
            
            // wrap baseIndex into [0..(paletteSize-1)] 
            // but we do fractional, so we can blend
            // step1: clamp or mod
            // let's mod by paletteSize for infinite wrap
            // but we'll do a float mod approach:
            float palRange = (float)(paletteSize - 1);
            float modIndex = fmodf(baseIndex, palRange);
            if(modIndex<0) modIndex += palRange; 

            int iBase = (int)floorf(modIndex);
            float frac= modIndex - (float)iBase;
            if(iBase<0) iBase=0;
            if(iBase>=paletteSize-1){
                iBase=paletteSize-1; 
                frac=0;
            }
            int iNext= iBase+1;
            if(iNext>=paletteSize) iNext=iBase;

            // blend
            CRGB c1= paletteColors[iBase];
            CRGB c2= paletteColors[iNext];
            CRGB finalC= blend(c1, c2, (uint8_t)(frac*255));

            // map to LED
            int ledIndex= getLedIndex(x,y);
            if(ledIndex>=0 && ledIndex<(int)_numLeds){
                leds[ledIndex]= finalC;
            }
        }
    }

    FastLED.setBrightness(_brightness);
}

// ---------- getLedIndex(...) ----------
int RainbowWaveAnimation::getLedIndex(int x, int y) const {
    int panel = (x<16)? 0:1;
    int localX= (x<16)? x:(x-16);
    int localY= y;
    // rotate
    if(panel==0) rotateCoordinates(localX, localY, _rotationAngle1);
    else rotateCoordinates(localX, localY, _rotationAngle2);

    // zigzag row
    if(localY%2!=0){
        localX= 15-localX;
    }
    int base= (_panelOrder==0)? (panel*256):((1-panel)*256);
    int idx= base + localY*16+ localX;
    if(idx<0||idx>=(int)_numLeds) return -1;
    return idx;
}

// ---------- rotateCoordinates(...) ----------
void RainbowWaveAnimation::rotateCoordinates(int &x, int &y, int angle) const {
    int tmpX, tmpY;
    switch(angle){
        case 90:
            tmpX = y;
            tmpY = 15 - x;
            x=tmpX; y=tmpY;
            break;
        case 180:
            tmpX = 15 - x;
            tmpY = 15 - y;
            x=tmpX; y=tmpY;
            break;
        case 270:
            tmpX = 15 - y;
            tmpY = x;
            x=tmpX; y=tmpY;
            break;
        default:
            // 0 => no rotation
            break;
    }
}
