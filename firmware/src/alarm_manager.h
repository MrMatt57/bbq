#pragma once

#include "config.h"
#include <stdint.h>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

// Alarm types
enum class AlarmType : uint8_t {
    NONE       = 0,
    PIT_HIGH   = 1,   // Pit above setpoint + band
    PIT_LOW    = 2,   // Pit below setpoint - band
    MEAT1_DONE = 3,   // Meat 1 reached target
    MEAT2_DONE = 4    // Meat 2 reached target
};

// Maximum number of simultaneous active alarms
#define MAX_ACTIVE_ALARMS 4

class AlarmManager {
public:
    AlarmManager();

    // Initialize buzzer pin. Call once from setup().
    void begin();

    // Check all alarm conditions. Call every loop().
    // pitReached: true if pit has at some point reached setpoint (for pit alarm arming)
    void update(float pitTemp, float meat1Temp, float meat2Temp,
                float setpoint, bool pitReached);

    // Get array of currently active alarm types
    // Returns count of active alarms, fills the array up to maxCount
    uint8_t getActiveAlarms(AlarmType* alarms, uint8_t maxCount) const;

    // Check if any alarm is currently firing
    bool isAlarming() const;

    // Acknowledge/silence the current alarm(s)
    void acknowledge();

    // Set meat target temperatures
    void setMeat1Target(float target);
    void setMeat2Target(float target);

    // Get meat targets
    float getMeat1Target() const { return _meat1Target; }
    float getMeat2Target() const { return _meat2Target; }

    // Set pit alarm band (+/- degrees from setpoint)
    void setPitBand(float band);
    float getPitBand() const { return _pitBand; }

    // Low-level buzzer control
    void setBuzzer(bool on);

    // Enable/disable all alarms
    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }

private:
    // Update the buzzer on/off cycling pattern
    void updateBuzzer();

    // Check if a specific alarm type is in the active list
    bool isAlarmActive(AlarmType type) const;

    // Add an alarm to the active list (if not already present)
    void addAlarm(AlarmType type);

    // Remove an alarm from the active list
    void removeAlarm(AlarmType type);

    // Meat targets
    float _meat1Target;
    float _meat2Target;

    // Pit band
    float _pitBand;

    // Active alarms
    AlarmType _activeAlarms[MAX_ACTIVE_ALARMS];
    uint8_t   _activeCount;

    // Alarm state
    bool _acknowledged;     // User has silenced the alarm
    bool _enabled;          // Master enable
    bool _buzzerOn;         // Current buzzer state

    // Hysteresis: track whether each alarm has been triggered and acknowledged
    bool _meat1Triggered;   // Meat1 alarm has fired (prevents re-trigger after ack)
    bool _meat2Triggered;
    bool _pitTriggered;

    // Buzzer timing
    unsigned long _lastBuzzerToggleMs;
};
