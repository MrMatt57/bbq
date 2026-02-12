#pragma once

#include "config.h"
#include <stdint.h>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <vector>
#endif

// Error codes
enum class ErrorCode : uint8_t {
    NONE         = 0,
    PROBE_OPEN   = 1,   // Probe disconnected (open circuit)
    PROBE_SHORT  = 2,   // Probe shorted
    FIRE_OUT     = 3,   // Fire appears to have gone out
    FAN_STALL    = 4,   // Fan not responding (future: tachometer)
    WIFI_LOST    = 5    // WiFi connection lost
};

// Error entry with code and descriptive message
struct ErrorEntry {
    ErrorCode code;
    uint8_t   probeIndex;  // Which probe, if applicable (0-2), or 0xFF for non-probe errors
    char      message[48]; // Human-readable description
};

// Probe states passed from TempManager
struct ProbeState {
    bool connected;
    bool openCircuit;
    bool shortCircuit;
    float temperature;  // Current temp in configured units
};

// Maximum number of simultaneous errors
#define MAX_ERRORS 8

class ErrorManager {
public:
    ErrorManager();

    // Initialize error manager. Call once from setup().
    void begin();

    // Check for error conditions. Call every loop().
    // pitTemp: current pit temperature
    // fanPct: current fan output percentage
    // probeStates: array of 3 ProbeState structs (pit, meat1, meat2)
    void update(float pitTemp, float fanPct, const ProbeState probeStates[3]);

    // Get all currently active errors
#ifndef NATIVE_BUILD
    std::vector<ErrorEntry> getErrors() const;
#endif

    // Get error count
    uint8_t getErrorCount() const;

    // Check if a specific error code is active
    bool hasError(ErrorCode code) const;

    // Check if fire-out condition is detected
    bool isFireOut() const;

    // Clear all errors (for testing or manual reset)
    void clearAll();

    // Set WiFi connection state (called by WiFi manager)
    void setWifiConnected(bool connected);

private:
    // Add an error if not already present
    void addError(ErrorCode code, uint8_t probeIndex, const char* message);

    // Remove errors matching a code (and optionally probe index)
    void removeError(ErrorCode code, uint8_t probeIndex = 0xFF);

    // Check if an error with this code and probe index exists
    bool errorExists(ErrorCode code, uint8_t probeIndex) const;

    // Active errors
    ErrorEntry _errors[MAX_ERRORS];
    uint8_t    _errorCount;

    // Fire-out detection state
    float         _pitTempHistory[10];  // Store last 10 minutes of pit temps (sampled per minute)
    uint8_t       _pitHistoryIndex;
    uint8_t       _pitHistoryCount;
    unsigned long _lastPitSampleMs;     // When we last sampled pit temp for fire-out
    uint32_t      _declineStartMs;      // When the decline started
    bool          _declining;           // Whether we're in a decline state
    float         _lastPitTemp;         // Previous pit temp reading

    // WiFi state
    bool _wifiConnected;
};
