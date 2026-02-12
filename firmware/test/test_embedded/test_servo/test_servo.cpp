/**
 * test_servo.cpp
 *
 * On-device integration tests for the damper servo.
 * Verifies ServoController initialization, positioning, and sweep.
 *
 * Prerequisites: MG90S servo connected to GPIO13.
 */

#include <unity.h>
#include <Arduino.h>

#include "../../src/config.h"
#include "../../src/servo_controller.h"
#include "../../src/servo_controller.cpp"

static ServoController* servo;

void setUp(void) {
    servo = new ServoController();
    servo->begin();
}

void tearDown(void) {
    servo->detach();
    delete servo;
    servo = nullptr;
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

void test_servo_initializes(void) {
    // After begin(), servo should be at closed position (0%)
    TEST_ASSERT_EQUAL_UINT8(DAMPER_CLOSED, servo->getCurrentAngle());
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, servo->getCurrentPositionPct());
}

void test_servo_closed_position(void) {
    servo->setPosition(0.0f);
    delay(500);  // Allow servo to physically move

    TEST_ASSERT_EQUAL_UINT8(DAMPER_CLOSED, servo->getCurrentAngle());
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, servo->getCurrentPositionPct());
}

void test_servo_open_position(void) {
    servo->setPosition(100.0f);
    delay(500);

    TEST_ASSERT_EQUAL_UINT8(DAMPER_OPEN, servo->getCurrentAngle());
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, servo->getCurrentPositionPct());
}

void test_servo_mid_position(void) {
    servo->setPosition(50.0f);
    delay(500);

    uint8_t expectedAngle = (DAMPER_CLOSED + DAMPER_OPEN) / 2;  // 45 degrees
    TEST_ASSERT_UINT8_WITHIN(2, expectedAngle, servo->getCurrentAngle());
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 50.0f, servo->getCurrentPositionPct());
}

void test_servo_sweep(void) {
    // Sweep from 0 to 100 and verify angles are monotonically increasing
    uint8_t prevAngle = 0;

    for (int pct = 0; pct <= 100; pct += 10) {
        servo->setPosition((float)pct);
        delay(100);

        uint8_t angle = servo->getCurrentAngle();
        TEST_ASSERT_GREATER_OR_EQUAL_UINT8_MESSAGE(prevAngle, angle,
            "Servo angle should increase monotonically during sweep up");
        prevAngle = angle;
    }

    // Sweep back down
    prevAngle = servo->getCurrentAngle();
    for (int pct = 100; pct >= 0; pct -= 10) {
        servo->setPosition((float)pct);
        delay(100);

        uint8_t angle = servo->getCurrentAngle();
        TEST_ASSERT_LESS_OR_EQUAL_UINT8_MESSAGE(prevAngle, angle,
            "Servo angle should decrease monotonically during sweep down");
        prevAngle = angle;
    }
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------

int main(int argc, char** argv) {
    delay(2000);  // Wait for serial port stabilization

    UNITY_BEGIN();

    RUN_TEST(test_servo_initializes);
    RUN_TEST(test_servo_closed_position);
    RUN_TEST(test_servo_open_position);
    RUN_TEST(test_servo_mid_position);
    RUN_TEST(test_servo_sweep);

    return UNITY_END();
}
