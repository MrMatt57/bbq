/**
 * test_i2c.cpp
 *
 * On-device integration tests for the I2C bus.
 * Verifies bus initialization, device scanning, and error-free communication.
 *
 * Prerequisites: ADS1115 connected on I2C bus (SDA=GPIO10, SCL=GPIO11).
 */

#include <unity.h>
#include <Arduino.h>
#include <Wire.h>

#include "../../src/config.h"

void setUp(void) {
    // I2C is initialized once in main; nothing per-test
}

void tearDown(void) {
    // Nothing to clean up
}

// --------------------------------------------------------------------------
// Tests
// --------------------------------------------------------------------------

void test_i2c_bus_initializes(void) {
    // Wire.begin() should succeed. On ESP32, it returns bool.
    bool ok = Wire.begin(PIN_SDA, PIN_SCL);
    TEST_ASSERT_TRUE_MESSAGE(ok, "Wire.begin() failed — check SDA/SCL pins");
}

void test_i2c_scan_finds_ads1115(void) {
    // Scan the I2C bus for the ADS1115 at address 0x48
    bool found = false;

    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();

        if (error == 0 && addr == ADS1115_ADDR) {
            found = true;
            Serial.printf("[I2C] Found ADS1115 at 0x%02X\n", addr);
        } else if (error == 0) {
            Serial.printf("[I2C] Found unknown device at 0x%02X\n", addr);
        }
    }

    TEST_ASSERT_TRUE_MESSAGE(found,
        "ADS1115 not found at address 0x48 during I2C scan");
}

void test_i2c_no_bus_errors(void) {
    // Perform a series of read/write transactions and verify no errors.
    // We'll do 10 consecutive reads from the ADS1115 config register (0x01).

    uint8_t errorCount = 0;

    for (uint8_t i = 0; i < 10; i++) {
        // Point to config register
        Wire.beginTransmission(ADS1115_ADDR);
        Wire.write(0x01);  // Config register address
        uint8_t txError = Wire.endTransmission(false);  // Repeated start

        if (txError != 0) {
            errorCount++;
            Serial.printf("[I2C] TX error %d on iteration %d\n", txError, i);
            continue;
        }

        // Read 2 bytes from the config register
        uint8_t bytesRead = Wire.requestFrom((uint8_t)ADS1115_ADDR, (uint8_t)2);
        if (bytesRead != 2) {
            errorCount++;
            Serial.printf("[I2C] Read only %d bytes on iteration %d (expected 2)\n",
                          bytesRead, i);
            continue;
        }

        uint8_t msb = Wire.read();
        uint8_t lsb = Wire.read();

        // The config register should have a sane default value.
        // ADS1115 default config is 0x8583.
        // Just verify we got non-zero data (bus isn't stuck).
        if (msb == 0 && lsb == 0) {
            Serial.printf("[I2C] WARNING: Config register reads 0x0000 on iteration %d\n", i);
        }
    }

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, errorCount,
        "I2C bus errors detected during read/write cycles");
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------

int main(int argc, char** argv) {
    delay(2000);  // Wait for serial port stabilization

    // Initialize I2C bus once for all tests
    Wire.begin(PIN_SDA, PIN_SCL);

    UNITY_BEGIN();

    RUN_TEST(test_i2c_bus_initializes);
    RUN_TEST(test_i2c_scan_finds_ads1115);
    RUN_TEST(test_i2c_no_bus_errors);

    return UNITY_END();
}
