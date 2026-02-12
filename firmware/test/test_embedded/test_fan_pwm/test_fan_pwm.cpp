/**
 * test_fan_pwm.cpp
 *
 * On-device integration tests for fan PWM output.
 * Verifies FanController initialization, speed settings, kick-start, and clamping.
 *
 * Prerequisites: Fan connected to GPIO12 via MOSFET.
 */

#include <unity.h>
#include <Arduino.h>

#include "../../src/config.h"
#include "../../src/fan_controller.h"
#include "../../src/fan_controller.cpp"

static FanController* fan;

void setUp(void) {
    fan = new FanController();
    fan->begin();
}

void tearDown(void) {
    fan->off();
    delete fan;
    fan = nullptr;
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

void test_fan_pwm_initializes(void) {
    // begin() should succeed without error.
    // After begin, fan should be off.
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, fan->getCurrentSpeedPct());
    TEST_ASSERT_EQUAL_UINT8(0, fan->getCurrentDuty());
}

void test_fan_off_outputs_zero_duty(void) {
    fan->setSpeed(0.0f);
    fan->update();
    delay(10);

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, fan->getCurrentSpeedPct());
    TEST_ASSERT_EQUAL_UINT8(0, fan->getCurrentDuty());
}

void test_fan_full_speed(void) {
    fan->setSpeed(100.0f);
    // Skip past kick-start phase
    delay(FAN_KICKSTART_MS + 100);
    fan->update();

    TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, fan->getCurrentSpeedPct());
    TEST_ASSERT_EQUAL_UINT8(255, fan->getCurrentDuty());
}

void test_fan_kickstart_activates(void) {
    // Fan was off, now request 50%
    fan->setSpeed(50.0f);
    fan->update();

    // Should be in kick-start mode (75% duty)
    TEST_ASSERT_TRUE_MESSAGE(fan->isKickStarting(),
        "Fan should be in kick-start phase after going from 0 to 50%");

    uint8_t kickDuty = (uint8_t)(FAN_KICKSTART_PCT * 255 / 100);
    TEST_ASSERT_UINT8_WITHIN(2, kickDuty, fan->getCurrentDuty());

    // Wait for kick-start to finish
    delay(FAN_KICKSTART_MS + 100);
    fan->update();

    TEST_ASSERT_FALSE_MESSAGE(fan->isKickStarting(),
        "Kick-start should have ended");
}

void test_fan_min_speed_clamp(void) {
    // Request speed below minimum (but > 0)
    fan->setSpeed(5.0f);  // Below FAN_MIN_SPEED (15%)

    // Wait past kick-start
    delay(FAN_KICKSTART_MS + 100);
    fan->update();

    // Should be clamped to minimum speed
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE((float)FAN_MIN_SPEED,
        fan->getCurrentSpeedPct(),
        "Fan speed should be clamped to FAN_MIN_SPEED");
}

void test_fan_pwm_frequency(void) {
    // Verify the PWM channel is configured at the expected frequency.
    // On ESP32, ledcReadFreq returns the actual frequency of the channel.
    fan->setSpeed(50.0f);
    delay(FAN_KICKSTART_MS + 100);
    fan->update();

    uint32_t freq = ledcReadFreq(FAN_PWM_CHANNEL);

    // Allow 10% tolerance on PWM frequency
    TEST_ASSERT_UINT32_WITHIN_MESSAGE(2500, FAN_PWM_FREQ, freq,
        "PWM frequency not within 10% of target 25kHz");
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------

int main(int argc, char** argv) {
    delay(2000);  // Wait for serial port stabilization

    UNITY_BEGIN();

    RUN_TEST(test_fan_pwm_initializes);
    RUN_TEST(test_fan_off_outputs_zero_duty);
    RUN_TEST(test_fan_full_speed);
    RUN_TEST(test_fan_kickstart_activates);
    RUN_TEST(test_fan_min_speed_clamp);
    RUN_TEST(test_fan_pwm_frequency);

    return UNITY_END();
}
