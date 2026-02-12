#include "temp_predictor.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <time.h>
#else
#include <cstring>
#include <ctime>

// Stub millis() for native builds
static unsigned long _nativeMillis = 0;
static unsigned long millis() { return _nativeMillis; }
#endif

TempPredictor::TempPredictor()
    : _lastSampleMs(0)
#ifdef NATIVE_BUILD
    , _testEpoch(0)
#endif
{
    for (uint8_t i = 0; i < PREDICTOR_NUM_PROBES; i++) {
        _probes[i].head   = 0;
        _probes[i].count  = 0;
        _probes[i].target = 0.0f;
    }
}

void TempPredictor::begin() {
    reset();
    _lastSampleMs = 0;
}

void TempPredictor::update(float meat1Temp, float meat2Temp,
                            bool meat1Connected, bool meat2Connected) {
#ifndef NATIVE_BUILD
    unsigned long now = millis();
    if (_lastSampleMs != 0 && (now - _lastSampleMs) < PREDICTOR_SAMPLE_INTERVAL) {
        return;  // Not time to sample yet
    }
    _lastSampleMs = now;
#endif

    uint32_t epoch = getCurrentEpoch();
    if (epoch == 0) {
        return;  // Time not available yet
    }

    if (meat1Connected) {
        addSampleInternal(PREDICTOR_MEAT1, epoch, meat1Temp);
    }

    if (meat2Connected) {
        addSampleInternal(PREDICTOR_MEAT2, epoch, meat2Temp);
    }
}

void TempPredictor::setMeat1Target(float target) {
    _probes[PREDICTOR_MEAT1].target = target;
}

void TempPredictor::setMeat2Target(float target) {
    _probes[PREDICTOR_MEAT2].target = target;
}

uint32_t TempPredictor::getMeat1EstTime() const {
    return computeEstTime(PREDICTOR_MEAT1);
}

uint32_t TempPredictor::getMeat2EstTime() const {
    return computeEstTime(PREDICTOR_MEAT2);
}

float TempPredictor::getMeat1Rate() const {
    // computeSlope returns degrees per second; convert to degrees per minute
    float slope = computeSlope(PREDICTOR_MEAT1);
    return slope * 60.0f;
}

float TempPredictor::getMeat2Rate() const {
    float slope = computeSlope(PREDICTOR_MEAT2);
    return slope * 60.0f;
}

void TempPredictor::reset() {
    for (uint8_t i = 0; i < PREDICTOR_NUM_PROBES; i++) {
        reset(i);
    }
}

void TempPredictor::reset(uint8_t probeIndex) {
    if (probeIndex >= PREDICTOR_NUM_PROBES) return;

    _probes[probeIndex].head  = 0;
    _probes[probeIndex].count = 0;
    // Note: target is intentionally preserved across reset
}

#ifdef NATIVE_BUILD
void TempPredictor::setCurrentTime(uint32_t epoch) {
    _testEpoch = epoch;
}

void TempPredictor::addSample(uint8_t probe, uint32_t timestamp, float temp) {
    if (probe >= PREDICTOR_NUM_PROBES) return;
    addSampleInternal(probe, timestamp, temp);
}
#endif

// --- Private implementation ---

void TempPredictor::addSampleInternal(uint8_t probe, uint32_t timestamp, float temp) {
    if (probe >= PREDICTOR_NUM_PROBES) return;

    ProbeWindow& w = _probes[probe];

    w.samples[w.head].timestamp = timestamp;
    w.samples[w.head].temp      = temp;

    w.head++;
    if (w.head >= PREDICTOR_WINDOW_SIZE) {
        w.head = 0;
    }

    if (w.count < PREDICTOR_WINDOW_SIZE) {
        w.count++;
    }
}

float TempPredictor::computeSlope(uint8_t probe) const {
    if (probe >= PREDICTOR_NUM_PROBES) return 0.0f;

    const ProbeWindow& w = _probes[probe];

    if (w.count < PREDICTOR_MIN_SAMPLES) {
        return 0.0f;
    }

    // Least-squares linear regression over the rolling window.
    // x = timestamp (seconds), y = temperature
    // slope = (N * sum(x*y) - sum(x) * sum(y)) / (N * sum(x^2) - (sum(x))^2)
    //
    // To avoid floating-point precision issues with large epoch values,
    // we offset x by the oldest sample's timestamp (x_i = timestamp_i - t0).

    uint16_t n = w.count;

    // Find the oldest sample index in the circular buffer
    uint16_t oldest;
    if (n < PREDICTOR_WINDOW_SIZE) {
        oldest = 0;  // Buffer hasn't wrapped yet
    } else {
        oldest = w.head;  // head points to the next write = oldest entry
    }

    uint32_t t0 = w.samples[oldest].timestamp;

    double sumX  = 0.0;
    double sumY  = 0.0;
    double sumXY = 0.0;
    double sumX2 = 0.0;

    for (uint16_t i = 0; i < n; i++) {
        uint16_t idx = (oldest + i) % PREDICTOR_WINDOW_SIZE;
        double x = (double)(w.samples[idx].timestamp - t0);
        double y = (double)w.samples[idx].temp;

        sumX  += x;
        sumY  += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    double denom = (double)n * sumX2 - sumX * sumX;
    if (denom == 0.0) {
        return 0.0f;  // All timestamps identical (shouldn't happen)
    }

    double slope = ((double)n * sumXY - sumX * sumY) / denom;

    return (float)slope;  // degrees per second
}

float TempPredictor::getLatestTemp(uint8_t probe) const {
    if (probe >= PREDICTOR_NUM_PROBES) return 0.0f;

    const ProbeWindow& w = _probes[probe];
    if (w.count == 0) return 0.0f;

    // The most recent sample is at (head - 1), wrapping around
    uint16_t latest = (w.head == 0) ? (PREDICTOR_WINDOW_SIZE - 1) : (w.head - 1);
    return w.samples[latest].temp;
}

uint32_t TempPredictor::computeEstTime(uint8_t probe) const {
    if (probe >= PREDICTOR_NUM_PROBES) return 0;

    const ProbeWindow& w = _probes[probe];

    // No target set
    if (w.target <= 0.0f) return 0;

    // Not enough samples
    if (w.count < PREDICTOR_MIN_SAMPLES) return 0;

    float currentTemp = getLatestTemp(probe);

    // Already at or above target
    if (currentTemp >= w.target) return 0;

    float slope = computeSlope(probe);

    // Temperature not rising
    if (slope <= 0.0f) return 0;

    // Time to reach target from current temp (in seconds)
    float deltaTemp = w.target - currentTemp;
    float timeToTarget = deltaTemp / slope;

    // Sanity check: reject predictions more than 24 hours out
    if (timeToTarget > (float)PREDICTOR_MAX_PREDICT_SEC) return 0;

    uint32_t epoch = getCurrentEpoch();
    if (epoch == 0) return 0;

    return epoch + (uint32_t)timeToTarget;
}

uint32_t TempPredictor::getCurrentEpoch() const {
#ifdef NATIVE_BUILD
    return _testEpoch;
#else
    time_t now;
    time(&now);
    // time() returns 0 or very small values if NTP hasn't synced yet
    if (now < 1700000000UL) {
        return 0;
    }
    return (uint32_t)now;
#endif
}
