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

    void setSpeed(uint8_t speed);
    void randomize(uint8_t density = 33);
    
    void setAllPalettes(const std::vector<std::vector<CRGB>>* allPalettes);
    void setCurrentPalette(int index);
    void setUsePalette(bool usePalette);

    void setRotationAngle1(int angle) { _rotationAngle1 = angle; }
    void setRotationAngle2(int angle) { _rotationAngle2 = angle; }
    void setRotationAngle3(int angle) { _rotationAngle3 = angle; }
    void setPanelOrder(int order) { _panelOrder = order; }

    // In GameOfLifeAnimation.h private section


    

private:
    enum WipeDirection { LEFT_TO_RIGHT, RIGHT_TO_LEFT };

    void updateGrid();
    void drawGrid();
    void calculateNextGrid();
    int mapXYtoLED(int x, int y);
    int countLiveCells();
    
    uint32_t _totalWipeTime;  // Total time for complete wipe cycle
    uint32_t _columnDelay;    // Time per column update

    void applyRotation(int& x, int& y, int panelIndex) const;
    CRGB getNewColor() const;

    inline int getCellIndex(int x, int y) const { return y * _width + x; }
    bool getCellState(uint8_t* grid, int x, int y) const;
    void setCellState(uint8_t* grid, int x, int y, bool state);

    uint8_t* _grid1;
    uint8_t* _grid2;
    CRGB* _colorMap;
    
    int _width;
    int _height;
    int _gridSizeBytes;
    int _gridSize;
    uint32_t _intervalMs;
    uint32_t _lastUpdateTime;

    // Wipe effect variables
    WipeDirection _currentWipeDirection;
    int _currentWipeColumn;
    bool _isWiping;
    bool _needsNewGrid;

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
};

#endif // GAMEOFLIFEANIMATION_H