// File: GameOfLifeAnimation.cpp
// Conway's Game of Life animation for LED matrices

#include "GameOfLifeAnimation.h"
#include <Arduino.h>
#include <FastLED.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>

// External reference to LED array
extern CRGB leds[];

// Constructor
GameOfLifeAnimation::GameOfLifeAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
    : BaseAnimation(numLeds, brightness, panelCount),
      _intervalMs(150),  // Default update interval
      _lastUpdateTime(0),
      _grid1(nullptr),
      _grid2(nullptr),
      _width(0),
      _height(0),
      _stagnationCounter(0),
      _maxStagnation(45), // Reset after repeated generations
      _lastCellCount(0),
      _panelOrder(0),
      _rotationAngle1(0),
      _rotationAngle2(0),
      _rotationAngle3(0),
      _currentPalette(nullptr),
      _allPalettes(nullptr),
      _birthMask(0),
      _surviveMask(0),
      _seedDensity(33),
      _wrapEdges(true),
      _colorMode(0),
      _ageGrid(nullptr),
      _historyIndex(0)
{
    // Calculate dimensions based on panel count (assuming square panels)
    _width = 16;  // Default width for one panel
    _height = 16; // Default height for one panel
    
    if (panelCount > 1) {
        // If we have multiple panels, adjust dimensions
        // For now, we'll just make it wider
        _width = _width * panelCount;
    }
    
    // Calculate grid size in bytes (each byte stores 8 cells)
    _gridSizeBytes = (_width * _height + 7) / 8;
    
    // Allocate memory for grids (bit-packed for efficiency)
    try {
        _grid1 = new uint8_t[_gridSizeBytes];
        _grid2 = new uint8_t[_gridSizeBytes];
        _ageGrid = new uint8_t[_width * _height];
        
        // Initialize grids to all zeros
        memset(_grid1, 0, _gridSizeBytes);
        memset(_grid2, 0, _gridSizeBytes);
        memset(_ageGrid, 0, _width * _height);
    } catch (std::bad_alloc& e) {
        // Handle memory allocation failure
        Serial.println("GameOfLife: Failed to allocate memory for grids");
        _grid1 = nullptr;
        _grid2 = nullptr;
        _ageGrid = nullptr;
    }

    // Default rule: Conway's Life (B3/S23)
    _birthMask = (1 << 3);
    _surviveMask = (1 << 2) | (1 << 3);

    resetHistory();
}

// Destructor
GameOfLifeAnimation::~GameOfLifeAnimation() {
    if (_grid1) {
        delete[] _grid1;
        _grid1 = nullptr;
    }
    if (_grid2) {
        delete[] _grid2;
        _grid2 = nullptr;
    }
    if (_ageGrid) {
        delete[] _ageGrid;
        _ageGrid = nullptr;
    }
}

// Initialize the animation
void GameOfLifeAnimation::begin() {
    if (!_grid1 || !_grid2) {
        Serial.println("GameOfLife: Grids not initialized, attempting to allocate memory...");
        try {
            if (!_grid1) _grid1 = new uint8_t[_gridSizeBytes];
            if (!_grid2) _grid2 = new uint8_t[_gridSizeBytes];
            if (!_ageGrid) _ageGrid = new uint8_t[_width * _height];
            
            memset(_grid1, 0, _gridSizeBytes);
            memset(_grid2, 0, _gridSizeBytes);
            if (_ageGrid) {
                memset(_ageGrid, 0, _width * _height);
            }
        } catch (std::bad_alloc& e) {
            Serial.println("GameOfLife: Failed to allocate memory in begin()");
            return;
        }
    }
    
    // Reset animation state
    _lastUpdateTime = millis();
    
    // Start with a random pattern
    randomize(_seedDensity); // Use configured density
}

// Update animation frame
void GameOfLifeAnimation::update() {
    if (!_grid1 || !_grid2) {
        // Safety check - if grids aren't allocated, try to allocate them again
        begin();
        return;
    }
    
    // Check if it's time to update the simulation
    unsigned long currentTime = millis();
    if (currentTime - _lastUpdateTime < _intervalMs) {
        return;
    }
    _lastUpdateTime = currentTime;

    // Count cells and evaluate current state
    int cellCount = countLiveCells();
    uint32_t stateHash = computeStateHash(_grid1);

    bool repeatedState = false;
    for (uint8_t i = 0; i < HISTORY_DEPTH; ++i) {
        if (_historyValid[i] && _stateHistory[i] == stateHash) {
            repeatedState = true;
            break;
        }
    }

    // Check for extinction
    if (cellCount == 0) {
        Serial.println("GameOfLife: All cells died, resetting simulation");
        randomize(33);
        drawGrid();
        return;
    }

    if (repeatedState) {
        _stagnationCounter = std::min(_stagnationCounter + 2, _maxStagnation);
    } else if (cellCount == _lastCellCount) {
        _stagnationCounter = std::min(_stagnationCounter + 1, _maxStagnation);
    } else {
        _stagnationCounter = 0;
    }

    if (_stagnationCounter >= _maxStagnation) {
        Serial.println("GameOfLife: Detected repeating pattern, resetting simulation");
        randomize(33);
        drawGrid();
        return;
    }

    // Record current state for future detection
    _stateHistory[_historyIndex] = stateHash;
    _historyValid[_historyIndex] = true;
    _historyIndex = (_historyIndex + 1) % HISTORY_DEPTH;
    _lastCellCount = cellCount;

    // Update simulation
    updateGrid();
    
    // Draw the updated grid
    drawGrid();
}

// Randomize the grid with a given density (0-100%)
void GameOfLifeAnimation::randomize(uint8_t density) {
    if (!_grid1 || !_grid2) return;

    density = constrain(density, (uint8_t)0, (uint8_t)100);
    _seedDensity = density;

    resetHistory();
    _lastUpdateTime = millis();
    
    // Reset the simulation state
    memset(_grid1, 0, _gridSizeBytes);
    memset(_grid2, 0, _gridSizeBytes);
    if (_ageGrid) {
        memset(_ageGrid, 0, _width * _height);
    }
    
    // Add random live cells based on density
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            if (random(100) < density) {
                // Set bit to 1 (cell alive)
                int byteIndex = (y * _width + x) / 8;
                int bitIndex = (y * _width + x) % 8;
                _grid1[byteIndex] |= (1 << bitIndex);
            }
        }
    }
}

// Set a predefined pattern (future feature)
void GameOfLifeAnimation::setPattern(int patternId) {
    // Clear the grid
    memset(_grid1, 0, _gridSizeBytes);
    
    // For now, just randomize with different densities
    switch (patternId) {
        case 0: randomize(20); break; // Sparse
        case 1: randomize(33); break; // Medium (default)
        case 2: randomize(50); break; // Dense
        default: randomize(33);
    }
}

// Update the simulation by one generation
void GameOfLifeAnimation::updateGrid() {
    // We're using two grids: _grid1 is the current state, _grid2 will be the next state
    memset(_grid2, 0, _gridSizeBytes);
    
    // For each cell in the grid
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            // Count neighbors (8-connected)
            int neighbors = 0;
            
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    // Skip the cell itself
                    if (dx == 0 && dy == 0) continue;
                    
                    // Calculate neighbor coordinates with wrapping
                    int nx = x + dx;
                    int ny = y + dy;
                    
                    if (_wrapEdges) {
                        nx = (nx + _width) % _width;
                        ny = (ny + _height) % _height;
                    } else {
                        if (nx < 0 || nx >= _width || ny < 0 || ny >= _height) {
                            continue;
                        }
                    }
                    
                    // Check if neighbor is alive
                    int neighborIndex = (ny * _width + nx) / 8;
                    int neighborBit = (ny * _width + nx) % 8;
                    
                    if (_grid1[neighborIndex] & (1 << neighborBit)) {
                        neighbors++;
                    }
                }
            }
            
            // Calculate cell index in the bit-packed array
            int cellIndex = (y * _width + x) / 8;
            int cellBit = (y * _width + x) % 8;
            
            // Get current cell state
            bool isAlive = _grid1[cellIndex] & (1 << cellBit);
            
            // Apply Conway's Game of Life rules
            bool willBeAlive = false;
            if (isAlive) {
                willBeAlive = (_surviveMask >> neighbors) & 0x1;
            } else {
                willBeAlive = (_birthMask >> neighbors) & 0x1;
            }
            
            // Set the cell's state in the next generation
            if (willBeAlive) {
                _grid2[cellIndex] |= (1 << cellBit);  // Set bit to 1
            }
        }
    }
    
    // Update age grid using new generation before swap
    if (_ageGrid) {
        int totalCells = _width * _height;
        for (int i = 0; i < totalCells; i++) {
            int byteIndex = i / 8;
            int bitIndex = i % 8;
            bool alive = _grid2[byteIndex] & (1 << bitIndex);
            if (alive) {
                uint8_t age = _ageGrid[i];
                _ageGrid[i] = (age < 255) ? age + 1 : 255;
            } else {
                _ageGrid[i] = 0;
            }
        }
    }

    // Swap grids (using pointer swap for efficiency)
    uint8_t* temp = _grid1;
    _grid1 = _grid2;
    _grid2 = temp;
}

// Draw the current grid to the LED array
void GameOfLifeAnimation::drawGrid() {
    if (!_grid1) return;
    
    // Use color palette if available
    CRGB aliveColor = CRGB::Green; // Default live cell color
    bool hasPalette = _currentPalette && !_currentPalette->empty();
    if (hasPalette) {
        aliveColor = (*_currentPalette)[0];
    }
    
    // Clear all LEDs first
    for (int i = 0; i < _numLeds; i++) {
        leds[i] = CRGB::Black;
    }
    
    // Draw live cells
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            // Calculate bit position
            int byteIndex = (y * _width + x) / 8;
            int bitIndex = (y * _width + x) % 8;
            
            // Check if cell is alive
            if (_grid1[byteIndex] & (1 << bitIndex)) {
                // Calculate LED index (mapping depends on your LED arrangement)
                int ledIndex = mapXYtoLED(x, y);
                
                // Set LED color if the index is valid
                if (ledIndex >= 0 && ledIndex < _numLeds) {
                    CRGB color = aliveColor;
                    if (_colorMode == 1 && _ageGrid) {
                        uint8_t age = _ageGrid[y * _width + x];
                        if (hasPalette) {
                            size_t paletteSize = _currentPalette->size();
                            size_t idx = (paletteSize > 1)
                                ? (age * (paletteSize - 1)) / 255
                                : 0;
                            color = (*_currentPalette)[idx];
                        } else {
                            color = CHSV(age, 255, 255);
                        }
                    } else if (_colorMode == 2 && _ageGrid) {
                        uint8_t age = _ageGrid[y * _width + x];
                        color = CHSV(age * 2, 255, 255);
                    }
                    leds[ledIndex] = color;
                }
            }
        }
    }
}

// Map x,y coordinates to LED index based on rotation and panel order
int GameOfLifeAnimation::mapXYtoLED(int x, int y) {
    const int panelSize = 16;

    if (x < 0 || y < 0 || x >= _width || y >= _height) {
        return -1;
    }

    int panel = x / panelSize;
    if (panel < 0 || panel >= _panelCount) {
        return -1;
    }

    int localX = x % panelSize;
    int localY = y;

    // Apply panel-specific rotation (supporting up to three configured panels)
    int rotation = 0;
    if (panel == 0) {
        rotation = _rotationAngle1;
    } else if (panel == 1) {
        rotation = _rotationAngle2;
    } else if (panel == 2) {
        rotation = _rotationAngle3;
    }
    rotateCoordinates(localX, localY, rotation);

    // Account for serpentine wiring
    if (localY % 2 != 0) {
        localX = (panelSize - 1) - localX;
    }

    int effectivePanel = (_panelOrder == 0) ? panel : (_panelCount - 1 - panel);
    int index = effectivePanel * panelSize * panelSize + localY * panelSize + localX;

    if (index < 0 || index >= _numLeds) {
        return -1;
    }
    
    return index;
}

void GameOfLifeAnimation::rotateCoordinates(int& x, int& y, int angle) const {
    const int panelSize = 16;
    int normalizedAngle = angle % 360;
    if (normalizedAngle < 0) {
        normalizedAngle += 360;
    }

    int tmpX, tmpY;
    switch (normalizedAngle) {
        case 0:
            break;
        case 90:
            tmpX = y;
            tmpY = panelSize - 1 - x;
            x = tmpX;
            y = tmpY;
            break;
        case 180:
            tmpX = panelSize - 1 - x;
            tmpY = panelSize - 1 - y;
            x = tmpX;
            y = tmpY;
            break;
        case 270:
            tmpX = panelSize - 1 - y;
            tmpY = x;
            x = tmpX;
            y = tmpY;
            break;
        default:
            break;
    }
}

// Count the number of live cells
int GameOfLifeAnimation::countLiveCells() {
    if (!_grid1) return 0;
    
    int count = 0;
    
    // Count bits set to 1 in the grid
    for (int i = 0; i < _gridSizeBytes; i++) {
        uint8_t byte = _grid1[i];
        
        // Count bits in this byte
        while (byte) {
            count += byte & 1;
            byte >>= 1;
        }
    }
    
    return count;
}

void GameOfLifeAnimation::resetHistory() {
    _stagnationCounter = 0;
    _historyIndex = 0;
    _lastCellCount = -1;
    std::fill(_stateHistory.begin(), _stateHistory.end(), 0u);
    std::fill(_historyValid.begin(), _historyValid.end(), false);
}

uint32_t GameOfLifeAnimation::computeStateHash(const uint8_t* grid) const {
    if (!grid) {
        return 0;
    }

    uint32_t hash = 2166136261u; // FNV-1a 32-bit offset basis
    for (int i = 0; i < _gridSizeBytes; ++i) {
        hash ^= grid[i];
        hash *= 16777619u; // FNV prime
    }
    return hash;
}

// Set current palette
void GameOfLifeAnimation::setCurrentPalette(int index) {
    if (_allPalettes && index >= 0 && index < _allPalettes->size()) {
        _currentPalette = &(*_allPalettes)[index];
    }
}
