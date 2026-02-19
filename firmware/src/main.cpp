#ifndef NATIVE_BUILD

#include <Arduino.h>
#include "config.h"
#include "split_range.h"

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
#include "display/ui_init.h"
#include "display/ui_update.h"
#include "display/ui_setup_wizard.h"
#include "display/ui_boot_splash.h"

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
static float    g_prevSetpoint   = 225.0f;   // Previous setpoint for change detection
static bool     g_pitReached     = false;     // Has pit ever reached setpoint?
static uint32_t g_cookStartTime  = 0;         // Epoch when cook timer started
static unsigned long g_lastPidMs = 0;         // Last PID computation timestamp

// --- Boot phase state machine ---
enum class BootPhase { SPLASH, WIZARD, RUNNING };
static BootPhase    g_bootPhase    = BootPhase::SPLASH;
static unsigned long g_wizardDoneMs = 0;

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

// --- WebSocket command callbacks ---
static void ws_onSetpoint(float sp) {
    g_setpoint = sp;
}

static void ws_onAlarm(const char* probe, float target) {
    if (strcmp(probe, "meat1") == 0)      alarmManager.setMeat1Target(target);
    else if (strcmp(probe, "meat2") == 0) alarmManager.setMeat2Target(target);
    else if (strcmp(probe, "pitBand") == 0) alarmManager.setPitBand(target);
}

static void ws_onFanMode(const char* mode) {
    configManager.setFanMode(mode);
    ui_update_settings_state(configManager.isFahrenheit(), configManager.getFanMode());
}

static void ws_onSession(const char* action, const char* format) {
    if (strcmp(action, "new") == 0) {
        cookSession.endSession();
        cookSession.startSession();
        g_cookStartTime = 0;
        g_pitReached = false;
        ui_graph_clear();
    }
}

// --- Display timing ---
static unsigned long g_lastDisplayMs = 0;
static unsigned long g_lastGraphMs   = 0;

// --- UI callbacks ---
static void ui_cb_setpoint(float sp) {
    g_setpoint = sp;
}

static void ui_cb_meat_target(uint8_t probe, float target) {
    if (probe == 1) alarmManager.setMeat1Target(target);
    if (probe == 2) alarmManager.setMeat2Target(target);
}

static void ui_cb_alarm_ack() {
    alarmManager.acknowledge();
}

static void ui_cb_units(bool isFahrenheit) {
    configManager.setUnits(isFahrenheit ? "F" : "C");
    tempManager.setUseFahrenheit(isFahrenheit);
}

static void ui_cb_fan_mode(const char* mode) {
    configManager.setFanMode(mode);
}

static void ui_cb_new_session() {
    cookSession.endSession();
    cookSession.startSession();
    g_cookStartTime = 0;
    g_pitReached = false;
    ui_graph_clear();
}

static void ui_cb_factory_reset() {
    configManager.factoryReset();
    ESP.restart();
}

// --- Setup wizard hardware test (non-blocking) ---
enum class HwTest { NONE, FAN, SERVO, BUZZER };
static HwTest g_hwTest = HwTest::NONE;
static unsigned long g_hwTestStartMs = 0;

static void wiz_fan_test() {
    fanController.setSpeed(75);
    fanController.update();
    g_hwTest = HwTest::FAN;
    g_hwTestStartMs = millis();
}

static void wiz_servo_test() {
    servoController.setPosition(100);
    g_hwTest = HwTest::SERVO;
    g_hwTestStartMs = millis();
}

static void wiz_buzzer_test() {
    alarmManager.setBuzzer(true);
    g_hwTest = HwTest::BUZZER;
    g_hwTestStartMs = millis();
}

static void wiz_complete() {
    configManager.setSetupComplete(true);
    configManager.save();
    Serial.println("[BOOT] Setup wizard complete");
}

static void ui_cb_wifi_action(const char* action) {
    if (strcmp(action, "disconnect") == 0) {
        wifiManager.disconnect();
    } else if (strcmp(action, "reconnect") == 0) {
        wifiManager.reconnect();
    } else if (strcmp(action, "setup_ap") == 0) {
        wifiManager.startAP();
    }
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

    // 3. Initialize display and show boot splash immediately
    //    Splash is visible while remaining hardware modules initialize.
    ui_init();
    ui_boot_splash_init();
    ui_tick(10);
    ui_handler();

    // 4. Initialize I2C bus and temperature probes (ADS1115)
    tempManager.begin();

    // Apply per-probe calibration coefficients and offsets from saved config
    for (uint8_t i = 0; i < NUM_PROBES; i++) {
        const ProbeSettings& ps = configManager.getProbeSettings(i);
        tempManager.setCoefficients(i, ps.a, ps.b, ps.c);
        tempManager.setOffset(i, ps.offset);
    }
    tempManager.setUseFahrenheit(configManager.isFahrenheit());

    // 5. Initialize PID controller with saved tunings
    pidController.begin(cfg.pid.kp, cfg.pid.ki, cfg.pid.kd);

    // 6. Initialize fan PWM output
    fanController.begin();

    // 7. Initialize servo / damper output
    servoController.begin();

    // 8. Initialize alarm manager (buzzer)
    alarmManager.begin();
    alarmManager.setPitBand(cfg.alarms.pitBand);

    // 9. Initialize error detection
    errorManager.begin();

    // 10. Connect WiFi (splash screen visible during connection)
    wifiManager.begin();

    // 11. Start HTTP server and WebSocket, pass module references
    webServer.begin();
    webServer.setModules(&tempManager, &pidController, &fanController,
                         &servoController, &configManager, &cookSession,
                         &alarmManager, &errorManager);
    webServer.onSetpoint(ws_onSetpoint);
    webServer.onAlarm(ws_onAlarm);
    webServer.onSession(ws_onSession);
    webServer.onFanMode(ws_onFanMode);

    // 12. Initialize OTA updates (needs the AsyncWebServer to register /update route)
    otaManager.begin(webServer.getAsyncServer());

    // 13. Recover any existing cook session from flash
    cookSession.begin();
    cookSession.setDataSources(cb_getPitTemp, cb_getMeat1Temp, cb_getMeat2Temp,
                               cb_getFanPct, cb_getDamperPct, cb_getFlags);

    // 14. Wire up dashboard callbacks and set initial state
    ui_set_callbacks(ui_cb_setpoint, ui_cb_meat_target, ui_cb_alarm_ack);
    ui_set_settings_callbacks(ui_cb_units, ui_cb_fan_mode, ui_cb_new_session, ui_cb_factory_reset);
    ui_set_wifi_callback(ui_cb_wifi_action);

    // Set initial display state
    ui_update_setpoint(g_setpoint);
    ui_update_meat1_target(alarmManager.getMeat1Target());
    ui_update_meat2_target(alarmManager.getMeat2Target());
    ui_update_settings_state(configManager.isFahrenheit(), configManager.getFanMode());

    // Pre-populate graph from recovered session data
    {
        uint32_t sessionPoints = cookSession.getTotalPointCount();
        for (uint32_t i = 0; i < sessionPoints; i++) {
            const DataPoint* dp = cookSession.getPoint(i);
            if (dp) {
                ui_graph_add_point(
                    dp->pitTemp / 10.0f,
                    dp->meat1Temp / 10.0f,
                    dp->meat2Temp / 10.0f,
                    g_setpoint,
                    (dp->flags & DP_FLAG_PIT_DISC) != 0,
                    (dp->flags & DP_FLAG_MEAT1_DISC) != 0,
                    (dp->flags & DP_FLAG_MEAT2_DISC) != 0
                );
            }
        }
    }

    // 15. Log "Setup complete" with IP address
    Serial.println();
    Serial.printf("[BOOT] Setup complete. IP: %s\n", wifiManager.getIPAddress());
    Serial.println();

    g_lastPidMs = millis();
    g_lastDisplayMs = millis();
    g_lastGraphMs = millis();
}

// ---------------------------------------------------------------------------
// loop()  — target ~100 Hz (10 ms delay at end); modules gate their own timing
// ---------------------------------------------------------------------------
void loop() {
    unsigned long now = millis();

    // --- Boot splash phase: only process LVGL and splash logic ---
    if (g_bootPhase == BootPhase::SPLASH) {
        ui_boot_splash_update();
        if (!ui_boot_splash_is_active()) {
            if (ui_boot_splash_factory_reset_triggered()) {
                Serial.println("[BOOT] Factory reset triggered from splash");
                configManager.factoryReset();
                ESP.restart();
            }
            if (!configManager.isSetupComplete()) {
                Serial.println("[BOOT] First boot — starting setup wizard");
                ui_boot_splash_cleanup();
                ui_wizard_init();
                ui_wizard_set_callbacks(wiz_fan_test, wiz_servo_test,
                                        wiz_buzzer_test, ui_cb_units, wiz_complete);
                g_bootPhase = BootPhase::WIZARD;
            } else {
                ui_boot_splash_cleanup();
                ui_switch_screen(Screen::DASHBOARD);
                g_bootPhase = BootPhase::RUNNING;
                Serial.println("[BOOT] Entering normal operation");
            }
        }
        ui_tick(10);
        ui_handler();
        delay(10);
        return;
    }

    // --- Setup wizard phase: process LVGL, temp readings, and WiFi ---
    if (g_bootPhase == BootPhase::WIZARD) {
        tempManager.update();
        wifiManager.update();

        // Handle hardware test timeouts (non-blocking)
        if (g_hwTest != HwTest::NONE) {
            unsigned long elapsed = now - g_hwTestStartMs;
            switch (g_hwTest) {
                case HwTest::FAN:
                    fanController.update();
                    if (elapsed >= 1000) {
                        fanController.setSpeed(0);
                        fanController.update();
                        g_hwTest = HwTest::NONE;
                    }
                    break;
                case HwTest::SERVO:
                    if (elapsed >= 500) {
                        servoController.setPosition(0);
                        g_hwTest = HwTest::NONE;
                    }
                    break;
                case HwTest::BUZZER:
                    if (elapsed >= 300) {
                        alarmManager.setBuzzer(false);
                        g_hwTest = HwTest::NONE;
                    }
                    break;
                default: break;
            }
        }

        if (ui_wizard_is_active()) {
            if (now - g_lastDisplayMs >= 1000) {
                g_lastDisplayMs = now;
                ui_wizard_update_probes(
                    tempManager.getPitTemp(),
                    tempManager.getMeat1Temp(),
                    tempManager.getMeat2Temp(),
                    tempManager.isConnected(PROBE_PIT),
                    tempManager.isConnected(PROBE_MEAT1),
                    tempManager.isConnected(PROBE_MEAT2));
            }
        } else {
            // Wizard finished — show Done screen for 2 seconds then go to dashboard
            if (g_wizardDoneMs == 0) {
                g_wizardDoneMs = now;
            } else if (now - g_wizardDoneMs >= 2000) {
                ui_switch_screen(Screen::DASHBOARD);
                g_bootPhase = BootPhase::RUNNING;
                Serial.println("[BOOT] Entering normal operation");
            }
        }
        ui_tick(10);
        ui_handler();
        delay(10);
        return;
    }

    // --- Normal running phase ---
    // 1. Read temperatures from all probes (internally gated at TEMP_SAMPLE_INTERVAL_MS)
    tempManager.update();

    // 2. PID computation (every PID_SAMPLE_MS)
    if (now - g_lastPidMs >= PID_SAMPLE_MS) {
        g_lastPidMs = now;

        // Reset integrator on setpoint change for bumpless transfer
        if (g_setpoint != g_prevSetpoint) {
            pidController.resetIntegrator();
            g_pitReached = false;  // Suppress pit-band alarms during ramp to new setpoint
            g_prevSetpoint = g_setpoint;
        }

        // Only compute PID when pit probe is connected. When disconnected,
        // _pidOutput retains its last value to maintain current fire management.
        if (tempManager.isConnected(PROBE_PIT)) {
            float pitTemp = tempManager.getPitTemp();
            pidController.compute(pitTemp, g_setpoint);

            // Track whether pit has ever reached setpoint (within 5 degrees F).
            if (!g_pitReached) {
                if (fabsf(pitTemp - g_setpoint) <= 5.0f) {
                    g_pitReached = true;
                }
            }
        }
    }

    // 3. Mode-aware fan + damper from PID output (split-range coordination)
    {
        SplitRangeOutput sr = splitRange(pidController.getOutput(),
                                         configManager.getFanMode(),
                                         configManager.getFanOnThreshold());
        servoController.setPosition(sr.damperPercent);
        fanController.setSpeed(sr.fanPercent);
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

    // 11. LVGL display update (~1 Hz for data, ~5s for graph)
    if (now - g_lastDisplayMs >= 1000) {
        g_lastDisplayMs = now;

        ui_update_temps(tempManager.getPitTemp(),
                        tempManager.getMeat1Temp(),
                        tempManager.getMeat2Temp(),
                        tempManager.isConnected(PROBE_PIT),
                        tempManager.isConnected(PROBE_MEAT1),
                        tempManager.isConnected(PROBE_MEAT2));

        ui_update_setpoint(g_setpoint);
        ui_update_output_bars(fanController.getCurrentSpeedPct(),
                              servoController.getCurrentPositionPct());

        // Cook timer — starts when first meat probe connects
        if (g_cookStartTime == 0) {
            if (tempManager.isConnected(PROBE_MEAT1) || tempManager.isConnected(PROBE_MEAT2)) {
                g_cookStartTime = (uint32_t)(millis() / 1000);
            }
        }
        {
            uint32_t elapsed = g_cookStartTime > 0
                ? (uint32_t)(millis() / 1000) - g_cookStartTime
                : 0;
            ui_update_cook_timer(0, elapsed, 0);
        }

        // WiFi status
        ui_update_wifi(wifiManager.isConnected() || wifiManager.isAPMode());

        // WiFi info on settings screen
        {
            static String ssidStr, ipStr;
            ssidStr = wifiManager.getSSID();
            ipStr = wifiManager.getIPAddress();
            WifiInfo winfo;
            winfo.connected = wifiManager.isConnected();
            winfo.apMode = wifiManager.isAPMode();
            winfo.ssid = ssidStr.c_str();
            winfo.ip = ipStr.c_str();
            winfo.rssi = wifiManager.getRSSI();
            ui_update_wifi_info(winfo);
        }

        // Alerts
        AlarmType activeAlarms[MAX_ACTIVE_ALARMS];
        uint8_t alarmCount = alarmManager.getActiveAlarms(activeAlarms, MAX_ACTIVE_ALARMS);
        uint8_t topAlarm = 0;
        for (uint8_t i = 0; i < alarmCount; i++) {
            topAlarm = (uint8_t)activeAlarms[i];
            break; // Take the first active alarm
        }
        uint8_t probeErrors = 0;
        if (tempManager.getStatus(PROBE_PIT) != ProbeStatus::OK)   probeErrors |= 0x01;
        if (tempManager.getStatus(PROBE_MEAT1) != ProbeStatus::OK) probeErrors |= 0x02;
        if (tempManager.getStatus(PROBE_MEAT2) != ProbeStatus::OK) probeErrors |= 0x04;
        ui_update_alerts(topAlarm, pidController.isLidOpen(), errorManager.isFireOut(), probeErrors);

        // Meat targets
        ui_update_meat1_target(alarmManager.getMeat1Target());
        ui_update_meat2_target(alarmManager.getMeat2Target());
    }

    // Graph update (every 5 seconds)
    if (now - g_lastGraphMs >= 5000) {
        g_lastGraphMs = now;
        ui_graph_add_point(tempManager.getPitTemp(),
                           tempManager.getMeat1Temp(),
                           tempManager.getMeat2Temp(),
                           g_setpoint,
                           !tempManager.isConnected(PROBE_PIT),
                           !tempManager.isConnected(PROBE_MEAT1),
                           !tempManager.isConnected(PROBE_MEAT2));
    }

    // 12. LVGL tick and task handler
    ui_tick(10);
    ui_handler();

    // Yield to FreeRTOS / keep loop at ~100 Hz
    delay(10);
}

#endif // NATIVE_BUILD
