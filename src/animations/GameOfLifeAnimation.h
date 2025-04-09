// File: GameOfLifeAnimation.h
// Conway's Game of Life animation for LED matrices

#ifndef GAMEOFLIFEANIMATION_H
#define GAMEOFLIFEANIMATION_H

#include "BaseAnimation.h"
#include <vector>

class GameOfLifeAnimation : public BaseAnimation {
public:
    GameOfLifeAnimation(uint16_t numLeds, uint8_t brightness, int panelCount = 2);
    virtual ~GameOfLifeAnimation();

    virtual void begin() override;
    virtual void update() override;

    // Speed control
    void setUpdateInterval(unsigned long intervalMs);

    void randomize(uint8_t density = 33);
    
    void setAllPalettes(const std::vector<std::vector<CRGB>>* allPalettes);
    void setCurrentPalette(int index);
    void setUsePalette(bool usePalette);
    void setWipeBarBrightness(uint8_t brightness) { _wipeBarBrightness = brightness; }
    
    // Calculate fade duration based on bar position and movement pattern
    uint32_t calculateFadeDuration(int x) const;

    void setRotationAngle1(int angle) { _rotationAngle1 = angle; }
    void setRotationAngle2(int angle) { _rotationAngle2 = angle; }
    void setRotationAngle3(int angle) { _rotationAngle3 = angle; }
    void setPanelOrder(int order) { _panelOrder = order; }

    // Type checking
    bool isGameOfLife() const override { return true; }

    // In GameOfLifeAnimation.h private section


    

private:
    enum WipeDirection { LEFT_TO_RIGHT, RIGHT_TO_LEFT };

    void updateGrid();
    void drawGrid(bool ignoreWipePosition = false);
    void drawFullGrid(); // New method to draw the entire grid at once (for high speeds)
    void calculateNextGrid();
    int mapXYtoLED(int x, int y);
    int countLiveCells();
    
    WipeDirection _currentWipeDirection;
    int _currentWipeColumn;
    bool _isWiping;
    bool _needsNewGrid;
    uint32_t _totalWipeTime;  // Total time for complete wipe cycle
    uint32_t _columnDelay;    // Time per column update

    void applyRotation(int& x, int& y, int panelIndex) const;
    CRGB getNewColor() const;

    // Helper functions for simplified implementation
    void cleanupPhantomAndStuckCells();
    void setupWipeAnimation();
    void updateWipePosition();
    int countNeighbors(int x, int y) const;
    bool applyLifeRules(bool isAlive, int neighbors) const;
    void handleCellBirth(int x, int y, int idx);
    void handleCellDeath(int x, int y, int idx);
    void checkForStagnation();
    
    inline int getCellIndex(int x, int y) const { return y * _width + x; }
    bool getCellState(uint8_t* grid, int x, int y) const;
    void setCellState(uint8_t* grid, int x, int y, bool state);

    uint8_t* _grid1;
    uint8_t* _grid2;
    uint8_t* _newBornCells; // Track cells that were just born in current generation
    uint8_t* _dyingCells;   // Track cells that are dying in current generation
    uint8_t* _highlightIntensity; // Track white highlight intensity for each cell
    uint32_t* _fadeStartTime;  // Timestamp when each cell started fading
    uint32_t* _fadeDuration;   // Custom fade duration for each cell
    CRGB* _colorMap;       // Final colors assigned to cells
    CRGB* _transitionMap;  // Current transitioning colors (for white-to-color fade)
    
    int _width;
    int _height;
    int _gridSizeBytes;
    int _gridSize;
    uint32_t _intervalMs;
    uint32_t _lastUpdateTime;

    // Game state
    int _stagnationCounter;
    int _lastCellCount;
    static constexpr int _maxStagnation = 100;
    static constexpr uint8_t _initialDensity = 33;

    // Display configuration
    int _panelOrder;
    int _rotationAngle1;
    int _rotationAngle2;
    int _rotationAngle3;

    const std::vector<std::vector<CRGB>>* _allPalettes;
    const std::vector<CRGB>* _currentPalette;
    bool _usePalette;
    uint8_t _wipeBarBrightness;  // Brightness of the wipe bar effect
};

#endif // GAMEOFLIFEANIMATION_H