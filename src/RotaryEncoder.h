#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <Arduino.h>

/**
 * A thin wrapper around the AiEsp32RotaryEncoder library that does
 * NOT use setEncoderSteps(), loop(), etc. Instead, we handle
 * everything with a simpler approach.
 */
class RotaryEncoder {
public:
    // We now provide BOTH a default constructor (no args)
    // and the original 3-arg constructor if you ever need it.
    RotaryEncoder();
    RotaryEncoder(int clkPin, int dtPin, int buttonPin);

    // Setup pins/interrupts inside the AiEsp32RotaryEncoder object
    void begin();

    // Called in loop() to track button changes, etc.
    void update();

    // Returns the current position
    int getPosition();

    // True exactly once when the button goes from not pressed -> pressed
    bool isButtonPressed();

    // True exactly once when the button goes from pressed -> not pressed
    bool isButtonReleased();

private:
    // We'll store the pins here
    int clkPin;
    int dtPin;
    int buttonPin;

    // We'll store the position locally as an int
    volatile int position;

    // Track button's previous state
    bool lastButtonState;

    // One-time flags for press/release
    bool pressedFlag;
    bool releasedFlag;

    // Minimal debouncing in update()
    unsigned long lastUpdateTime;
    static constexpr unsigned long debounceDelay = 1;

    // We keep a static pointer to this instance for the ISR
    static RotaryEncoder* instance;

    // The ISR function
    static void IRAM_ATTR readEncoderISR();
};

#endif
