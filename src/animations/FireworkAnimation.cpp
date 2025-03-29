#include "FireworkAnimation.h"
#include <Arduino.h>
#include <FastLED.h>

extern CRGB leds[];

FireworkAnimation::FireworkAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
    : BaseAnimation(numLeds, brightness)
    , _panelCount(panelCount)
    , _width(panelCount * 16)
    , _height(16)
    , _intervalMs(15)  // Faster update interval (was 30)
    , _lastUpdate(0)
    , _panelOrder(1)
    , _rotationAngle1(0)  // Changed to 0 (was 90)
    , _rotationAngle2(0)  // Changed to 0 (was 90)
    , _rotationAngle3(0)  // Changed to 0 (was 90)
    , _maxFireworks(10)   // Increased max fireworks (was 5)
    , _particleCount(40)  // Increased particle count (was 20)
    , _gravity(0.15f)     // Increased gravity effect (was 0.1f)
    , _launchProbability(0.15f) // Increased launch probability (was 0.05f)
{
}

void FireworkAnimation::begin() {
    FastLED.clear(true);
    _fireworks.clear();
    _lastUpdate = millis();
}

void FireworkAnimation::update() {
    unsigned long now = millis();
    if ((now - _lastUpdate) >= _intervalMs) {
        _lastUpdate = now;
        
        // Fade all LEDs slightly for trail effect
        fadeToBlackBy(leds, _numLeds, 40);
        
        // Update firework physics and possibly launch new ones
        updateFireworks();
        
        // Draw fireworks to the LED array
        drawFireworks();
        
        FastLED.show();
    }
}

void FireworkAnimation::updateFireworks() {
    // Possibly launch a new firework
    if (random(100) < _launchProbability * 100 && _fireworks.size() < _maxFireworks) {
        launchFirework();
    }
    
    // Update existing fireworks
    for (auto it = _fireworks.begin(); it != _fireworks.end(); ) {
        if (!it->exploded) {
            // Update rising firework
            it->y -= it->vy;
            it->vy *= 0.98; // Slight deceleration
            
            // Check if firework should explode
            if (it->vy < 0.3 || it->y < 2) {
                explodeFirework(*it);
            }
            
            ++it;
        } else {
            // Update particles
            bool allParticlesDead = true;
            
            for (auto& p : it->particles) {
                // Update particle position
                p.x += p.vx;
                p.y += p.vy;
                
                // Apply gravity
                p.vy += p.gravity;
                
                // Reduce life
                if (p.life > 0) {
                    p.life--;
                    p.brightness = map(p.life, 0, 50, 0, 255);
                    allParticlesDead = false;
                }
            }
            
            // Remove firework if all particles are dead
            if (allParticlesDead) {
                it = _fireworks.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void FireworkAnimation::launchFirework() {
    Firework fw;
    fw.x = random(_width);
    fw.y = _height - 1; // Start from bottom
    fw.vy = 0.4 + (random(60) / 100.0); // Increased upward velocity (was 0.3 + random(40)/100.0)
    fw.hue = random(256); // Random color
    fw.exploded = false;
    
    _fireworks.push_back(fw);
}

void FireworkAnimation::explodeFirework(Firework& firework) {
    firework.exploded = true;
    
    // Create particles in all directions
    for (int i = 0; i < _particleCount; i++) {
        Particle p;
        p.x = firework.x;
        p.y = firework.y;
        
        // Random direction with random velocity
        float angle = random(360) * PI / 180.0;
        float speed = 0.2 + (random(40) / 100.0); // Increased particle speed (was 0.1 + random(30)/100.0)
        p.vx = cos(angle) * speed;
        p.vy = sin(angle) * speed;
        
        // Slightly vary the color
        p.hue = firework.hue + random(-10, 11);
        p.brightness = 255;
        p.gravity = _gravity;
        p.life = 30 + random(20); // Random lifetime
        
        firework.particles.push_back(p);
    }
}

void FireworkAnimation::drawFireworks() {
    for (const auto& fw : _fireworks) {
        if (!fw.exploded) {
            // Draw rising firework
            int x = round(fw.x);
            int y = round(fw.y);
            
            if (x >= 0 && x < _width && y >= 0 && y < _height) {
                int idx = getLedIndex(x, y);
                if (idx >= 0 && idx < _numLeds) {
                    leds[idx] = CHSV(fw.hue, 255, 255);
                }
            }
        } else {
            // Draw particles
            for (const auto& p : fw.particles) {
                if (p.life > 0) {
                    int x = round(p.x);
                    int y = round(p.y);
                    
                    if (x >= 0 && x < _width && y >= 0 && y < _height) {
                        int idx = getLedIndex(x, y);
                        if (idx >= 0 && idx < _numLeds) {
                            leds[idx] = CHSV(p.hue, 255, p.brightness);
                        }
                    }
                }
            }
        }
    }
}

int FireworkAnimation::getLedIndex(int x, int y) {
    // Handle panel rotation and order
    int panelWidth = 16;
    int panelHeight = 16;
    
    // Determine which panel this (x,y) falls into
    int panelX = x / panelWidth;
    int localX = x % panelWidth;
    int localY = y;
    
    // Reorder panels if needed
    int actualPanelX = panelX;
    if (_panelOrder == 0) { // left to right
        actualPanelX = (_panelCount - 1) - panelX;
    }
    
    // Apply rotation based on which panel we're in
    int rotationAngle = 0;
    if (actualPanelX == 0) rotationAngle = _rotationAngle1;
    else if (actualPanelX == 1) rotationAngle = _rotationAngle2;
    else if (actualPanelX == 2) rotationAngle = _rotationAngle3;
    
    // Rotate 90 degrees counterclockwise for all panels (this is in addition to the panel-specific rotation)
    rotateCoordinates(localX, localY, 90); // 90 degrees counterclockwise
    
    // Ensure coordinates are valid after rotation
    if (localX < 0 || localX >= panelWidth || localY < 0 || localY >= panelHeight) {
        return -1;
    }
    
    // Calculate final index
    int panelOffset = actualPanelX * panelWidth * panelHeight;
    
    // Convert local coordinates to LED index within panel
    int localIndex;
    if (localX % 2 == 0) {
        // Even columns go bottom to top
        localIndex = localX * panelHeight + localY;
    } else {
        // Odd columns go top to bottom
        localIndex = localX * panelHeight + (panelHeight - 1 - localY);
    }
    
    return panelOffset + localIndex;
}

void FireworkAnimation::rotateCoordinates(int &x, int &y, int angle) {
    // Define the center of the panel.
    int centerX = 8;
    int centerY = 8;
    
    // Calculate offsets from the center.
    int dx = x - centerX;
    int dy = y - centerY;
    
    int newX, newY;
    
    // Use a switch statement for clarity.
    switch(angle) {
        case 90:
            // No rotation.
            newX = dx;
            newY = dy;
            break;
        case 0:
            // Clockwise 90°:
            // Formula: newX = centerX + (y - centerY)
            //          newY = centerY - (x - centerX)
            // Here: newX offset = dy, newY offset = -dx.
            newX = dy;
            newY = -dx;
            break;
        case 270:
            // 180° rotation (same in any direction).
            newX = -dx;
            newY = -dy;
            break;
        case 180:
            // Clockwise 270° (equivalent to 90° counterclockwise):
            // Formula: newX = centerX - (y - centerY)
            //          newY = centerY + (x - centerX)
            // Here: newX offset = -dy, newY offset = dx.
            newX = -dy;
            newY = dx;
            break;
        default:
            // If an unsupported angle is passed, no rotation occurs.
            newX = dx;
            newY = dy;
            break;
    }
    
    // Translate the rotated coordinates back relative to the center.
    x = newX + centerX;
    y = newY + centerY;
}


void FireworkAnimation::setBrightness(uint8_t b) {
    _brightness = b;
    FastLED.setBrightness(b);
}

void FireworkAnimation::setUpdateInterval(unsigned long intervalMs) {
    _intervalMs = intervalMs;
}

void FireworkAnimation::setPanelOrder(int order) {
    _panelOrder = order;
}

void FireworkAnimation::setRotationAngle1(int angle) {
    _rotationAngle1 = angle;
}

void FireworkAnimation::setRotationAngle2(int angle) {
    _rotationAngle2 = angle;
}

void FireworkAnimation::setRotationAngle3(int angle) {
    _rotationAngle3 = angle;
}

void FireworkAnimation::setMaxFireworks(int max) {
    _maxFireworks = max;
}

void FireworkAnimation::setParticleCount(int count) {
    _particleCount = count;
}

void FireworkAnimation::setGravity(float gravity) {
    _gravity = gravity;
}

void FireworkAnimation::setLaunchProbability(float prob) {
    _launchProbability = prob;
}
