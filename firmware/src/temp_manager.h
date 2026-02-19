#pragma once

#include "config.h"
#include "units.h"
#include <stdint.h>
#include <math.h>

#ifndef NATIVE_BUILD
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#endif

// Number of probe channels
#define NUM_PROBES 3

// Probe indices
enum ProbeIndex : uint8_t {
    PROBE_PIT   = 0,
    PROBE_MEAT1 = 1,
    PROBE_MEAT2 = 2
};

// Probe status
enum class ProbeStatus : uint8_t {
    OK,
    OPEN_CIRCUIT,    // ADC reads very high (probe disconnected)
    SHORT_CIRCUIT    // ADC reads very low (probe shorted)
};

// Per-probe calibration and Steinhart-Hart coefficients
struct ProbeConfig {
    float a      = THERM_A;
    float b      = THERM_B;
    float c      = THERM_C;
    float offset = 0.0f;  // Calibration offset in degrees C
};

class TempManager {
public:
    TempManager();

    // Initialize ADS1115 on I2C bus. Call once from setup().
    bool begin();

    // Poll probes if the sample interval has elapsed. Call every loop().
    void update();

    // Latest smoothed temperature for a given probe (in configured units: F or C)
    float getTemp(uint8_t probe) const;

    // Get filtered temperature in Celsius (internal representation)
    float getTempC(uint8_t probe) const;

    // Convenience: pit probe temperature
    float getPitTemp() const   { return getTemp(PROBE_PIT); }
    float getMeat1Temp() const { return getTemp(PROBE_MEAT1); }
    float getMeat2Temp() const { return getTemp(PROBE_MEAT2); }

    // Check if a probe is connected and reading normally
    bool isConnected(uint8_t probe) const;

    // Probe health status
    ProbeStatus getStatus(uint8_t probe) const;

    // Raw ADC value (useful for diagnostics)
    int16_t getRawADC(uint8_t probe) const;

    // Set EMA alpha (smoothing factor, 0-1, higher = less smoothing)
    void setEMAAlpha(float alpha);

    // Set calibration offset for a probe (in degrees C)
    void setOffset(uint8_t probe, float offset);

    // Set Steinhart-Hart coefficients for a probe
    void setCoefficients(uint8_t probe, float a, float b, float c);

    // Set whether to return temperatures in Fahrenheit
    void setUseFahrenheit(bool useF);

    // Convert Celsius to Fahrenheit (delegates to shared units.h)
    static float cToF(float tempC) { return celsiusToFahrenheit(tempC); }

private:
    // Convert raw ADC value to resistance using voltage divider formula
    float adcToResistance(int16_t raw) const;

    // Convert resistance to temperature in Celsius using Steinhart-Hart
    float resistanceToTempC(float resistance, const ProbeConfig& cfg) const;

#ifndef NATIVE_BUILD
    Adafruit_ADS1115 _ads;
#endif

    // Per-probe state
    int16_t     _rawADC[NUM_PROBES];
    float       _filteredTempC[NUM_PROBES];
    ProbeStatus _status[NUM_PROBES];
    ProbeConfig _probeConfig[NUM_PROBES];

    // EMA smoothing factor
    float _emaAlpha;

    // Whether first reading has been taken (for EMA initialization)
    bool _firstReading[NUM_PROBES];

    // Temperature units
    bool _useFahrenheit;

    // Timing
    unsigned long _lastSampleMs;

    // ADC channel mapping
    static const uint8_t _adcChannels[NUM_PROBES];
};
