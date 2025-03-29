// File: GameOfLifeAnimation.h
// Conway's Game of Life animation for LED matrices

#ifndef GAMEOFLIFEANIMATION_H
#define GAMEOFLIFEANIMATION_H

#include "BaseAnimation.h"
#include <vector>

class GameOfLifeAnimation : public BaseAnimation {
public:
    // Constructor
    GameOfLifeAnimation(uint16_t numLeds, uint8_t brightness, int panelCount);
    
    // Destructor
    virtual ~GameOfLifeAnimation();
    
    // Virtual methods from BaseAnimation
    virtual void begin() override;
    virtual void update() override;
    
    // Set simulation speed (interval between generations)
    void setSpeed(uint8_t speed) { _intervalMs = map(speed, 0, 255, 50, 1000); }
    
    // Set panel rotation angles and order
    void setRotationAngle1(int angle) { _rotationAngle1 = angle; }
    void setRotationAngle2(int angle) { _rotationAngle2 = angle; }
    void setRotationAngle3(int angle) { _rotationAngle3 = angle; }
    void setPanelOrder(int order) { _panelOrder = order; }
    
    // Randomize the grid with a certain density (0-100%)
    void randomize(uint8_t density = 33);
    
    // Reset with a specific pattern (e.g., glider, pulsar, etc.)
    void setPattern(int patternId);

private:
    // Grid dimensions
    int _width;
    int _height;
    int _panelCount;
    
    // Grid state (true = alive, false = dead)
    std::vector<bool> _grid;
    std::vector<bool> _nextGrid;
    
    // Animation timing
    unsigned long _lastUpdate;
    unsigned long _intervalMs;
    unsigned long _generationCount;
    unsigned long _stuckTimeout;  // Timeout for when animation gets stuck
    
    // Panel configuration
    int _panelOrder;
    int _rotationAngle1;
    int _rotationAngle2;
    int _rotationAngle3;
    
    // Color settings
    uint8_t _hue;
    uint8_t _hueStep;
    
    // Calculate the next generation based on Game of Life rules
    void nextGeneration();
    
    // Compute the index in the LED array for a given (x,y) coordinate
    int getLedIndex(int x, int y);
    
    // Count live neighbors for a cell
    int countLiveNeighbors(int x, int y);
    
    // Update the LED display with the current grid state
    void updateDisplay();
    
    // Set up predefined patterns
    void setupGlider(int x, int y);
    void setupPulsar(int x, int y);
    void setupGosperGliderGun(int x, int y);
};

#endif // GAMEOFLIFEANIMATION_H
