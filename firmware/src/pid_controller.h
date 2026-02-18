#pragma once

#include "config.h"
#include <stdint.h>

#ifndef NATIVE_BUILD
#include <QuickPID.h>
#endif

// Lid-open state machine
enum class LidState : uint8_t {
    CLOSED,     // Normal operation
    OPEN        // Lid detected open, PID output suspended
};

class PidController {
public:
    PidController();

    // Initialize PID with defaults from config.h. Call once from setup().
    void begin();

    // Initialize PID with custom tunings
    void begin(float kp, float ki, float kd);

    // Run one PID computation if the sample interval has elapsed.
    // Returns the PID output (0-100%). Handles lid-open detection internally.
    float compute(float currentTemp, float setpoint);

    // PID output in the range [0..100] percent
    float getOutput() const;

    // Update tuning parameters at runtime
    void setTunings(float kp, float ki, float kd);

    // Get current tuning values
    float getKp() const { return _kp; }
    float getKi() const { return _ki; }
    float getKd() const { return _kd; }

    // Lid-open detection
    bool isLidOpen() const;

    // Reset integrator for bumpless transfer on setpoint change
    void resetIntegrator();

    // Enable or disable PID computation
    void setEnabled(bool enabled);
    bool isEnabled() const;

private:
    // Check lid-open condition and update state
    void updateLidState(float currentTemp, float setpoint);

    float _kp, _ki, _kd;

    // QuickPID uses float references for input/output/setpoint
    float _pidInput;
    float _pidOutput;
    float _pidSetpoint;

#ifndef NATIVE_BUILD
    QuickPID* _pid;
#endif

    LidState _lidState;
    bool _enabled;

    // Timing
    unsigned long _lastComputeMs;
};
