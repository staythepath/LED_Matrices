// File: GameOfLifeAnimation.cpp
// Conway's Game of Life animation for LED matrices

#include "GameOfLifeAnimation.h"
#include <Arduino.h>
#include <FastLED.h>
#include <cstdlib>
#include <cstring>

extern CRGB leds[];

// Constants
static constexpr int BASE_PANEL_SIZE = 16;

GameOfLifeAnimation::GameOfLifeAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
    : BaseAnimation(numLeds, brightness, panelCount),
      _grid1(nullptr),
      _grid2(nullptr),
      _colorMap(nullptr),
      _width(BASE_PANEL_SIZE * panelCount),
      _height(BASE_PANEL_SIZE),
      _intervalMs(150),  // Default interval in ms
      _lastUpdateTime(0),
      _stagnationCounter(0),
      _lastCellCount(0),
      _panelOrder(1),
      _totalWipeTime(100),
      _columnDelay(0),
      _rotationAngle1(0),
      _rotationAngle2(0),
      _rotationAngle3(0),
      _allPalettes(nullptr),
      _currentPalette(nullptr),
      _usePalette(true),
      _currentWipeDirection(LEFT_TO_RIGHT),
      _currentWipeColumn(0),
      _isWiping(false),
      _needsNewGrid(true) {
    
    _gridSize = _width * _height;
    _gridSizeBytes = (_gridSize + 7) / 8;

    // Allocate memory with null checks
    _grid1 = new (std::nothrow) uint8_t[_gridSizeBytes];
    _grid2 = new (std::nothrow) uint8_t[_gridSizeBytes];
    _colorMap = new (std::nothrow) CRGB[_gridSize];

    if (_grid1 && _grid2 && _colorMap) {
        memset(_grid1, 0, _gridSizeBytes);
        memset(_grid2, 0, _gridSizeBytes);
        for (int i = 0; i < _gridSize; i++) {
            _colorMap[i] = CRGB::Black;
        }
    } else {
        Serial.println("GOL: Memory allocation failed");
        delete[] _grid1;
        delete[] _grid2;
        delete[] _colorMap;
        _grid1 = _grid2 = nullptr;
        _colorMap = nullptr;
    }
}

GameOfLifeAnimation::~GameOfLifeAnimation() {
    delete[] _grid1;
    delete[] _grid2;
    delete[] _colorMap;
}

void GameOfLifeAnimation::begin() {
    if (!_grid1 || !_grid2 || !_colorMap) return;
    randomize(33);  // Initialize with 33% density
    _lastUpdateTime = millis();
    _currentWipeDirection = LEFT_TO_RIGHT;
    _currentWipeColumn = 0;
    _isWiping = false;
    _needsNewGrid = true;
    
    // Draw the initial full grid
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            if (getCellState(_grid1, x, y)) {
                const int ledIndex = mapXYtoLED(x, y);
                if (ledIndex >= 0 && ledIndex < _numLeds) {
                    leds[ledIndex] = _colorMap[getCellIndex(x, y)];
                    leds[ledIndex].nscale8(_brightness);
                }
            } else {
                const int ledIndex = mapXYtoLED(x, y);
                if (ledIndex >= 0 && ledIndex < _numLeds) {
                    leds[ledIndex] = CRGB::Black;
                }
            }
        }
    }
}

void GameOfLifeAnimation::update() {
    if (!_grid1 || !_grid2 || !_colorMap) return;
    
    unsigned long currentTime = millis();
    
    // CRITICAL: The _intervalMs directly controls how quickly we process columns
    // This value comes from LEDManager based on the speed slider position
    // For speed=0 (slowest): _intervalMs ≈ 2000ms = 2 seconds per column
    // For speed=100 (fastest): _intervalMs ≈ 5ms = 0.005 seconds per column
    
    // Only proceed if enough time has passed since the last update
    if (currentTime - _lastUpdateTime < _intervalMs) return;
    
    // State machine logic - either calculate new generation or continue wiping
    if (_needsNewGrid) {
        // Start of animation cycle - calculate new generation
        calculateNextGrid();
        _needsNewGrid = false;
        _isWiping = true;
        
        // Always alternate wipe direction for visual interest
        _currentWipeDirection = (_currentWipeDirection == LEFT_TO_RIGHT) ? RIGHT_TO_LEFT : LEFT_TO_RIGHT;
        _currentWipeColumn = (_currentWipeDirection == LEFT_TO_RIGHT) ? 0 : _width - 1;
        
        // Draw the initial state with current column highlighted
        drawGrid();
    }
    else if (_isWiping) {
        // Process exactly ONE column at a time - this ensures every column gets processed
        // The speed control comes from how frequently this code block runs (_intervalMs)
        
        // Move to the next column in the current wipe direction
        if (_currentWipeDirection == LEFT_TO_RIGHT) {
            _currentWipeColumn++;
            
            // Check if we've completed the wipe
            if (_currentWipeColumn >= _width) {
                _isWiping = false;
                _needsNewGrid = true;
            }
        } else { // RIGHT_TO_LEFT
            _currentWipeColumn--;
            
            // Check if we've completed the wipe
            if (_currentWipeColumn < 0) {
                _isWiping = false;
                _needsNewGrid = true;
            }
        }
        
        // Draw the grid with current wipe position
        drawGrid();
    }
    else {
        // Shouldn't normally reach here, but as a fallback:
        _needsNewGrid = true;
    }

    // IMPORTANT: Reset the timer for the next column update
    // This is what controls the speed of the column wipe effect
    _lastUpdateTime = currentTime;
    
    // Show the updated LEDs
    FastLED.show();
}

// Helper method to draw the full grid at once (for high speeds)
void GameOfLifeAnimation::drawFullGrid() {
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            const int ledIndex = mapXYtoLED(x, y);
            if (ledIndex >= 0 && ledIndex < _numLeds) {
                if (getCellState(_grid1, x, y)) {
                    leds[ledIndex] = _colorMap[getCellIndex(x, y)];
                    leds[ledIndex].nscale8(_brightness);
                } else {
                    leds[ledIndex] = CRGB::Black;
                }
            }
        }
    }
}

void GameOfLifeAnimation::calculateNextGrid() {
    // Existing updateGrid logic moved here
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            int neighbors = 0;
            
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    
                    const int nx = (x + dx + _width) % _width;
                    const int ny = (y + dy + _height) % _height;
                    
                    if (getCellState(_grid1, nx, ny)) {
                        neighbors++;
                    }
                }
            }

            const bool isAlive = getCellState(_grid1, x, y);
            bool willLive = false;
            const int idx = getCellIndex(x, y);

            if (isAlive) {
                willLive = (neighbors == 2 || neighbors == 3);
                if (!willLive) _colorMap[idx] = CRGB::Black;
            } else {
                willLive = (neighbors == 3);
                if (willLive) _colorMap[idx] = getNewColor();
            }

            setCellState(_grid2, x, y, willLive);
        }
    }

    std::swap(_grid1, _grid2);
    
    // Check for stagnation/extinction
    const int cellCount = countLiveCells();
    if (cellCount == _lastCellCount) {
        if (++_stagnationCounter >= _maxStagnation) {
            randomize(_initialDensity);
            _stagnationCounter = 0;
        }
    } else {
        _stagnationCounter = 0;
        _lastCellCount = cellCount;
    }

    if (cellCount == 0) {
        randomize(_initialDensity);
    }
}

void GameOfLifeAnimation::randomize(uint8_t density) {
    if (!_grid1 || !_colorMap) return;

    memset(_grid1, 0, _gridSizeBytes);
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            if (random(100) < density) {
                setCellState(_grid1, x, y, true);
                _colorMap[getCellIndex(x, y)] = getNewColor();
            } else {
                setCellState(_grid1, x, y, false);
                _colorMap[getCellIndex(x, y)] = CRGB::Black;
            }
        }
    }
}

void GameOfLifeAnimation::drawGrid() {
    if (!_grid1 || !_colorMap) return;
    
    // First draw all cells in their final state (the current game state)
    // This ensures that all cells up to the current wipe position show their proper state
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            // For the wipe effect, only show the cells up to the current wipe position
            // Skip anything beyond the current column in the wipe direction
            if ((_currentWipeDirection == LEFT_TO_RIGHT && x > _currentWipeColumn) ||
                (_currentWipeDirection == RIGHT_TO_LEFT && x < _currentWipeColumn)) {
                continue;
            }
            
            const int ledIndex = mapXYtoLED(x, y);
            if (ledIndex >= 0 && ledIndex < _numLeds) {
                if (getCellState(_grid1, x, y)) {
                    // Draw the active cell with its proper color
                    leds[ledIndex] = _colorMap[getCellIndex(x, y)];
                    leds[ledIndex].nscale8(_brightness);
                } else {
                    // Empty cell is black
                    leds[ledIndex] = CRGB::Black;
                }
            }
        }
    }
    
    // Now highlight just the current wipe column with white
    for (int y = 0; y < _height; y++) {
        int x = _currentWipeColumn;
        const int ledIndex = mapXYtoLED(x, y);
        
        if (ledIndex >= 0 && ledIndex < _numLeds) {
            // Always highlight the current column, but show cell state
            if (getCellState(_grid1, x, y)) {
                // For active cells, add white to their existing color to make them brighter
                // Store the original color first
                CRGB originalColor = leds[ledIndex];
                // Add a very subtle white highlight effect
                leds[ledIndex] += CRGB(15, 15, 15);
            } else {
                // For empty cells, make them a very dim white to show column position
                leds[ledIndex] = CRGB(12, 12, 12); // Very dim white for empty cells
            }
        }
    }
}

void GameOfLifeAnimation::applyRotation(int& x, int& y, int panelIndex) const {
    int angle = 0;
    if (panelIndex == 0) angle = _rotationAngle1;
    else if (panelIndex == 1) angle = _rotationAngle2;
    else angle = _rotationAngle3;

    const int size = BASE_PANEL_SIZE - 1;
    int tmp;
    switch (angle) {
        case 90:
            tmp = x;
            x = size - y;
            y = tmp;
            break;
        case 180:
            x = size - x;
            y = size - y;
            break;
        case 270:
            tmp = x;
            x = y;
            y = size - tmp;
            break;
    }
}

int GameOfLifeAnimation::mapXYtoLED(int x, int y) {
    // Apply panel rotations and mapping
    int panelIndex = x / BASE_PANEL_SIZE;
    int panelX = x % BASE_PANEL_SIZE;
    int panelY = y;
    
    // Apply rotation for this panel
    applyRotation(panelX, panelY, panelIndex);

    // Handle panel order
    if (_panelOrder == 0) { // Right-to-left
        panelIndex = (_width/BASE_PANEL_SIZE - 1) - panelIndex;
    }

    return (panelIndex * BASE_PANEL_SIZE * _height) + (panelY * BASE_PANEL_SIZE) + panelX;
}

int GameOfLifeAnimation::countLiveCells() {
    if (!_grid1) return 0;

    int count = 0;
    for (int i = 0; i < _gridSizeBytes; i++) {
        uint8_t byte = _grid1[i];
        while (byte) {
            count += byte & 1;
            byte >>= 1;
        }
    }
    return count;
}

bool GameOfLifeAnimation::getCellState(uint8_t* grid, int x, int y) const {
    const int index = getCellIndex(x, y);
    return (grid[index / 8] & (1 << (index % 8))) != 0;
}

void GameOfLifeAnimation::setCellState(uint8_t* grid, int x, int y, bool state) {
    const int index = getCellIndex(x, y);
    uint8_t& byte = grid[index / 8];
    const uint8_t mask = 1 << (index % 8);
    
    if (state) byte |= mask;
    else byte &= ~mask;
}

CRGB GameOfLifeAnimation::getNewColor() const {
    if (_usePalette && _currentPalette && !_currentPalette->empty()) {
        return (*_currentPalette)[random(_currentPalette->size())];
    }
    return CRGB(random(256), random(256), random(256));
}

void GameOfLifeAnimation::setUpdateInterval(unsigned long intervalMs) {
    // Simply store the update interval - keep this direct and consistent
    _intervalMs = intervalMs;
    
    // For consistent wipe effect timing that doesn't affect the base speed,
    // we'll use a fixed multiplier that's appropriate for the speed range
    if (intervalMs < 25) {
        // At fast speeds, don't bother with wipe effects
        // The wipe time doesn't matter since we bypass it in update() anyway
        _totalWipeTime = 50; // Just a placeholder value
    } else {
        // For slower speeds, make wipe time proportional but not excessive
        _totalWipeTime = intervalMs * 2;
    }
    
    // Precompute the column delay for wipe effect
    _columnDelay = _totalWipeTime / _width;
    
    // Debug info
    Serial.printf("Game of Life speed: intervalMs=%lu, totalWipe=%lu\n", 
                 _intervalMs, _totalWipeTime);
    
    // Make sure we respond immediately to speed changes
    _lastUpdateTime = millis() - _intervalMs;
}


void GameOfLifeAnimation::setAllPalettes(const std::vector<std::vector<CRGB>>* allPalettes) {
    _allPalettes = allPalettes;
}

void GameOfLifeAnimation::setCurrentPalette(int index) {
    if (_allPalettes && index >= 0 && index < (int)_allPalettes->size()) {
        _currentPalette = &(*_allPalettes)[index];
    }
}

void GameOfLifeAnimation::setUsePalette(bool usePalette) {
    _usePalette = usePalette;
}