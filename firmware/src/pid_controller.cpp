#include "pid_controller.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

PidController::PidController()
    : _kp(PID_KP)
    , _ki(PID_KI)
    , _kd(PID_KD)
    , _pidInput(0.0f)
    , _pidOutput(0.0f)
    , _pidSetpoint(0.0f)
#ifndef NATIVE_BUILD
    , _pid(nullptr)
#endif
    , _lidState(LidState::CLOSED)
    , _enabled(true)
    , _lastComputeMs(0)
{
}

void PidController::begin() {
    begin(PID_KP, PID_KI, PID_KD);
}

void PidController::begin(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
    _pidInput = 0.0f;
    _pidOutput = 0.0f;
    _pidSetpoint = 0.0f;
    _lidState = LidState::CLOSED;
    _enabled = true;

#ifndef NATIVE_BUILD
    if (_pid != nullptr) {
        delete _pid;
    }

    _pid = new QuickPID(&_pidInput, &_pidOutput, &_pidSetpoint,
                        _kp, _ki, _kd,
                        QuickPID::pMode::pOnMeas,
                        QuickPID::dMode::dOnMeas,
                        QuickPID::iAwMode::iAwCondition,
                        QuickPID::Action::direct);

    _pid->SetOutputLimits(PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    _pid->SetSampleTimeUs(PID_SAMPLE_MS * 1000UL);
    _pid->SetMode(QuickPID::Control::automatic);

    _lastComputeMs = millis();

    Serial.printf("[PID] Initialized: Kp=%.2f Ki=%.3f Kd=%.2f, interval=%dms\n",
                  _kp, _ki, _kd, PID_SAMPLE_MS);
#endif
}

float PidController::compute(float currentTemp, float setpoint) {
    if (!_enabled) {
        _pidOutput = 0.0f;
        return 0.0f;
    }

    // Update lid-open detection
    updateLidState(currentTemp, setpoint);

    // If lid is open, suspend PID output
    if (_lidState == LidState::OPEN) {
        _pidOutput = 0.0f;
        return 0.0f;
    }

#ifndef NATIVE_BUILD
    unsigned long now = millis();

    // QuickPID manages its own sample timing internally, but we feed it
    // current values each call
    _pidInput = currentTemp;
    _pidSetpoint = setpoint;

    _pid->Compute();

    // Clamp output to 0-100%
    if (_pidOutput < PID_OUTPUT_MIN) _pidOutput = PID_OUTPUT_MIN;
    if (_pidOutput > PID_OUTPUT_MAX) _pidOutput = PID_OUTPUT_MAX;
#endif

    return _pidOutput;
}

float PidController::getOutput() const {
    return _pidOutput;
}

void PidController::setTunings(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;

#ifndef NATIVE_BUILD
    if (_pid != nullptr) {
        _pid->SetTunings(_kp, _ki, _kd);
    }
    Serial.printf("[PID] Tunings updated: Kp=%.2f Ki=%.3f Kd=%.2f\n", _kp, _ki, _kd);
#endif
}

void PidController::resetIntegrator() {
#ifndef NATIVE_BUILD
    if (_pid != nullptr) {
        _pid->Reset();
        _pid->SetMode(QuickPID::Control::automatic);
    }
    Serial.println("[PID] Integrator reset (setpoint change)");
#endif
}

bool PidController::isLidOpen() const {
    return _lidState == LidState::OPEN;
}

void PidController::setEnabled(bool enabled) {
    _enabled = enabled;

#ifndef NATIVE_BUILD
    if (_pid != nullptr) {
        _pid->SetMode(enabled ? QuickPID::Control::automatic : QuickPID::Control::manual);
    }
#endif

    if (!enabled) {
        _pidOutput = 0.0f;
    }
}

bool PidController::isEnabled() const {
    return _enabled;
}

void PidController::updateLidState(float currentTemp, float setpoint) {
    if (setpoint <= 0.0f) return;  // No setpoint, no lid detection

    float dropThreshold = setpoint * (1.0f - LID_OPEN_DROP_PCT / 100.0f);
    float recoverThreshold = setpoint * (1.0f - LID_OPEN_RECOVER_PCT / 100.0f);

    switch (_lidState) {
        case LidState::CLOSED:
            // Detect lid open: temp drops more than LID_OPEN_DROP_PCT below setpoint
            if (currentTemp < dropThreshold) {
                _lidState = LidState::OPEN;
#ifndef NATIVE_BUILD
                Serial.printf("[PID] Lid-open detected! Temp=%.1f, threshold=%.1f\n",
                              currentTemp, dropThreshold);
#endif
            }
            break;

        case LidState::OPEN:
            // Recover: temp comes back within LID_OPEN_RECOVER_PCT of setpoint
            if (currentTemp >= recoverThreshold) {
                _lidState = LidState::CLOSED;
#ifndef NATIVE_BUILD
                Serial.printf("[PID] Lid-open recovery. Temp=%.1f, threshold=%.1f\n",
                              currentTemp, recoverThreshold);
#endif
            }
            break;
    }
}
