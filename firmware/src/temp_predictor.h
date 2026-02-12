#pragma once

#include "config.h"
#include <stdint.h>

// --- Predictor Constants ---
#define PREDICTOR_WINDOW_SIZE       60      // 60 samples * 5s = 5 minutes of data
#define PREDICTOR_MIN_SAMPLES       12      // Need at least 12 samples (1 min) before predicting
#define PREDICTOR_SAMPLE_INTERVAL   5000    // 5 seconds between samples (matches SESSION_SAMPLE_INTERVAL)
#define PREDICTOR_MAX_PREDICT_SEC   86400   // 24 hours max prediction horizon

// Number of meat probes tracked by the predictor
#define PREDICTOR_NUM_PROBES  2

// Probe indices for the predictor (meat probes only)
#define PREDICTOR_MEAT1  0
#define PREDICTOR_MEAT2  1

// A single (timestamp, temperature) sample in the rolling window
struct PredictorSample {
    uint32_t timestamp;   // Epoch seconds
    float    temp;        // Temperature in current units (F or C)
};

class TempPredictor {
public:
    TempPredictor();

    // Initialize / reset all state. Call once from setup().
    void begin();

    // Feed current meat probe temperatures. Call periodically (~every 5 seconds).
    // Only records samples for connected probes.
    void update(float meat1Temp, float meat2Temp, bool meat1Connected, bool meat2Connected);

    // Set target temperatures for each meat probe
    void setMeat1Target(float target);
    void setMeat2Target(float target);

    // Get predicted epoch (seconds) when the probe will reach its target.
    // Returns 0 if prediction is unavailable.
    uint32_t getMeat1EstTime() const;
    uint32_t getMeat2EstTime() const;

    // Get rate of temperature change in degrees per minute.
    // Returns 0.0 if insufficient data.
    float getMeat1Rate() const;
    float getMeat2Rate() const;

    // Clear all history for both probes
    void reset();

    // Clear history for a single probe (0 = meat1, 1 = meat2)
    void reset(uint8_t probeIndex);

#ifdef NATIVE_BUILD
    // Test helpers: inject time and samples directly
    void setCurrentTime(uint32_t epoch);
    void addSample(uint8_t probe, uint32_t timestamp, float temp);
#endif

private:
    // Per-probe rolling window state
    struct ProbeWindow {
        PredictorSample samples[PREDICTOR_WINDOW_SIZE];
        uint16_t head;        // Next write position in the circular buffer
        uint16_t count;       // Number of valid samples (up to PREDICTOR_WINDOW_SIZE)
        float    target;      // Target temperature (0 = not set)
    };

    // Add a sample to a probe's rolling window
    void addSampleInternal(uint8_t probe, uint32_t timestamp, float temp);

    // Compute linear regression slope (degrees per second) for a probe.
    // Returns 0.0 if insufficient data.
    float computeSlope(uint8_t probe) const;

    // Get the current temperature (latest sample) for a probe.
    // Returns 0.0 if no samples.
    float getLatestTemp(uint8_t probe) const;

    // Compute the estimated arrival epoch for a probe.
    // Returns 0 if prediction is unavailable.
    uint32_t computeEstTime(uint8_t probe) const;

    // Get current epoch time in seconds
    uint32_t getCurrentEpoch() const;

    ProbeWindow _probes[PREDICTOR_NUM_PROBES];

    // Timing for sample interval gating
    unsigned long _lastSampleMs;

#ifdef NATIVE_BUILD
    // Injected epoch for testing (0 means use real time)
    uint32_t _testEpoch;
#endif
};
