#include "alarm_manager.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

AlarmManager::AlarmManager()
    : _meat1Target(0.0f)
    , _meat2Target(0.0f)
    , _pitBand(ALARM_PIT_BAND_DEFAULT)
    , _activeCount(0)
    , _acknowledged(false)
    , _enabled(true)
    , _buzzerOn(false)
    , _meat1Triggered(false)
    , _meat2Triggered(false)
    , _pitTriggered(false)
    , _lastBuzzerToggleMs(0)
{
    for (uint8_t i = 0; i < MAX_ACTIVE_ALARMS; i++) {
        _activeAlarms[i] = AlarmType::NONE;
    }
}

void AlarmManager::begin() {
#ifndef NATIVE_BUILD
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    Serial.printf("[ALARM] Buzzer initialized on pin %d.\n", PIN_BUZZER);
#endif
}

void AlarmManager::update(float pitTemp, float meat1Temp, float meat2Temp,
                           float setpoint, bool pitReached) {
    if (!_enabled) {
        setBuzzer(false);
        return;
    }

    // --- Pit alarm (only active after pit has first reached setpoint) ---
    if (pitReached && setpoint > 0.0f && pitTemp > 0.0f) {
        bool pitOutOfBand = (pitTemp > setpoint + _pitBand) ||
                            (pitTemp < setpoint - _pitBand);

        if (pitOutOfBand && !_pitTriggered) {
            if (pitTemp > setpoint + _pitBand) {
                addAlarm(AlarmType::PIT_HIGH);
            } else {
                addAlarm(AlarmType::PIT_LOW);
            }
        } else if (!pitOutOfBand) {
            // Pit is back in band, clear pit alarms
            removeAlarm(AlarmType::PIT_HIGH);
            removeAlarm(AlarmType::PIT_LOW);
            _pitTriggered = false;  // Allow re-trigger if it goes out of band again
        }
    }

    // --- Meat 1 alarm ---
    if (_meat1Target > 0.0f && meat1Temp > 0.0f && !_meat1Triggered) {
        if (meat1Temp >= _meat1Target) {
            addAlarm(AlarmType::MEAT1_DONE);
            _meat1Triggered = true;  // Prevent re-trigger after acknowledgment
        }
    }

    // --- Meat 2 alarm ---
    if (_meat2Target > 0.0f && meat2Temp > 0.0f && !_meat2Triggered) {
        if (meat2Temp >= _meat2Target) {
            addAlarm(AlarmType::MEAT2_DONE);
            _meat2Triggered = true;
        }
    }

    // Update buzzer pattern
    updateBuzzer();
}

uint8_t AlarmManager::getActiveAlarms(AlarmType* alarms, uint8_t maxCount) const {
    uint8_t count = (_activeCount < maxCount) ? _activeCount : maxCount;
    for (uint8_t i = 0; i < count; i++) {
        alarms[i] = _activeAlarms[i];
    }
    return count;
}

bool AlarmManager::isAlarming() const {
    return _activeCount > 0 && !_acknowledged;
}

void AlarmManager::acknowledge() {
    _acknowledged = true;
    setBuzzer(false);

    // Mark current alarms as triggered so they don't re-fire
    for (uint8_t i = 0; i < _activeCount; i++) {
        switch (_activeAlarms[i]) {
            case AlarmType::PIT_HIGH:
            case AlarmType::PIT_LOW:
                _pitTriggered = true;
                break;
            case AlarmType::MEAT1_DONE:
                _meat1Triggered = true;
                break;
            case AlarmType::MEAT2_DONE:
                _meat2Triggered = true;
                break;
            default:
                break;
        }
    }

    // Clear active alarms
    _activeCount = 0;
    for (uint8_t i = 0; i < MAX_ACTIVE_ALARMS; i++) {
        _activeAlarms[i] = AlarmType::NONE;
    }

#ifndef NATIVE_BUILD
    Serial.println("[ALARM] Alarms acknowledged.");
#endif
}

void AlarmManager::setMeat1Target(float target) {
    _meat1Target = target;
    _meat1Triggered = false;  // Reset trigger on new target
}

void AlarmManager::setMeat2Target(float target) {
    _meat2Target = target;
    _meat2Triggered = false;
}

void AlarmManager::setPitBand(float band) {
    if (band > 0.0f) {
        _pitBand = band;
    }
}

void AlarmManager::setBuzzer(bool on) {
#ifndef NATIVE_BUILD
    if (on) {
        tone(PIN_BUZZER, ALARM_BUZZER_FREQ);
    } else {
        noTone(PIN_BUZZER);
    }
#endif
    _buzzerOn = on;
}

void AlarmManager::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!enabled) {
        setBuzzer(false);
    }
}

void AlarmManager::updateBuzzer() {
    if (!isAlarming()) {
        if (_buzzerOn) setBuzzer(false);
        return;
    }

#ifndef NATIVE_BUILD
    unsigned long now = millis();
    unsigned long elapsed = now - _lastBuzzerToggleMs;

    if (_buzzerOn) {
        // Buzzer is on: check if it's time to turn off (500ms on)
        if (elapsed >= ALARM_BUZZER_DURATION) {
            setBuzzer(false);
            _lastBuzzerToggleMs = now;
        }
    } else {
        // Buzzer is off: check if it's time to turn on (500ms off)
        if (elapsed >= ALARM_BUZZER_PAUSE) {
            setBuzzer(true);
            _lastBuzzerToggleMs = now;
        }
    }
#endif
}

bool AlarmManager::isAlarmActive(AlarmType type) const {
    for (uint8_t i = 0; i < _activeCount; i++) {
        if (_activeAlarms[i] == type) return true;
    }
    return false;
}

void AlarmManager::addAlarm(AlarmType type) {
    if (isAlarmActive(type)) return;
    if (_activeCount >= MAX_ACTIVE_ALARMS) return;

    _activeAlarms[_activeCount] = type;
    _activeCount++;
    _acknowledged = false;  // New alarm clears acknowledgment

#ifndef NATIVE_BUILD
    Serial.printf("[ALARM] Alarm triggered: type=%d\n", (int)type);
#endif
}

void AlarmManager::removeAlarm(AlarmType type) {
    for (uint8_t i = 0; i < _activeCount; i++) {
        if (_activeAlarms[i] == type) {
            // Shift remaining alarms down
            for (uint8_t j = i; j < _activeCount - 1; j++) {
                _activeAlarms[j] = _activeAlarms[j + 1];
            }
            _activeCount--;
            _activeAlarms[_activeCount] = AlarmType::NONE;
            return;
        }
    }
}
