// File: GameOfLifeAnimation.h
// Conway's Game of Life animation for LED matrices

#ifndef GAMEOFLIFEANIMATION_H
#define GAMEOFLIFEANIMATION_H

#include "BaseAnimation.h"
#include <array>
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
        const long minInterval = 20;
        const long maxInterval = 500;
        long mapped = map(speed, 0, 255, minInterval, maxInterval);
        _intervalMs = static_cast<uint32_t>(constrain(mapped, minInterval, maxInterval));
    }

    // Life-like rules
    void setRuleMasks(uint16_t birthMask, uint16_t surviveMask) {
        _birthMask = birthMask;
        _surviveMask = surviveMask;
    }

    // Random seed density (0-100%)
    void setSeedDensity(uint8_t density) { _seedDensity = constrain(density, (uint8_t)0, (uint8_t)100); }

    // Wrap edges (toroidal) vs bounded edges
    void setWrapMode(bool wrap) { _wrapEdges = wrap; }

    // Stagnation reset threshold (generations)
    void setStagnationLimit(uint16_t limit) { _maxStagnation = limit; }

    // Color mode: 0=solid, 1=age palette, 2=age hue
    void setColorMode(uint8_t mode) { _colorMode = mode; }

    // Reseed using current density
    void reseed() { randomize(_seedDensity); }
    
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
    // Update the simulation by one generation
    void updateGrid();
    
    // Draw the current grid to the LED array
    void drawGrid();
    
    // Map x,y coordinates to LED index
    int mapXYtoLED(int x, int y);
    
    // Count the number of live cells
    int countLiveCells();

    // Reset history buffers and stagnation tracking
    void resetHistory();

    // Compute a simple hash for the current grid state
    uint32_t computeStateHash(const uint8_t* grid) const;
    void rotateCoordinates(int& x, int& y, int angle) const;
    
    // Animation state
    uint32_t _intervalMs;       // Milliseconds between updates
    uint32_t _lastUpdateTime;   // Last update timestamp
    
    // Grid state
    uint8_t* _grid1;            // Current generation grid (bit-packed)
    uint8_t* _grid2;            // Next generation grid (bit-packed)
    int _width;                 // Grid width
    int _height;                // Grid height
    int _gridSizeBytes;         // Size of grid in bytes
    
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

    // Life-like rule masks
    uint16_t _birthMask;
    uint16_t _surviveMask;
    uint8_t _seedDensity;
    bool _wrapEdges;
    uint8_t _colorMode;

    // Age tracking (per-cell)
    uint8_t* _ageGrid;

    // Recent state tracking for stagnation detection
    static constexpr uint8_t HISTORY_DEPTH = 6;
    std::array<uint32_t, HISTORY_DEPTH> _stateHistory;
    std::array<bool, HISTORY_DEPTH> _historyValid;
    uint8_t _historyIndex;
};

#endif // GAMEOFLIFEANIMATION_H
