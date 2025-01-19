#include "RotaryEncoder.h"
#include <AiEsp32RotaryEncoder.h>

// -----------------------------------------------------------
// Move the Rotary Encoder pin definitions here
// (No longer in main.cpp)
// -----------------------------------------------------------
static const int ENC_A   = 12;
static const int ENC_B   = 11;
static const int ENC_BTN = 10;

// We create a global AiEsp32RotaryEncoder. We'll re-init in begin().
static AiEsp32RotaryEncoder internalEncoder(
    0,  // pinA placeholder
    0,  // pinB placeholder
    -1, // pinSW placeholder
    -1, // pinVCC placeholder
    1,  // steps
    true
);

// Static pointer so the ISR can call back into this object
RotaryEncoder* RotaryEncoder::instance = nullptr;

// -----------------------------------------------------------
// Default constructor (no arguments)
//   - We'll assign pins from our static constants above
// -----------------------------------------------------------
RotaryEncoder::RotaryEncoder()
    : clkPin(ENC_A),
      dtPin(ENC_B),
      buttonPin(ENC_BTN),
      position(0),
      lastButtonState(false),
      pressedFlag(false),
      releasedFlag(false),
      lastUpdateTime(0)
{
    instance = this;
}

// -----------------------------------------------------------
// 3-arg constructor (if you'd ever want to specify custom pins)
// -----------------------------------------------------------
RotaryEncoder::RotaryEncoder(int clkPin, int dtPin, int buttonPin)
    : clkPin(clkPin),
      dtPin(dtPin),
      buttonPin(buttonPin),
      position(0),
      lastButtonState(false),
      pressedFlag(false),
      releasedFlag(false),
      lastUpdateTime(0)
{
    instance = this;
}

// -----------------------------------------------------------
// ISR: calls the library's readEncoder_ISR()
// -----------------------------------------------------------
void IRAM_ATTR RotaryEncoder::readEncoderISR() {
    if (instance) {
        internalEncoder.readEncoder_ISR();
    }
}

// -----------------------------------------------------------
// begin(): sets up the internal AiEsp32RotaryEncoder
// -----------------------------------------------------------
void RotaryEncoder::begin() {
    // If your EC11 needs 4 steps per detent, set to 4, etc.
    uint8_t desiredSteps = 4;

    internalEncoder = AiEsp32RotaryEncoder(
        static_cast<uint8_t>(clkPin),
        static_cast<uint8_t>(dtPin),
        static_cast<int>(buttonPin),
        -1,  // no VCC pin
        desiredSteps,
        true // internal pull-down for ESP32 if needed
    );

    Serial.println("RotaryEncoder: internalEncoder.begin()");
    internalEncoder.begin();

    Serial.println("RotaryEncoder: internalEncoder.setup()");
    internalEncoder.setup(readEncoderISR);

    Serial.println("RotaryEncoder: attachInterrupt()");
    attachInterrupt(digitalPinToInterrupt(clkPin), readEncoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(dtPin),  readEncoderISR, CHANGE);

    // Optional: disable acceleration
    internalEncoder.disableAcceleration();
}

// -----------------------------------------------------------
// update(): Called in loop() to track button transitions
// -----------------------------------------------------------
void RotaryEncoder::update() {
    unsigned long now = millis();
    if (now - lastUpdateTime < debounceDelay) {
        return;
    }
    lastUpdateTime = now;

    // Update local position from the encoder
    position = static_cast<int>(internalEncoder.readEncoder());

    // Check the encoder button
    bool currentButtonState = internalEncoder.isEncoderButtonDown(); // true if pressed

    // Pressed transition
    if (currentButtonState && !lastButtonState) {
        Serial.println("RotaryEncoder: Button pressed");
        pressedFlag  = true;
        releasedFlag = false;
    }
    // Released transition
    else if (!currentButtonState && lastButtonState) {
        Serial.println("RotaryEncoder: Button released");
        pressedFlag  = false;
        releasedFlag = true;
    }
    else {
        // No event
        pressedFlag  = false;
        releasedFlag = false;
    }

    lastButtonState = currentButtonState;
}

// -----------------------------------------------------------
// getPosition()
// -----------------------------------------------------------
int RotaryEncoder::getPosition() {
    return position;
}

// -----------------------------------------------------------
// isButtonPressed()
// -----------------------------------------------------------
bool RotaryEncoder::isButtonPressed() {
    return pressedFlag;
}

// -----------------------------------------------------------
// isButtonReleased()
// -----------------------------------------------------------
bool RotaryEncoder::isButtonReleased() {
    return releasedFlag;
}
