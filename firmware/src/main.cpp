#ifndef NATIVE_BUILD

#include <Arduino.h>
#include "config.h"

// --- Module headers ---
#include "temp_manager.h"
#include "pid_controller.h"
#include "fan_controller.h"
#include "servo_controller.h"
#include "config_manager.h"
#include "cook_session.h"
#include "alarm_manager.h"
#include "error_manager.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "ota_manager.h"

// --- Module instances ---
TempManager     tempManager;
PidController   pidController;
FanController   fanController;
ServoController servoController;
ConfigManager   configManager;
CookSession     cookSession;
AlarmManager    alarmManager;
ErrorManager    errorManager;
WifiManager     wifiManager;
BBQWebServer    webServer;
OtaManager      otaManager;

// --- Control state ---
static float    g_setpoint       = 225.0f;   // Default pit setpoint (degrees F)
static bool     g_pitReached     = false;     // Has pit ever reached setpoint?
static uint32_t g_cookStartTime  = 0;         // Epoch when cook timer started
static unsigned long g_lastPidMs = 0;         // Last PID computation timestamp

// --- CookSession data-source callbacks ---
// These free functions bridge the global module instances into the function-pointer
// interface that CookSession::setDataSources() expects.

static float cb_getPitTemp()   { return tempManager.getPitTemp(); }
static float cb_getMeat1Temp() { return tempManager.getMeat1Temp(); }
static float cb_getMeat2Temp() { return tempManager.getMeat2Temp(); }

static uint8_t cb_getFanPct() {
    return static_cast<uint8_t>(fanController.getCurrentSpeedPct());
}

static uint8_t cb_getDamperPct() {
    return static_cast<uint8_t>(servoController.getCurrentPositionPct());
}

static uint8_t cb_getFlags() {
    uint8_t flags = 0;
    if (pidController.isLidOpen())                    flags |= DP_FLAG_LID_OPEN;
    if (!tempManager.isConnected(PROBE_PIT))          flags |= DP_FLAG_PIT_DISC;
    if (!tempManager.isConnected(PROBE_MEAT1))        flags |= DP_FLAG_MEAT1_DISC;
    if (!tempManager.isConnected(PROBE_MEAT2))        flags |= DP_FLAG_MEAT2_DISC;
    if (errorManager.isFireOut())                     flags |= DP_FLAG_ERROR_FIREOUT;

    AlarmType activeAlarms[MAX_ACTIVE_ALARMS];
    uint8_t count = alarmManager.getActiveAlarms(activeAlarms, MAX_ACTIVE_ALARMS);
    for (uint8_t i = 0; i < count; i++) {
        if (activeAlarms[i] == AlarmType::PIT_HIGH ||
            activeAlarms[i] == AlarmType::PIT_LOW)    flags |= DP_FLAG_ALARM_PIT;
        if (activeAlarms[i] == AlarmType::MEAT1_DONE) flags |= DP_FLAG_ALARM_MEAT1;
        if (activeAlarms[i] == AlarmType::MEAT2_DONE) flags |= DP_FLAG_ALARM_MEAT2;
    }
    return flags;
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    // 1. Serial
    Serial.begin(115200);
    delay(500);  // Allow USB-CDC to connect

    Serial.println();
    Serial.println("========================================");
    Serial.printf("  Pit Claw v%s\n", FIRMWARE_VERSION);
    Serial.println("  Board: WT32-SC01 Plus (ESP32-S3)");
    Serial.println("========================================");
    Serial.println();

    // 2. Load configuration from LittleFS
    configManager.begin();
    const AppConfig& cfg = configManager.getConfig();

    // 3. Initialize I2C bus and temperature probes (ADS1115)
    tempManager.begin();

    // Apply per-probe calibration coefficients and offsets from saved config
    for (uint8_t i = 0; i < NUM_PROBES; i++) {
        const ProbeSettings& ps = configManager.getProbeSettings(i);
        tempManager.setCoefficients(i, ps.a, ps.b, ps.c);
        tempManager.setOffset(i, ps.offset);
    }
    tempManager.setUseFahrenheit(configManager.isFahrenheit());

    // 4. Initialize PID controller with saved tunings
    pidController.begin(cfg.pid.kp, cfg.pid.ki, cfg.pid.kd);

    // 5. Initialize fan PWM output
    fanController.begin();

    // 6. Initialize servo / damper output
    servoController.begin();

    // 7. Initialize alarm manager (buzzer)
    alarmManager.begin();
    alarmManager.setPitBand(cfg.alarms.pitBand);

    // 8. Initialize error detection
    errorManager.begin();

    // 9. Connect WiFi (STA or AP fallback based on config)
    wifiManager.begin();

    // 10. Start HTTP server and WebSocket, pass module references
    webServer.begin();
    webServer.setModules(&tempManager, &pidController, &fanController,
                         &servoController, &configManager, &cookSession,
                         &alarmManager, &errorManager);

    // 11. Initialize OTA updates (needs the AsyncWebServer to register /update route)
    otaManager.begin(webServer.getAsyncServer());

    // 12. Recover any existing cook session from flash
    cookSession.begin();
    cookSession.setDataSources(cb_getPitTemp, cb_getMeat1Temp, cb_getMeat2Temp,
                               cb_getFanPct, cb_getDamperPct, cb_getFlags);

    // 13. Log "Setup complete" with IP address
    Serial.println();
    Serial.printf("[BOOT] Setup complete. IP: %s\n", wifiManager.getIPAddress());
    Serial.println();

    g_lastPidMs = millis();
}

// ---------------------------------------------------------------------------
// loop()  — target ~100 Hz (10 ms delay at end); modules gate their own timing
// ---------------------------------------------------------------------------
void loop() {
    unsigned long now = millis();

    // 1. Read temperatures from all probes (internally gated at TEMP_SAMPLE_INTERVAL_MS)
    tempManager.update();

    // 2. PID computation (every PID_SAMPLE_MS)
    if (now - g_lastPidMs >= PID_SAMPLE_MS) {
        g_lastPidMs = now;

        float pitTemp = tempManager.getPitTemp();
        pidController.compute(pitTemp, g_setpoint);

        // Track whether pit has ever reached setpoint (within 5 degrees F).
        // Once set, stays true for the duration of the cook so that ramp-up
        // alarms are suppressed until initial approach is complete.
        if (!g_pitReached && tempManager.isConnected(PROBE_PIT)) {
            if (fabsf(pitTemp - g_setpoint) <= 5.0f) {
                g_pitReached = true;
            }
        }
    }

    // 3. Split-range fan + damper from PID output
    {
        float pidOutput = pidController.getOutput();

        // Damper: linear map of PID output 0-100%
        servoController.setPosition(pidOutput);

        // Fan: only activates above FAN_ON_THRESHOLD
        float fanPct = 0.0f;
        if (pidOutput > static_cast<float>(FAN_ON_THRESHOLD)) {
            // Scale the portion above threshold into full fan range 0-100%
            fanPct = (pidOutput - static_cast<float>(FAN_ON_THRESHOLD))
                   / (100.0f - static_cast<float>(FAN_ON_THRESHOLD))
                   * 100.0f;
        }
        fanController.setSpeed(fanPct);
    }

    // 4. Fan controller update (kick-start timing, long-pulse cycling)
    fanController.update();

    // 5. Alarm manager
    alarmManager.update(tempManager.getPitTemp(),
                        tempManager.getMeat1Temp(),
                        tempManager.getMeat2Temp(),
                        g_setpoint,
                        g_pitReached);

    // 6. Error manager
    {
        ProbeState probeStates[NUM_PROBES];
        for (uint8_t i = 0; i < NUM_PROBES; i++) {
            ProbeStatus st = tempManager.getStatus(i);
            probeStates[i].connected    = tempManager.isConnected(i);
            probeStates[i].openCircuit  = (st == ProbeStatus::OPEN_CIRCUIT);
            probeStates[i].shortCircuit = (st == ProbeStatus::SHORT_CIRCUIT);
            probeStates[i].temperature  = tempManager.getTemp(i);
        }
        errorManager.update(tempManager.getPitTemp(),
                            fanController.getCurrentSpeedPct(),
                            probeStates);
    }

    // 7. Cook session update (auto-samples and flushes on its own timers)
    cookSession.update();

    // 8. Web server update (broadcasts to WebSocket clients at WS_SEND_INTERVAL)
    webServer.setSetpoint(g_setpoint);
    webServer.update();

    // 9. WiFi manager (handles reconnection)
    wifiManager.update();

    // 10. OTA manager (handles OTA progress)
    otaManager.update();

    // Yield to FreeRTOS / keep loop at ~100 Hz
    delay(10);
}

#endif // NATIVE_BUILD
