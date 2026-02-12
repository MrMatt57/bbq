#include "servo_controller.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

ServoController::ServoController()
    : _currentAngle(DAMPER_CLOSED)
    , _attached(false)
{
}

void ServoController::begin() {
#ifndef NATIVE_BUILD
    _servo.setPeriodHertz(50);  // Standard 50Hz servo frequency
    _servo.attach(PIN_SERVO, SERVO_MIN_US, SERVO_MAX_US);
    _attached = true;

    // Start at closed position
    uint16_t us = angleToMicroseconds((float)DAMPER_CLOSED);
    _servo.writeMicroseconds(us);
    _currentAngle = DAMPER_CLOSED;

    Serial.printf("[SERVO] Attached to pin %d, range %d-%d us, closed=%d deg, open=%d deg\n",
                  PIN_SERVO, SERVO_MIN_US, SERVO_MAX_US, DAMPER_CLOSED, DAMPER_OPEN);
#endif
}

void ServoController::setPosition(float percent) {
    // Clamp to 0-100%
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    // Map 0-100% to DAMPER_CLOSED..DAMPER_OPEN angle range
    float angle = (float)DAMPER_CLOSED +
                  (percent / 100.0f) * (float)(DAMPER_OPEN - DAMPER_CLOSED);

    // Clamp angle to valid range
    if (angle < (float)DAMPER_CLOSED) angle = (float)DAMPER_CLOSED;
    if (angle > (float)DAMPER_OPEN) angle = (float)DAMPER_OPEN;

    _currentAngle = (uint8_t)(angle + 0.5f);

    uint16_t us = angleToMicroseconds(angle);
    writeMicroseconds(us);
}

void ServoController::setAngle(uint8_t angleDeg) {
    if (angleDeg > 180) angleDeg = 180;
    _currentAngle = angleDeg;

    uint16_t us = angleToMicroseconds((float)angleDeg);
    writeMicroseconds(us);
}

uint8_t ServoController::getCurrentAngle() const {
    return _currentAngle;
}

float ServoController::getCurrentPositionPct() const {
    if (DAMPER_OPEN == DAMPER_CLOSED) return 0.0f;
    float pct = (float)(_currentAngle - DAMPER_CLOSED) /
                (float)(DAMPER_OPEN - DAMPER_CLOSED) * 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return pct;
}

void ServoController::detach() {
#ifndef NATIVE_BUILD
    if (_attached) {
        _servo.detach();
        _attached = false;
        Serial.println("[SERVO] Detached.");
    }
#endif
}

void ServoController::writeMicroseconds(uint16_t us) {
#ifndef NATIVE_BUILD
    if (!_attached) {
        _servo.attach(PIN_SERVO, SERVO_MIN_US, SERVO_MAX_US);
        _attached = true;
    }
    _servo.writeMicroseconds(us);
#endif
}

uint16_t ServoController::angleToMicroseconds(float angle) const {
    // Linear interpolation from 0-180 degrees to SERVO_MIN_US-SERVO_MAX_US
    // This maps the full servo range. The damper only uses 0-90 degrees.
    if (angle < 0.0f) angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;

    float us = (float)SERVO_MIN_US +
               (angle / 180.0f) * (float)(SERVO_MAX_US - SERVO_MIN_US);
    return (uint16_t)(us + 0.5f);
}
