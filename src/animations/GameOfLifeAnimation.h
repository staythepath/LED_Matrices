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
    
    // Set pattern (placeholder for future patterns)
    void setPattern(int patternId);

    // Palette support
    void setAllPalettes(const std::vector<std::vector<CRGB>>* allPalettes) { _allPalettes = allPalettes; }
    void setCurrentPalette(int index);
    void setPalette(const std::vector<CRGB>* palette) { _currentPalette = palette; }

private:
    // Grid dimensions
    int _width;
    int _height;
    int _panelCount;
    
    // Grid state stored as bit-packed uint8_t arrays for memory efficiency
    // Each byte stores 8 cells, so we need (width*height*panelCount)/8 + 1 bytes
    std::vector<uint8_t> _gridBytes;
    std::vector<uint8_t> _nextGridBytes;
    int _totalBytes;
    
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
    
    // Palette support
    const std::vector<std::vector<CRGB>>* _allPalettes;
    const std::vector<CRGB>* _currentPalette;
    int _currentPaletteIndex;
    
    // Helper methods for bit-packed grid operations
    bool getCellState(const std::vector<uint8_t>& grid, int x, int y);
    void setCellState(std::vector<uint8_t>& grid, int x, int y, bool state);
    
    // Calculate the next generation based on Game of Life rules
    void nextGeneration();
    
    // Compute the index in the LED array for a given (x,y) coordinate
    int getLedIndex(int x, int y);
    
    // Count live neighbors for a cell
    int countLiveNeighbors(int x, int y);
    
    // Update the LED display with the current grid state
    void updateDisplay();
};

#endif // GAMEOFLIFEANIMATION_H
