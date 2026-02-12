#pragma once

#include "config.h"
#include <stdint.h>

#ifndef NATIVE_BUILD
#include <LittleFS.h>
#include <ArduinoJson.h>
#endif

// Compact data point struct (13 bytes) for RAM and flash storage
struct DataPoint {
    uint32_t timestamp;     // Unix epoch seconds
    int16_t  pitTemp;       // Pit temperature * 10 (e.g., 2255 = 225.5F)
    int16_t  meat1Temp;     // Meat 1 temperature * 10
    int16_t  meat2Temp;     // Meat 2 temperature * 10
    uint8_t  fanPct;        // Fan speed 0-100%
    uint8_t  damperPct;     // Damper position 0-100%
    uint8_t  flags;         // Bit flags (lid-open, alarms, errors)
};

// Flag bits for DataPoint.flags
#define DP_FLAG_LID_OPEN      0x01
#define DP_FLAG_ALARM_PIT     0x02
#define DP_FLAG_ALARM_MEAT1   0x04
#define DP_FLAG_ALARM_MEAT2   0x08
#define DP_FLAG_ERROR_FIREOUT 0x10
#define DP_FLAG_PIT_DISC      0x20
#define DP_FLAG_MEAT1_DISC    0x40
#define DP_FLAG_MEAT2_DISC    0x80

class CookSession {
public:
    CookSession();

    // Initialize buffer and attempt to recover a session from LittleFS.
    // Call once from setup().
    void begin();

    // Sample a data point if the interval has elapsed. Call every loop().
    void update();

    // Start a new cook session (clears buffer, creates new file)
    void startSession();

    // End the current session and flush remaining data
    void endSession();

    // Add a data point to the circular buffer
    void addPoint(const DataPoint& point);

    // Force-flush the RAM buffer to LittleFS now
    void flush();

    // Load/recover session data from LittleFS on boot (power-loss recovery)
    bool loadFromFlash();

    // Clear all session data (RAM + file)
    void clear();

    // Generate CSV string of all data points
    // WARNING: This can be large. Caller should use chunked transfer or stream.
    String toCSV() const;

    // Generate JSON array of all data points
    String toJSON() const;

    // Number of stored data points
    uint32_t getPointCount() const;

    // Whether a session is currently being recorded
    bool isActive() const;

    // Get elapsed time since session start (seconds)
    uint32_t getElapsedSec() const;

    // Get the session start timestamp (epoch)
    uint32_t getStartTime() const { return _startTime; }

    // Access a specific data point by index (0 = oldest)
    const DataPoint* getPoint(uint32_t index) const;

    // Get total number of points (including those on flash)
    uint32_t getTotalPointCount() const;

    // Set function pointers for getting current sensor data
    // (called by update() to auto-fill data points)
    typedef float (*TempGetter)();
    typedef uint8_t (*PctGetter)();
    typedef uint8_t (*FlagGetter)();

    void setDataSources(TempGetter pitFn, TempGetter meat1Fn, TempGetter meat2Fn,
                        PctGetter fanFn, PctGetter damperFn, FlagGetter flagFn);

private:
    // Circular buffer
    DataPoint _buffer[SESSION_BUFFER_SIZE];
    uint32_t  _head;          // Next write position
    uint32_t  _count;         // Number of valid entries in buffer
    bool      _wrapped;       // Buffer has wrapped around

    // Session state
    bool      _active;
    uint32_t  _startTime;     // Session start epoch
    uint32_t  _totalPoints;   // Total points including flushed

    // Timing
    unsigned long _lastSampleMs;
    unsigned long _lastFlushMs;

    // Number of points written to flash (for flush tracking)
    uint32_t _flushedToIndex;

    // Data source callbacks
    TempGetter _getPitTemp;
    TempGetter _getMeat1Temp;
    TempGetter _getMeat2Temp;
    PctGetter  _getFanPct;
    PctGetter  _getDamperPct;
    FlagGetter _getFlags;
};
