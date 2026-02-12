#pragma once

#include "config.h"
#include <stdint.h>

#ifndef NATIVE_BUILD
#include <LittleFS.h>
#include <ArduinoJson.h>
#endif

// Maximum string lengths for config fields
#define CFG_SSID_MAX_LEN     64
#define CFG_PASSWORD_MAX_LEN 64
#define CFG_NAME_MAX_LEN     32
#define CFG_KEY_MAX_LEN      64

// Probe config within the overall Config struct
struct ProbeSettings {
    char  name[CFG_NAME_MAX_LEN];
    float a;        // Steinhart-Hart A
    float b;        // Steinhart-Hart B
    float c;        // Steinhart-Hart C
    float offset;   // Calibration offset in degrees
};

// Pushover notification settings
struct PushoverSettings {
    bool enabled;
    char userKey[CFG_KEY_MAX_LEN];
    char apiToken[CFG_KEY_MAX_LEN];
};

// Alarm settings
struct AlarmSettings {
    float pitBand;  // +/- deviation from setpoint (in configured units)
    PushoverSettings pushover;
};

// PID settings
struct PidSettings {
    float kp;
    float ki;
    float kd;
};

// Fan settings
struct FanSettings {
    char  mode[CFG_NAME_MAX_LEN];  // "fan_only", "fan_and_damper", "damper_primary"
    float minSpeed;
    float fanOnThreshold;
};

// WiFi settings
struct WifiSettings {
    char ssid[CFG_SSID_MAX_LEN];
    char password[CFG_PASSWORD_MAX_LEN];
};

// Complete configuration structure matching config.json schema
struct AppConfig {
    WifiSettings    wifi;
    char            units[4];     // "F" or "C"
    PidSettings     pid;
    FanSettings     fan;
    ProbeSettings   probes[3];    // pit, meat1, meat2
    AlarmSettings   alarms;
    bool            setupComplete;
};

class ConfigManager {
public:
    ConfigManager();

    // Mount LittleFS and load config from file. Call once from setup().
    bool begin();

    // Save current config to LittleFS
    bool save();

    // Reload config from LittleFS into RAM
    bool load();

    // Reset all settings to compiled-in defaults, save, and reboot
    void factoryReset();

    // Reset to defaults without reboot
    void resetDefaults();

    // Get read-only reference to the full config
    const AppConfig& getConfig() const { return _config; }

    // Get mutable reference to the full config (for bulk updates)
    AppConfig& getConfigMutable() { return _config; }

    // --- WiFi ---
    const char* getWifiSSID() const { return _config.wifi.ssid; }
    const char* getWifiPassword() const { return _config.wifi.password; }
    void setWifiCredentials(const char* ssid, const char* password);

    // --- Units ---
    const char* getUnits() const { return _config.units; }
    bool isFahrenheit() const { return _config.units[0] == 'F'; }
    void setUnits(const char* units);

    // --- PID ---
    float getPidKp() const { return _config.pid.kp; }
    float getPidKi() const { return _config.pid.ki; }
    float getPidKd() const { return _config.pid.kd; }
    void setPidTunings(float kp, float ki, float kd);

    // --- Fan ---
    const char* getFanMode() const { return _config.fan.mode; }
    float getFanMinSpeed() const { return _config.fan.minSpeed; }
    float getFanOnThreshold() const { return _config.fan.fanOnThreshold; }
    void setFanMode(const char* mode);
    void setFanMinSpeed(float minSpeed);
    void setFanOnThreshold(float threshold);

    // --- Probes ---
    const ProbeSettings& getProbeSettings(uint8_t probe) const;
    void setProbeName(uint8_t probe, const char* name);
    void setProbeCoefficients(uint8_t probe, float a, float b, float c);
    void setProbeOffset(uint8_t probe, float offset);

    // --- Alarms ---
    float getAlarmPitBand() const { return _config.alarms.pitBand; }
    void setAlarmPitBand(float band);
    const PushoverSettings& getPushoverSettings() const { return _config.alarms.pushover; }
    void setPushoverSettings(bool enabled, const char* userKey, const char* apiToken);

    // --- Setup ---
    bool isSetupComplete() const { return _config.setupComplete; }
    void setSetupComplete(bool complete);

private:
    // Set all fields to compiled-in defaults
    void applyDefaults();

    // Serialize config to JSON document
    void toJson(JsonDocument& doc) const;

    // Deserialize JSON document to config
    void fromJson(const JsonDocument& doc);

    AppConfig _config;
    bool      _mounted;   // Whether LittleFS is mounted

    // Static default probe settings (for getProbeSettings fallback)
    static const ProbeSettings _defaultProbe;
};
