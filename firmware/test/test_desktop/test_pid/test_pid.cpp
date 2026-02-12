/**
 * test_pid.cpp
 *
 * Tests for PidController logic on the native platform.
 *
 * On native build, QuickPID is not available (guarded by #ifndef NATIVE_BUILD).
 * The PID compute() path that calls QuickPID::Compute() is compiled out, so
 * _pidOutput stays at 0 on native. However, we CAN test:
 *   - Lid-open detection state machine (updateLidState is fully compiled)
 *   - Enabled/disabled logic
 *   - Tuning parameter storage
 *   - Output clamping behavior when disabled
 *   - Constructor defaults
 */

#include <unity.h>
#include <stdint.h>

// Include the actual module under test
#include "pid_controller.h"
#include "pid_controller.cpp"

// --------------------------------------------------------------------------
// setUp / tearDown
// --------------------------------------------------------------------------

static PidController* pid;

void setUp(void) {
    pid = new PidController();
    pid->begin();
}

void tearDown(void) {
    delete pid;
    pid = nullptr;
}

// --------------------------------------------------------------------------
// Tests: Constructor defaults and begin()
// --------------------------------------------------------------------------

void test_default_tunings(void) {
    // After begin(), tunings should be the defaults from config.h
    TEST_ASSERT_FLOAT_WITHIN(0.001f, PID_KP, pid->getKp());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, PID_KI, pid->getKi());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, PID_KD, pid->getKd());
}

void test_initially_enabled(void) {
    TEST_ASSERT_TRUE(pid->isEnabled());
}

void test_initially_lid_closed(void) {
    TEST_ASSERT_FALSE(pid->isLidOpen());
}

void test_initial_output_is_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid->getOutput());
}

// --------------------------------------------------------------------------
// Tests: Disabled state
// --------------------------------------------------------------------------

void test_disabled_output_is_zero(void) {
    pid->setEnabled(false);
    float output = pid->compute(200.0f, 250.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid->getOutput());
}

void test_disable_clears_output(void) {
    pid->setEnabled(false);
    TEST_ASSERT_FALSE(pid->isEnabled());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid->getOutput());
}

void test_enable_after_disable(void) {
    pid->setEnabled(false);
    TEST_ASSERT_FALSE(pid->isEnabled());
    pid->setEnabled(true);
    TEST_ASSERT_TRUE(pid->isEnabled());
}

// --------------------------------------------------------------------------
// Tests: setTunings
// --------------------------------------------------------------------------

void test_setTunings_updates_gains(void) {
    pid->setTunings(10.0f, 0.5f, 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, pid->getKp());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, pid->getKi());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, pid->getKd());
}

void test_setTunings_zero_gains(void) {
    pid->setTunings(0.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid->getKp());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid->getKi());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid->getKd());
}

void test_begin_with_custom_tunings(void) {
    pid->begin(8.0f, 0.1f, 3.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.0f, pid->getKp());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, pid->getKi());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, pid->getKd());
}

// --------------------------------------------------------------------------
// Tests: Lid-open detection
//
// Lid-open triggers when temp drops more than LID_OPEN_DROP_PCT (6%) below
// setpoint. Recovery when temp comes back within LID_OPEN_RECOVER_PCT (2%)
// of setpoint.
//
// For setpoint 250F:
//   Drop threshold  = 250 * (1 - 0.06) = 235.0
//   Recover threshold = 250 * (1 - 0.02) = 245.0
// --------------------------------------------------------------------------

void test_lid_open_detection_temp_drop(void) {
    float setpoint = 250.0f;

    // Temperature at setpoint -- lid should stay closed
    pid->compute(250.0f, setpoint);
    TEST_ASSERT_FALSE(pid->isLidOpen());

    // Temperature drops slightly but not enough (still above 235)
    pid->compute(236.0f, setpoint);
    TEST_ASSERT_FALSE(pid->isLidOpen());

    // Temperature drops below 6% threshold (below 235)
    pid->compute(230.0f, setpoint);
    TEST_ASSERT_TRUE(pid->isLidOpen());
}

void test_lid_open_output_is_zero(void) {
    float setpoint = 250.0f;

    // Trigger lid-open
    pid->compute(230.0f, setpoint);
    TEST_ASSERT_TRUE(pid->isLidOpen());

    // Output should be 0 when lid is open
    float output = pid->compute(230.0f, setpoint);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output);
}

void test_lid_open_recovery(void) {
    float setpoint = 250.0f;

    // Trigger lid-open
    pid->compute(230.0f, setpoint);
    TEST_ASSERT_TRUE(pid->isLidOpen());

    // Temperature recovers but not enough (between 235 and 245)
    pid->compute(240.0f, setpoint);
    TEST_ASSERT_TRUE(pid->isLidOpen());  // Still open, need to reach 245

    // Temperature recovers to within 2% (at or above 245)
    pid->compute(245.0f, setpoint);
    TEST_ASSERT_FALSE(pid->isLidOpen());
}

void test_lid_open_recovery_exact_threshold(void) {
    float setpoint = 250.0f;
    float recoverThreshold = setpoint * (1.0f - LID_OPEN_RECOVER_PCT / 100.0f);  // 245.0

    // Trigger lid-open
    pid->compute(230.0f, setpoint);
    TEST_ASSERT_TRUE(pid->isLidOpen());

    // Exactly at recover threshold should recover (>= comparison in source)
    pid->compute(recoverThreshold, setpoint);
    TEST_ASSERT_FALSE(pid->isLidOpen());
}

void test_lid_open_no_detection_at_zero_setpoint(void) {
    // With setpoint <= 0, lid detection should not activate
    pid->compute(0.0f, 0.0f);
    TEST_ASSERT_FALSE(pid->isLidOpen());

    // Even a huge temperature (or zero) shouldn't trigger anything
    pid->compute(-50.0f, 0.0f);
    TEST_ASSERT_FALSE(pid->isLidOpen());
}

void test_lid_open_with_different_setpoint(void) {
    // Test with setpoint 400F
    // Drop threshold = 400 * 0.94 = 376
    // Recover threshold = 400 * 0.98 = 392
    float setpoint = 400.0f;

    pid->compute(375.0f, setpoint);
    TEST_ASSERT_TRUE(pid->isLidOpen());

    pid->compute(390.0f, setpoint);
    TEST_ASSERT_TRUE(pid->isLidOpen());  // Still below 392

    pid->compute(392.0f, setpoint);
    TEST_ASSERT_FALSE(pid->isLidOpen());
}

void test_lid_open_repeated_cycles(void) {
    float setpoint = 250.0f;

    // First lid-open cycle
    pid->compute(230.0f, setpoint);
    TEST_ASSERT_TRUE(pid->isLidOpen());
    pid->compute(246.0f, setpoint);
    TEST_ASSERT_FALSE(pid->isLidOpen());

    // Second lid-open cycle
    pid->compute(230.0f, setpoint);
    TEST_ASSERT_TRUE(pid->isLidOpen());
    pid->compute(248.0f, setpoint);
    TEST_ASSERT_FALSE(pid->isLidOpen());
}

// --------------------------------------------------------------------------
// Tests: Compute returns zero on native (QuickPID not available)
// --------------------------------------------------------------------------

void test_compute_returns_zero_on_native(void) {
    // On native build, the QuickPID section is compiled out, so output stays 0
    float output = pid->compute(200.0f, 250.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, output);
}

// --------------------------------------------------------------------------
// Tests: begin() resets state
// --------------------------------------------------------------------------

void test_begin_resets_lid_state(void) {
    float setpoint = 250.0f;

    // Trigger lid-open
    pid->compute(230.0f, setpoint);
    TEST_ASSERT_TRUE(pid->isLidOpen());

    // Re-initialize
    pid->begin();
    TEST_ASSERT_FALSE(pid->isLidOpen());
}

void test_begin_resets_enabled(void) {
    pid->setEnabled(false);
    pid->begin();
    TEST_ASSERT_TRUE(pid->isEnabled());
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Constructor/begin defaults
    RUN_TEST(test_default_tunings);
    RUN_TEST(test_initially_enabled);
    RUN_TEST(test_initially_lid_closed);
    RUN_TEST(test_initial_output_is_zero);

    // Disabled state
    RUN_TEST(test_disabled_output_is_zero);
    RUN_TEST(test_disable_clears_output);
    RUN_TEST(test_enable_after_disable);

    // setTunings
    RUN_TEST(test_setTunings_updates_gains);
    RUN_TEST(test_setTunings_zero_gains);
    RUN_TEST(test_begin_with_custom_tunings);

    // Lid-open detection
    RUN_TEST(test_lid_open_detection_temp_drop);
    RUN_TEST(test_lid_open_output_is_zero);
    RUN_TEST(test_lid_open_recovery);
    RUN_TEST(test_lid_open_recovery_exact_threshold);
    RUN_TEST(test_lid_open_no_detection_at_zero_setpoint);
    RUN_TEST(test_lid_open_with_different_setpoint);
    RUN_TEST(test_lid_open_repeated_cycles);

    // Native-specific behavior
    RUN_TEST(test_compute_returns_zero_on_native);

    // begin() resets
    RUN_TEST(test_begin_resets_lid_state);
    RUN_TEST(test_begin_resets_enabled);

    return UNITY_END();
}
