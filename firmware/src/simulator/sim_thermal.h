#pragma once

#include "sim_profiles.h"
#include <cmath>

// Result of a thermal model update step
struct SimResult {
    float pitTemp;
    float meat1Temp;
    float meat2Temp;
    float fanPercent;
    float damperPercent;
    bool lidOpen;
    bool fireOut;
    bool meat1Connected;
    bool meat2Connected;
};

// Simplified charcoal smoker physics simulation.
// C++ port of simulator/thermal-model.js.
class SimThermalModel {
public:
    // Core state (public for direct access from sim_main)
    float pitTemp;
    float meat1Temp;
    float meat2Temp;
    float ambientTemp;
    float setpoint;
    float fanPercent;
    float damperPercent;
    float fireEnergy;
    bool fireOut;
    bool lidOpen;
    float lidOpenTimer;
    bool meat1Connected;
    bool meat2Connected;
    float simTime;

    SimThermalModel();
    void init(const SimProfile& profile);
    SimResult update(float dt);

private:
    // Internal state
    bool lidDropApplied_;
    float preLidPitTemp_;
    bool stallEnabled_;
    float stallTempLow_;
    float stallTempHigh_;
    float stallDurationSeconds_;
    float stallTimeAccumulated_;
    bool inStall_;
    float fireDecayRate_;

    // PID state
    float pidIntegral_;
    float pidPrevError_;

    // Overshoot tracking
    bool hasReachedSetpoint_;
    float overshootRemaining_;

    // Noise
    float noisePhase_;

    // Events
    SimEvent* events_;
    int eventCount_;

    float computePID(float dt);
    void updatePitTemp(float dt);
    void updateMeatTemps(float dt);
    void processEvents();
    float addNoise(float temp, float magnitude);
};
