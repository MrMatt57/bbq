/**
 * test_temp_conversion.cpp
 *
 * Tests for the Steinhart-Hart temperature conversion math used by TempManager.
 * Since TempManager's conversion methods (adcToResistance, resistanceToTempC) are
 * private and the class depends on ADS1115 hardware, we replicate the pure-math
 * conversion formulas here and test them directly.
 *
 * Formulas under test:
 *   - Voltage divider: R_therm = R_ref * (ADC_MAX / raw - 1)
 *   - Steinhart-Hart:  1/T = A + B*ln(R) + C*(ln(R))^3, T in Kelvin
 *   - C to F:          F = C * 9/5 + 32
 */

#include <unity.h>
#include <math.h>
#include <stdint.h>

// Pull in constants from config.h
// config.h has #pragma once and no hardware deps, safe to include directly
#include "config.h"

// --------------------------------------------------------------------------
// Replicated conversion functions (mirrors TempManager private methods)
// --------------------------------------------------------------------------

static float adcToResistance(int16_t raw) {
    if (raw <= 0) return 0.0f;
    return REFERENCE_RESISTANCE * ((float)ADC_MAX_VALUE / (float)raw - 1.0f);
}

static float resistanceToTempC(float resistance, float a, float b, float c) {
    if (resistance <= 0.0f) return 0.0f;
    float lnR = logf(resistance);
    float lnR3 = lnR * lnR * lnR;
    float invT = a + b * lnR + c * lnR3;
    if (invT == 0.0f) return 0.0f;
    float tempK = 1.0f / invT;
    float tempC = tempK - 273.15f;
    return tempC;
}

static float cToF(float tempC) {
    return tempC * 9.0f / 5.0f + 32.0f;
}

// Helper: full pipeline from resistance to degrees F using default probe coefficients
static float resistanceToTempF(float resistance) {
    float tempC = resistanceToTempC(resistance, THERM_A, THERM_B, THERM_C);
    return cToF(tempC);
}

// --------------------------------------------------------------------------
// setUp / tearDown
// --------------------------------------------------------------------------

void setUp(void) {
    // Nothing to set up
}

void tearDown(void) {
    // Nothing to tear down
}

// --------------------------------------------------------------------------
// Tests: Known resistance/temperature pairs (Thermoworks Pro-Series probes)
//
// These are approximate values. The Steinhart-Hart coefficients provided
// in config.h should produce temperatures reasonably close to the expected
// values. We allow a tolerance of +/- 5 degrees F for the approximation.
// --------------------------------------------------------------------------

void test_resistance_33k_is_near_77F(void) {
    // ~33K ohm should be approximately 77F (25C) -- room temperature
    float tempF = resistanceToTempF(33000.0f);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 77.0f, tempF);
}

void test_resistance_10k_is_near_170F(void) {
    // ~10K ohm should be approximately 170F (77C)
    float tempF = resistanceToTempF(10000.0f);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 170.0f, tempF);
}

void test_resistance_5k_is_near_220F(void) {
    // ~5K ohm should be approximately 220F (104C)
    float tempF = resistanceToTempF(5000.0f);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 220.0f, tempF);
}

void test_resistance_3k_is_near_270F(void) {
    // ~3K ohm should be approximately 270F (132C)
    float tempF = resistanceToTempF(3000.0f);
    TEST_ASSERT_FLOAT_WITHIN(8.0f, 270.0f, tempF);
}

// --------------------------------------------------------------------------
// Tests: Monotonicity -- lower resistance should yield higher temperature
// --------------------------------------------------------------------------

void test_lower_resistance_gives_higher_temperature(void) {
    float temp33k = resistanceToTempF(33000.0f);
    float temp10k = resistanceToTempF(10000.0f);
    float temp5k  = resistanceToTempF(5000.0f);
    float temp3k  = resistanceToTempF(3000.0f);

    TEST_ASSERT_TRUE(temp10k > temp33k);
    TEST_ASSERT_TRUE(temp5k > temp10k);
    TEST_ASSERT_TRUE(temp3k > temp5k);
}

// --------------------------------------------------------------------------
// Tests: Voltage divider ADC-to-resistance conversion
// --------------------------------------------------------------------------

void test_adc_midpoint_gives_reference_resistance(void) {
    // When raw ADC = ADC_MAX/2, the thermistor resistance should equal
    // the reference resistance (balanced voltage divider)
    // R = R_ref * (ADC_MAX / (ADC_MAX/2) - 1) = R_ref * (2 - 1) = R_ref
    int16_t midpoint = ADC_MAX_VALUE / 2;  // 16383
    float resistance = adcToResistance(midpoint);
    // Should be very close to 10000 ohm (with some rounding from integer division)
    TEST_ASSERT_FLOAT_WITHIN(1.0f, REFERENCE_RESISTANCE, resistance);
}

void test_adc_low_value_gives_high_resistance(void) {
    // Low ADC value means low voltage, which means thermistor has high resistance
    // (cold probe). raw = 1000 -> R = 10000 * (32767/1000 - 1) = 10000 * 31.767 = ~317670
    float resistance = adcToResistance(1000);
    TEST_ASSERT_TRUE(resistance > 100000.0f);
}

void test_adc_high_value_gives_low_resistance(void) {
    // High ADC value means high voltage, thermistor has low resistance (hot probe)
    // raw = 30000 -> R = 10000 * (32767/30000 - 1) = 10000 * 0.09223 = ~922
    float resistance = adcToResistance(30000);
    TEST_ASSERT_TRUE(resistance < 1500.0f);
    TEST_ASSERT_TRUE(resistance > 0.0f);
}

void test_adc_zero_returns_zero_resistance(void) {
    // Zero or negative ADC should return 0 (guard against division by zero)
    float resistance = adcToResistance(0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, resistance);
}

void test_adc_negative_returns_zero_resistance(void) {
    float resistance = adcToResistance(-100);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, resistance);
}

// --------------------------------------------------------------------------
// Tests: C to F conversion
// --------------------------------------------------------------------------

void test_cToF_freezing_point(void) {
    // 0C = 32F
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 32.0f, cToF(0.0f));
}

void test_cToF_boiling_point(void) {
    // 100C = 212F
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 212.0f, cToF(100.0f));
}

void test_cToF_body_temp(void) {
    // 37C = 98.6F
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 98.6f, cToF(37.0f));
}

void test_cToF_negative(void) {
    // -40C = -40F (the crossover point)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -40.0f, cToF(-40.0f));
}

// --------------------------------------------------------------------------
// Tests: Edge cases -- very high resistance (very cold)
// --------------------------------------------------------------------------

void test_very_high_resistance_gives_sub_freezing(void) {
    // 200K ohm -> very cold, should be well below freezing (32F)
    float tempF = resistanceToTempF(200000.0f);
    TEST_ASSERT_TRUE(tempF < 32.0f);
}

// --------------------------------------------------------------------------
// Tests: Edge cases -- very low resistance (very hot)
// --------------------------------------------------------------------------

void test_very_low_resistance_gives_very_hot(void) {
    // 500 ohm -> very hot, should be well above 300F
    float tempF = resistanceToTempF(500.0f);
    TEST_ASSERT_TRUE(tempF > 300.0f);
}

// --------------------------------------------------------------------------
// Tests: Zero/invalid resistance
// --------------------------------------------------------------------------

void test_zero_resistance_returns_zero(void) {
    float tempC = resistanceToTempC(0.0f, THERM_A, THERM_B, THERM_C);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, tempC);
}

void test_negative_resistance_returns_zero(void) {
    float tempC = resistanceToTempC(-100.0f, THERM_A, THERM_B, THERM_C);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, tempC);
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Known resistance/temperature pairs
    RUN_TEST(test_resistance_33k_is_near_77F);
    RUN_TEST(test_resistance_10k_is_near_170F);
    RUN_TEST(test_resistance_5k_is_near_220F);
    RUN_TEST(test_resistance_3k_is_near_270F);

    // Monotonicity
    RUN_TEST(test_lower_resistance_gives_higher_temperature);

    // ADC to resistance
    RUN_TEST(test_adc_midpoint_gives_reference_resistance);
    RUN_TEST(test_adc_low_value_gives_high_resistance);
    RUN_TEST(test_adc_high_value_gives_low_resistance);
    RUN_TEST(test_adc_zero_returns_zero_resistance);
    RUN_TEST(test_adc_negative_returns_zero_resistance);

    // C to F conversion
    RUN_TEST(test_cToF_freezing_point);
    RUN_TEST(test_cToF_boiling_point);
    RUN_TEST(test_cToF_body_temp);
    RUN_TEST(test_cToF_negative);

    // Edge cases
    RUN_TEST(test_very_high_resistance_gives_sub_freezing);
    RUN_TEST(test_very_low_resistance_gives_very_hot);
    RUN_TEST(test_zero_resistance_returns_zero);
    RUN_TEST(test_negative_resistance_returns_zero);

    return UNITY_END();
}
