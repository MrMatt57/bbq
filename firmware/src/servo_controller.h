#pragma once

#include "config.h"
#include <stdint.h>

#ifndef NATIVE_BUILD
#include <ESP32Servo.h>
#endif

class ServoController {
public:
    ServoController();

    // Attach servo to PIN_SERVO. Call once from setup().
    void begin();

    // Set damper position from 0-100% (0=closed, 100=open).
    // Maps to DAMPER_CLOSED..DAMPER_OPEN angle range.
    void setPosition(float percent);

    // Move servo to a specific angle in degrees. Useful for testing.
    void setAngle(uint8_t angleDeg);

    // Current servo angle in degrees
    uint8_t getCurrentAngle() const;

    // Current position as a percentage (0-100)
    float getCurrentPositionPct() const;

    // Detach the servo signal to avoid jitter when not actively moving
    void detach();

private:
    // Write microsecond pulse value to the servo
    void writeMicroseconds(uint16_t us);

    // Map angle to microseconds for precise control
    uint16_t angleToMicroseconds(float angle) const;

#ifndef NATIVE_BUILD
    Servo _servo;
#endif

    uint8_t _currentAngle;
    bool    _attached;
};
