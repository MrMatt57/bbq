#include "config_manager.h"
#include <string.h>

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

// Default probe settings (static)
const ProbeSettings ConfigManager::_defaultProbe = {
    "Probe", THERM_A, THERM_B, THERM_C, 0.0f
};

ConfigManager::ConfigManager()
    : _mounted(false)
{
    applyDefaults();
}

bool ConfigManager::begin() {
#ifndef NATIVE_BUILD
    if (!LittleFS.begin(true)) {  // true = format on fail
        Serial.println("[CFG] LittleFS mount failed!");
        _mounted = false;
        return false;
    }
    _mounted = true;
    Serial.println("[CFG] LittleFS mounted.");

    if (!load()) {
        Serial.println("[CFG] No valid config found, creating defaults.");
        applyDefaults();
        save();
    } else {
        Serial.println("[CFG] Configuration loaded from flash.");
    }
#endif
    return true;
}

bool ConfigManager::save() {
#ifndef NATIVE_BUILD
    if (!_mounted) return false;

    JsonDocument doc;
    toJson(doc);

    File file = LittleFS.open(CONFIG_FILE_PATH, "w");
    if (!file) {
        Serial.println("[CFG] Failed to open config file for writing!");
        return false;
    }

    size_t written = serializeJson(doc, file);
    file.close();

    Serial.printf("[CFG] Config saved (%u bytes).\n", (unsigned)written);
    return written > 0;
#else
    return true;
#endif
}

bool ConfigManager::load() {
#ifndef NATIVE_BUILD
    if (!_mounted) return false;

    File file = LittleFS.open(CONFIG_FILE_PATH, "r");
    if (!file) {
        return false;  // File doesn't exist
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        Serial.printf("[CFG] JSON parse error: %s\n", err.c_str());
        return false;
    }

    fromJson(doc);
    return true;
#else
    return false;
#endif
}

void ConfigManager::factoryReset() {
#ifndef NATIVE_BUILD
    Serial.println("[CFG] Factory reset! Deleting config and rebooting...");
    if (_mounted) {
        LittleFS.remove(CONFIG_FILE_PATH);
        LittleFS.remove(SESSION_FILE_PATH);
    }
    delay(500);
    ESP.restart();
#endif
}

void ConfigManager::resetDefaults() {
    applyDefaults();
}

void ConfigManager::setWifiCredentials(const char* ssid, const char* password) {
    strncpy(_config.wifi.ssid, ssid ? ssid : "", CFG_SSID_MAX_LEN - 1);
    _config.wifi.ssid[CFG_SSID_MAX_LEN - 1] = '\0';
    strncpy(_config.wifi.password, password ? password : "", CFG_PASSWORD_MAX_LEN - 1);
    _config.wifi.password[CFG_PASSWORD_MAX_LEN - 1] = '\0';
}

void ConfigManager::setUnits(const char* units) {
    strncpy(_config.units, units ? units : "F", sizeof(_config.units) - 1);
    _config.units[sizeof(_config.units) - 1] = '\0';
}

void ConfigManager::setPidTunings(float kp, float ki, float kd) {
    _config.pid.kp = kp;
    _config.pid.ki = ki;
    _config.pid.kd = kd;
}

void ConfigManager::setFanMode(const char* mode) {
    strncpy(_config.fan.mode, mode ? mode : "fan_and_damper", CFG_NAME_MAX_LEN - 1);
    _config.fan.mode[CFG_NAME_MAX_LEN - 1] = '\0';
}

void ConfigManager::setFanMinSpeed(float minSpeed) {
    _config.fan.minSpeed = minSpeed;
}

void ConfigManager::setFanOnThreshold(float threshold) {
    _config.fan.fanOnThreshold = threshold;
}

const ProbeSettings& ConfigManager::getProbeSettings(uint8_t probe) const {
    if (probe >= 3) return _defaultProbe;
    return _config.probes[probe];
}

void ConfigManager::setProbeName(uint8_t probe, const char* name) {
    if (probe >= 3) return;
    strncpy(_config.probes[probe].name, name ? name : "", CFG_NAME_MAX_LEN - 1);
    _config.probes[probe].name[CFG_NAME_MAX_LEN - 1] = '\0';
}

void ConfigManager::setProbeCoefficients(uint8_t probe, float a, float b, float c) {
    if (probe >= 3) return;
    _config.probes[probe].a = a;
    _config.probes[probe].b = b;
    _config.probes[probe].c = c;
}

void ConfigManager::setProbeOffset(uint8_t probe, float offset) {
    if (probe >= 3) return;
    _config.probes[probe].offset = offset;
}

void ConfigManager::setAlarmPitBand(float band) {
    _config.alarms.pitBand = band;
}

void ConfigManager::setPushoverSettings(bool enabled, const char* userKey, const char* apiToken) {
    _config.alarms.pushover.enabled = enabled;
    strncpy(_config.alarms.pushover.userKey, userKey ? userKey : "", CFG_KEY_MAX_LEN - 1);
    _config.alarms.pushover.userKey[CFG_KEY_MAX_LEN - 1] = '\0';
    strncpy(_config.alarms.pushover.apiToken, apiToken ? apiToken : "", CFG_KEY_MAX_LEN - 1);
    _config.alarms.pushover.apiToken[CFG_KEY_MAX_LEN - 1] = '\0';
}

void ConfigManager::setSetupComplete(bool complete) {
    _config.setupComplete = complete;
}

void ConfigManager::applyDefaults() {
    memset(&_config, 0, sizeof(_config));

    // WiFi: empty strings (unconfigured)
    _config.wifi.ssid[0] = '\0';
    _config.wifi.password[0] = '\0';

    // Units
    strncpy(_config.units, "F", sizeof(_config.units));

    // PID
    _config.pid.kp = PID_KP;
    _config.pid.ki = PID_KI;
    _config.pid.kd = PID_KD;

    // Fan
    strncpy(_config.fan.mode, "fan_and_damper", CFG_NAME_MAX_LEN);
    _config.fan.minSpeed = FAN_MIN_SPEED;
    _config.fan.fanOnThreshold = FAN_ON_THRESHOLD;

    // Probes
    const char* probeNames[] = {"Pit", "Meat 1", "Meat 2"};
    for (int i = 0; i < 3; i++) {
        strncpy(_config.probes[i].name, probeNames[i], CFG_NAME_MAX_LEN - 1);
        _config.probes[i].name[CFG_NAME_MAX_LEN - 1] = '\0';
        _config.probes[i].a = THERM_A;
        _config.probes[i].b = THERM_B;
        _config.probes[i].c = THERM_C;
        _config.probes[i].offset = 0.0f;
    }

    // Alarms
    _config.alarms.pitBand = ALARM_PIT_BAND_DEFAULT;
    _config.alarms.pushover.enabled = false;
    _config.alarms.pushover.userKey[0] = '\0';
    _config.alarms.pushover.apiToken[0] = '\0';

    // Setup
    _config.setupComplete = false;
}

void ConfigManager::toJson(JsonDocument& doc) const {
    // WiFi
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = _config.wifi.ssid;
    wifi["password"] = _config.wifi.password;

    // Units
    doc["units"] = _config.units;

    // PID
    JsonObject pid = doc["pid"].to<JsonObject>();
    pid["p"] = _config.pid.kp;
    pid["i"] = _config.pid.ki;
    pid["d"] = _config.pid.kd;

    // Fan
    JsonObject fan = doc["fan"].to<JsonObject>();
    fan["mode"] = _config.fan.mode;
    fan["minSpeed"] = _config.fan.minSpeed;
    fan["fanOnThreshold"] = _config.fan.fanOnThreshold;

    // Probes
    JsonObject probes = doc["probes"].to<JsonObject>();
    const char* probeKeys[] = {"pit", "meat1", "meat2"};
    for (int i = 0; i < 3; i++) {
        JsonObject p = probes[probeKeys[i]].to<JsonObject>();
        p["name"] = _config.probes[i].name;
        p["a"] = _config.probes[i].a;
        p["b"] = _config.probes[i].b;
        p["c"] = _config.probes[i].c;
        p["offset"] = _config.probes[i].offset;
    }

    // Alarms
    JsonObject alarms = doc["alarms"].to<JsonObject>();
    alarms["pitBand"] = _config.alarms.pitBand;
    JsonObject pushover = alarms["pushover"].to<JsonObject>();
    pushover["enabled"] = _config.alarms.pushover.enabled;
    pushover["userKey"] = _config.alarms.pushover.userKey;
    pushover["apiToken"] = _config.alarms.pushover.apiToken;

    // Setup
    doc["setupComplete"] = _config.setupComplete;
}

void ConfigManager::fromJson(const JsonDocument& doc) {
    // Start from defaults, then overlay with what's in JSON
    applyDefaults();

    // WiFi
    if (doc["wifi"]["ssid"].is<const char*>()) {
        strncpy(_config.wifi.ssid, doc["wifi"]["ssid"].as<const char*>(), CFG_SSID_MAX_LEN - 1);
    }
    if (doc["wifi"]["password"].is<const char*>()) {
        strncpy(_config.wifi.password, doc["wifi"]["password"].as<const char*>(), CFG_PASSWORD_MAX_LEN - 1);
    }

    // Units
    if (doc["units"].is<const char*>()) {
        strncpy(_config.units, doc["units"].as<const char*>(), sizeof(_config.units) - 1);
    }

    // PID
    if (doc["pid"]["p"].is<float>()) _config.pid.kp = doc["pid"]["p"].as<float>();
    if (doc["pid"]["i"].is<float>()) _config.pid.ki = doc["pid"]["i"].as<float>();
    if (doc["pid"]["d"].is<float>()) _config.pid.kd = doc["pid"]["d"].as<float>();

    // Fan
    if (doc["fan"]["mode"].is<const char*>()) {
        strncpy(_config.fan.mode, doc["fan"]["mode"].as<const char*>(), CFG_NAME_MAX_LEN - 1);
    }
    if (doc["fan"]["minSpeed"].is<float>()) _config.fan.minSpeed = doc["fan"]["minSpeed"].as<float>();
    if (doc["fan"]["fanOnThreshold"].is<float>()) _config.fan.fanOnThreshold = doc["fan"]["fanOnThreshold"].as<float>();

    // Probes
    const char* probeKeys[] = {"pit", "meat1", "meat2"};
    for (int i = 0; i < 3; i++) {
        JsonObjectConst p = doc["probes"][probeKeys[i]];
        if (p) {
            if (p["name"].is<const char*>()) {
                strncpy(_config.probes[i].name, p["name"].as<const char*>(), CFG_NAME_MAX_LEN - 1);
            }
            if (p["a"].is<float>()) _config.probes[i].a = p["a"].as<float>();
            if (p["b"].is<float>()) _config.probes[i].b = p["b"].as<float>();
            if (p["c"].is<float>()) _config.probes[i].c = p["c"].as<float>();
            if (p["offset"].is<float>()) _config.probes[i].offset = p["offset"].as<float>();
        }
    }

    // Alarms
    if (doc["alarms"]["pitBand"].is<float>()) {
        _config.alarms.pitBand = doc["alarms"]["pitBand"].as<float>();
    }
    if (doc["alarms"]["pushover"]["enabled"].is<bool>()) {
        _config.alarms.pushover.enabled = doc["alarms"]["pushover"]["enabled"].as<bool>();
    }
    if (doc["alarms"]["pushover"]["userKey"].is<const char*>()) {
        strncpy(_config.alarms.pushover.userKey,
                doc["alarms"]["pushover"]["userKey"].as<const char*>(), CFG_KEY_MAX_LEN - 1);
    }
    if (doc["alarms"]["pushover"]["apiToken"].is<const char*>()) {
        strncpy(_config.alarms.pushover.apiToken,
                doc["alarms"]["pushover"]["apiToken"].as<const char*>(), CFG_KEY_MAX_LEN - 1);
    }

    // Setup
    if (doc["setupComplete"].is<bool>()) {
        _config.setupComplete = doc["setupComplete"].as<bool>();
    }
}
