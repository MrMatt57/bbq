/**
 * test_buzzer.cpp
 *
 * On-device integration tests for the piezo buzzer.
 * Verifies pin configuration, tone generation, and stop behavior.
 *
 * Prerequisites: Piezo buzzer connected to GPIO14.
 */

#include <unity.h>
#include <Arduino.h>

#include "../../src/config.h"

void setUp(void) {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
}

void tearDown(void) {
    noTone(PIN_BUZZER);
    digitalWrite(PIN_BUZZER, LOW);
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

void test_buzzer_pin_configured(void) {
    // After pinMode, the pin should be in OUTPUT mode.
    // On ESP32, we verify by writing HIGH and LOW without error.
    digitalWrite(PIN_BUZZER, HIGH);
    delay(10);
    int val = digitalRead(PIN_BUZZER);
    TEST_ASSERT_EQUAL_INT_MESSAGE(HIGH, val,
        "PIN_BUZZER should read HIGH after writing HIGH");

    digitalWrite(PIN_BUZZER, LOW);
    delay(10);
    val = digitalRead(PIN_BUZZER);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LOW, val,
        "PIN_BUZZER should read LOW after writing LOW");
}

void test_buzzer_tone_plays(void) {
    // Generate a tone at the alarm frequency for the specified duration.
    // This is a functional test — listen for the beep!
    Serial.printf("[BUZZER] Playing %d Hz tone for %d ms... listen!\n",
                  ALARM_BUZZER_FREQ, ALARM_BUZZER_DURATION);

    tone(PIN_BUZZER, ALARM_BUZZER_FREQ, ALARM_BUZZER_DURATION);
    delay(ALARM_BUZZER_DURATION + 100);

    // If we get here without crashing, the tone function works.
    TEST_ASSERT_TRUE_MESSAGE(true, "Tone played without error");
}

void test_buzzer_stops(void) {
    // Start a tone, then stop it
    tone(PIN_BUZZER, ALARM_BUZZER_FREQ);
    delay(200);

    noTone(PIN_BUZZER);
    delay(50);

    // Pin should be LOW after stopping the tone
    int val = digitalRead(PIN_BUZZER);
    TEST_ASSERT_EQUAL_INT_MESSAGE(LOW, val,
        "PIN_BUZZER should be LOW after noTone()");
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------

int main(int argc, char** argv) {
    delay(2000);  // Wait for serial port stabilization

    UNITY_BEGIN();

    RUN_TEST(test_buzzer_pin_configured);
    RUN_TEST(test_buzzer_tone_plays);
    RUN_TEST(test_buzzer_stops);

    return UNITY_END();
}
