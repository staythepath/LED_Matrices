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
      _wipeBarBrightness(20),
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

void GameOfLifeAnimation::cleanupPhantomAndStuckCells() {
    unsigned long currentTime = millis();
    
    // Check for cells that have been in transition for too long AND for phantom cells
    for (int i = 0; i < _gridSize; i++) {
        // Get coordinates for this cell
        int x = i % _width;
        int y = i / _width;
        
        // First check if it's a legitimate live cell
        bool isLiveCell = getCellState(_grid1, x, y);
        bool isDyingCell = getCellState(_dyingCells, x, y);
        
        if (!isLiveCell && !isDyingCell) {
            // This is not a live or dying cell - it shouldn't have any color or state
            // This is the most aggressive fix for phantom cells
            _colorMap[i] = CRGB::Black;
            _transitionMap[i] = CRGB::Black;
            _fadeStartTime[i] = 0;
            _fadeDuration[i] = 0;
            _highlightIntensity[i] = 0;
            setCellState(_newBornCells, x, y, false);
        } else {
            // For actual live cells, check for stuck transitions
            uint32_t fadeStartedAt = _fadeStartTime[i];
            if (fadeStartedAt > 0 && currentTime - fadeStartedAt > 3000) {
                // This cell has been transitioning for over 3 seconds - force completion
                _fadeStartTime[i] = 0;
                _highlightIntensity[i] = 0;
                
                // If it's a live cell, check for problematic colors
                if (isLiveCell) {
                    // Check for gray/white colors that might be stuck
                    if ((_colorMap[i].r == _colorMap[i].g && _colorMap[i].g == _colorMap[i].b && _colorMap[i].r > 0) ||
                        (_colorMap[i].r > 200 && _colorMap[i].g > 200 && _colorMap[i].b > 200)) {
                        // Replace gray/white with a proper palette color
                        _colorMap[i] = getNewColor();
                    }
                } else if (isDyingCell) {
                    // Finish the dying process
                    setCellState(_dyingCells, x, y, false);
                    _transitionMap[i] = CRGB::Black;
                    _colorMap[i] = CRGB::Black;
                }
            }
        }
    }
}

void GameOfLifeAnimation::setupWipeAnimation() {
    _needsNewGrid = false;
    _isWiping = true;
    
    // Always alternate wipe direction for visual interest
    _currentWipeDirection = (_currentWipeDirection == LEFT_TO_RIGHT) ? RIGHT_TO_LEFT : LEFT_TO_RIGHT;
    _currentWipeColumn = (_currentWipeDirection == LEFT_TO_RIGHT) ? 0 : _width - 1;
}

void GameOfLifeAnimation::updateWipePosition() {
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
}

void GameOfLifeAnimation::update() {
    if (!_grid1 || !_grid2 || !_colorMap) return;
    
    unsigned long currentTime = millis();
    
    // Periodic cleanup (once per second)
    static uint32_t lastCleanupTime = 0;
    const uint32_t CLEANUP_INTERVAL_MS = 1000; // Check every second
    
    if (currentTime - lastCleanupTime > CLEANUP_INTERVAL_MS) {
        lastCleanupTime = currentTime;
        cleanupPhantomAndStuckCells();
    }
    
    // Only proceed if enough time has passed since the last update
    if (currentTime - _lastUpdateTime < _intervalMs) return;
    _lastUpdateTime = currentTime;
    
    // Two main paths based on speed
    if (_intervalMs <= 5) {
        // Fast path: Skip column-by-column animation at high speeds
        if (_needsNewGrid) {
            calculateNextGrid();
            _needsNewGrid = false;
        } else {
            // Draw entire grid at once at high speeds
            drawFullGrid();
            _needsNewGrid = true;
        }
    } else {
        // Normal path for medium to slow speeds with column-by-column wipe
        if (_needsNewGrid) {
            calculateNextGrid();
            setupWipeAnimation();
            drawGrid();
        }
        else if (_isWiping) {
            updateWipePosition();
            drawGrid();
        }
        else {
            _needsNewGrid = true;
        }
    }
    
    // Show the updated LEDs
    FastLED.show();
}

// Helper method to draw the full grid at once (for high speeds)
void GameOfLifeAnimation::drawFullGrid() {
    // Simply reuse the drawGrid function with ignoreWipePosition=true
    drawGrid(true);
}

int GameOfLifeAnimation::countNeighbors(int x, int y) const {
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
    
    return neighbors;
}

bool GameOfLifeAnimation::applyLifeRules(bool isAlive, int neighbors) const {
    // Conway's Game of Life rules:
    // 1. Any live cell with 2 or 3 live neighbors survives
    // 2. Any dead cell with exactly 3 live neighbors becomes alive
    // 3. All other cells die or stay dead
    if (isAlive) {
        return (neighbors == 2 || neighbors == 3);
    } else {
        return (neighbors == 3);
    }
}

void GameOfLifeAnimation::handleCellDeath(int x, int y, int idx) {
    // Mark this cell as dying
    setCellState(_dyingCells, x, y, true);
    
    // If it's already black, don't bother with transition
    if (_colorMap[idx].r == 0 && _colorMap[idx].g == 0 && _colorMap[idx].b == 0) {
        _transitionMap[idx] = CRGB::Black;
        _fadeDuration[idx] = 1; // Very short duration
    } else {
        // Use current color as starting point for fade
        _transitionMap[idx] = _colorMap[idx];
    }
    
    // Reset lingering fade state from birth animation
    _highlightIntensity[idx] = 0;
    
    // Start the fade-out immediately
    _fadeStartTime[idx] = millis();
    
    // Give each cell a slightly different fade duration (1-2 seconds)
    _fadeDuration[idx] = 1000 + random(1000);
    
    // Set final color to black
    _colorMap[idx] = CRGB::Black;
}

void GameOfLifeAnimation::handleCellBirth(int x, int y, int idx) {
    // Get the color this cell will eventually have
    CRGB targetColor = getNewColor();
    _colorMap[idx] = targetColor;
    
    // Use pure white for new cells
    _transitionMap[idx] = CRGB(255, 255, 255);
    
    // Reset any lingering death fade state
    setCellState(_dyingCells, x, y, false);
    
    // Mark it in the newBornCells array
    setCellState(_newBornCells, x, y, true);
    
    // Set initial transition intensity to maximum
    _highlightIntensity[idx] = 255; // Full white initially
    
    // Start the fade immediately
    _fadeStartTime[idx] = millis();
    
    // Give each cell a slightly different fade duration (1.5-2.5 seconds)
    _fadeDuration[idx] = 1500 + random(1000);
}

void GameOfLifeAnimation::checkForStagnation() {
    const int cellCount = countLiveCells();
    
    // If no cells are alive, reset the grid
    if (cellCount == 0) {
        randomize(_initialDensity);
        return;
    }
    
    // Check if the pattern is stagnant (same cell count for multiple generations)
    if (cellCount == _lastCellCount) {
        if (++_stagnationCounter >= _maxStagnation) {
            randomize(_initialDensity);
            _stagnationCounter = 0;
        }
    } else {
        _stagnationCounter = 0;
        _lastCellCount = cellCount;
    }
}

void GameOfLifeAnimation::calculateNextGrid() {
    // Clear the arrays for tracking cell state changes
    memset(_newBornCells, 0, _gridSizeBytes);
    memset(_dyingCells, 0, _gridSizeBytes);
    
    // Apply Game of Life rules to each cell
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            // Count neighbors and apply rules
            int neighbors = countNeighbors(x, y);
            bool isAlive = getCellState(_grid1, x, y);
            bool willLive = applyLifeRules(isAlive, neighbors);
            int idx = getCellIndex(x, y);
            
            // Handle state transitions
            if (isAlive && !willLive) {
                handleCellDeath(x, y, idx);
            } else if (!isAlive && willLive) {
                handleCellBirth(x, y, idx);
            }
            
            // Set the cell's state in the next generation
            setCellState(_grid2, x, y, willLive);
        }
    }
    
    // Swap grid pointers for next generation
    std::swap(_grid1, _grid2);
    
    // Check for stagnation or extinction
    checkForStagnation();
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
    
    // Also clear all transition maps to avoid stuck colors
    for (int i = 0; i < _gridSize; i++) {
        _transitionMap[i] = CRGB::Black;
        _colorMap[i] = CRGB::Black;
    }
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

void GameOfLifeAnimation::drawGrid(bool ignoreWipePosition) {
    if (!_grid1 || !_colorMap) return;
    
    // First draw all cells in their final state (the current game state)
    // This ensures that all cells up to the current wipe position show their proper state
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            // For the wipe effect, only show the cells up to the current wipe position
            // Skip anything beyond the current column in the wipe direction unless ignoreWipePosition is true
            if (!ignoreWipePosition && 
                ((_currentWipeDirection == LEFT_TO_RIGHT && x > _currentWipeColumn) ||
                 (_currentWipeDirection == RIGHT_TO_LEFT && x < _currentWipeColumn))) {
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
                            
                            // Reset transition state once fully faded
                            _highlightIntensity[idx] = 0;
                            _fadeStartTime[idx] = 0;
                        } else {
                            // Calculate proportional fade (0-255)
                            uint8_t fadeProgress = (fadeTime * 255) / FADE_DURATION_MS;
                            uint8_t whiteAmount = 255 - fadeProgress;
                            
                            // Blend between target color and white based on fade progress
                            CRGB targetColor = _colorMap[idx];
                            
                            // Ensure the target color isn't gray or white (potential stuck color)
                            if ((targetColor.r == targetColor.g && targetColor.g == targetColor.b && targetColor.r > 0) ||
                                (targetColor.r > 200 && targetColor.g > 200 && targetColor.b > 200)) {
                                // Replace gray/white with a palette color
                                targetColor = getNewColor();
                                _colorMap[idx] = targetColor; // Update for future frames
                            }
                            
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
                            
                            // Reset ALL transition-related variables for this cell
                            // This is critical to ensure no color data remains
                            _fadeStartTime[idx] = 0;
                            _fadeDuration[idx] = 0;
                            _highlightIntensity[idx] = 0;
                            _transitionMap[idx] = CRGB::Black;
                            _colorMap[idx] = CRGB::Black; // Also clear the color map
                        } else {
                            // Calculate proportional fade (0-255)
                            uint8_t fadeProgress = (fadeTime * 255) / FADE_DURATION_MS;
                                                        // Get the original color this cell had before dying
                            CRGB originalColor = _transitionMap[idx];
                            
                            // Safety check for invalid/black colors - skip transition
                            if (originalColor.r == 0 && originalColor.g == 0 && originalColor.b == 0) {
                                // If there's no color to fade from, don't transition
                                setCellState(_dyingCells, x, y, false);
                                _fadeStartTime[idx] = 0;
                                _highlightIntensity[idx] = 0;
                                _transitionMap[idx] = CRGB::Black;
                                _colorMap[idx] = CRGB::Black; // Ensure both are black
                                leds[ledIndex] = CRGB::Black; // Draw black immediately
                                continue; // Skip the rest of this cell's processing
                            }
                            
                            // Apply fade based on progress (255 = completely faded to black)
                            uint8_t remaining = 255 - fadeProgress;
                            
                            // As cells get closer to black, make sure they're REALLY heading to pure black
                            // This prevents cells from getting stuck with a faint color
                            if (remaining < 64) {
                                // When we're close to black, drop even faster to ensure complete darkness
                                remaining = remaining / 2;
                            }
                            
                            // Apply brightness and fade scaling
                            originalColor.nscale8(_brightness);
                            originalColor.nscale8(remaining);
                            
                            // For almost-faded cells, ensure they're completely black
                            if (remaining < 10) {
                                originalColor = CRGB::Black;
                            }
                            
                            // Set the LED to the calculated color
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
    
    // Only highlight the wipe column if we're not ignoring wipe position
    if (!ignoreWipePosition) {
        // Now highlight just the current wipe column with a subtle overlay
        // This is ONLY a visual effect and doesn't modify any persistent cell colors
        for (int y = 0; y < _height; y++) {
            int x = _currentWipeColumn;
            const int ledIndex = mapXYtoLED(x, y);
            
            if (ledIndex >= 0 && ledIndex < _numLeds) {
                // NEVER color cells in the wipe column to avoid phantom cells
                int idx = getCellIndex(x, y);
                
                // Check if there's a live cell at this position
                bool isLiveCell = getCellState(_grid1, x, y);
                bool isDyingCell = getCellState(_dyingCells, x, y);
                
                if (isLiveCell) {
                    // For live cells, PRESERVE their original color but add a subtle overlay
                    // This is a temporary visual-only effect that won't persist
                    CRGB temp = leds[ledIndex];
                    
                    // Add the wipe bar brightness value to blue to create a highlight
                    // This is controlled by the Fade Amount slider in the web UI
                    temp.b = qadd8(temp.b, _wipeBarBrightness);
                    leds[ledIndex] = temp;
                } else if (isDyingCell) {
                    // For dying cells, don't add ANY visual effect - just let them fade naturally
                    // Don't change the color or add any highlights
                } else {
                    // COMPLETELY EMPTY cell - make sure it has no color data anywhere
                    _colorMap[idx] = CRGB::Black;
                    _transitionMap[idx] = CRGB::Black;
                    _highlightIntensity[idx] = 0;
                    _fadeStartTime[idx] = 0;
                    
                    // Set empty cell color for visual effect based on wipe bar brightness setting
                    // This ensures it won't contribute to any "stuck" cells
                    uint8_t emptyBrightness = constrain(_wipeBarBrightness / 4, 0, 20); // Scale down but cap at 20
                    leds[ledIndex] = CRGB(0, 0, emptyBrightness); // Pure blue based on wipe bar brightness
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
    // Return a palette color if using palette and it's available
    if (_usePalette && _currentPalette && !_currentPalette->empty()) {
        return (*_currentPalette)[random(_currentPalette->size())];
    }
    
    // Fallback to a vibrant random color (avoid grays) if no palette
    // We'll generate pure RGB colors to avoid creating gray tones
    uint8_t r = random(2) * 255;  // Either 0 or 255
    uint8_t g = random(2) * 255;  // Either 0 or 255
    uint8_t b = random(2) * 255;  // Either 0 or 255
    
    // Ensure we don't return black (all 0) or white (all 255)
    if (r == 0 && g == 0 && b == 0) r = 255;  // Avoid black
    if (r == 255 && g == 255 && b == 255) b = 0;  // Avoid white
    
    return CRGB(r, g, b);
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