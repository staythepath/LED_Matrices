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
      _newBornCells(nullptr),
      _highlightIntensity(nullptr),
      _colorMap(nullptr),
      _transitionMap(nullptr),
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
    _newBornCells = new (std::nothrow) uint8_t[_gridSizeBytes];
    _dyingCells = new (std::nothrow) uint8_t[_gridSizeBytes];   // Track cells that are dying
    _highlightIntensity = new (std::nothrow) uint8_t[_gridSize]; // Full array since intensity needs a value per cell
    _fadeStartTime = new (std::nothrow) uint32_t[_gridSize];    // Timestamps for independent fading
    _fadeDuration = new (std::nothrow) uint32_t[_gridSize];     // Custom fade duration for each cell
    _colorMap = new (std::nothrow) CRGB[_gridSize];
    _transitionMap = new (std::nothrow) CRGB[_gridSize];

    if (_grid1 && _grid2 && _newBornCells && _dyingCells && _highlightIntensity && _fadeStartTime && _fadeDuration && _colorMap && _transitionMap) {
        memset(_grid1, 0, _gridSizeBytes);
        memset(_grid2, 0, _gridSizeBytes);
        memset(_newBornCells, 0, _gridSizeBytes);
        memset(_dyingCells, 0, _gridSizeBytes);
        memset(_highlightIntensity, 0, _gridSize);
        memset(_fadeStartTime, 0, _gridSize * sizeof(uint32_t));
        memset(_fadeDuration, 0, _gridSize * sizeof(uint32_t));
        for (int i = 0; i < _gridSize; i++) {
            _colorMap[i] = CRGB::Black;
            _transitionMap[i] = CRGB::Black;
        }
    } else {
        Serial.println("GOL: Memory allocation failed");
        delete[] _grid1;
        delete[] _grid2;
        delete[] _newBornCells;
        delete[] _highlightIntensity;
        delete[] _colorMap;
        delete[] _transitionMap;
        _grid1 = _grid2 = _newBornCells = _highlightIntensity = nullptr;
        _colorMap = _transitionMap = nullptr;
    }
}

GameOfLifeAnimation::~GameOfLifeAnimation() {
    delete[] _grid1;
    delete[] _grid2;
    delete[] _newBornCells;
    delete[] _dyingCells;
    delete[] _highlightIntensity;
    delete[] _fadeStartTime;
    delete[] _fadeDuration;
    delete[] _colorMap;
    delete[] _transitionMap;
}

void GameOfLifeAnimation::begin() {
    if (!_grid1 || !_grid2 || !_colorMap) return;
    randomize(33);  // Initialize with 33% density
    
    // Clear any previous state
    memset(_newBornCells, 0, _gridSizeBytes);
    memset(_dyingCells, 0, _gridSizeBytes);
    
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
    // For speed=0 (slowest): _intervalMs ≈ 2000ms = 2 seconds per column
    // For speed=100 (fastest): _intervalMs ≈ 1ms = 0.001 seconds per column
    
    // Only proceed if enough time has passed since the last update
    if (currentTime - _lastUpdateTime < _intervalMs) return;
    
    // Optimization for maximum speed (when _intervalMs is very small)
    if (_intervalMs <= 5) {
        // Fast path: Skip column-by-column animation at high speeds
        // Just calculate and draw the entire grid at once
        if (_needsNewGrid) {
            calculateNextGrid();
            _needsNewGrid = false;
            
            // Check for stagnation only at the end of a full grid update
            int currentCellCount = countLiveCells();
            if (currentCellCount == _lastCellCount) {
                _stagnationCounter++;
                if (_stagnationCounter >= 5) {
                    // Pattern is stagnant, reinitialize
                    randomize(33);
                    _stagnationCounter = 0;
                }
            } else {
                _stagnationCounter = 0;
                _lastCellCount = currentCellCount;
            }
        } else {
            // Fade transition for all cells (white to color)
            static unsigned long lastTransitionTime = 0;
            unsigned long currentMillis = millis();
            
            // Perform transition update every ~33ms (30fps) for smooth transitions
            // This ensures consistent timing regardless of main animation speed
            if (currentMillis - lastTransitionTime >= 33) {
                lastTransitionTime = currentMillis;
                
                // For a ~2 second fade (255 steps at 30fps would be 8.5 seconds)
                // So we need to decrease by ~4 units per frame to achieve a 2-second transition
                uint8_t fadeAmount = 4;
                
                for (int i = 0; i < _gridSize; i++) {
                    // We no longer need this code since all cells start fading
                    // immediately when born
                }
            }
            
            // Draw entire grid at once at high speeds
            drawFullGrid();
            
            // Reset for next generation immediately
            _needsNewGrid = true;
        }
        
        _lastUpdateTime = currentTime;
        return;
    }
    
    // Normal path for medium to slow speeds
    if (_needsNewGrid) {
        // Start of animation cycle - calculate new generation
        calculateNextGrid();
        _needsNewGrid = false;
        _isWiping = true;
        
        // Always alternate wipe direction for visual interest
        _currentWipeDirection = (_currentWipeDirection == LEFT_TO_RIGHT) ? RIGHT_TO_LEFT : LEFT_TO_RIGHT;
        _currentWipeColumn = (_currentWipeDirection == LEFT_TO_RIGHT) ? 0 : _width - 1;
        
        // We'll handle transition updates in the column processing loop
        // This ensures cells only start fading AFTER the swipe has passed them
        
        // Draw the initial state with current column highlighted
        drawGrid();
    }
    else if (_isWiping) {
        // Process exactly ONE column at a time - this ensures every column gets processed
        // The speed control comes from how frequently this code block runs (_intervalMs)
        
        // Move to the next column in the current wipe direction
        if (_currentWipeDirection == LEFT_TO_RIGHT) {
            // Transition management is now handled in the main fade logic at the end of update()
            
            _currentWipeColumn++;
            
            // Check if we've completed the wipe
            if (_currentWipeColumn >= _width) {
                _isWiping = false;
                _needsNewGrid = true;
                
                // Check for stagnation at the end of a full grid update
                int currentCellCount = countLiveCells();
                if (currentCellCount == _lastCellCount) {
                    _stagnationCounter++;
                    if (_stagnationCounter >= 5) {
                        // Pattern is stagnant, reinitialize
                        randomize(33);
                        _stagnationCounter = 0;
                    }
                } else {
                    _stagnationCounter = 0;
                    _lastCellCount = currentCellCount;
                }
            }
        } else { // RIGHT_TO_LEFT
            // Transition management is now handled in the main fade logic at the end of update()
            
            _currentWipeColumn--;
            
            // Check if we've completed the wipe
            if (_currentWipeColumn < 0) {
                _isWiping = false;
                _needsNewGrid = true;
                
                // Check for stagnation at the end of a full grid update
                int currentCellCount = countLiveCells();
                if (currentCellCount == _lastCellCount) {
                    _stagnationCounter++;
                    if (_stagnationCounter >= 5) {
                        // Pattern is stagnant, reinitialize
                        randomize(33);
                        _stagnationCounter = 0;
                    }
                } else {
                    _stagnationCounter = 0;
                    _lastCellCount = currentCellCount;
                }
            }
        }
        
        // Fade management is now handled in the main update loop at the end
        // This allows proper logic for when cells should start fading after the swipe passes
        
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
    
    // Fade all highlights slightly every frame, regardless of column updates
    // This ensures a smooth fade effect even at slow speeds
    if (_isWiping) {
        // White-to-color transition management that happens continuously after a swipe passes
        static unsigned long lastTransitionTime = 0;
        unsigned long currentMillis = millis();
        
        // Perform transition update approximately every 33ms (30fps) for smooth fades
        if (currentMillis - lastTransitionTime >= 33) {
            lastTransitionTime = currentMillis;
            
            // For a ~2 second fade (255 steps / 60 frames = ~4.25 units per frame)
            uint8_t fadeAmount = 4;
            
            // Process all columns that the swipe bar has already passed
            for (int x = 0; x < _width; x++) {
                bool shouldFade = false;
                if (_currentWipeDirection == LEFT_TO_RIGHT && x < _currentWipeColumn) {
                    shouldFade = true;
                } else if (_currentWipeDirection == RIGHT_TO_LEFT && x > _currentWipeColumn) {
                    shouldFade = true;
                }
                
                if (shouldFade) {
                    for (int y = 0; y < _height; y++) {
                        int idx = getCellIndex(x, y);
                        // We no longer need to handle transitions here
                        // All cells start fading immediately when born
                    }
                }
            }
        }
    }
    
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
                    
                    // Get the cell index for this position
                    int idx = getCellIndex(x, y);
                    
                    // Check if this cell is in transition (newly born)
                    uint8_t transitionProgress = _highlightIntensity[idx];
                    
                    if (transitionProgress > 0) {
                        // This cell is in transition from white to its target color
                        
                        // Calculate how far along the transition is (0-255)
                        // 255 = full white, 0 = full target color
                        uint8_t whiteAmount = transitionProgress;
                        uint8_t colorAmount = 255 - whiteAmount;
                        
                        // Get the target color with brightness applied
                        CRGB targetColor = _colorMap[idx];
                        targetColor.nscale8(_brightness);
                        
                        // Get the current transition color (starts as white)
                        CRGB transitionColor = _transitionMap[idx];
                        
                        // Blend between white and target color based on transition progress
                        leds[ledIndex] = blend(targetColor, transitionColor, whiteAmount);
                    } else if (getCellState(_newBornCells, x, y)) {
                        // If we just discovered this new cell in high-speed mode, set it to bright white
                        // In high-speed mode, we'll start the transition on the next update cycle
                        _highlightIntensity[idx] = 255; // Full white intensity
                        _transitionMap[idx] = CRGB(255, 255, 255); // Bright white initial color
                        leds[ledIndex] = _transitionMap[idx]; // Display as white
                    } else {
                        // Normal cell color for existing cells
                        CRGB cellColor = _colorMap[idx];
                        cellColor.nscale8(_brightness);
                        leds[ledIndex] = cellColor;
                    }
                } else {
                    leds[ledIndex] = CRGB::Black;
                }
            }
        }
    }
}

void GameOfLifeAnimation::calculateNextGrid() {
    // Clear the newBornCells array before calculating the next generation
    memset(_newBornCells, 0, _gridSizeBytes);
    
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
                if (!willLive) {
                    // This cell is dying
                    setCellState(_dyingCells, x, y, true);
                    
                    // Store its current color in the transition map for fade-out effect
                    // Check if it has a valid color first
                    if (_colorMap[idx].r == 0 && _colorMap[idx].g == 0 && _colorMap[idx].b == 0) {
                        // If it doesn't have a proper color, use a dim magenta
                        _transitionMap[idx] = CRGB(50, 0, 50);
                    } else {
                        _transitionMap[idx] = _colorMap[idx];
                    }
                    
                    // Reset any lingering fade state from birth animation
                    _highlightIntensity[idx] = 0;
                    
                    // Start the fade-out immediately
                    _fadeStartTime[idx] = millis();
                    
                    // Give each cell a slightly different fade duration (1-2 seconds)
                    _fadeDuration[idx] = 1000 + random(1000);
                    
                    // Set final color to black
                    _colorMap[idx] = CRGB::Black;
                }
            } else {
                willLive = (neighbors == 3);
                if (willLive) {
                    // This is a newly born cell
                    CRGB targetColor = getNewColor(); // Get the color this cell will eventually have
                    _colorMap[idx] = targetColor;     // Store the target color
                    _transitionMap[idx] = CRGB(255, 255, 255); // Use bright white
                    
                    // Reset any lingering death fade state
                    setCellState(_dyingCells, x, y, false);
                    
                    // Mark it in the newBornCells array
                    setCellState(_newBornCells, x, y, true);
                    
                    // Set initial transition intensity to maximum
                    // This will control how much of the white vs. target color is shown
                    _highlightIntensity[idx] = 255; // Full transition effect (100% white initially)
                    
                    // Start the fade immediately - don't wait for the bar to pass
                    // Set the timestamp for when this cell started fading
                    _fadeStartTime[idx] = millis();
                    
                    // Give each cell a slightly different fade duration (1.5-2.5 seconds)
                    // This creates a more natural, independent look
                    _fadeDuration[idx] = 1500 + random(1000);
                }
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

    // Reset all grid and transition related arrays
    memset(_grid1, 0, _gridSizeBytes);
    memset(_newBornCells, 0, _gridSizeBytes);
    memset(_dyingCells, 0, _gridSizeBytes);
    memset(_highlightIntensity, 0, _gridSize);
    memset(_fadeStartTime, 0, _gridSize * sizeof(uint32_t));
    memset(_fadeDuration, 0, _gridSize * sizeof(uint32_t));
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
                const uint8_t idx = getCellIndex(x, y);
                const uint32_t currentTime = millis();
                
                if (getCellState(_grid1, x, y)) { // Is the cell alive in the current grid?
                    // Handle cells that are alive
                    uint8_t transitionProgress = _highlightIntensity[idx];
                    uint32_t fadeStartedAt = _fadeStartTime[idx];
                    
                    if (fadeStartedAt > 0) { // This cell has started fading
                        // Calculate how long this cell has been fading
                        uint32_t fadeTime = currentTime - fadeStartedAt;
                        // Get this cell's custom fade duration
                        const uint32_t FADE_DURATION_MS = _fadeDuration[idx];
                        
                        if (fadeTime >= FADE_DURATION_MS) {
                            // Fade is complete, draw target color
                            leds[ledIndex] = _colorMap[idx];
                            leds[ledIndex].nscale8(_brightness);
                        } else {
                            // Calculate proportional fade (0-255)
                            uint8_t fadeProgress = (fadeTime * 255) / FADE_DURATION_MS;
                            uint8_t whiteAmount = 255 - fadeProgress;
                            
                            // Blend between target color and white based on fade progress
                            CRGB targetColor = _colorMap[idx];
                            targetColor.nscale8(_brightness);
                            CRGB transitionColor = _transitionMap[idx];
                            leds[ledIndex] = blend(targetColor, transitionColor, whiteAmount);
                        }
                    } else if (transitionProgress == 255) { // Cell is waiting to fade
                        // Draw full white for cells that haven't started fading yet
                        leds[ledIndex] = _transitionMap[idx];
                    } else { // Not a fading cell
                        leds[ledIndex] = _colorMap[idx];
                        leds[ledIndex].nscale8(_brightness);
                    }
                } else if (getCellState(_dyingCells, x, y)) { // Is this a dying cell?
                    // Handle cells that are dying with a fade-out effect
                    uint32_t fadeStartedAt = _fadeStartTime[idx];
                    
                    if (fadeStartedAt > 0) {
                        // Calculate how long this cell has been fading
                        uint32_t fadeTime = currentTime - fadeStartedAt;
                        // Get this cell's custom fade duration
                        const uint32_t FADE_DURATION_MS = _fadeDuration[idx];
                        
                        if (fadeTime >= FADE_DURATION_MS) {
                            // Fade is complete, cell is now completely dead
                            leds[ledIndex] = CRGB::Black;
                            
                            // Clear the dying flag since the animation is complete
                            setCellState(_dyingCells, x, y, false);
                        } else {
                            // Calculate proportional fade (0-255)
                            uint8_t fadeProgress = (fadeTime * 255) / FADE_DURATION_MS;
                            
                            // Blend from original color to black based on fade progress
                            CRGB originalColor = _transitionMap[idx];
                            
                            // Safety check for invalid colors
                            if (originalColor.r == 0 && originalColor.g == 0 && originalColor.b == 0) {
                                originalColor = CRGB(50, 0, 50); // Use magenta as fallback
                            }
                            
                            originalColor.nscale8(_brightness);
                            originalColor.nscale8(255 - fadeProgress); // Fade to black
                            
                            leds[ledIndex] = originalColor;
                        }
                    }
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
                // Get the cell index for this position
                uint8_t idx = getCellIndex(x, y);
                
                // Check if this cell is in transition (newly born)
                uint8_t transitionProgress = _highlightIntensity[idx];
                
                if (transitionProgress > 0) {
                    // This cell is marked for transition (255) or actively fading
                    
                    // Get the current time for fade calculations
                    uint32_t currentTime = millis();
                    uint32_t fadeStartedAt = _fadeStartTime[idx];
                    
                    // Calculate progress for fading (0-255 where 0 = no fade, 255 = complete fade)
                    uint8_t fadeProgress = 0;
                    uint8_t whiteAmount = 255; // Start fully white
                    
                    if (fadeStartedAt > 0) { // Has this cell started fading?
                        // Calculate how long this cell has been fading
                        uint32_t fadeTime = currentTime - fadeStartedAt;
                        // Get this cell's custom fade duration
                        const uint32_t FADE_DURATION_MS = _fadeDuration[idx];
                        
                        if (fadeTime >= FADE_DURATION_MS) {
                            // Fade is complete
                            fadeProgress = 255;
                            whiteAmount = 0;
                        } else {
                            // Calculate proportional fade (0-255)
                            fadeProgress = (fadeTime * 255) / FADE_DURATION_MS;
                            whiteAmount = 255 - fadeProgress;
                        }
                    }
                    
                    // Get the target color with brightness applied
                    CRGB targetColor = _colorMap[idx];
                    targetColor.nscale8(_brightness);
                    
                    // Get the white transition color
                    CRGB transitionColor = _transitionMap[idx];
                    
                    // Blend between white and target color based on fade progress
                    leds[ledIndex] = blend(targetColor, transitionColor, whiteAmount);
                    
                    // Add a small white highlight since we're in the current column
                    leds[ledIndex] += CRGB(15, 15, 15);
                } else {
                    // Regular cells get a subtle highlight in the active column
                    // Use the normal cell color
                    leds[ledIndex] = _colorMap[idx];
                    leds[ledIndex].nscale8(_brightness);
                    leds[ledIndex] += CRGB(15, 15, 15); // Add highlight for active column
                }
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

// Calculate how long a cell at position x should take to fade based on bar movement
uint32_t GameOfLifeAnimation::calculateFadeDuration(int x) const {
    // Determine how many columns the wipe needs to traverse for a full cycle back to x
    uint32_t totalColumnsToCycle;
    
    if (_currentWipeDirection == LEFT_TO_RIGHT) {
        // Bar moving right: time to reach right edge + time to cross entire width left + time to reach x
        totalColumnsToCycle = (_width - _currentWipeColumn) + _width + x;
    } else {
        // Bar moving left: time to reach left edge + time to cross entire width right + time to reach x
        totalColumnsToCycle = _currentWipeColumn + _width + (_width - x);
    }
    
    // Calculate how long that will take based on the column delay
    uint32_t timeUntilReturn = totalColumnsToCycle * _columnDelay;
    
    // Fade duration is half the time until the bar returns
    uint32_t fadeDuration = timeUntilReturn / 2;
    
    // Set reasonable limits
    return constrain(fadeDuration, 300UL, 2000UL);
}