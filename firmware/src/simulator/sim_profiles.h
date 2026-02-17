#pragma once

// Cook profiles for the LVGL desktop simulator.
// C++ port of simulator/profiles.js.

struct SimEvent {
    float time;          // sim seconds when event fires
    const char* type;    // "setpoint", "lid-open", "fire-out", "probe-disconnect"
    float param1;        // setpoint: target temp; lid-open: duration (s)
    const char* param2;  // probe-disconnect: probe name ("meat1" or "meat2")
    bool fired;
};

struct SimProfile {
    const char* name;
    float initialPitTemp;
    float targetPitTemp;
    float meat1Start;
    float meat2Start;
    float meat1Target;
    float meat2Target;
    bool stallEnabled;
    float stallTempLow;
    float stallTempHigh;
    float stallDurationHours;
    SimEvent* events;
    int eventCount;
};

// --- Normal profile ---
inline SimProfile sim_profile_normal = {
    "Normal", 70, 225, 40, 40, 203, 180,
    false, 0, 0, 0, nullptr, 0
};

// --- Brisket stall profile ---
inline SimProfile sim_profile_stall = {
    "Brisket Stall", 70, 225, 38, 40, 203, 185,
    true, 150, 170, 4, nullptr, 0
};

// --- Hot & fast profile ---
inline SimProfile sim_profile_hot_fast = {
    "Hot & Fast", 70, 300, 40, 42, 185, 185,
    false, 0, 0, 0, nullptr, 0
};

// --- Temperature change profile ---
static SimEvent temp_change_events[] = {
    { 4 * 3600.0f, "setpoint", 275, nullptr, false }
};
inline SimProfile sim_profile_temp_change = {
    "Temperature Change", 70, 225, 40, 40, 203, 185,
    false, 0, 0, 0, temp_change_events, 1
};

// --- Lid open profile ---
static SimEvent lid_open_events[] = {
    { 2 * 3600.0f, "lid-open", 60, nullptr, false },
    { 5 * 3600.0f, "lid-open", 90, nullptr, false },
    { 8 * 3600.0f, "lid-open", 60, nullptr, false }
};
inline SimProfile sim_profile_lid_open = {
    "Lid Opens", 70, 225, 40, 40, 203, 180,
    false, 0, 0, 0, lid_open_events, 3
};

// --- Fire out profile ---
static SimEvent fire_out_events[] = {
    { 4 * 3600.0f, "fire-out", 0, nullptr, false }
};
inline SimProfile sim_profile_fire_out = {
    "Fire Out", 70, 225, 40, 40, 203, 180,
    false, 0, 0, 0, fire_out_events, 1
};

// --- Probe disconnect profile ---
static SimEvent probe_disconnect_events[] = {
    { 3 * 3600.0f, "probe-disconnect", 0, "meat1", false }
};
inline SimProfile sim_profile_probe_disconnect = {
    "Probe Disconnect", 70, 225, 40, 40, 203, 180,
    false, 0, 0, 0, probe_disconnect_events, 1
};

// Profile lookup table
struct ProfileEntry {
    const char* key;
    SimProfile* profile;
};

static ProfileEntry sim_profiles[] = {
    { "normal",           &sim_profile_normal },
    { "stall",            &sim_profile_stall },
    { "hot-fast",         &sim_profile_hot_fast },
    { "temp-change",      &sim_profile_temp_change },
    { "lid-open",         &sim_profile_lid_open },
    { "fire-out",         &sim_profile_fire_out },
    { "probe-disconnect", &sim_profile_probe_disconnect },
};
static const int sim_profile_count = sizeof(sim_profiles) / sizeof(sim_profiles[0]);
