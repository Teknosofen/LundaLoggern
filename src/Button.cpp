// =============================================================================
//  Button.cpp — interrupt-driven button. See Button.hpp.
// =============================================================================

#include "Button.hpp"

Button::Button(uint8_t pin, unsigned long longPressMs, unsigned long debounceMs)
    : _pin(pin), _longPressMs(longPressMs), _debounceMs(debounceMs) {}

// attachInterruptArg() passes `this` to a capture-less lambda, which forwards
// to the member function. That is how a C-style ISR reaches the right object.
void Button::begin() {
    pinMode(_pin, INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(_pin), [](void* arg) {
        reinterpret_cast<Button*>(arg)->handleInterrupt();
    }, this, CHANGE);
}

// Runs in interrupt context (IRAM_ATTR keeps it in RAM). Debounce by time:
// any edge within _debounceMs of the previous accepted edge is ignored.
// The pin level after the edge tells press (LOW) from release (HIGH).
void IRAM_ATTR Button::handleInterrupt() {
    unsigned long currentTime = millis();
    if (currentTime - _lastInterruptTime < _debounceMs) return;
    _lastInterruptTime = currentTime;

    if (digitalRead(_pin) == LOW) {
        // pressed
        _pressedFlag = true;
        _pressStartTime = currentTime;
    } else {
        // released: classify by how long the button was held
        _releasedFlag = true;
        _pressDuration = currentTime - _pressStartTime;
        if (_pressDuration >= _longPressMs) {
            _longPressFlag = true;
        }
    }
}

void Button::update() {
    // nothing to do: all detection happens in the ISR
}

// The three getters below are "read and clear": each event is reported once.
bool Button::wasPressed() {
    if (_pressedFlag) {
        _pressedFlag = false;
        return true;
    }
    return false;
}

bool Button::wasReleased() {
    if (_releasedFlag) {
        _releasedFlag = false;
        return true;
    }
    return false;
}

bool Button::wasLongPress() {
    if (_longPressFlag) {
        _longPressFlag = false;
        return true;
    }
    return false;
}
