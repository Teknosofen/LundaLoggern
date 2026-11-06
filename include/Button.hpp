#pragma once
// =============================================================================
//  Button.hpp — debounced push button with short/long press detection
// -----------------------------------------------------------------------------
//  Interrupt driven: a pin-change ISR timestamps press and release, and the
//  main loop polls the resulting one-shot event flags. The button connects the
//  pin to GND (active LOW, internal pull-up).
//
//  Typical use (see main.cpp):
//      if (button.wasReleased()) {
//          if (button.wasLongPress()) { ... } else { ... }
//      }
//  Each was...() call returns true once per event and then clears the flag.
// =============================================================================
#include <Arduino.h>

class Button {
public:
    /// @param longPressMs press length that counts as a long press.
    /// @param debounceMs  edges closer together than this are ignored.
    Button(uint8_t pin, unsigned long longPressMs = 1000, unsigned long debounceMs = 50);

    /// Configure the pin (INPUT_PULLUP) and attach the change interrupt.
    void begin();
    /// ISR body; called on every edge. Not for use from normal code.
    void handleInterrupt();
    /// Placeholder for polling-based logic; currently does nothing.
    void update();

    /// Event flags, set by the ISR and cleared when read.
    bool wasPressed();
    bool wasReleased();
    /// True if the last release ended a long press. Check after wasReleased().
    bool wasLongPress();

private:
    uint8_t _pin;
    unsigned long _longPressMs;
    unsigned long _debounceMs;

    // Written in the ISR, read in loop(): volatile so the compiler re-reads them.
    volatile bool _pressedFlag = false;
    volatile bool _releasedFlag = false;
    volatile unsigned long _lastInterruptTime = 0;  // for debouncing
    volatile unsigned long _pressStartTime = 0;
    volatile unsigned long _pressDuration = 0;

    bool _longPressFlag = false;   // also written in the ISR (not volatile)
};
