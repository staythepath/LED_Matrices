// File: GameOfLifeAnimation.cpp
// Conway's Game of Life animation for LED matrices

#include "GameOfLifeAnimation.h"
#include <Arduino.h>
#include <FastLED.h>

extern CRGB leds[];

// Constructor
GameOfLifeAnimation::GameOfLifeAnimation(uint16_t numLeds, uint8_t brightness, int panelCount)
    : BaseAnimation(numLeds, brightness)
    , _width(16)  // Fixed width per panel
    , _height(16)
    , _panelCount(panelCount)
    , _lastUpdate(0)
    , _intervalMs(300)  // Default update speed (300ms between generations)
    , _generationCount(0)
    , _stuckTimeout(0)   // Initialize timeout counter
    , _panelOrder(1)
    , _rotationAngle1(90)
    , _rotationAngle2(90)
    , _rotationAngle3(90)
    , _hue(0)
    , _hueStep(1)
    , _allPalettes(nullptr)
    , _currentPalette(nullptr)
    , _currentPaletteIndex(0)
{
    // Calculate how many bytes we need to store the grid
    // Each byte stores 8 cells, so we need (width*height*panelCount)/8 + 1 bytes
    _totalBytes = (_panelCount * _width * _height + 7) / 8;
    
    // Resize our byte arrays
    _gridBytes.resize(_totalBytes, 0);
    _nextGridBytes.resize(_totalBytes, 0);
    
    // Debug info
    Serial.printf("GameOfLife Animation created. Grid size: %d x %d, panels: %d, bytes: %d\n", 
                  _width * _panelCount, _height, _panelCount, _totalBytes);
}

// Destructor
GameOfLifeAnimation::~GameOfLifeAnimation() {
    Serial.println("GameOfLife Animation destroyed");
}

// Get cell state from bit-packed array
bool GameOfLifeAnimation::getCellState(const std::vector<uint8_t>& grid, int x, int y) {
    int gridWidth = _width * _panelCount;
    int cellIndex = y * gridWidth + x;
    int byteIndex = cellIndex / 8;
    int bitIndex = cellIndex % 8;
    
    // Return the state of the specific bit
    return (grid[byteIndex] & (1 << bitIndex)) != 0;
}

// Set cell state in bit-packed array
void GameOfLifeAnimation::setCellState(std::vector<uint8_t>& grid, int x, int y, bool state) {
    int gridWidth = _width * _panelCount;
    int cellIndex = y * gridWidth + x;
    int byteIndex = cellIndex / 8;
    int bitIndex = cellIndex % 8;
    
    if (state) {
        // Set the bit
        grid[byteIndex] |= (1 << bitIndex);
    } else {
        // Clear the bit
        grid[byteIndex] &= ~(1 << bitIndex);
    }
}

// Initialize the animation
void GameOfLifeAnimation::begin() {
    Serial.println("GameOfLife Animation: begin()");
    FastLED.clear(true);
    _lastUpdate = millis();
    _generationCount = 0;
    _stuckTimeout = 0;
    
    // Initialize with a random pattern
    randomize(20);  // 20% density of live cells
    
    // Initialize color
    _hue = random8();
    
    Serial.println("GameOfLife Animation: begin() completed");
}

// Randomize the grid with a certain density
void GameOfLifeAnimation::randomize(uint8_t density) {
    int gridWidth = _width * _panelCount;
    int totalCells = gridWidth * _height;
    
    // Clear all cells
    for (int i = 0; i < _totalBytes; i++) {
        _gridBytes[i] = 0;
        _nextGridBytes[i] = 0;
    }
    
    // Set random cells as alive based on density (0-100%)
    int cellsToActivate = (totalCells * density) / 100;
    for (int i = 0; i < cellsToActivate; i++) {
        int x = random(gridWidth);
        int y = random(_height);
        setCellState(_gridBytes, x, y, true);
        setCellState(_nextGridBytes, x, y, true);
    }
    
    Serial.printf("Randomized grid with density %d%%, activated %d cells\n", 
                 density, cellsToActivate);
}

// Set a specific pattern (placeholder for future implementation)
void GameOfLifeAnimation::setPattern(int patternId) {
    // We'll just randomize for now
    randomize(20);
}

// Set current palette by index
void GameOfLifeAnimation::setCurrentPalette(int index) {
    if (_allPalettes == nullptr) {
        return;
    }
    
    if (index < 0 || index >= (int)_allPalettes->size()) {
        Serial.printf("Invalid palette index: %d (max: %d)\n", index, _allPalettes->size() - 1);
        return;
    }
    
    _currentPaletteIndex = index;
    _currentPalette = &(*_allPalettes)[index];
    Serial.printf("GameOfLife Animation: Set palette to index %d\n", index);
}

// Update the animation (called in the main loop)
void GameOfLifeAnimation::update() {
    unsigned long now = millis();
    if ((now - _lastUpdate) >= _intervalMs) {
        _lastUpdate = now;
        _generationCount++;
        
        // Calculate next generation
        nextGeneration();
        
        // Update the display
        updateDisplay();
        
        // Show the LEDs
        FastLED.show();
        
        // Update color periodically
        if (_generationCount % 10 == 0) {
            _hue += _hueStep;
        }
        
        // Check if the simulation has stagnated
        // If no changes after several generations, randomize
        bool hasChanged = false;
        
        // Only check every 5 generations to reduce CPU load
        if (_generationCount % 5 == 0) {
            // Check if any cells have changed
            for (int i = 0; i < _totalBytes; i++) {
                if (_gridBytes[i] != _nextGridBytes[i]) {
                    hasChanged = true;
                    break;
                }
            }
            
            if (hasChanged) {
                _stuckTimeout = 0; // Reset timeout if there are changes
            } else {
                _stuckTimeout += 5; // Count generations with no changes
                
                // If stuck for 30 generations (about 15-30 seconds depending on speed), randomize
                if (_stuckTimeout >= 30) {
                    Serial.println("GameOfLife Animation: Stagnated, randomizing");
                    randomize(20);
                    _stuckTimeout = 0;
                }
            }
        }
        
        // Swap the grids (simply swap the pointers since we're checking for changes first)
        std::swap(_gridBytes, _nextGridBytes);
    }
}

// Calculate the next generation based on Conway's Game of Life rules
void GameOfLifeAnimation::nextGeneration() {
    int gridWidth = _width * _panelCount;
    
    // First calculate all next states
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < gridWidth; x++) {
            int neighbors = countLiveNeighbors(x, y);
            bool currentState = getCellState(_gridBytes, x, y);
            
            // Apply Conway's Game of Life rules
            bool newState = false;
            if (currentState) {
                newState = (neighbors == 2 || neighbors == 3);
            } else {
                newState = (neighbors == 3);
            }
            setCellState(_nextGridBytes, x, y, newState);
        }
    }
}

// Count live neighbors for a cell
int GameOfLifeAnimation::countLiveNeighbors(int x, int y) {
    int count = 0;
    int gridWidth = _width * _panelCount;
    
    // Check all 8 surrounding cells
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            // Skip the cell itself
            if (dx == 0 && dy == 0) continue;
            
            // Calculate neighbor coordinates with wrapping
            int nx = (x + dx + gridWidth) % gridWidth;
            int ny = (y + dy + _height) % _height;
            
            // Count live neighbor
            if (getCellState(_gridBytes, nx, ny)) {
                count++;
            }
        }
    }
    
    return count;
}

// Update the LED display with the current grid state
void GameOfLifeAnimation::updateDisplay() {
    // Clear the display
    FastLED.clear();
    
    // Set brightness
    FastLED.setBrightness(_brightness);
    
    int gridWidth = _width * _panelCount;
    
    // Draw the live cells with colors
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < gridWidth; x++) {
            if (getCellState(_gridBytes, x, y)) {
                // Get the LED index based on the physical layout
                int ledIndex = getLedIndex(x, y);
                
                if (ledIndex >= 0 && ledIndex < _numLeds) {
                    // Use palette if available, otherwise use HSV
                    if (_currentPalette != nullptr && !_currentPalette->empty()) {
                        // Use position and generation count to cycle through palette
                        int colorIndex = (x + y + _generationCount / 10) % _currentPalette->size();
                        leds[ledIndex] = (*_currentPalette)[colorIndex];
                    } else {
                        // Fallback to HSV color based on position
                        uint8_t posHue = _hue + ((x * 3 + y * 5) % 64);
                        leds[ledIndex] = CHSV(posHue, 255, 255);
                    }
                }
            }
        }
    }
}

// Get the LED index for a given (x,y) coordinate
int GameOfLifeAnimation::getLedIndex(int x, int y) {
    // Determine which panel this pixel belongs to
    int panel = x / _width;
    int localX = x % _width;
    int localY = y;
    
    // Apply panel rotation if needed
    int angle = 0;
    switch (panel) {
        case 0: angle = _rotationAngle1; break;
        case 1: angle = _rotationAngle2; break;
        case 2: angle = _rotationAngle3; break;
    }
    
    int rotatedX = localX;
    int rotatedY = localY;
    
    // Rotate the coordinates if needed
    switch (angle) {
        case 0:   // No rotation
            break;
        case 90:  // 90 degrees
            rotatedX = localY;
            rotatedY = 15 - localX;
            break;
        case 180: // 180 degrees
            rotatedX = 15 - localX;
            rotatedY = 15 - localY;
            break;
        case 270: // 270 degrees
            rotatedX = 15 - localY;
            rotatedY = localX;
            break;
    }
    
    // Adjust the panel based on panel order
    int adjustedPanel = panel;
    if (_panelOrder == 1) { // Right to left
        adjustedPanel = _panelCount - 1 - panel;
    }
    
    // Calculate the LED index using zigzag pattern
    int index = adjustedPanel * 256; // 16x16 = 256 LEDs per panel
    
    // Even rows go left to right, odd rows go right to left (zigzag)
    if (rotatedY % 2 == 0) {
        index += rotatedY * 16 + rotatedX;
    } else {
        index += rotatedY * 16 + (15 - rotatedX);
    }
    
    return index;
}
