// File: GameOfLifeAnimation.cpp
// Conway's Game of Life animation for LED matrices

#include "GameOfLifeAnimation.h"
#include <Arduino.h>
#include <FastLED.h>
#include <cstdlib>
#include <cstring>

// External reference to LED array
extern CRGB leds[];

// Constructor
GameOfLifeAnimation::GameOfLifeAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
    : BaseAnimation(numLeds, brightness, panelCount),
      _frameCount(0),
      _intervalMs(150),  // Default update interval
      _lastUpdateTime(0),
      _grid1(nullptr),
      _grid2(nullptr),
      _colorMap(nullptr),
      _width(0),
      _height(0),
      _gridSizeBytes(0),
      _gridSize(0),
      _stagnationCounter(0),
      _maxStagnation(100), // Reset after 100 identical generations
      _lastCellCount(0),
      _panelOrder(0),
      _rotationAngle1(0),
      _rotationAngle2(0),
      _rotationAngle3(0),
      _currentPalette(nullptr),
      _allPalettes(nullptr),
      _usePalette(true)
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
    _gridSize = _width * _height;
    
    // Allocate memory for grids (bit-packed for efficiency)
    try {
        _grid1 = new uint8_t[_gridSizeBytes];
        _grid2 = new uint8_t[_gridSizeBytes];
        _colorMap = new CRGB[_gridSize];
        
        // Initialize grids to all zeros
        memset(_grid1, 0, _gridSizeBytes);
        memset(_grid2, 0, _gridSizeBytes);
        
        // Initialize color map to black
        for (int i = 0; i < _gridSize; i++) {
            _colorMap[i] = CRGB::Black;
        }
    } catch (std::bad_alloc& e) {
        // Handle memory allocation failure
        Serial.println("GameOfLife: Failed to allocate memory for grids or color map");
        _grid1 = nullptr;
        _grid2 = nullptr;
        _colorMap = nullptr;
    }
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
    if (_colorMap) {
        delete[] _colorMap;
        _colorMap = nullptr;
    }
}

// Initialize the animation
void GameOfLifeAnimation::begin() {
    if (!_grid1 || !_grid2 || !_colorMap) {
        Serial.println("GameOfLife: Grids not initialized, attempting to allocate memory...");
        try {
            if (!_grid1) _grid1 = new uint8_t[_gridSizeBytes];
            if (!_grid2) _grid2 = new uint8_t[_gridSizeBytes];
            if (!_colorMap) _colorMap = new CRGB[_gridSize];
            
            memset(_grid1, 0, _gridSizeBytes);
            memset(_grid2, 0, _gridSizeBytes);
            for (int i = 0; i < _gridSize; i++) {
                _colorMap[i] = CRGB::Black;
            }
        } catch (std::bad_alloc& e) {
            Serial.println("GameOfLife: Failed to allocate memory in begin()");
            return;
        }
    }
    
    // Reset animation state
    _frameCount = 0;
    _lastUpdateTime = millis();
    _stagnationCounter = 0;
    
    // Start with a random pattern
    randomize(33); // 33% initial density
}

// Update animation frame
void GameOfLifeAnimation::update() {
    if (!_grid1 || !_grid2 || !_colorMap) {
        // Safety check - if grids aren't allocated, try to allocate them again
        begin();
        return;
    }
    
    _frameCount++;
    
    // Only update the simulation every 3 frames to reduce CPU load
    if (_frameCount % 3 != 0) {
        // Still draw the current state on non-update frames
        drawGrid();
        return;
    }
    
    // Check if it's time to update the simulation
    unsigned long currentTime = millis();
    if (currentTime - _lastUpdateTime < _intervalMs) {
        return;
    }
    _lastUpdateTime = currentTime;
    
    // Count cells before update
    int cellCount = countLiveCells();
    
    // Check for stagnation
    if (cellCount == _lastCellCount) {
        _stagnationCounter++;
        
        // If stagnation lasted for too long, reset the simulation
        if (_stagnationCounter >= _maxStagnation) {
            Serial.println("GameOfLife: Detected stagnation, resetting simulation");
            randomize(33);
            _stagnationCounter = 0;
            drawGrid();
            return;
        }
    } else {
        // Reset stagnation counter if the number of cells changed
        _stagnationCounter = 0;
        _lastCellCount = cellCount;
    }
    
    // Check for extinction
    if (cellCount == 0) {
        Serial.println("GameOfLife: All cells died, resetting simulation");
        randomize(33);
        drawGrid();
        return;
    }
    
    // Update simulation
    updateGrid();
    
    // Draw the updated grid
    drawGrid();
}

// Randomize the grid with a given density (0-100%)
void GameOfLifeAnimation::randomize(uint8_t density) {
    if (!_grid1 || !_colorMap) return;
    
    // Reset the simulation state
    memset(_grid1, 0, _gridSizeBytes);
    
    // Add random live cells based on density
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            if (random(100) < density) {
                // Set bit to 1 (cell alive)
                int byteIndex = (y * _width + x) / 8;
                int bitIndex = (y * _width + x) % 8;
                _grid1[byteIndex] |= (1 << bitIndex);
                
                // Assign random color from palette
                if (_usePalette && _currentPalette && !_currentPalette->empty()) {
                    int colorIndex = random(_currentPalette->size());
                    _colorMap[y * _width + x] = (*_currentPalette)[colorIndex];
                } else {
                    // Fallback to random color if no palette
                    _colorMap[y * _width + x] = CRGB(random(256), random(256), random(256));
                }
            } else {
                // Ensure dead cells have black color
                _colorMap[y * _width + x] = CRGB::Black;
            }
        }
    }
}

// Update the simulation by one generation
void GameOfLifeAnimation::updateGrid() {
    // We're using two grids: _grid1 is the current state, _grid2 will be the next state
    
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
                    int nx = (x + dx + _width) % _width;
                    int ny = (y + dy + _height) % _height;
                    
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
                // Live cell with 2 or 3 neighbors survives
                willBeAlive = (neighbors == 2 || neighbors == 3);
                
                // Keep the same color if the cell survives
                if (willBeAlive) {
                    _colorMap[y * _width + x] = _colorMap[y * _width + x];
                }
            } else {
                // Dead cell with exactly 3 neighbors becomes alive
                willBeAlive = (neighbors == 3);
                
                // Assign new color if the cell becomes alive
                if (willBeAlive) {
                    if (_usePalette && _currentPalette && !_currentPalette->empty()) {
                        int colorIndex = random(_currentPalette->size());
                        _colorMap[y * _width + x] = (*_currentPalette)[colorIndex];
                    } else {
                        // Fallback to random color if no palette
                        _colorMap[y * _width + x] = CRGB(random(256), random(256), random(256));
                    }
                }
            }
            
            // Set the cell's state in the next generation
            if (willBeAlive) {
                _grid2[cellIndex] |= (1 << cellBit);  // Set bit to 1
            } else {
                _grid2[cellIndex] &= ~(1 << cellBit); // Set bit to 0
                // Dead cells should have black color
                _colorMap[y * _width + x] = CRGB::Black;
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
    if (!_grid1 || !_colorMap) return;
    
    // Clear all LEDs first
    for (int i = 0; i < _numLeds; i++) {
        leds[i] = CRGB::Black;
    }
    
    // Draw live cells with their assigned colors
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
                    // Apply brightness
                    CRGB color = _colorMap[y * _width + x];
                    color.nscale8(_brightness);
                    leds[ledIndex] = color;
                }
            }
        }
    }
}

// Map x,y coordinates to LED index based on rotation and panel order
int GameOfLifeAnimation::mapXYtoLED(int x, int y) {
    // This mapping function needs to be customized based on your LED layout
    // and panel arrangement.
    
    // For now, assume a simple row-major layout with 16x16 grid
    // TODO: Implement proper mapping with rotation and panel order
    
    // Simple row-major ordering
    return y * _width + x;
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

// Set current palette
void GameOfLifeAnimation::setCurrentPalette(int index) {
    if (_allPalettes && index >= 0 && index < _allPalettes->size()) {
        _currentPalette = &(*_allPalettes)[index];
    }
}
