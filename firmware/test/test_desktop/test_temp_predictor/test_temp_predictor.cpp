/**
 * test_temp_predictor.cpp
 *
 * Tests for TempPredictor logic on the native platform.
 *
 * Tests cover:
 *   - Initialization defaults
 *   - Linear regression accuracy with known slopes
 *   - Edge cases (insufficient samples, decreasing temp, no target, etc.)
 *   - Rolling window behavior
 *   - Reset functionality
 *   - Two-probe independence
 */

#include <unity.h>
#include <stdint.h>

// Include the actual module under test
#include "temp_predictor.h"
#include "temp_predictor.cpp"

// --------------------------------------------------------------------------
// setUp / tearDown
// --------------------------------------------------------------------------

static TempPredictor* predictor;

void setUp(void) {
    predictor = new TempPredictor();
    predictor->begin();
}

void tearDown(void) {
    delete predictor;
    predictor = nullptr;
}

// --------------------------------------------------------------------------
// Helper: feed N samples with a linear temperature rise
// --------------------------------------------------------------------------
static void feedLinearRise(uint8_t probe, uint32_t startTime, float startTemp,
                           float degreesPerSample, uint16_t numSamples) {
    for (uint16_t i = 0; i < numSamples; i++) {
        uint32_t ts = startTime + i * 5;  // 5-second intervals
        float temp = startTemp + i * degreesPerSample;
        predictor->addSample(probe, ts, temp);
    }
}

// --------------------------------------------------------------------------
// Tests: Initialization
// --------------------------------------------------------------------------

void test_initial_state(void) {
    // After begin(), all predictions should be unavailable
    TEST_ASSERT_EQUAL_UINT32(0, predictor->getMeat1EstTime());
    TEST_ASSERT_EQUAL_UINT32(0, predictor->getMeat2EstTime());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, predictor->getMeat1Rate());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, predictor->getMeat2Rate());
}

// --------------------------------------------------------------------------
// Tests: Linear Regression Accuracy
// --------------------------------------------------------------------------

void test_linear_rise_prediction(void) {
    // Feed a steady linear rise: 1 degree every 5 seconds = 12 degrees/minute
    // Start at 100F, target 200F, need 100 more degrees
    // At 12 deg/min = 0.2 deg/sec, time to target = 100 / 0.2 = 500 seconds

    uint32_t baseTime = 1700000000;
    predictor->setCurrentTime(baseTime + 20 * 5);  // "now" is after all samples
    predictor->setMeat1Target(200.0f);

    feedLinearRise(PREDICTOR_MEAT1, baseTime, 100.0f, 1.0f, 20);

    // Rate should be ~12 degrees per minute (1 deg per 5s * 60s/min)
    float rate = predictor->getMeat1Rate();
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 12.0f, rate);

    // Est time: current temp = 100 + 19 = 119F, target 200F, delta = 81F
    // At 0.2 deg/sec, time = 81 / 0.2 = 405 seconds
    uint32_t est = predictor->getMeat1EstTime();
    TEST_ASSERT_NOT_EQUAL(0, est);
    uint32_t expectedEst = (baseTime + 20 * 5) + 405;
    // Allow 10 seconds tolerance for floating point
    TEST_ASSERT_UINT32_WITHIN(10, expectedEst, est);
}

void test_prediction_with_known_slope(void) {
    // Feed exactly: 0.5 degrees every 5 seconds = 6 degrees/minute
    uint32_t baseTime = 1700000000;
    predictor->setMeat1Target(250.0f);

    feedLinearRise(PREDICTOR_MEAT1, baseTime, 150.0f, 0.5f, 20);
    predictor->setCurrentTime(baseTime + 19 * 5);

    float rate = predictor->getMeat1Rate();
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 6.0f, rate);
}

void test_prediction_accuracy_slow_rise(void) {
    // Simulate a slow brisket rise: ~0.5F/min = 0.00833 deg/sec
    // At 5s intervals: 0.0417 deg per sample
    uint32_t baseTime = 1700000000;
    float degreesPerSample = 0.5f / 60.0f * 5.0f;  // 0.0417 deg per 5s

    predictor->setMeat1Target(203.0f);

    feedLinearRise(PREDICTOR_MEAT1, baseTime, 150.0f, degreesPerSample, 30);
    predictor->setCurrentTime(baseTime + 29 * 5);

    float rate = predictor->getMeat1Rate();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.5f, rate);

    uint32_t est = predictor->getMeat1EstTime();
    TEST_ASSERT_NOT_EQUAL(0, est);

    // Current temp ~ 150 + 29 * 0.0417 = ~151.2F, target 203, delta ~51.8
    // At 0.5F/min, time = 51.8 / 0.5 * 60 = ~6216 seconds
    float currentTemp = 150.0f + 29 * degreesPerSample;
    float expectedSec = (203.0f - currentTemp) / (0.5f / 60.0f);
    uint32_t expectedEst = (baseTime + 29 * 5) + (uint32_t)expectedSec;
    TEST_ASSERT_UINT32_WITHIN(120, expectedEst, est);  // 2 min tolerance
}

// --------------------------------------------------------------------------
// Tests: Edge Cases
// --------------------------------------------------------------------------

void test_no_prediction_insufficient_samples(void) {
    // Feed only 5 samples (need at least PREDICTOR_MIN_SAMPLES = 12)
    uint32_t baseTime = 1700000000;
    predictor->setMeat1Target(200.0f);
    predictor->setCurrentTime(baseTime + 4 * 5);

    feedLinearRise(PREDICTOR_MEAT1, baseTime, 100.0f, 1.0f, 5);

    TEST_ASSERT_EQUAL_UINT32(0, predictor->getMeat1EstTime());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, predictor->getMeat1Rate());
}

void test_no_prediction_temp_decreasing(void) {
    // Feed decreasing temperatures
    uint32_t baseTime = 1700000000;
    predictor->setMeat1Target(200.0f);
    predictor->setCurrentTime(baseTime + 19 * 5);

    feedLinearRise(PREDICTOR_MEAT1, baseTime, 180.0f, -0.5f, 20);

    // Rate should be negative (or zero from the getter)
    TEST_ASSERT_EQUAL_UINT32(0, predictor->getMeat1EstTime());
}

void test_no_prediction_no_target(void) {
    // Feed good data but don't set a target
    uint32_t baseTime = 1700000000;
    predictor->setCurrentTime(baseTime + 19 * 5);

    feedLinearRise(PREDICTOR_MEAT1, baseTime, 100.0f, 1.0f, 20);

    // Target defaults to 0 (not set)
    TEST_ASSERT_EQUAL_UINT32(0, predictor->getMeat1EstTime());
}

void test_no_prediction_already_at_target(void) {
    uint32_t baseTime = 1700000000;
    predictor->setMeat1Target(200.0f);
    predictor->setCurrentTime(baseTime + 19 * 5);

    // Temperature already at target
    feedLinearRise(PREDICTOR_MEAT1, baseTime, 200.0f, 0.5f, 20);

    TEST_ASSERT_EQUAL_UINT32(0, predictor->getMeat1EstTime());
}

void test_no_prediction_probe_disconnected(void) {
    // The update() method skips disconnected probes, so if we only call
    // update with meat1Connected=false, no samples get added
    uint32_t baseTime = 1700000000;
    predictor->setMeat1Target(200.0f);
    predictor->setCurrentTime(baseTime);

    // No samples added for meat1 = no prediction
    TEST_ASSERT_EQUAL_UINT32(0, predictor->getMeat1EstTime());
}

// --------------------------------------------------------------------------
// Tests: Rolling Window
// --------------------------------------------------------------------------

void test_window_slides(void) {
    // Fill beyond PREDICTOR_WINDOW_SIZE to test circular buffer wrapping
    uint32_t baseTime = 1700000000;
    predictor->setMeat1Target(300.0f);

    // Feed 80 samples (window is 60) with a known slope
    feedLinearRise(PREDICTOR_MEAT1, baseTime, 100.0f, 0.5f, 80);
    predictor->setCurrentTime(baseTime + 79 * 5);

    // Rate should still reflect the consistent slope
    float rate = predictor->getMeat1Rate();
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 6.0f, rate);  // 0.5 deg/5s = 6 deg/min

    // Should have a valid prediction
    TEST_ASSERT_NOT_EQUAL(0, predictor->getMeat1EstTime());
}

void test_rate_changes_with_stall(void) {
    // First phase: rising at 1 deg per sample
    uint32_t baseTime = 1700000000;
    predictor->setMeat1Target(250.0f);

    feedLinearRise(PREDICTOR_MEAT1, baseTime, 140.0f, 1.0f, 30);

    predictor->setCurrentTime(baseTime + 29 * 5);
    float risingRate = predictor->getMeat1Rate();
    TEST_ASSERT_TRUE(risingRate > 5.0f);  // Should be ~12 deg/min

    // Second phase: stall (flat temperature) — enough to fill window
    for (uint16_t i = 30; i < 100; i++) {
        predictor->addSample(PREDICTOR_MEAT1, baseTime + i * 5, 170.0f);
    }
    predictor->setCurrentTime(baseTime + 99 * 5);

    float stallRate = predictor->getMeat1Rate();
    // Rate should be near zero during stall (window now filled with flat data)
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, stallRate);
}

// --------------------------------------------------------------------------
// Tests: Reset
// --------------------------------------------------------------------------

void test_reset_clears_all(void) {
    uint32_t baseTime = 1700000000;
    predictor->setMeat1Target(200.0f);
    predictor->setMeat2Target(210.0f);
    predictor->setCurrentTime(baseTime + 19 * 5);

    feedLinearRise(PREDICTOR_MEAT1, baseTime, 100.0f, 1.0f, 20);
    feedLinearRise(PREDICTOR_MEAT2, baseTime, 110.0f, 0.8f, 20);

    // Verify we have predictions
    TEST_ASSERT_NOT_EQUAL(0, predictor->getMeat1EstTime());
    TEST_ASSERT_NOT_EQUAL(0, predictor->getMeat2EstTime());

    // Reset everything
    predictor->reset();

    TEST_ASSERT_EQUAL_UINT32(0, predictor->getMeat1EstTime());
    TEST_ASSERT_EQUAL_UINT32(0, predictor->getMeat2EstTime());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, predictor->getMeat1Rate());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, predictor->getMeat2Rate());
}

void test_reset_single_probe(void) {
    uint32_t baseTime = 1700000000;
    predictor->setMeat1Target(200.0f);
    predictor->setMeat2Target(210.0f);
    predictor->setCurrentTime(baseTime + 19 * 5);

    feedLinearRise(PREDICTOR_MEAT1, baseTime, 100.0f, 1.0f, 20);
    feedLinearRise(PREDICTOR_MEAT2, baseTime, 110.0f, 0.8f, 20);

    // Reset only meat1
    predictor->reset(PREDICTOR_MEAT1);

    // Meat1 should be cleared
    TEST_ASSERT_EQUAL_UINT32(0, predictor->getMeat1EstTime());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, predictor->getMeat1Rate());

    // Meat2 should still have data
    TEST_ASSERT_NOT_EQUAL(0, predictor->getMeat2EstTime());
    TEST_ASSERT_TRUE(predictor->getMeat2Rate() > 0.0f);
}

// --------------------------------------------------------------------------
// Tests: Two-Probe Independence
// --------------------------------------------------------------------------

void test_probes_independent(void) {
    uint32_t baseTime = 1700000000;
    predictor->setMeat1Target(200.0f);
    predictor->setMeat2Target(185.0f);
    predictor->setCurrentTime(baseTime + 19 * 5);

    // Meat1: fast rise (1 deg per sample = 12 deg/min)
    feedLinearRise(PREDICTOR_MEAT1, baseTime, 150.0f, 1.0f, 20);

    // Meat2: slow rise (0.2 deg per sample = 2.4 deg/min)
    feedLinearRise(PREDICTOR_MEAT2, baseTime, 160.0f, 0.2f, 20);

    float rate1 = predictor->getMeat1Rate();
    float rate2 = predictor->getMeat2Rate();

    // Rates should be different
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 12.0f, rate1);
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 2.4f, rate2);

    // Both should have valid predictions
    uint32_t est1 = predictor->getMeat1EstTime();
    uint32_t est2 = predictor->getMeat2EstTime();
    TEST_ASSERT_NOT_EQUAL(0, est1);
    TEST_ASSERT_NOT_EQUAL(0, est2);

    // Meat1 should finish sooner (closer to target, faster rate)
    // Meat1: 150+19=169F, target 200, delta 31, at 0.2deg/s = 155s
    // Meat2: 160+19*0.2=163.8F, target 185, delta 21.2, at 0.04deg/s = 530s
    TEST_ASSERT_TRUE(est1 < est2);
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Initialization
    RUN_TEST(test_initial_state);

    // Linear regression accuracy
    RUN_TEST(test_linear_rise_prediction);
    RUN_TEST(test_prediction_with_known_slope);
    RUN_TEST(test_prediction_accuracy_slow_rise);

    // Edge cases
    RUN_TEST(test_no_prediction_insufficient_samples);
    RUN_TEST(test_no_prediction_temp_decreasing);
    RUN_TEST(test_no_prediction_no_target);
    RUN_TEST(test_no_prediction_already_at_target);
    RUN_TEST(test_no_prediction_probe_disconnected);

    // Rolling window
    RUN_TEST(test_window_slides);
    RUN_TEST(test_rate_changes_with_stall);

    // Reset
    RUN_TEST(test_reset_clears_all);
    RUN_TEST(test_reset_single_probe);

    // Two-probe independence
    RUN_TEST(test_probes_independent);

    return UNITY_END();
}
