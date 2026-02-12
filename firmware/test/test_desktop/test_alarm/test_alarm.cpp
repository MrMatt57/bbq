/**
 * test_alarm.cpp
 *
 * Tests for AlarmManager logic on the native platform.
 *
 * The buzzer hardware (tone/noTone, pinMode, digitalWrite) is guarded by
 * #ifndef NATIVE_BUILD in alarm_manager.cpp, so no stubs are needed.
 * The setBuzzer() method simply updates the _buzzerOn flag on native.
 * The updateBuzzer() timing is also compiled out on native.
 *
 * We can fully test the alarm logic: pit alarm activation, meat alarm
 * triggering, hysteresis, acknowledge behavior, and enable/disable.
 */

#include <unity.h>
#include <stdint.h>

// Include the actual module under test
#include "alarm_manager.h"
#include "alarm_manager.cpp"

// --------------------------------------------------------------------------
// Helper: check if a specific alarm type is in the active list
// --------------------------------------------------------------------------

static bool hasAlarm(AlarmManager& mgr, AlarmType type) {
    AlarmType alarms[MAX_ACTIVE_ALARMS];
    uint8_t count = mgr.getActiveAlarms(alarms, MAX_ACTIVE_ALARMS);
    for (uint8_t i = 0; i < count; i++) {
        if (alarms[i] == type) return true;
    }
    return false;
}

// --------------------------------------------------------------------------
// setUp / tearDown
// --------------------------------------------------------------------------

static AlarmManager* alarm;

void setUp(void) {
    alarm = new AlarmManager();
    alarm->begin();
}

void tearDown(void) {
    delete alarm;
    alarm = nullptr;
}

// --------------------------------------------------------------------------
// Tests: Initial state
// --------------------------------------------------------------------------

void test_initial_no_alarms(void) {
    TEST_ASSERT_FALSE(alarm->isAlarming());
}

void test_initial_enabled(void) {
    TEST_ASSERT_TRUE(alarm->isEnabled());
}

void test_initial_pit_band_default(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, ALARM_PIT_BAND_DEFAULT, alarm->getPitBand());
}

void test_initial_meat_targets_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, alarm->getMeat1Target());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, alarm->getMeat2Target());
}

// --------------------------------------------------------------------------
// Tests: Pit alarm only activates after pit first reaches setpoint
// --------------------------------------------------------------------------

void test_pit_alarm_not_active_during_rampup(void) {
    float setpoint = 250.0f;

    // During ramp-up, pitReached is false. Even if temp is out of band,
    // pit alarm should not trigger.
    alarm->update(200.0f, 0.0f, 0.0f, setpoint, false);
    TEST_ASSERT_FALSE(alarm->isAlarming());
    TEST_ASSERT_FALSE(hasAlarm(*alarm, AlarmType::PIT_HIGH));
    TEST_ASSERT_FALSE(hasAlarm(*alarm, AlarmType::PIT_LOW));
}

void test_pit_alarm_not_active_when_temp_way_above_during_rampup(void) {
    float setpoint = 250.0f;
    // Even with pitTemp above setpoint+band, should not alarm if pitReached=false
    alarm->update(300.0f, 0.0f, 0.0f, setpoint, false);
    TEST_ASSERT_FALSE(alarm->isAlarming());
}

// --------------------------------------------------------------------------
// Tests: Pit alarm triggers on deviation beyond band
// --------------------------------------------------------------------------

void test_pit_alarm_high_triggers(void) {
    float setpoint = 250.0f;
    float pitBand = ALARM_PIT_BAND_DEFAULT;  // 15F

    // Pit has reached setpoint (pitReached=true), temp is above band
    alarm->update(setpoint + pitBand + 1.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_TRUE(alarm->isAlarming());
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::PIT_HIGH));
}

void test_pit_alarm_low_triggers(void) {
    float setpoint = 250.0f;
    float pitBand = ALARM_PIT_BAND_DEFAULT;  // 15F

    // Pit has reached setpoint, temp dropped below band
    alarm->update(setpoint - pitBand - 1.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_TRUE(alarm->isAlarming());
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::PIT_LOW));
}

void test_pit_alarm_within_band_no_trigger(void) {
    float setpoint = 250.0f;
    float pitBand = ALARM_PIT_BAND_DEFAULT;  // 15F

    // Temp is within band, should not alarm
    alarm->update(setpoint + pitBand - 1.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_FALSE(alarm->isAlarming());

    alarm->update(setpoint - pitBand + 1.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_FALSE(alarm->isAlarming());
}

void test_pit_alarm_exactly_at_band_no_trigger(void) {
    float setpoint = 250.0f;
    float pitBand = ALARM_PIT_BAND_DEFAULT;  // 15F

    // Exactly at the band edge: setpoint + pitBand = 265. The condition
    // is pitTemp > setpoint + pitBand, so exactly at the edge should NOT trigger.
    alarm->update(setpoint + pitBand, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_FALSE(alarm->isAlarming());
}

// --------------------------------------------------------------------------
// Tests: Pit alarm clears when temp returns to band
// --------------------------------------------------------------------------

void test_pit_alarm_clears_when_back_in_band(void) {
    float setpoint = 250.0f;

    // Trigger pit high alarm
    alarm->update(270.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_TRUE(alarm->isAlarming());

    // Return to within band
    alarm->update(255.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_FALSE(alarm->isAlarming());
    TEST_ASSERT_FALSE(hasAlarm(*alarm, AlarmType::PIT_HIGH));
}

void test_pit_alarm_can_retrigger_after_recovery(void) {
    float setpoint = 250.0f;

    // Trigger pit high
    alarm->update(270.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::PIT_HIGH));

    // Return to band (clears _pitTriggered and removes alarm)
    alarm->update(255.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_FALSE(hasAlarm(*alarm, AlarmType::PIT_HIGH));

    // Go out of band again: should re-trigger
    alarm->update(270.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::PIT_HIGH));
}

// --------------------------------------------------------------------------
// Tests: Custom pit band
// --------------------------------------------------------------------------

void test_custom_pit_band(void) {
    alarm->setPitBand(5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, alarm->getPitBand());

    float setpoint = 250.0f;
    // With 5F band, 256F should trigger (> 255)
    alarm->update(256.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_TRUE(alarm->isAlarming());
}

void test_setPitBand_rejects_zero(void) {
    float original = alarm->getPitBand();
    alarm->setPitBand(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, original, alarm->getPitBand());
}

void test_setPitBand_rejects_negative(void) {
    float original = alarm->getPitBand();
    alarm->setPitBand(-5.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, original, alarm->getPitBand());
}

// --------------------------------------------------------------------------
// Tests: Meat alarm triggers when temp >= target
// --------------------------------------------------------------------------

void test_meat1_alarm_triggers_at_target(void) {
    alarm->setMeat1Target(200.0f);
    alarm->update(250.0f, 200.0f, 0.0f, 250.0f, true);
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::MEAT1_DONE));
    TEST_ASSERT_TRUE(alarm->isAlarming());
}

void test_meat1_alarm_triggers_above_target(void) {
    alarm->setMeat1Target(200.0f);
    alarm->update(250.0f, 205.0f, 0.0f, 250.0f, true);
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::MEAT1_DONE));
}

void test_meat1_alarm_not_triggered_below_target(void) {
    alarm->setMeat1Target(200.0f);
    alarm->update(250.0f, 195.0f, 0.0f, 250.0f, true);
    TEST_ASSERT_FALSE(hasAlarm(*alarm, AlarmType::MEAT1_DONE));
}

void test_meat2_alarm_triggers_at_target(void) {
    alarm->setMeat2Target(165.0f);
    alarm->update(250.0f, 0.0f, 165.0f, 250.0f, true);
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::MEAT2_DONE));
}

void test_meat2_alarm_not_triggered_below_target(void) {
    alarm->setMeat2Target(165.0f);
    alarm->update(250.0f, 0.0f, 160.0f, 250.0f, true);
    TEST_ASSERT_FALSE(hasAlarm(*alarm, AlarmType::MEAT2_DONE));
}

void test_meat_alarm_no_trigger_when_target_is_zero(void) {
    // Target 0 means not set; should not trigger
    alarm->update(250.0f, 300.0f, 300.0f, 250.0f, true);
    TEST_ASSERT_FALSE(hasAlarm(*alarm, AlarmType::MEAT1_DONE));
    TEST_ASSERT_FALSE(hasAlarm(*alarm, AlarmType::MEAT2_DONE));
}

void test_meat_alarm_no_trigger_when_temp_is_zero(void) {
    // Temp 0 means probe disconnected; should not trigger
    alarm->setMeat1Target(200.0f);
    alarm->update(250.0f, 0.0f, 0.0f, 250.0f, true);
    TEST_ASSERT_FALSE(hasAlarm(*alarm, AlarmType::MEAT1_DONE));
}

// --------------------------------------------------------------------------
// Tests: Meat alarm hysteresis (doesn't re-trigger after acknowledge)
// --------------------------------------------------------------------------

void test_meat1_no_retrigger_after_acknowledge(void) {
    alarm->setMeat1Target(200.0f);

    // Trigger
    alarm->update(250.0f, 200.0f, 0.0f, 250.0f, true);
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::MEAT1_DONE));

    // Acknowledge
    alarm->acknowledge();
    TEST_ASSERT_FALSE(alarm->isAlarming());

    // Same conditions: should NOT re-trigger because _meat1Triggered is true
    alarm->update(250.0f, 205.0f, 0.0f, 250.0f, true);
    TEST_ASSERT_FALSE(hasAlarm(*alarm, AlarmType::MEAT1_DONE));
    TEST_ASSERT_FALSE(alarm->isAlarming());
}

void test_meat2_no_retrigger_after_acknowledge(void) {
    alarm->setMeat2Target(165.0f);

    // Trigger
    alarm->update(250.0f, 0.0f, 170.0f, 250.0f, true);
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::MEAT2_DONE));

    // Acknowledge
    alarm->acknowledge();

    // Should not re-trigger
    alarm->update(250.0f, 0.0f, 175.0f, 250.0f, true);
    TEST_ASSERT_FALSE(hasAlarm(*alarm, AlarmType::MEAT2_DONE));
}

void test_meat_alarm_retriggers_after_new_target_set(void) {
    alarm->setMeat1Target(200.0f);

    // Trigger and acknowledge
    alarm->update(250.0f, 200.0f, 0.0f, 250.0f, true);
    alarm->acknowledge();

    // Set a new target -- this should reset _meat1Triggered
    alarm->setMeat1Target(210.0f);

    // Now reaching the new target should trigger again
    alarm->update(250.0f, 210.0f, 0.0f, 250.0f, true);
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::MEAT1_DONE));
}

// --------------------------------------------------------------------------
// Tests: Acknowledge silences alarms
// --------------------------------------------------------------------------

void test_acknowledge_silences(void) {
    alarm->setMeat1Target(200.0f);
    alarm->update(250.0f, 200.0f, 0.0f, 250.0f, true);
    TEST_ASSERT_TRUE(alarm->isAlarming());

    alarm->acknowledge();
    TEST_ASSERT_FALSE(alarm->isAlarming());
}

void test_acknowledge_clears_active_alarms(void) {
    alarm->setMeat1Target(200.0f);
    alarm->setMeat2Target(165.0f);

    alarm->update(250.0f, 200.0f, 170.0f, 250.0f, true);
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::MEAT1_DONE));
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::MEAT2_DONE));

    alarm->acknowledge();

    AlarmType alarms[MAX_ACTIVE_ALARMS];
    uint8_t count = alarm->getActiveAlarms(alarms, MAX_ACTIVE_ALARMS);
    TEST_ASSERT_EQUAL_UINT8(0, count);
}

// --------------------------------------------------------------------------
// Tests: Pit alarm acknowledge + hysteresis
// --------------------------------------------------------------------------

void test_pit_alarm_no_retrigger_after_acknowledge_while_still_out(void) {
    float setpoint = 250.0f;

    // Trigger pit high
    alarm->update(270.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::PIT_HIGH));

    // Acknowledge
    alarm->acknowledge();
    TEST_ASSERT_FALSE(alarm->isAlarming());

    // Still out of band -- should NOT re-trigger because _pitTriggered is true
    alarm->update(270.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_FALSE(alarm->isAlarming());
}

void test_pit_alarm_retriggers_after_return_and_deviate_again(void) {
    float setpoint = 250.0f;

    // Trigger pit high
    alarm->update(270.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::PIT_HIGH));

    // Acknowledge
    alarm->acknowledge();

    // Return to band -- this clears _pitTriggered
    alarm->update(255.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_FALSE(alarm->isAlarming());

    // Go out of band again -- should re-trigger
    alarm->update(270.0f, 0.0f, 0.0f, setpoint, true);
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::PIT_HIGH));
    TEST_ASSERT_TRUE(alarm->isAlarming());
}

// --------------------------------------------------------------------------
// Tests: Enable/disable
// --------------------------------------------------------------------------

void test_disabled_no_alarms(void) {
    alarm->setEnabled(false);
    alarm->setMeat1Target(200.0f);
    alarm->update(250.0f, 200.0f, 0.0f, 250.0f, true);
    TEST_ASSERT_FALSE(alarm->isAlarming());
}

void test_reenable_allows_alarms(void) {
    alarm->setEnabled(false);
    alarm->setEnabled(true);
    TEST_ASSERT_TRUE(alarm->isEnabled());

    alarm->setMeat1Target(200.0f);
    alarm->update(250.0f, 200.0f, 0.0f, 250.0f, true);
    TEST_ASSERT_TRUE(alarm->isAlarming());
}

// --------------------------------------------------------------------------
// Tests: Multiple simultaneous alarms
// --------------------------------------------------------------------------

void test_multiple_alarms_simultaneously(void) {
    alarm->setMeat1Target(200.0f);
    alarm->setMeat2Target(165.0f);

    // Trigger both meat alarms and a pit alarm
    alarm->update(270.0f, 200.0f, 170.0f, 250.0f, true);

    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::PIT_HIGH));
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::MEAT1_DONE));
    TEST_ASSERT_TRUE(hasAlarm(*alarm, AlarmType::MEAT2_DONE));

    AlarmType alarms[MAX_ACTIVE_ALARMS];
    uint8_t count = alarm->getActiveAlarms(alarms, MAX_ACTIVE_ALARMS);
    TEST_ASSERT_EQUAL_UINT8(3, count);
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Initial state
    RUN_TEST(test_initial_no_alarms);
    RUN_TEST(test_initial_enabled);
    RUN_TEST(test_initial_pit_band_default);
    RUN_TEST(test_initial_meat_targets_zero);

    // Pit alarm: ramp-up
    RUN_TEST(test_pit_alarm_not_active_during_rampup);
    RUN_TEST(test_pit_alarm_not_active_when_temp_way_above_during_rampup);

    // Pit alarm: deviation
    RUN_TEST(test_pit_alarm_high_triggers);
    RUN_TEST(test_pit_alarm_low_triggers);
    RUN_TEST(test_pit_alarm_within_band_no_trigger);
    RUN_TEST(test_pit_alarm_exactly_at_band_no_trigger);

    // Pit alarm: recovery
    RUN_TEST(test_pit_alarm_clears_when_back_in_band);
    RUN_TEST(test_pit_alarm_can_retrigger_after_recovery);

    // Custom pit band
    RUN_TEST(test_custom_pit_band);
    RUN_TEST(test_setPitBand_rejects_zero);
    RUN_TEST(test_setPitBand_rejects_negative);

    // Meat alarms
    RUN_TEST(test_meat1_alarm_triggers_at_target);
    RUN_TEST(test_meat1_alarm_triggers_above_target);
    RUN_TEST(test_meat1_alarm_not_triggered_below_target);
    RUN_TEST(test_meat2_alarm_triggers_at_target);
    RUN_TEST(test_meat2_alarm_not_triggered_below_target);
    RUN_TEST(test_meat_alarm_no_trigger_when_target_is_zero);
    RUN_TEST(test_meat_alarm_no_trigger_when_temp_is_zero);

    // Meat alarm hysteresis
    RUN_TEST(test_meat1_no_retrigger_after_acknowledge);
    RUN_TEST(test_meat2_no_retrigger_after_acknowledge);
    RUN_TEST(test_meat_alarm_retriggers_after_new_target_set);

    // Acknowledge
    RUN_TEST(test_acknowledge_silences);
    RUN_TEST(test_acknowledge_clears_active_alarms);

    // Pit alarm + acknowledge hysteresis
    RUN_TEST(test_pit_alarm_no_retrigger_after_acknowledge_while_still_out);
    RUN_TEST(test_pit_alarm_retriggers_after_return_and_deviate_again);

    // Enable/disable
    RUN_TEST(test_disabled_no_alarms);
    RUN_TEST(test_reenable_allows_alarms);

    // Multiple simultaneous
    RUN_TEST(test_multiple_alarms_simultaneously);

    return UNITY_END();
}
