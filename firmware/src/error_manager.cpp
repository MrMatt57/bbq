#include "error_manager.h"
#include <string.h>
#include <stdio.h>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

ErrorManager::ErrorManager()
    : _errorCount(0)
    , _pitHistoryIndex(0)
    , _pitHistoryCount(0)
    , _lastPitSampleMs(0)
    , _declineStartMs(0)
    , _declining(false)
    , _lastPitTemp(0.0f)
    , _wifiConnected(true)
{
    memset(_errors, 0, sizeof(_errors));
    memset(_pitTempHistory, 0, sizeof(_pitTempHistory));
}

void ErrorManager::begin() {
#ifndef NATIVE_BUILD
    Serial.println("[ERROR] Error manager initialized.");
#endif
}

void ErrorManager::update(float pitTemp, float fanPct, const ProbeState probeStates[3]) {
#ifndef NATIVE_BUILD
    unsigned long now = millis();
#else
    unsigned long now = 0;
#endif

    // --- Probe errors ---
    const char* probeNames[] = {"Pit", "Meat 1", "Meat 2"};

    for (uint8_t i = 0; i < 3; i++) {
        if (probeStates[i].openCircuit) {
            char msg[48];
            snprintf(msg, sizeof(msg), "%s probe disconnected", probeNames[i]);
            addError(ErrorCode::PROBE_OPEN, i, msg);
            // Remove short error if present for this probe
            removeError(ErrorCode::PROBE_SHORT, i);
        } else if (probeStates[i].shortCircuit) {
            char msg[48];
            snprintf(msg, sizeof(msg), "%s probe shorted", probeNames[i]);
            addError(ErrorCode::PROBE_SHORT, i, msg);
            // Remove open error if present for this probe
            removeError(ErrorCode::PROBE_OPEN, i);
        } else {
            // Probe is OK, clear any errors for this probe
            removeError(ErrorCode::PROBE_OPEN, i);
            removeError(ErrorCode::PROBE_SHORT, i);
        }
    }

    // --- Fire-out detection ---
    // Sample pit temperature once per minute for fire-out analysis.
    // Fire-out: pit declining > ERROR_FIREOUT_RATE deg/min for
    // ERROR_FIREOUT_DURATION_MS continuous minutes despite fan at 100%.
    if (now - _lastPitSampleMs >= 60000 || _lastPitSampleMs == 0) {
        _lastPitSampleMs = now;

        if (pitTemp > 0.0f) {
            // Store in circular history buffer
            _pitTempHistory[_pitHistoryIndex] = pitTemp;
            _pitHistoryIndex = (_pitHistoryIndex + 1) % 10;
            if (_pitHistoryCount < 10) _pitHistoryCount++;

            // Check for sustained decline over the last minute
            if (_pitHistoryCount >= 2 && _lastPitTemp > 0.0f) {
                float ratePerMin = _lastPitTemp - pitTemp;  // Positive = declining

                if (ratePerMin >= ERROR_FIREOUT_RATE && fanPct >= 95.0f) {
                    // Temperature is declining despite fan at near-maximum
                    if (!_declining) {
                        _declining = true;
                        _declineStartMs = now;
                    }

                    // Check if decline has persisted for the required duration
                    if (now - _declineStartMs >= ERROR_FIREOUT_DURATION_MS) {
                        addError(ErrorCode::FIRE_OUT, 0xFF, "Fire may be out");
                    }
                } else {
                    // Not declining or fan not at max -- reset the timer
                    _declining = false;
                    _declineStartMs = 0;
                    removeError(ErrorCode::FIRE_OUT, 0xFF);
                }
            }

            _lastPitTemp = pitTemp;
        }
    }

    // --- WiFi lost ---
    if (!_wifiConnected) {
        addError(ErrorCode::WIFI_LOST, 0xFF, "WiFi connection lost");
    } else {
        removeError(ErrorCode::WIFI_LOST, 0xFF);
    }
}

#ifndef NATIVE_BUILD
std::vector<ErrorEntry> ErrorManager::getErrors() const {
    std::vector<ErrorEntry> result;
    result.reserve(_errorCount);
    for (uint8_t i = 0; i < _errorCount; i++) {
        result.push_back(_errors[i]);
    }
    return result;
}
#endif

uint8_t ErrorManager::getErrorCount() const {
    return _errorCount;
}

bool ErrorManager::hasError(ErrorCode code) const {
    for (uint8_t i = 0; i < _errorCount; i++) {
        if (_errors[i].code == code) return true;
    }
    return false;
}

bool ErrorManager::isFireOut() const {
    return hasError(ErrorCode::FIRE_OUT);
}

void ErrorManager::clearAll() {
    _errorCount = 0;
    memset(_errors, 0, sizeof(_errors));
    _declining = false;
    _declineStartMs = 0;
}

void ErrorManager::setWifiConnected(bool connected) {
    _wifiConnected = connected;
}

void ErrorManager::addError(ErrorCode code, uint8_t probeIndex, const char* message) {
    // Check if this exact error already exists
    if (errorExists(code, probeIndex)) return;

    // Add to list if space available
    if (_errorCount >= MAX_ERRORS) return;

    _errors[_errorCount].code = code;
    _errors[_errorCount].probeIndex = probeIndex;
    strncpy(_errors[_errorCount].message, message, sizeof(_errors[_errorCount].message) - 1);
    _errors[_errorCount].message[sizeof(_errors[_errorCount].message) - 1] = '\0';
    _errorCount++;

#ifndef NATIVE_BUILD
    Serial.printf("[ERROR] Error added: %s (code=%d)\n", message, (int)code);
#endif
}

void ErrorManager::removeError(ErrorCode code, uint8_t probeIndex) {
    for (uint8_t i = 0; i < _errorCount; ) {
        bool match = (_errors[i].code == code);
        if (probeIndex != 0xFF) {
            match = match && (_errors[i].probeIndex == probeIndex);
        }

        if (match) {
            // Shift remaining errors down
            for (uint8_t j = i; j < _errorCount - 1; j++) {
                _errors[j] = _errors[j + 1];
            }
            _errorCount--;
            memset(&_errors[_errorCount], 0, sizeof(ErrorEntry));
            // Don't increment i -- re-check this index since we shifted
        } else {
            i++;
        }
    }
}

bool ErrorManager::errorExists(ErrorCode code, uint8_t probeIndex) const {
    for (uint8_t i = 0; i < _errorCount; i++) {
        if (_errors[i].code == code && _errors[i].probeIndex == probeIndex) {
            return true;
        }
    }
    return false;
}
