#include "FireworkAnimation.h"
#include <Arduino.h>
#include <FastLED.h>

extern CRGB leds[];

// Constructor
FireworkAnimation::FireworkAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
    : BaseAnimation(numLeds, brightness)
    , _panelCount(panelCount)
    , _width(16)  // Fixed width per panel
    , _height(16)
    , _intervalMs(15)  // Default update speed (15ms for smooth animation)
    , _lastUpdate(0)
    , _panelOrder(1)
    , _rotationAngle1(90)
    , _rotationAngle2(90)
    , _rotationAngle3(90)
    , _maxFireworks(10)
    , _particleCount(40)
    , _gravity(0.15f)
    , _launchProbability(0.15f)
{
    Serial.printf("Firework Animation created. Grid size: %d x %d, panels: %d\n", 
                  _width * _panelCount, _height, _panelCount);
}

// Initialize the animation
void FireworkAnimation::begin() {
    Serial.println("Firework Animation: begin()");
    FastLED.clear(true);
    _lastUpdate = millis();
    _fireworks.clear();
    
    // Launch a few initial fireworks
    for (int i = 0; i < 3; i++) {
        launchFirework();
    }
    
    Serial.println("Firework Animation: begin() completed");
}

// Set update interval
void FireworkAnimation::setUpdateInterval(unsigned long intervalMs) {
    _intervalMs = intervalMs;
}

// Set panel order
void FireworkAnimation::setPanelOrder(int order) {
    _panelOrder = order;
}

// Set rotation angles
void FireworkAnimation::setRotationAngle1(int angle) {
    _rotationAngle1 = angle;
}

void FireworkAnimation::setRotationAngle2(int angle) {
    _rotationAngle2 = angle;
}

void FireworkAnimation::setRotationAngle3(int angle) {
    _rotationAngle3 = angle;
}

// Set maximum number of fireworks
void FireworkAnimation::setMaxFireworks(int max) {
    _maxFireworks = max;
}

// Set particle count per explosion
void FireworkAnimation::setParticleCount(int count) {
    _particleCount = count;
}

// Set gravity effect
void FireworkAnimation::setGravity(float gravity) {
    _gravity = gravity;
}

// Set launch probability
void FireworkAnimation::setLaunchProbability(float prob) {
    _launchProbability = prob;
}

// Override brightness setting
void FireworkAnimation::setBrightness(uint8_t b) {
    _brightness = b;
    FastLED.setBrightness(b);
}

// Update the animation (called in the main loop)
void FireworkAnimation::update() {
    unsigned long now = millis();
    if ((now - _lastUpdate) >= _intervalMs) {
        _lastUpdate = now;
        
        // Yield to other tasks before heavy computation
        yield();
        
        // Clear the display
        FastLED.clear();
        
        // Update fireworks
        updateFireworks();
        
        // Draw fireworks
        drawFireworks();
        
        // Show the LEDs
        FastLED.show();
        
        // Randomly launch new fireworks if we have room
        if (_fireworks.size() < _maxFireworks && random(100) < (_launchProbability * 100)) {
            launchFirework();
        }
    }
}

// Update all fireworks
void FireworkAnimation::updateFireworks() {
    // Update each firework
    for (auto it = _fireworks.begin(); it != _fireworks.end(); ) {
        Firework& fw = *it;
        
        if (!fw.exploded) {
            // Update rising firework
            fw.y -= fw.vy;
            fw.vy *= 0.98f; // Slow down due to gravity
            
            // If it reached peak height, explode
            if (fw.vy < 0.3f) {
                explodeFirework(fw);
            }
            
            ++it;
        } else {
            // Update particles
            bool allDead = true;
            
            for (auto& p : fw.particles) {
                // Update position
                p.x += p.vx;
                p.y += p.vy;
                
                // Apply gravity
                p.vy += p.gravity;
                
                // Decrease life
                if (p.life > 0) {
                    p.life--;
                    p.brightness = map(p.life, 0, 100, 0, 255);
                    allDead = false;
                }
            }
            
            // Remove firework if all particles are dead
            if (allDead) {
                it = _fireworks.erase(it);
            } else {
                ++it;
            }
        }
    }
}

// Launch a new firework
void FireworkAnimation::launchFirework() {
    Firework fw;
    
    // Random starting position at bottom
    fw.x = random(_width * _panelCount);
    fw.y = _height - 1;
    
    // Random upward velocity
    fw.vy = 0.5f + (random(50) / 100.0f);
    
    // Random color
    fw.hue = random(256);
    
    // Not exploded yet
    fw.exploded = false;
    
    // Add to list
    _fireworks.push_back(fw);
}

// Explode a firework into particles
void FireworkAnimation::explodeFirework(Firework& fw) {
    fw.exploded = true;
    fw.particles.clear();
    
    // Create particles
    for (int i = 0; i < _particleCount; i++) {
        Particle p;
        
        // Start at explosion point
        p.x = fw.x;
        p.y = fw.y;
        
        // Random velocity in all directions
        float angle = random(360) * PI / 180.0f;
        float speed = 0.1f + (random(40) / 100.0f);
        p.vx = cos(angle) * speed;
        p.vy = sin(angle) * speed;
        
        // Gravity effect
        p.gravity = _gravity;
        
        // Color similar to firework with slight variation
        p.hue = fw.hue + random(-10, 10);
        
        // Full brightness initially
        p.brightness = 255;
        
        // Random life
        p.life = 50 + random(50);
        
        // Add to firework
        fw.particles.push_back(p);
    }
}

// Draw all fireworks
void FireworkAnimation::drawFireworks() {
    // Set brightness
    FastLED.setBrightness(_brightness);
    
    // Draw each firework
    for (const auto& fw : _fireworks) {
        if (!fw.exploded) {
            // Draw rising firework as a trail
            for (int i = 0; i < 3; i++) {
                int y = fw.y + i;
                if (y >= 0 && y < _height) {
                    int ledIndex = getLedIndex(fw.x, y);
                    if (ledIndex >= 0 && ledIndex < _numLeds) {
                        // Fade trail
                        uint8_t fade = 255 - (i * 80);
                        leds[ledIndex] = CHSV(fw.hue, 255, fade);
                    }
                }
            }
        } else {
            // Draw particles
            for (const auto& p : fw.particles) {
                int x = round(p.x);
                int y = round(p.y);
                
                if (x >= 0 && x < _width * _panelCount && y >= 0 && y < _height) {
                    int ledIndex = getLedIndex(x, y);
                    if (ledIndex >= 0 && ledIndex < _numLeds) {
                        leds[ledIndex] = CHSV(p.hue, 255, p.brightness);
                    }
                }
            }
        }
    }
}

// Get the LED index for a given (x,y) coordinate
int FireworkAnimation::getLedIndex(int x, int y) {
    // Determine which panel this pixel belongs to
    int panel = x / _width;
    int localX = x % _width;
    int localY = y;
    
    // Apply panel rotation if needed
    int angle = 0;
    switch (panel) {
        case 0: angle = _rotationAngle1; break;
        case 1: angle = _rotationAngle2; break;
        case 2: angle = _rotationAngle3; break;
    }
    
    // Rotate the coordinates if needed
    rotateCoordinates(localX, localY, angle);
    
    // Adjust the panel based on panel order
    int adjustedPanel = panel;
    if (_panelOrder == 1) { // Right to left
        adjustedPanel = _panelCount - 1 - panel;
    }
    
    // Calculate the LED index using zigzag pattern
    int index = adjustedPanel * 256; // 16x16 = 256 LEDs per panel
    
    // Even rows go left to right, odd rows go right to left (zigzag)
    if (localY % 2 == 0) {
        index += localY * 16 + localX;
    } else {
        index += localY * 16 + (15 - localX);
    }
    
    return index;
}

// Rotate coordinates based on angle
void FireworkAnimation::rotateCoordinates(int &x, int &y, int angle) {
    int rotatedX = x;
    int rotatedY = y;
    
    switch (angle) {
        case 0:   // No rotation
            break;
        case 90:  // 90 degrees
            rotatedX = y;
            rotatedY = 15 - x;
            break;
        case 180: // 180 degrees
            rotatedX = 15 - x;
            rotatedY = 15 - y;
            break;
        case 270: // 270 degrees
            rotatedX = 15 - y;
            rotatedY = x;
            break;
    }
    
    x = rotatedX;
    y = rotatedY;
}
