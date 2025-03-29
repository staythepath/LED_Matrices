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
{
    // Create grids with the correct size for all panels
    int totalCells = _panelCount * _width * _height;
    _grid.resize(totalCells, false);
    _nextGrid.resize(totalCells, false);
    
    // Debug info
    Serial.printf("GameOfLife Animation created. Grid size: %d x %d, panels: %d\n", 
                  _width * _panelCount, _height, _panelCount);
}

// Destructor
GameOfLifeAnimation::~GameOfLifeAnimation() {
    Serial.println("GameOfLife Animation destroyed");
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

// Update the animation (called in the main loop)
void GameOfLifeAnimation::update() {
    unsigned long now = millis();
    if ((now - _lastUpdate) >= _intervalMs) {
        _lastUpdate = now;
        _generationCount++;
        
        // Only compute the next generation every other frame to reduce CPU load
        if (_generationCount % 2 == 0) {
            // Calculate next generation and update the display
            nextGeneration();
        }
        
        // Always update the display for smooth transitions
        updateDisplay();
        
        // Update color periodically
        if (_generationCount % 10 == 0) {
            _hue += _hueStep;
        }
        
        // Check if the simulation has stagnated
        // If no changes after several generations, randomize
        bool hasChanged = false;
        
        // Only check every 5 generations to reduce CPU load
        if (_generationCount % 5 == 0) {
            int totalCells = _panelCount * _width * _height;
            for (int i = 0; i < totalCells; i++) {
                if (_grid[i] != _nextGrid[i]) {
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
        
        // Swap the grids
        _grid = _nextGrid;
        
        // Only show every other frame to reduce SPI bus traffic
        if (_generationCount % 2 == 0) {
            FastLED.show();
        }
    }
}

// Calculate the next generation based on Conway's Game of Life rules
void GameOfLifeAnimation::nextGeneration() {
    int gridWidth = _width * _panelCount;
    
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < gridWidth; x++) {
            int index = y * gridWidth + x;
            int neighbors = countLiveNeighbors(x, y);
            
            // Apply Game of Life rules to determine cell state
            if (_grid[index]) {
                // Cell is currently alive
                if (neighbors < 2 || neighbors > 3) {
                    // Dies due to under-population or over-population
                    _nextGrid[index] = false;
                } else {
                    // Survives
                    _nextGrid[index] = true;
                }
            } else {
                // Cell is currently dead
                if (neighbors == 3) {
                    // Becomes alive due to reproduction
                    _nextGrid[index] = true;
                } else {
                    // Stays dead
                    _nextGrid[index] = false;
                }
            }
        }
    }
}

// Count the number of live neighbors for a cell
int GameOfLifeAnimation::countLiveNeighbors(int x, int y) {
    int count = 0;
    int gridWidth = _width * _panelCount;
    
    // Check all 8 surrounding cells with wrapping around the grid
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            // Skip the cell itself
            if (dx == 0 && dy == 0) continue;
            
            // Calculate neighbor coordinates with wrapping
            int nx = (x + dx + gridWidth) % gridWidth;
            int ny = (y + dy + _height) % _height;
            
            // Count live neighbor
            if (_grid[ny * gridWidth + nx]) {
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
            int index = y * gridWidth + x;
            
            if (_grid[index]) {
                // Get the LED index based on the physical layout
                int ledIndex = getLedIndex(x, y);
                
                if (ledIndex >= 0 && ledIndex < _numLeds) {
                    // Use HSV color based on position for visual interest
                    uint8_t posHue = _hue + ((x * 3 + y * 5) % 64);
                    leds[ledIndex] = CHSV(posHue, 255, 255);
                }
            }
        }
    }
}

// Get the LED index for a given (x,y) coordinate
int GameOfLifeAnimation::getLedIndex(int x, int y) {
    // Determine which panel this point belongs to
    int panel = x / _width;
    
    // Get the local x,y within the panel (0-15, 0-15)
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

// Randomize the grid with a certain density (0-100%)
void GameOfLifeAnimation::randomize(uint8_t density) {
    int gridWidth = _width * _panelCount;
    int totalCells = gridWidth * _height;
    
    Serial.printf("GameOfLife Animation: Randomizing grid with %d%% density\n", density);
    
    for (int i = 0; i < totalCells; i++) {
        _grid[i] = (random(100) < density);
        _nextGrid[i] = _grid[i];
    }
}

// Set up a predefined pattern
void GameOfLifeAnimation::setPattern(int patternId) {
    int gridWidth = _width * _panelCount;
    int totalCells = gridWidth * _height;
    
    Serial.printf("GameOfLife Animation: Setting pattern %d\n", patternId);
    
    // Clear the grid first
    for (int i = 0; i < totalCells; i++) {
        _grid[i] = false;
        _nextGrid[i] = false;
    }
    
    switch (patternId) {
        case 0: // Gosper glider gun
            setupGosperGliderGun(10, 3);
            break;
        case 1: // Glider
            setupGlider(5, 5);
            break;
        case 2: // Pulsar
            setupPulsar(gridWidth / 2 - 6, _height / 2 - 6);
            break;
        case 3: // Multiple gliders
            setupGlider(5, 5);
            setupGlider(15, 10);
            setupGlider(25, 5);
            break;
        default:
            // Random by default
            randomize(20);
            break;
    }
}

// Set up a glider (simple spaceship)
void GameOfLifeAnimation::setupGlider(int x, int y) {
    int gridWidth = _width * _panelCount;
    
    // Classic glider pattern
    if (x + 2 < gridWidth && y + 2 < _height) {
        _grid[(y+0) * gridWidth + (x+1)] = true;
        _grid[(y+1) * gridWidth + (x+2)] = true;
        _grid[(y+2) * gridWidth + (x+0)] = true;
        _grid[(y+2) * gridWidth + (x+1)] = true;
        _grid[(y+2) * gridWidth + (x+2)] = true;
        
        // Copy to next grid as well
        for (size_t i = 0; i < _grid.size(); i++) {
            _nextGrid[i] = _grid[i];
        }
    }
}

// Set up a pulsar (period 3 oscillator)
void GameOfLifeAnimation::setupPulsar(int x, int y) {
    int gridWidth = _width * _panelCount;
    
    // Pulsar pattern (period 3 oscillator)
    // Only setup if it fits within the grid
    if (x + 12 < gridWidth && y + 12 < _height) {
        // Top and bottom rows
        for (int dx = 2; dx <= 4; dx++) {
            _grid[(y+0) * gridWidth + (x+dx)] = true;
            _grid[(y+0) * gridWidth + (x+dx+6)] = true;
            _grid[(y+5) * gridWidth + (x+dx)] = true;
            _grid[(y+5) * gridWidth + (x+dx+6)] = true;
            
            _grid[(y+7) * gridWidth + (x+dx)] = true;
            _grid[(y+7) * gridWidth + (x+dx+6)] = true;
            _grid[(y+12) * gridWidth + (x+dx)] = true;
            _grid[(y+12) * gridWidth + (x+dx+6)] = true;
        }
        
        // Left and right columns
        for (int dy = 2; dy <= 4; dy++) {
            _grid[(y+dy) * gridWidth + (x+0)] = true;
            _grid[(y+dy) * gridWidth + (x+5)] = true;
            _grid[(y+dy) * gridWidth + (x+7)] = true;
            _grid[(y+dy) * gridWidth + (x+12)] = true;
            
            _grid[(y+dy+6) * gridWidth + (x+0)] = true;
            _grid[(y+dy+6) * gridWidth + (x+5)] = true;
            _grid[(y+dy+6) * gridWidth + (x+7)] = true;
            _grid[(y+dy+6) * gridWidth + (x+12)] = true;
        }
        
        // Copy to next grid
        for (size_t i = 0; i < _grid.size(); i++) {
            _nextGrid[i] = _grid[i];
        }
    }
}

// Set up a Gosper glider gun
void GameOfLifeAnimation::setupGosperGliderGun(int x, int y) {
    int gridWidth = _width * _panelCount;
    
    // Only setup if it fits within the grid
    if (x + 36 < gridWidth && y + 9 < _height) {
        // Left block
        _grid[(y+4) * gridWidth + (x+0)] = true;
        _grid[(y+4) * gridWidth + (x+1)] = true;
        _grid[(y+5) * gridWidth + (x+0)] = true;
        _grid[(y+5) * gridWidth + (x+1)] = true;
        
        // Left ship
        _grid[(y+2) * gridWidth + (x+12)] = true;
        _grid[(y+2) * gridWidth + (x+13)] = true;
        _grid[(y+3) * gridWidth + (x+11)] = true;
        _grid[(y+3) * gridWidth + (x+15)] = true;
        _grid[(y+4) * gridWidth + (x+10)] = true;
        _grid[(y+4) * gridWidth + (x+16)] = true;
        _grid[(y+5) * gridWidth + (x+10)] = true;
        _grid[(y+5) * gridWidth + (x+14)] = true;
        _grid[(y+5) * gridWidth + (x+16)] = true;
        _grid[(y+5) * gridWidth + (x+17)] = true;
        _grid[(y+6) * gridWidth + (x+10)] = true;
        _grid[(y+6) * gridWidth + (x+16)] = true;
        _grid[(y+7) * gridWidth + (x+11)] = true;
        _grid[(y+7) * gridWidth + (x+15)] = true;
        _grid[(y+8) * gridWidth + (x+12)] = true;
        _grid[(y+8) * gridWidth + (x+13)] = true;
        
        // Right ship
        _grid[(y+0) * gridWidth + (x+24)] = true;
        _grid[(y+1) * gridWidth + (x+22)] = true;
        _grid[(y+1) * gridWidth + (x+24)] = true;
        _grid[(y+2) * gridWidth + (x+20)] = true;
        _grid[(y+2) * gridWidth + (x+21)] = true;
        _grid[(y+3) * gridWidth + (x+20)] = true;
        _grid[(y+3) * gridWidth + (x+21)] = true;
        _grid[(y+4) * gridWidth + (x+20)] = true;
        _grid[(y+4) * gridWidth + (x+21)] = true;
        _grid[(y+5) * gridWidth + (x+22)] = true;
        _grid[(y+5) * gridWidth + (x+24)] = true;
        _grid[(y+6) * gridWidth + (x+24)] = true;
        
        // Right block
        _grid[(y+2) * gridWidth + (x+34)] = true;
        _grid[(y+2) * gridWidth + (x+35)] = true;
        _grid[(y+3) * gridWidth + (x+34)] = true;
        _grid[(y+3) * gridWidth + (x+35)] = true;
        
        // Copy to next grid
        for (size_t i = 0; i < _grid.size(); i++) {
            _nextGrid[i] = _grid[i];
        }
    }
}
