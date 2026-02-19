#include "sim_thermal.h"
#include "../split_range.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

SimThermalModel::SimThermalModel() {
    init(sim_profile_normal);
}

void SimThermalModel::setFanMode(const char* mode) {
    strncpy(fanMode, mode, sizeof(fanMode) - 1);
    fanMode[sizeof(fanMode) - 1] = '\0';
}

void SimThermalModel::setFanOnThreshold(float threshold) {
    fanOnThreshold = threshold;
}

void SimThermalModel::init(const SimProfile& profile) {
    pitTemp = profile.initialPitTemp;
    meat1Temp = profile.meat1Start;
    meat2Temp = profile.meat2Start;
    ambientTemp = profile.initialPitTemp;
    setpoint = profile.targetPitTemp;
    fanPercent = 0;
    damperPercent = 0;
    fireEnergy = 1.0f;
    fireOut = false;
    lidOpen = false;
    lidOpenTimer = 0;
    lidDropApplied_ = false;
    preLidPitTemp_ = 0;
    meat1Connected = true;
    meat2Connected = true;
    simTime = 0;
    strncpy(fanMode, "fan_and_damper", sizeof(fanMode));
    fanOnThreshold = 30.0f;

    stallEnabled_ = profile.stallEnabled;
    stallTempLow_ = profile.stallTempLow;
    stallTempHigh_ = profile.stallTempHigh;
    stallDurationSeconds_ = profile.stallDurationHours * 3600.0f;
    stallTimeAccumulated_ = 0;
    inStall_ = false;
    fireDecayRate_ = 0.000003f;

    pidIntegral_ = 0;
    pidPrevError_ = 0;
    hasReachedSetpoint_ = false;
    overshootRemaining_ = 0;
    noisePhase_ = (float)(rand() % 1000) / 1000.0f * 6.283f;

    events_ = profile.events;
    eventCount_ = profile.eventCount;

    // Reset event fired flags
    for (int i = 0; i < eventCount_; i++) {
        events_[i].fired = false;
    }
}

SimResult SimThermalModel::update(float dt) {
    simTime += dt;

    processEvents();

    // Lid open timer
    if (lidOpenTimer > 0) {
        lidOpenTimer -= dt;
        if (lidOpenTimer <= 0) {
            lidOpen = false;
            lidOpenTimer = 0;
            lidDropApplied_ = false;
        }
    }

    // Simplified PID to compute fan/damper from pit error
    float pidOutput = computePID(dt);
    pidOutput = fmaxf(0, fminf(100, pidOutput));

    // Split-range fan/damper coordination (shared with firmware)
    {
        SplitRangeOutput sr = splitRange(pidOutput, fanMode, fanOnThreshold);
        fanPercent = sr.fanPercent;
        damperPercent = sr.damperPercent;
    }

    // If fire is out, fan runs at 100% but has no effect
    if (fireOut) {
        fanPercent = 100;
        damperPercent = 100;
    }

    // Fire energy model
    if (fireOut) {
        fireEnergy = fmaxf(0, fireEnergy - 0.0005f * dt);
    } else {
        fireEnergy = fmaxf(0.05f, fireEnergy - fireDecayRate_ * dt);
    }

    // Update temperatures
    updatePitTemp(dt);
    updateMeatTemps(dt);

    SimResult result;
    result.pitTemp = addNoise(pitTemp, 0.8f);
    result.meat1Temp = meat1Connected ? addNoise(meat1Temp, 0.3f) : 0;
    result.meat2Temp = meat2Connected ? addNoise(meat2Temp, 0.3f) : 0;
    result.fanPercent = roundf(fanPercent);
    result.damperPercent = roundf(damperPercent);
    result.lidOpen = lidOpen;
    result.fireOut = fireOut;
    result.meat1Connected = meat1Connected;
    result.meat2Connected = meat2Connected;
    return result;
}

float SimThermalModel::computePID(float dt) {
    const float Kp = 4.0f;
    const float Ki = 0.02f;
    const float Kd = 5.0f;

    float error = setpoint - pitTemp;

    // Integral with anti-windup
    pidIntegral_ += error * dt;
    if (pidIntegral_ > 2000) pidIntegral_ = 2000;
    if (pidIntegral_ < -2000) pidIntegral_ = -2000;

    // Derivative
    float derivative = (dt > 0) ? (error - pidPrevError_) / dt : 0;
    pidPrevError_ = error;

    float output = Kp * error + Ki * pidIntegral_ + Kd * derivative;

    // If lid is open, back off
    if (lidOpen) {
        output = 0;
    }

    return fmaxf(0, fminf(100, output));
}

void SimThermalModel::updatePitTemp(float dt) {
    const float PIT_TAU = 300.0f;

    // Lid open: rapid heat loss
    if (lidOpen) {
        if (!lidDropApplied_) {
            preLidPitTemp_ = pitTemp;
            lidDropApplied_ = true;
        }
        float targetTemp = ambientTemp + 20;
        float lidTau = 60.0f;
        float alpha = 1.0f - expf(-dt / lidTau);
        pitTemp += (targetTemp - pitTemp) * alpha;
        return;
    }

    // Effective airflow: damper gates airflow, fan adds forced draft above natural
    float naturalDraft = 0.15f;
    float damperOpen = damperPercent / 100.0f;
    float fanFlow = fanPercent / 100.0f;
    float airflow = damperOpen * fmaxf(naturalDraft, fanFlow);

    // Fire limits the max achievable pit temperature, modulated by airflow
    float maxFireTemp = 400.0f;
    float maxAchievable = ambientTemp + (maxFireTemp - ambientTemp) * fireEnergy * fmaxf(airflow, 0.05f);

    // Pit approaches the setpoint, capped by fire
    float targetTemp = fminf(setpoint, maxAchievable);

    // Normal exponential approach to target
    float alpha = 1.0f - expf(-dt / PIT_TAU);
    pitTemp += (targetTemp - pitTemp) * alpha;

    // Overshoot on initial ramp-up
    if (!hasReachedSetpoint_ && pitTemp >= setpoint * 0.95f) {
        hasReachedSetpoint_ = true;
        overshootRemaining_ = (setpoint - ambientTemp) * 0.05f;
    }
    if (overshootRemaining_ > 0) {
        float overshootDecay = 1.0f - expf(-dt / 180.0f);
        float applied = overshootRemaining_ * overshootDecay;
        pitTemp += applied;
        overshootRemaining_ -= applied;
        if (overshootRemaining_ < 0.5f) {
            overshootRemaining_ = 0;
        }
    }

    // Natural cooling toward ambient when fire energy is very low
    if (fireEnergy < 0.1f) {
        float coolingAlpha = 1.0f - expf(-dt / 600.0f);
        pitTemp += (ambientTemp - pitTemp) * coolingAlpha;
    }
}

void SimThermalModel::updateMeatTemps(float dt) {
    const float MEAT_TAU = 1800.0f;

    // Meat 1
    if (meat1Connected) {
        float meat1Alpha = 1.0f - expf(-dt / MEAT_TAU);

        // Stall model
        if (stallEnabled_) {
            if (meat1Temp >= stallTempLow_ && stallTimeAccumulated_ < stallDurationSeconds_) {
                inStall_ = true;
                stallTimeAccumulated_ += dt;

                float stallProgress = stallTimeAccumulated_ / stallDurationSeconds_;
                // Sigmoid-like release: starts near 0, gradually releases toward 1
                float stallFactor = 0.02f + 0.98f * powf(stallProgress, 3.0f);
                meat1Alpha *= stallFactor;

                // Clamp meat temp within stall band during deep stall
                if (stallProgress < 0.5f && meat1Temp > stallTempHigh_) {
                    meat1Temp = stallTempHigh_;
                }
            } else if (stallTimeAccumulated_ >= stallDurationSeconds_) {
                inStall_ = false;
            }
        }

        float meat1Delta = (pitTemp - meat1Temp) * meat1Alpha;
        meat1Temp += meat1Delta;
    }

    // Meat 2 (slightly different thermal properties — smaller cut, heats faster)
    if (meat2Connected) {
        float meat2Tau = MEAT_TAU * 0.75f;
        float meat2Alpha = 1.0f - expf(-dt / meat2Tau);
        float meat2Delta = (pitTemp - meat2Temp) * meat2Alpha;
        meat2Temp += meat2Delta;
    }
}

void SimThermalModel::processEvents() {
    for (int i = 0; i < eventCount_; i++) {
        SimEvent& event = events_[i];
        if (event.fired) continue;
        if (simTime < event.time) continue;

        event.fired = true;

        if (strcmp(event.type, "setpoint") == 0) {
            setpoint = event.param1;
            hasReachedSetpoint_ = false;
            pidIntegral_ = 0;
            printf("[SIM] Setpoint changed to %.0f\n", event.param1);
        } else if (strcmp(event.type, "lid-open") == 0) {
            lidOpen = true;
            lidOpenTimer = event.param1 > 0 ? event.param1 : 60;
            printf("[SIM] Lid opened for %.0fs\n", lidOpenTimer);
        } else if (strcmp(event.type, "fire-out") == 0) {
            fireOut = true;
            printf("[SIM] Fire out!\n");
        } else if (strcmp(event.type, "probe-disconnect") == 0) {
            if (event.param2 && strcmp(event.param2, "meat1") == 0) {
                meat1Connected = false;
                printf("[SIM] Meat1 probe disconnected\n");
            } else if (event.param2 && strcmp(event.param2, "meat2") == 0) {
                meat2Connected = false;
                printf("[SIM] Meat2 probe disconnected\n");
            }
        }
    }
}

float SimThermalModel::addNoise(float temp, float magnitude) {
    noisePhase_ += 0.01f;
    float noise = sinf(noisePhase_ * 7.3f) * magnitude * 0.5f
                + sinf(noisePhase_ * 13.1f) * magnitude * 0.3f
                + ((float)(rand() % 1000) / 1000.0f - 0.5f) * magnitude * 0.4f;
    return roundf((temp + noise) * 10.0f) / 10.0f;
}
