#ifndef BLINKANIMATION_H
#define BLINKANIMATION_H

#include "BaseAnimation.h"
#include <FastLED.h>
#include <vector>

/**
 * A "Blink" animation that cycles through a palette of colors.
 * Every interval, we pick the next color from the palette 
 * and fill the entire strip with it, then toggle off.
 */
class BlinkAnimation : public BaseAnimation {
public:
    /**
     * @param numLeds     - total LED count (panelCount * 16 * 16, typically)
     * @param brightness  - initial brightness
     * @param panelCount  - number of 16x16 panels (default 2)
     */
    BlinkAnimation(uint16_t numLeds, uint8_t brightness, int panelCount=2);

    // Required by BaseAnimation
    void begin() override;
    void update() override;

    // Overridden brightness
    void setBrightness(uint8_t b) override;

    // Set how fast we blink
    void setInterval(unsigned long intervalMs);

    // If we want to use a palette of colors
    void setPalette(const std::vector<CRGB>* palette);

private:
    // For consistency, we store the panelCount even though we don't use it
    int _panelCount;

    unsigned long _intervalMs;  
    unsigned long _lastToggle;
    bool          _isOn;

    // We'll cycle through a palette
    const std::vector<CRGB>* _palette;
    int   _paletteIndex; // which color in the palette
};

#endif // BLINKANIMATION_H
