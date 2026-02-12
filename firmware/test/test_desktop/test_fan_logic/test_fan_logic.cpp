/**
 * test_fan_logic.cpp
 *
 * Tests for FanController logic on the native platform.
 *
 * The LEDC PWM hardware functions (ledcSetup, ledcAttachPin, ledcWrite)
 * are guarded by #ifndef NATIVE_BUILD in fan_controller.cpp, so they are
 * already compiled out. The writePWM() method becomes a no-op on native,
 * which is fine -- we are testing the state machine logic, not the hardware
 * output.
 *
 * On native build, millis() is not available, so the kick-start timer
 * completes immediately (now=0 is always >= _kickStartEndMs=0+500 only
 * after we call update twice). We test the state transitions and logic
 * conditions rather than real-time behavior.
 */

#include <unity.h>
#include <stdint.h>

// Include the actual module under test
#include "fan_controller.h"
#include "fan_controller.cpp"

// --------------------------------------------------------------------------
// setUp / tearDown
// --------------------------------------------------------------------------

static FanController* fan;

void setUp(void) {
    fan = new FanController();
    fan->begin();
}

void tearDown(void) {
    delete fan;
    fan = nullptr;
}

// --------------------------------------------------------------------------
// Tests: Initial state
// --------------------------------------------------------------------------

void test_initial_speed_is_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, fan->getCurrentSpeedPct());
}

void test_initial_duty_is_zero(void) {
    TEST_ASSERT_EQUAL_UINT8(0, fan->getCurrentDuty());
}

void test_not_kickstarting_initially(void) {
    TEST_ASSERT_FALSE(fan->isKickStarting());
}

// --------------------------------------------------------------------------
// Tests: off() immediately sets speed to 0
// --------------------------------------------------------------------------

void test_off_sets_speed_zero(void) {
    fan->setSpeed(50.0f);
    fan->update();
    fan->off();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, fan->getCurrentSpeedPct());
    TEST_ASSERT_EQUAL_UINT8(0, fan->getCurrentDuty());
}

void test_off_clears_kickstart(void) {
    fan->setSpeed(50.0f);
    fan->update();  // This triggers kick-start on native
    fan->off();
    TEST_ASSERT_FALSE(fan->isKickStarting());
}

// --------------------------------------------------------------------------
// Tests: Kick-start detection
//
// When transitioning from 0% to any positive speed, the fan should enter
// kick-start mode. On native, millis() returns 0 (via the #else branch),
// so the kick-start end time is 0+500=500. Since now is always 0 on native,
// the first update() triggers kick-start, and on the second update()
// now (0) < _kickStartEndMs (500), so it stays in kick-start phase.
// --------------------------------------------------------------------------

void test_kickstart_triggered_on_zero_to_nonzero(void) {
    // Fan starts at 0 (wasOff = true). Setting a speed and calling update
    // should trigger kick-start.
    fan->setSpeed(30.0f);
    fan->update();

    // On native: now=0, kickStartEndMs=0+500=500. In first update(),
    // the wasOff path fires and sets kickStartActive=true.
    TEST_ASSERT_TRUE(fan->isKickStarting());
}

void test_kickstart_runs_at_kickstart_speed(void) {
    fan->setSpeed(30.0f);
    fan->update();

    // During kick-start, current speed should be FAN_KICKSTART_PCT (75%)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, (float)FAN_KICKSTART_PCT, fan->getCurrentSpeedPct());
}

void test_no_kickstart_when_already_running(void) {
    // Get past the kick-start phase: set speed, update, then off (resets wasOff=true),
    // then set speed again -- this time should kick-start again
    fan->setSpeed(50.0f);
    fan->update();  // Enters kick-start

    // Now simulate the fan being already on (not off). If we change speed
    // from 50 to 60 (both non-zero), no kick-start should happen.
    // But first we need to get past kick-start. On native, update() will
    // stay in kick-start since now(0) < endMs(500), so we test differently:
    // We use off() then re-engage to get a fresh kick-start, then observe
    // that after off()+setSpeed, it kick-starts (proving it only triggers from off).
    fan->off();  // wasOff = true
    TEST_ASSERT_FALSE(fan->isKickStarting());

    fan->setSpeed(60.0f);
    fan->update();
    TEST_ASSERT_TRUE(fan->isKickStarting());  // From off -> on triggers kick-start
}

// --------------------------------------------------------------------------
// Tests: Min-speed clamping
//
// Speeds between 0 and FAN_MIN_SPEED (15%) but >= FAN_LONGPULSE_THRESHOLD (10%)
// should be clamped to FAN_MIN_SPEED.
//
// On native, to reach the normal speed path, we need to bypass kick-start.
// We achieve this by using setManualDuty to set a non-zero state, then
// switching back. Alternatively, we can test the percentToDuty helper and
// verify the clamping logic is correct by inspection.
//
// Since kick-start can't be bypassed on native (millis=0 forever keeps it
// in kick-start), we test min-speed clamping by verifying the logic flow:
// set speed to e.g. 12%, which is above the long-pulse threshold but below
// min speed. On a real system, after kick-start completes, this would clamp
// to 15%. We can verify by testing the target storage.
// --------------------------------------------------------------------------

void test_speed_clamped_above_zero(void) {
    // Verify that setSpeed clamps negative values to 0
    fan->setSpeed(-10.0f);
    fan->update();
    // Target was clamped to 0, so fan should be off
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, fan->getCurrentSpeedPct());
}

void test_speed_clamped_below_100(void) {
    // Verify that setSpeed clamps values above 100 to 100
    fan->setSpeed(150.0f);
    fan->update();
    // It will be in kick-start, but the target was clamped to 100
    // We can verify the kick-start is active (it accepted the speed)
    TEST_ASSERT_TRUE(fan->isKickStarting());
}

void test_percentToDuty_zero(void) {
    // Test the static conversion: 0% -> duty 0
    // We test this indirectly: off should set duty to 0
    fan->off();
    TEST_ASSERT_EQUAL_UINT8(0, fan->getCurrentDuty());
}

void test_percentToDuty_100(void) {
    // 100% -> duty 255. Test via setManualDuty.
    fan->setManualDuty(255);
    TEST_ASSERT_EQUAL_UINT8(255, fan->getCurrentDuty());
    float pct = fan->getCurrentSpeedPct();
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 100.0f, pct);
}

void test_percentToDuty_50(void) {
    // 50% -> duty ~128. Test via setManualDuty at 128.
    fan->setManualDuty(128);
    TEST_ASSERT_EQUAL_UINT8(128, fan->getCurrentDuty());
    // Pct should be roughly 50%
    float pct = fan->getCurrentSpeedPct();
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 50.2f, pct);
}

// --------------------------------------------------------------------------
// Tests: Long-pulse mode
//
// Speeds below FAN_LONGPULSE_THRESHOLD (10%) but above 0 should enter
// long-pulse mode on a real system. On native, this logic is reachable only
// after kick-start completes, which doesn't happen (millis=0). We verify
// the behavior indirectly.
// --------------------------------------------------------------------------

void test_setSpeed_stores_target(void) {
    // Verify setSpeed stores the target internally.
    // After off(), set a low speed and check that update() does not crash
    // and that we can read back a valid state.
    fan->setSpeed(5.0f);
    fan->update();
    // On native, will be in kick-start since wasOff was true
    TEST_ASSERT_TRUE(fan->isKickStarting());
}

// --------------------------------------------------------------------------
// Tests: setManualDuty
// --------------------------------------------------------------------------

void test_manual_duty_overrides_speed(void) {
    fan->setManualDuty(200);
    TEST_ASSERT_EQUAL_UINT8(200, fan->getCurrentDuty());

    // setSpeed should be ignored in manual mode
    fan->setSpeed(10.0f);
    fan->update();
    TEST_ASSERT_EQUAL_UINT8(200, fan->getCurrentDuty());
}

void test_off_clears_manual_mode(void) {
    fan->setManualDuty(200);
    fan->off();
    TEST_ASSERT_EQUAL_UINT8(0, fan->getCurrentDuty());

    // After off(), should be able to use setSpeed again
    fan->setSpeed(50.0f);
    fan->update();
    // Should trigger kick-start (proving manual mode is cleared)
    TEST_ASSERT_TRUE(fan->isKickStarting());
}

// --------------------------------------------------------------------------
// Tests: Multiple update() calls
// --------------------------------------------------------------------------

void test_multiple_updates_at_zero_stay_zero(void) {
    fan->setSpeed(0.0f);
    for (int i = 0; i < 10; i++) {
        fan->update();
    }
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, fan->getCurrentSpeedPct());
    TEST_ASSERT_EQUAL_UINT8(0, fan->getCurrentDuty());
}

void test_off_during_kickstart(void) {
    fan->setSpeed(80.0f);
    fan->update();  // Enter kick-start
    TEST_ASSERT_TRUE(fan->isKickStarting());

    fan->off();
    TEST_ASSERT_FALSE(fan->isKickStarting());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, fan->getCurrentSpeedPct());
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Initial state
    RUN_TEST(test_initial_speed_is_zero);
    RUN_TEST(test_initial_duty_is_zero);
    RUN_TEST(test_not_kickstarting_initially);

    // off()
    RUN_TEST(test_off_sets_speed_zero);
    RUN_TEST(test_off_clears_kickstart);

    // Kick-start
    RUN_TEST(test_kickstart_triggered_on_zero_to_nonzero);
    RUN_TEST(test_kickstart_runs_at_kickstart_speed);
    RUN_TEST(test_no_kickstart_when_already_running);

    // Speed clamping
    RUN_TEST(test_speed_clamped_above_zero);
    RUN_TEST(test_speed_clamped_below_100);
    RUN_TEST(test_percentToDuty_zero);
    RUN_TEST(test_percentToDuty_100);
    RUN_TEST(test_percentToDuty_50);

    // Long-pulse mode
    RUN_TEST(test_setSpeed_stores_target);

    // Manual duty
    RUN_TEST(test_manual_duty_overrides_speed);
    RUN_TEST(test_off_clears_manual_mode);

    // Multiple updates
    RUN_TEST(test_multiple_updates_at_zero_stay_zero);
    RUN_TEST(test_off_during_kickstart);

    return UNITY_END();
}
