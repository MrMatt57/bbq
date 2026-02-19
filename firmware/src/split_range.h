#pragma once

#include <cmath>
#include <cstring>

struct SplitRangeOutput {
    float fanPercent;
    float damperPercent;
};

// Compute fan and damper percentages from a PID output using split-range coordination.
// fanMode: "fan_only", "damper_primary", or "fan_and_damper" (default)
// fanOnThreshold: PID output threshold above which fan activates (fan_and_damper mode)
inline SplitRangeOutput splitRange(float pidOutput, const char* fanMode, float fanOnThreshold) {
    float fanPct = 0.0f;
    float damperPct = 0.0f;

    if (strcmp(fanMode, "fan_only") == 0) {
        fanPct = pidOutput;
        damperPct = 100.0f;
    } else if (strcmp(fanMode, "damper_primary") == 0) {
        float dpThreshold = fmaxf(fanOnThreshold, 50.0f);
        damperPct = pidOutput;
        if (pidOutput > dpThreshold) {
            fanPct = (pidOutput - dpThreshold) / (100.0f - dpThreshold) * 100.0f;
            damperPct = 100.0f;
        }
    } else {
        // fan_and_damper (default)
        damperPct = pidOutput;
        if (pidOutput > fanOnThreshold) {
            fanPct = (pidOutput - fanOnThreshold) / (100.0f - fanOnThreshold) * 100.0f;
        }
    }

    return { fanPct, damperPct };
}
