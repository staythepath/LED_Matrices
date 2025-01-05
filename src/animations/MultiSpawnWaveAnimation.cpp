#include "MultiSpawnWaveAnimation.h"
#include <Arduino.h>
#include <FastLED.h>
#include <math.h>  // sinf, cosf, sqrtf

extern CRGB leds[]; // from LEDManager or main

MultiSpawnWaveAnimation::MultiSpawnWaveAnimation(uint16_t numLeds, uint8_t brightness)
  : _numLeds(numLeds),
    _brightness(brightness),
    _intervalMs(60),   // default frame time
    _lastUpdate(0),
    _frameCounter(0),
    _allPalettes(nullptr),
    _currentPalette(0),
    _panelOrder(0),
    _rotationAngle1(90),
    _rotationAngle2(90),
    // default wave params => updated from LEDManager
    _waveSpeed(0.5f),
    _waveAmplitude(1.0f),
    _waveFreq(0.1f)
{
    // Initialize spawns to something
    for(int i=0; i<MAX_SPAWNS; i++){
        _spawns[i] = { 0,0, 0.1f, 0.2f, 0.0f, 1.0f };
    }
}

void MultiSpawnWaveAnimation::begin(){
    FastLED.clear(true);
    _lastUpdate = millis();
    _frameCounter = 0;

    // Randomly pick wave spawn centers, freq, speed, amplitude
    // We'll incorporate the user’s global _waveSpeed, _waveAmplitude, _waveFreq
    // but add some random variations
    for(int i=0; i<MAX_SPAWNS; i++){
        _spawns[i].cx = (float)random(0, WIDTH); // random X in [0..31]
        _spawns[i].cy = (float)random(0, HEIGHT); // random Y in [0..15]

        // e.g. freq = base waveFreq plus random tweak in [0..0.2]
        _spawns[i].freq = _waveFreq + ((float)random(0, 20)/100.0f);

        // speed = base waveSpeed plus random tweak
        _spawns[i].speed = _waveSpeed + ((float)random(-10, 10)/100.0f);

        // random phase offset
        _spawns[i].phase = (float)random(0, 1000)/100.0f; // e.g. 0..10

        // amplitude = base amplitude plus small random tweak
        _spawns[i].amplitude = _waveAmplitude + ((float)random(-10,10)/100.0f);
        if(_spawns[i].amplitude<0.1f) _spawns[i].amplitude=0.1f; // clamp
    }
}

void MultiSpawnWaveAnimation::update(){
    unsigned long now= millis();
    if((now - _lastUpdate) >= _intervalMs){
        _lastUpdate= now;
        _frameCounter++;

        fillMultiSpawnWave();
        FastLED.show();
    }
}

#include "MultiSpawnWaveAnimation.h"
#include <Arduino.h>
#include <FastLED.h>

// example
void MultiSpawnWaveAnimation::setAllPalettes(const std::vector<std::vector<CRGB>>* allPalettes){
    _allPalettes = allPalettes;
}
void MultiSpawnWaveAnimation::setCurrentPalette(int paletteIndex){
    _currentPalette = paletteIndex;
}
void MultiSpawnWaveAnimation::setUpdateInterval(unsigned long interval){
    _intervalMs = interval; // or whatever variable
}
void MultiSpawnWaveAnimation::setPanelOrder(int order){
    _panelOrder = order;
}
void MultiSpawnWaveAnimation::setRotationAngle1(int angle){
    _rotationAngle1 = angle;
}
void MultiSpawnWaveAnimation::setRotationAngle2(int angle){
    _rotationAngle2 = angle;
}


// --------------------- Fill MultiSpawn Wave ---------------------
// Each spawn acts like a ripple center. We sum them up => waveVal
void MultiSpawnWaveAnimation::fillMultiSpawnWave(){
    if(!_allPalettes || _currentPalette<0 
       || _currentPalette>=(int)_allPalettes->size()){
        // fallback
        for(int i=0; i<(int)_numLeds; i++){
            leds[i]= CRGB::Black;
        }
        return;
    }
    const auto& palette = (*_allPalettes)[_currentPalette];
    int palSize= (int)palette.size();
    if(palSize<2){
        // if single color, just fill
        for(int i=0; i<(int)_numLeds; i++){
            leds[i]= palette[0];
        }
        FastLED.setBrightness(_brightness);
        return;
    }

    // For each pixel => sum wave from each spawn
    for(int y=0; y<HEIGHT; y++){
        for(int x=0; x<WIDTH; x++){
            // sum of wave contributions
            float sumWave=0.0f;

            // For each spawn
            for(int i=0; i<MAX_SPAWNS; i++){
                float dx= (float)x - _spawns[i].cx;
                float dy= (float)y - _spawns[i].cy;
                float dist= sqrtf(dx*dx + dy*dy); // radial distance from center

                // wave = sin( dist*freq - time*speed + phase ) * amplitude
                float time= (float)_frameCounter*0.05f; // a small time factor
                float wVal= sinf(dist*_spawns[i].freq 
                                 - time*_spawns[i].speed 
                                 + _spawns[i].phase);

                // scale by amplitude
                wVal*= _spawns[i].amplitude;
                sumWave+= wVal;
            }

            // Now sumWave might be ~[-(MAX_SPAWNS)..(MAX_SPAWNS)]
            // shift/clamp to [0..1]
            // e.g. shift by MAX_SPAWNS, then scale
            float shiftVal= sumWave + (float)MAX_SPAWNS; 
            // range => [0..2*MAX_SPAWNS]. e.g. if 4 spawns => [0..8]
            float halfRange= (float)(MAX_SPAWNS*2); 
            float waveVal= shiftVal/halfRange; // => ~[0..1]

            if(waveVal<0.0f) waveVal=0.0f;
            if(waveVal>1.0f) waveVal=1.0f;

            // Map waveVal => palette blend
            float cIdxF= waveVal*(palSize-1);
            int iBase= (int)floorf(cIdxF);
            float frac= cIdxF-(float)iBase;

            if(iBase<0) iBase=0;
            if(iBase>= palSize-1){
                iBase= palSize-1; 
                frac=0.0f;
            }
            int iNext= iBase+1;
            if(iNext>= palSize) iNext=iBase;

            CRGB c1= palette[iBase];
            CRGB c2= palette[iNext];
            CRGB finalC= blend(c1,c2,(uint8_t)(frac*255));

            int ledIndex= getLedIndex(x,y);
            if(ledIndex>=0 && ledIndex<(int)_numLeds){
                leds[ledIndex]= finalC;
            }
        }
    }

    FastLED.setBrightness(_brightness);
}

// --------------------- getLedIndex(...) ---------------------
int MultiSpawnWaveAnimation::getLedIndex(int x, int y) const {
    int panel= (x<16)?0:1;
    int localX= (x<16)? x:(x-16);
    int localY= y;

    if(panel==0) rotateCoordinates(localX,localY,_rotationAngle1);
    else rotateCoordinates(localX,localY,_rotationAngle2);

    if(localY%2!=0) localX= 15-localX;

    int base= (_panelOrder==0)?(panel*256):((1-panel)*256);
    int idx= base + localY*16 + localX;
    if(idx<0||idx>=(int)_numLeds) return -1;
    return idx;
}

void MultiSpawnWaveAnimation::rotateCoordinates(int &x, int &y, int angle) const {
    int tmpX, tmpY;
    switch(angle){
        case 90:
            tmpX= y; 
            tmpY= 15-x;
            x=tmpX; y=tmpY;
            break;
        case 180:
            tmpX= 15-x; 
            tmpY= 15-y;
            x=tmpX; y=tmpY;
            break;
        case 270:
            tmpX= 15-y; 
            tmpY= x;
            x=tmpX; y=tmpY;
            break;
        default:
            break;
    }
}

// --------------- Mappings from LEDManager ---------------
void MultiSpawnWaveAnimation::setWaveSpeed(float spawnRate){
    // e.g. spawnRate in [0..1]. Let's map => speed in [0..1.5]
    // you can tweak to taste
    _waveSpeed= 1.5f * spawnRate;
}
void MultiSpawnWaveAnimation::setWaveAmplitude(uint8_t fadeAmount){
    // [0..255] => amplitude in [0..2]
    _waveAmplitude= (float)fadeAmount/128.0f;
}
void MultiSpawnWaveAnimation::setWaveFrequency(int tailLength){
    // [1..30] => freq in [0.05..0.35]
    _waveFreq= 0.05f + (float)tailLength*0.01f;
}
