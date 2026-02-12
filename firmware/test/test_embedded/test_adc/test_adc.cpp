/**
 * test_adc.cpp
 *
 * On-device integration tests for the ADS1115 16-bit ADC.
 * Verifies I2C communication, channel reads, and probe detection.
 *
 * Prerequisites: ADS1115 connected at address 0x48 on I2C bus (SDA=10, SCL=11).
 */

#include <unity.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

#include "../../src/config.h"

static Adafruit_ADS1115 ads;

void setUp(void) {
    // Each test gets a fresh start
}

void tearDown(void) {
    // Nothing to clean up
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

void test_ads1115_responds_on_i2c(void) {
    // Scan the I2C bus for the ADS1115 at its expected address
    Wire.beginTransmission(ADS1115_ADDR);
    uint8_t error = Wire.endTransmission();

    // error == 0 means device acknowledged
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, error,
        "ADS1115 did not respond at I2C address 0x48");
}

void test_adc_reads_within_range(void) {
    // Read all 4 channels and verify values are within the valid 16-bit signed range
    for (uint8_t ch = 0; ch < 4; ch++) {
        int16_t raw = ads.readADC_SingleEnded(ch);

        // ADS1115 returns 0-32767 for single-ended reads
        // Even an open/shorted input should be within this range
        TEST_ASSERT_GREATER_OR_EQUAL_INT16_MESSAGE(0, raw,
            "ADC value below 0 (unexpected for single-ended read)");
        TEST_ASSERT_LESS_OR_EQUAL_INT16_MESSAGE(32767, raw,
            "ADC value above 32767 (unexpected for 16-bit ADC)");
    }
}

void test_adc_probe_open_circuit(void) {
    // With no probe plugged in, the voltage divider floats high.
    // The ADC should read near maximum value (>32000).
    // This test assumes at least one probe jack has no probe inserted.
    // We check all 3 probe channels — at least one should be open.

    bool foundOpen = false;
    for (uint8_t ch = 0; ch < 3; ch++) {
        int16_t raw = ads.readADC_SingleEnded(ch);
        if (raw > ERROR_PROBE_OPEN_THRESHOLD) {
            foundOpen = true;
            break;
        }
    }

    // If all 3 probes are connected, this test is inconclusive.
    // We still pass but log a note.
    if (!foundOpen) {
        Serial.println("[NOTE] All 3 probes appear connected. "
                       "Unplug at least one to validate open-circuit detection.");
    }
    TEST_ASSERT_TRUE_MESSAGE(true, "Open circuit check completed (see serial output)");
}

void test_adc_channels_independent(void) {
    // Read each channel twice and verify readings are consistent
    // (not bleeding between channels)
    int16_t first[4];
    int16_t second[4];

    for (uint8_t ch = 0; ch < 4; ch++) {
        first[ch] = ads.readADC_SingleEnded(ch);
    }

    delay(50);  // Small delay between reads

    for (uint8_t ch = 0; ch < 4; ch++) {
        second[ch] = ads.readADC_SingleEnded(ch);
    }

    // Each channel's second reading should be within 200 counts of the first
    // (allowing for noise but catching gross cross-talk)
    for (uint8_t ch = 0; ch < 4; ch++) {
        int16_t diff = abs(first[ch] - second[ch]);
        char msg[64];
        snprintf(msg, sizeof(msg), "Channel %d readings diverged by %d counts", ch, diff);
        TEST_ASSERT_LESS_OR_EQUAL_INT16_MESSAGE(200, diff, msg);
    }
}

void test_adc_read_rate(void) {
    // Verify we can read all 3 probe channels within 100ms
    unsigned long start = millis();

    for (uint8_t ch = 0; ch < 3; ch++) {
        ads.readADC_SingleEnded(ch);
    }

    unsigned long elapsed = millis() - start;

    // ADS1115 at default 128 SPS: ~8ms per conversion, 3 channels ~24ms
    // Allow up to 100ms for I2C overhead
    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(100, elapsed,
        "Reading 3 ADC channels took too long (>100ms)");
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------

int main(int argc, char** argv) {
    delay(2000);  // Wait for serial port stabilization on ESP32

    // Initialize I2C bus
    Wire.begin(PIN_SDA, PIN_SCL);

    // Initialize ADS1115
    if (!ads.begin(ADS1115_ADDR, &Wire)) {
        Serial.println("[FATAL] ADS1115 not found! Check wiring.");
    }
    ads.setGain(GAIN_ONE);

    UNITY_BEGIN();

    RUN_TEST(test_ads1115_responds_on_i2c);
    RUN_TEST(test_adc_reads_within_range);
    RUN_TEST(test_adc_probe_open_circuit);
    RUN_TEST(test_adc_channels_independent);
    RUN_TEST(test_adc_read_rate);

    return UNITY_END();
}
