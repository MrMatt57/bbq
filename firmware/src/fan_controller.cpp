#include "fan_controller.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

FanController::FanController()
    : _targetPct(0.0f)
    , _currentPct(0.0f)
    , _currentDuty(0)
    , _kickStartActive(false)
    , _kickStartEndMs(0)
    , _kickStartTargetPct(0.0f)
    , _longPulseActive(false)
    , _longPulseCycleStartMs(0)
    , _wasOff(true)
    , _manualMode(false)
{
}

void FanController::begin() {
#ifndef NATIVE_BUILD
    // Configure LEDC for PWM output
    ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ, FAN_PWM_RESOLUTION);
    ledcAttachPin(PIN_FAN_PWM, FAN_PWM_CHANNEL);
    ledcWrite(FAN_PWM_CHANNEL, 0);

    Serial.printf("[FAN] PWM initialized: pin=%d, freq=%dHz, resolution=%d-bit\n",
                  PIN_FAN_PWM, FAN_PWM_FREQ, FAN_PWM_RESOLUTION);
#endif
    _currentDuty = 0;
    _currentPct = 0.0f;
    _wasOff = true;
}

void FanController::setSpeed(float percent) {
    if (_manualMode) return;

    // Clamp to valid range
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    _targetPct = percent;
}

void FanController::update() {
    if (_manualMode) return;

#ifndef NATIVE_BUILD
    unsigned long now = millis();
#else
    unsigned long now = 0;
#endif

    // --- Handle kick-start phase ---
    if (_kickStartActive) {
        if (now >= _kickStartEndMs) {
            // Kick-start done, apply the real target speed
            _kickStartActive = false;
            // Fall through to normal speed logic below
        } else {
            // Still in kick-start: keep at kick-start speed
            uint8_t duty = percentToDuty((float)FAN_KICKSTART_PCT);
            _currentPct = (float)FAN_KICKSTART_PCT;
            _currentDuty = duty;
            writePWM(duty);
            return;
        }
    }

    float effectivePct = _targetPct;

    // --- Off ---
    if (effectivePct <= 0.0f) {
        _wasOff = true;
        _longPulseActive = false;
        _currentPct = 0.0f;
        _currentDuty = 0;
        writePWM(0);
        return;
    }

    // --- Kick-start: transitioning from 0% to any speed ---
    if (_wasOff && effectivePct > 0.0f) {
        _wasOff = false;
        _kickStartActive = true;
        _kickStartEndMs = now + FAN_KICKSTART_MS;
        _kickStartTargetPct = effectivePct;

        uint8_t duty = percentToDuty((float)FAN_KICKSTART_PCT);
        _currentPct = (float)FAN_KICKSTART_PCT;
        _currentDuty = duty;
        writePWM(duty);
        return;
    }

    _wasOff = false;

    // --- Long-pulse mode: below FAN_LONGPULSE_THRESHOLD ---
    if (effectivePct > 0.0f && effectivePct < (float)FAN_LONGPULSE_THRESHOLD) {
        // Instead of constant low PWM, cycle between min speed and off
        // over a FAN_LONGPULSE_CYCLE_MS period.
        // The on-fraction is proportional to the target percentage / threshold.
        if (!_longPulseActive) {
            _longPulseActive = true;
            _longPulseCycleStartMs = now;
        }

        unsigned long elapsed = now - _longPulseCycleStartMs;
        unsigned long cycleMs = FAN_LONGPULSE_CYCLE_MS;
        unsigned long posInCycle = elapsed % cycleMs;

        // On-time fraction: target / threshold
        float onFraction = effectivePct / (float)FAN_LONGPULSE_THRESHOLD;
        unsigned long onTimeMs = (unsigned long)(onFraction * cycleMs);

        if (posInCycle < onTimeMs) {
            // ON phase: run at min speed
            uint8_t duty = percentToDuty((float)FAN_MIN_SPEED);
            _currentPct = (float)FAN_MIN_SPEED;
            _currentDuty = duty;
            writePWM(duty);
        } else {
            // OFF phase
            _currentPct = 0.0f;
            _currentDuty = 0;
            writePWM(0);
        }
        return;
    }

    // --- Normal speed: apply min-speed clamping ---
    _longPulseActive = false;

    if (effectivePct > 0.0f && effectivePct < (float)FAN_MIN_SPEED) {
        effectivePct = (float)FAN_MIN_SPEED;
    }

    uint8_t duty = percentToDuty(effectivePct);
    _currentPct = effectivePct;
    _currentDuty = duty;
    writePWM(duty);
}

void FanController::off() {
    _targetPct = 0.0f;
    _kickStartActive = false;
    _longPulseActive = false;
    _manualMode = false;
    _wasOff = true;
    _currentPct = 0.0f;
    _currentDuty = 0;
    writePWM(0);
}

float FanController::getCurrentSpeedPct() const {
    return _currentPct;
}

uint8_t FanController::getCurrentDuty() const {
    return _currentDuty;
}

bool FanController::isKickStarting() const {
    return _kickStartActive;
}

void FanController::setManualDuty(uint8_t duty) {
    _manualMode = true;
    _currentDuty = duty;
    _currentPct = (float)duty / 255.0f * 100.0f;
    writePWM(duty);
}

void FanController::writePWM(uint8_t duty) {
#ifndef NATIVE_BUILD
    ledcWrite(FAN_PWM_CHANNEL, duty);
#endif
}

uint8_t FanController::percentToDuty(float pct) {
    if (pct <= 0.0f) return 0;
    if (pct >= 100.0f) return 255;
    return (uint8_t)(pct * 255.0f / 100.0f + 0.5f);
}
