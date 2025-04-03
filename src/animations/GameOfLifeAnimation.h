// File: GameOfLifeAnimation.h
// Conway's Game of Life animation for LED matrices

#ifndef GAMEOFLIFEANIMATION_H
#define GAMEOFLIFEANIMATION_H

#include "BaseAnimation.h"
#include <vector>

class GameOfLifeAnimation : public BaseAnimation {
public:
    // Constructor
    GameOfLifeAnimation(uint16_t numLeds, uint8_t brightness, int panelCount = 2);
    
    // Destructor
    virtual ~GameOfLifeAnimation();
    
    // Virtual methods from BaseAnimation
    virtual void begin() override;
    virtual void update() override;
    
    // Set simulation speed (interval between generations)
    void setSpeed(uint8_t speed) { 
        _intervalMs = map(speed, 0, 255, 50, 1000); 
        _lastUpdateTime = millis(); // Reset the timer
    }
    
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
    void setUsePalette(bool usePalette) { _usePalette = usePalette; }

private:
    // Update the simulation by one generation
    void updateGrid();
    
    // Draw the current grid to the LED array
    void drawGrid();
    
    // Map x,y coordinates to LED index
    int mapXYtoLED(int x, int y);
    
    // Count the number of live cells
    int countLiveCells();
    
    // Animation state
    uint32_t _frameCount;       // Frame counter
    uint32_t _intervalMs;       // Milliseconds between updates
    uint32_t _lastUpdateTime;   // Last update timestamp
    
    // Grid state
    uint8_t* _grid1;            // Current generation grid (bit-packed)
    uint8_t* _grid2;            // Next generation grid (bit-packed)
    CRGB* _colorMap;            // Color map for each cell
    int _width;                 // Grid width
    int _height;                // Grid height
    int _gridSizeBytes;         // Size of grid in bytes
    int _gridSize;              // Total number of cells
    
    // Stagnation detection
    int _stagnationCounter;     // Counter for identical generations
    int _maxStagnation;         // Max identical generations before reset
    int _lastCellCount;         // Previous generation cell count
    
    // Panel configuration
    int _panelOrder;            // 0=left-to-right, 1=right-to-left
    int _rotationAngle1;        // Rotation angle for panel 1 (0,90,180,270)
    int _rotationAngle2;        // Rotation angle for panel 2
    int _rotationAngle3;        // Rotation angle for panel 3
    
    // Color and palette
    const std::vector<std::vector<CRGB>>* _allPalettes;  // Pointer to all palettes
    const std::vector<CRGB>* _currentPalette;           // Current palette
    bool _usePalette;                                  // Whether to use palette colors
};

#endif // GAMEOFLIFEANIMATION_H