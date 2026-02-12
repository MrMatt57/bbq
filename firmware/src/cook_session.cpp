#include "cook_session.h"
#include <string.h>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <time.h>
#endif

CookSession::CookSession()
    : _head(0)
    , _count(0)
    , _wrapped(false)
    , _active(false)
    , _startTime(0)
    , _totalPoints(0)
    , _lastSampleMs(0)
    , _lastFlushMs(0)
    , _flushedToIndex(0)
    , _getPitTemp(nullptr)
    , _getMeat1Temp(nullptr)
    , _getMeat2Temp(nullptr)
    , _getFanPct(nullptr)
    , _getDamperPct(nullptr)
    , _getFlags(nullptr)
{
    memset(_buffer, 0, sizeof(_buffer));
}

void CookSession::begin() {
#ifndef NATIVE_BUILD
    // Attempt to recover a session from flash
    if (loadFromFlash()) {
        Serial.printf("[SESSION] Recovered session from flash. %u points loaded.\n",
                      _totalPoints);
        _active = true;
    } else {
        Serial.println("[SESSION] No previous session found.");
    }
#endif
}

void CookSession::update() {
    if (!_active) return;

#ifndef NATIVE_BUILD
    unsigned long now = millis();

    // Sample interval check
    if (now - _lastSampleMs < SESSION_SAMPLE_INTERVAL) {
        // Check flush interval even if no new sample
        if (now - _lastFlushMs >= SESSION_FLUSH_INTERVAL) {
            flush();
            _lastFlushMs = now;
        }
        return;
    }
    _lastSampleMs = now;

    // Build a data point from current sensor data
    DataPoint dp;
    memset(&dp, 0, sizeof(dp));

    // Get current epoch time
    time_t nowEpoch;
    time(&nowEpoch);
    dp.timestamp = (uint32_t)nowEpoch;

    // Fill from data sources if set
    if (_getPitTemp)   dp.pitTemp   = (int16_t)(_getPitTemp() * 10.0f);
    if (_getMeat1Temp) dp.meat1Temp = (int16_t)(_getMeat1Temp() * 10.0f);
    if (_getMeat2Temp) dp.meat2Temp = (int16_t)(_getMeat2Temp() * 10.0f);
    if (_getFanPct)    dp.fanPct    = _getFanPct();
    if (_getDamperPct) dp.damperPct = _getDamperPct();
    if (_getFlags)     dp.flags     = _getFlags();

    addPoint(dp);

    // Periodic flush to LittleFS
    if (now - _lastFlushMs >= SESSION_FLUSH_INTERVAL) {
        flush();
        _lastFlushMs = now;
    }
#endif
}

void CookSession::startSession() {
    clear();
    _active = true;

#ifndef NATIVE_BUILD
    time_t now;
    time(&now);
    _startTime = (uint32_t)now;
    _lastSampleMs = millis();
    _lastFlushMs = millis();

    Serial.printf("[SESSION] New session started at epoch %u.\n", _startTime);
#endif
}

void CookSession::endSession() {
    if (_active) {
        flush();  // Final flush
        _active = false;
#ifndef NATIVE_BUILD
        Serial.printf("[SESSION] Session ended. %u total points.\n", _totalPoints);
#endif
    }
}

void CookSession::addPoint(const DataPoint& point) {
    _buffer[_head] = point;
    _head = (_head + 1) % SESSION_BUFFER_SIZE;

    if (_count < SESSION_BUFFER_SIZE) {
        _count++;
    } else {
        _wrapped = true;
    }

    _totalPoints++;
}

void CookSession::flush() {
#ifndef NATIVE_BUILD
    if (_count == 0) return;

    // Append unflushed data points to the session file
    File file = LittleFS.open(SESSION_FILE_PATH, "a");
    if (!file) {
        // Try creating the file
        file = LittleFS.open(SESSION_FILE_PATH, "w");
        if (!file) {
            Serial.println("[SESSION] Failed to open session file for writing!");
            return;
        }
        // Write header: start time + data point size for recovery
        file.write((uint8_t*)&_startTime, sizeof(_startTime));
    }

    // Determine which points to flush (those not yet written)
    // We write from _flushedToIndex to _totalPoints
    uint32_t pointsToFlush = _totalPoints - _flushedToIndex;

    if (pointsToFlush > _count) {
        pointsToFlush = _count;  // Can't flush more than what's in RAM
    }

    if (pointsToFlush > 0) {
        // Write from the oldest unflushed point in the circular buffer
        uint32_t startIdx;
        if (_wrapped) {
            // Buffer has wrapped; oldest point is at _head
            startIdx = _head;
        } else {
            startIdx = 0;
        }

        // Calculate how far back we need to go for unflushed points
        uint32_t flushStart;
        if (pointsToFlush <= _count) {
            // Write the most recent pointsToFlush entries
            if (_head >= pointsToFlush) {
                flushStart = _head - pointsToFlush;
            } else {
                flushStart = SESSION_BUFFER_SIZE - (pointsToFlush - _head);
            }
        } else {
            flushStart = startIdx;
        }

        for (uint32_t i = 0; i < pointsToFlush; i++) {
            uint32_t idx = (flushStart + i) % SESSION_BUFFER_SIZE;
            file.write((uint8_t*)&_buffer[idx], sizeof(DataPoint));
        }

        _flushedToIndex = _totalPoints;

        Serial.printf("[SESSION] Flushed %u points to flash.\n", pointsToFlush);
    }

    file.close();
#endif
}

bool CookSession::loadFromFlash() {
#ifndef NATIVE_BUILD
    File file = LittleFS.open(SESSION_FILE_PATH, "r");
    if (!file) return false;

    size_t fileSize = file.size();
    if (fileSize < sizeof(uint32_t)) {
        file.close();
        return false;
    }

    // Read start time header
    file.read((uint8_t*)&_startTime, sizeof(_startTime));

    // Read data points
    size_t dataSize = fileSize - sizeof(uint32_t);
    uint32_t numPoints = dataSize / sizeof(DataPoint);

    if (numPoints == 0) {
        file.close();
        return false;
    }

    // Clear buffer first
    _head = 0;
    _count = 0;
    _wrapped = false;
    _totalPoints = 0;

    // Read points into circular buffer (keep only the most recent SESSION_BUFFER_SIZE)
    if (numPoints > SESSION_BUFFER_SIZE) {
        // Skip older points that don't fit in buffer
        uint32_t skip = numPoints - SESSION_BUFFER_SIZE;
        file.seek(sizeof(uint32_t) + skip * sizeof(DataPoint));
        numPoints = SESSION_BUFFER_SIZE;
    }

    for (uint32_t i = 0; i < numPoints; i++) {
        DataPoint dp;
        if (file.read((uint8_t*)&dp, sizeof(DataPoint)) == sizeof(DataPoint)) {
            _buffer[_head] = dp;
            _head = (_head + 1) % SESSION_BUFFER_SIZE;
            _count++;
            _totalPoints++;
        }
    }

    _flushedToIndex = _totalPoints;
    file.close();
    return _count > 0;
#else
    return false;
#endif
}

void CookSession::clear() {
    _head = 0;
    _count = 0;
    _wrapped = false;
    _totalPoints = 0;
    _flushedToIndex = 0;
    _startTime = 0;
    _active = false;
    memset(_buffer, 0, sizeof(_buffer));

#ifndef NATIVE_BUILD
    LittleFS.remove(SESSION_FILE_PATH);
    Serial.println("[SESSION] Session data cleared.");
#endif
}

String CookSession::toCSV() const {
    String csv;
    csv.reserve(_count * 60);  // Rough estimate

    // Header
    csv += "timestamp,pit,meat1,meat2,fan,damper,flags\n";

    // Data rows
    for (uint32_t i = 0; i < _count; i++) {
        const DataPoint* dp = getPoint(i);
        if (dp == nullptr) continue;

        char line[80];
        snprintf(line, sizeof(line), "%u,%.1f,%.1f,%.1f,%u,%u,%u\n",
                 dp->timestamp,
                 dp->pitTemp / 10.0f,
                 dp->meat1Temp / 10.0f,
                 dp->meat2Temp / 10.0f,
                 dp->fanPct,
                 dp->damperPct,
                 dp->flags);
        csv += line;
    }

    return csv;
}

String CookSession::toJSON() const {
    String json;
    json.reserve(_count * 80);

    json += "[";

    for (uint32_t i = 0; i < _count; i++) {
        const DataPoint* dp = getPoint(i);
        if (dp == nullptr) continue;

        if (i > 0) json += ",";

        char entry[120];
        snprintf(entry, sizeof(entry),
                 "{\"ts\":%u,\"pit\":%.1f,\"meat1\":%.1f,\"meat2\":%.1f,"
                 "\"fan\":%u,\"damper\":%u,\"flags\":%u}",
                 dp->timestamp,
                 dp->pitTemp / 10.0f,
                 dp->meat1Temp / 10.0f,
                 dp->meat2Temp / 10.0f,
                 dp->fanPct,
                 dp->damperPct,
                 dp->flags);
        json += entry;
    }

    json += "]";
    return json;
}

uint32_t CookSession::getPointCount() const {
    return _count;
}

bool CookSession::isActive() const {
    return _active;
}

uint32_t CookSession::getElapsedSec() const {
    if (!_active || _startTime == 0) return 0;

#ifndef NATIVE_BUILD
    time_t now;
    time(&now);
    if ((uint32_t)now < _startTime) return 0;
    return (uint32_t)now - _startTime;
#else
    return 0;
#endif
}

const DataPoint* CookSession::getPoint(uint32_t index) const {
    if (index >= _count) return nullptr;

    // Calculate actual buffer position
    // Oldest point is at different positions depending on wrap state
    uint32_t actualIdx;
    if (_wrapped) {
        // When wrapped, oldest is at _head (it was overwritten next)
        actualIdx = (_head + index) % SESSION_BUFFER_SIZE;
    } else {
        // Not wrapped: oldest is at index 0
        actualIdx = index;
    }

    return &_buffer[actualIdx];
}

uint32_t CookSession::getTotalPointCount() const {
    return _totalPoints;
}

void CookSession::setDataSources(TempGetter pitFn, TempGetter meat1Fn, TempGetter meat2Fn,
                                  PctGetter fanFn, PctGetter damperFn, FlagGetter flagFn) {
    _getPitTemp   = pitFn;
    _getMeat1Temp = meat1Fn;
    _getMeat2Temp = meat2Fn;
    _getFanPct    = fanFn;
    _getDamperPct = damperFn;
    _getFlags     = flagFn;
}
