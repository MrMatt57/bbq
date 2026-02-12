#pragma once

#include "config.h"
#include <stdint.h>

class FanController {
public:
    FanController();

    // Configure PWM channel at 25kHz. Call once from setup().
    void begin();

    // Set fan speed from 0-100%. Handles kick-start, min-speed clamping,
    // and long-pulse mode internally.
    void setSpeed(float percent);

    // Must be called every loop iteration to manage kick-start timing
    // and long-pulse cycling.
    void update();

    // Immediately stop the fan
    void off();

    // Current effective speed as a percentage (0-100)
    float getCurrentSpeedPct() const;

    // Current effective duty cycle (0-255)
    uint8_t getCurrentDuty() const;

    // Whether the fan is in kick-start phase right now
    bool isKickStarting() const;

    // Force fan to a specific duty (0-255) bypassing all logic. Useful for testing.
    void setManualDuty(uint8_t duty);

private:
    // Write a PWM duty value (0-255) to the hardware
    void writePWM(uint8_t duty);

    // Convert percent (0-100) to duty (0-255)
    static uint8_t percentToDuty(float pct);

    float    _targetPct;         // Requested speed percent (0-100)
    float    _currentPct;        // Actual output percent
    uint8_t  _currentDuty;       // Actual PWM duty value

    // Kick-start state
    bool          _kickStartActive;
    unsigned long _kickStartEndMs;
    float         _kickStartTargetPct;  // Speed to apply after kick-start

    // Long-pulse state
    bool          _longPulseActive;
    unsigned long _longPulseCycleStartMs;

    // Whether we were previously at 0% (for kick-start triggering)
    bool _wasOff;

    // Manual override mode
    bool _manualMode;
};
