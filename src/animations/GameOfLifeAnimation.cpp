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
    
    // Use _intervalMs directly for timing to ensure speed slider works properly
    if (currentTime - _lastUpdateTime < _intervalMs) return;
    
    // CONTINUOUS APPROACH: Use the same animation logic at all speeds
    // with continuous exponential scaling for smooth transitions
    
    // State machine logic - either calculate new generation or continue wiping
    if (_needsNewGrid) {
        // Start of animation cycle - calculate new generation
        calculateNextGrid();
        _needsNewGrid = false;
        _isWiping = true;
        
        // Always alternate wipe direction for visual interest
        _currentWipeDirection = (_currentWipeDirection == LEFT_TO_RIGHT) ? RIGHT_TO_LEFT : LEFT_TO_RIGHT;
        _currentWipeColumn = (_currentWipeDirection == LEFT_TO_RIGHT) ? 0 : _width - 1;
    }
    else if (_isWiping) {
        // CONTINUOUS SCALING: Calculate columns per update using an exponential curve
        // that smoothly transitions from 1 column (slow) to many columns (fast)
        
        // Base formula: f(x) = a * e^(-bx)
        // Where 'x' is _intervalMs, and result is columns per update
        
        // Interval range: ~5ms (fastest) to ~2000ms (slowest)
        // Column range: ~1 (slowest) to ~width/3 (fastest)
        
        // Limit intervalMs to avoid division by zero (safety)
        float interval = std::max((float)_intervalMs, 5.0f);
        
        // Calculate exponential factor - more dramatic curve for better visual effect
        // This yields approximately 1 column at 2000ms and width/3 columns at 5ms
        float exponentialFactor = 15.0f * exp(-0.003f * interval);
        
        // Ensure we move at least 1 column, but never too many
        int columnsPerUpdate = std::max(1, (int)exponentialFactor);
        columnsPerUpdate = std::min(columnsPerUpdate, _width / 3);
        
        // Apply column movement with continuous scaling
        if (_currentWipeDirection == LEFT_TO_RIGHT) {
            _currentWipeColumn += columnsPerUpdate;
            if (_currentWipeColumn >= _width) {
                _isWiping = false;
                _needsNewGrid = true;
            }
        } else {
            _currentWipeColumn -= columnsPerUpdate;
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

    _lastUpdateTime = currentTime;
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

    for (int y = 0; y < _height; y++) {
        // Only process the current wipe column
        int x = _currentWipeColumn;
        
        if (getCellState(_grid1, x, y)) {
            const int ledIndex = mapXYtoLED(x, y);
            if (ledIndex >= 0 && ledIndex < _numLeds) {
                CRGB newColor = _colorMap[getCellIndex(x, y)];
                newColor.nscale8(_brightness);
                
                // Proper CRGB comparison
                if (leds[ledIndex].r != newColor.r || 
                    leds[ledIndex].g != newColor.g || 
                    leds[ledIndex].b != newColor.b) {
                    leds[ledIndex] = newColor;
                }
            }
        } else {
            const int ledIndex = mapXYtoLED(x, y);
            if (ledIndex >= 0 && ledIndex < _numLeds) {
                // Proper comparison to black
                if (leds[ledIndex].r != 0 || 
                    leds[ledIndex].g != 0 || 
                    leds[ledIndex].b != 0) {
                    leds[ledIndex] = CRGB::Black;
                }
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