// LVGL Desktop Simulator for Pit Claw BBQ Controller
//
// Renders the touchscreen UI in an SDL2 window with simulated cook data.
// Usage:
//   pio run -e simulator && .pio/build/simulator/program
//   .pio/build/simulator/program --speed 10       # 10x time acceleration
//   .pio/build/simulator/program --profile stall  # brisket stall scenario
//   .pio/build/simulator/program --wizard         # test setup wizard flow

#ifdef SIMULATOR_BUILD

#include <SDL2/SDL.h>
#include <lvgl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "../display/ui_init.h"
#include "../display/ui_update.h"
#include "../display/ui_setup_wizard.h"
#include "../display/ui_boot_splash.h"
#include "../web_protocol.h"
#include "../units.h"
#include "sim_thermal.h"
#include "sim_profiles.h"
#include "sim_web_server.h"

// Simulator-local state
static SimThermalModel* g_model = nullptr;
static SimWebServer* g_webServer = nullptr;
static float g_meat1_target = 203;
static float g_meat2_target = 0;
static bool  g_alarm_active = false;
static uint8_t g_alarm_type = 0;
static bool  g_alarm_acked = false;
static bool  g_is_fahrenheit = true;
static char  g_fan_mode[20] = "fan_and_damper";
static double g_sessionStartSimTime = 0;
static uint32_t g_simStartTs = 0;
static SimProfile* g_activeProfile = nullptr;

// Simulator boot phase
enum class SimPhase { SPLASH, WIZARD, WIZARD_DONE, RUNNING };
static bool g_factory_reset_requested = false;

// Persistent setup state (mirrors configManager.isSetupComplete() on real hardware)
static const char* SIM_SETUP_FILE = ".sim_setup_complete";

static bool sim_is_setup_complete() {
    FILE* f = fopen(SIM_SETUP_FILE, "r");
    if (f) { fclose(f); return true; }
    return false;
}

static void sim_set_setup_complete() {
    FILE* f = fopen(SIM_SETUP_FILE, "w");
    if (f) fclose(f);
}

static void sim_clear_setup() {
    remove(SIM_SETUP_FILE);
}

static float display_temp(float f) {
    return g_is_fahrenheit ? f : fahrenheitToCelsius(f);
}

// --------------------------------------------------------------------------
// UI callbacks — wired to thermal model and local state
// --------------------------------------------------------------------------

static void on_setpoint(float sp) {
    if (g_model) {
        g_model->setpoint = sp;
        printf("[SIM] Setpoint changed to %.0f via touchscreen\n", sp);
    }
}

static void on_meat_target(uint8_t probe, float target) {
    if (probe == 1) {
        g_meat1_target = target;
        ui_update_meat1_target(target);
        printf("[SIM] Meat1 target set to %.0f\n", target);
    } else if (probe == 2) {
        g_meat2_target = target;
        ui_update_meat2_target(target);
        printf("[SIM] Meat2 target set to %.0f\n", target);
    }
    // Reset alarm state when target changes
    g_alarm_active = false;
    g_alarm_acked = false;
    g_alarm_type = 0;
}

static void on_alarm_ack() {
    g_alarm_acked = true;
    g_alarm_active = false;
    g_alarm_type = 0;
    printf("[SIM] Alarm acknowledged\n");
}

static void on_units(bool isFahrenheit) {
    g_is_fahrenheit = isFahrenheit;
    ui_set_units(isFahrenheit);

    // Re-display all temperatures in the new unit
    if (g_model) {
        ui_update_setpoint(display_temp(g_model->setpoint));
        ui_update_temps(display_temp(g_model->pitTemp),
                        display_temp(g_model->meat1Temp),
                        display_temp(g_model->meat2Temp),
                        true, g_model->meat1Connected, g_model->meat2Connected);
    }
    ui_update_meat1_target(g_meat1_target > 0 ? display_temp(g_meat1_target) : 0);
    ui_update_meat2_target(g_meat2_target > 0 ? display_temp(g_meat2_target) : 0);

    printf("[SIM] Units changed to %s\n", isFahrenheit ? "F" : "C");
}

static void on_fan_mode(const char* mode) {
    strncpy(g_fan_mode, mode, sizeof(g_fan_mode) - 1);
    g_fan_mode[sizeof(g_fan_mode) - 1] = '\0';
    if (g_model) g_model->setFanMode(mode);
    printf("[SIM] Fan mode changed to %s\n", mode);
}

static void reset_session() {
    // Re-init thermal model from cold
    if (g_model && g_activeProfile) {
        g_model->init(*g_activeProfile);
    }

    // Reset targets to profile defaults
    if (g_activeProfile) {
        g_meat1_target = g_activeProfile->meat1Target;
        g_meat2_target = g_activeProfile->meat2Target;
    }

    // Clear UI
    ui_graph_clear();
    g_sessionStartSimTime = 0;
    g_simStartTs = (uint32_t)time(nullptr);

    // Reset alarms
    g_alarm_active = false;
    g_alarm_acked = false;
    g_alarm_type = 0;

    // Update touchscreen UI to reflect reset state
    if (g_model) {
        ui_update_setpoint(g_model->setpoint);
    }
    ui_update_meat1_target(g_meat1_target);
    ui_update_meat2_target(g_meat2_target);

    // Sync web server state
    if (g_webServer) {
        g_webServer->setState(
            g_model ? g_model->setpoint : 225,
            g_meat1_target, g_meat2_target);
    }
}

static void on_new_session() {
    reset_session();
    if (g_webServer) g_webServer->resetSession();
    printf("[SIM] New session started via touchscreen\n");
}

static void on_factory_reset() {
    sim_clear_setup();
    g_factory_reset_requested = true;
    printf("[SIM] Factory reset — restarting setup wizard\n");
}

static void on_wifi_action(const char* action) {
    printf("[SIM] Wi-Fi action: %s (no-op in simulator)\n", action);
}

// --------------------------------------------------------------------------
// Setup wizard callbacks (simulator stubs)
// --------------------------------------------------------------------------

static void wiz_sim_fan_test() {
    printf("[SIM] Wizard: fan test (simulated)\n");
}

static void wiz_sim_servo_test() {
    printf("[SIM] Wizard: servo test (simulated)\n");
}

static void wiz_sim_buzzer_test() {
    printf("[SIM] Wizard: buzzer test (simulated)\n");
}

static void wiz_sim_units(bool isFahrenheit) {
    g_is_fahrenheit = isFahrenheit;
    ui_set_units(isFahrenheit);
    printf("[SIM] Wizard: units set to %s\n", isFahrenheit ? "F" : "C");
}

static void wiz_sim_complete() {
    sim_set_setup_complete();
    printf("[SIM] Wizard: setup complete (saved to %s)\n", SIM_SETUP_FILE);
}

// --------------------------------------------------------------------------
// Profile lookup and usage
// --------------------------------------------------------------------------

static SimProfile* find_profile(const char* name) {
    for (int i = 0; i < sim_profile_count; i++) {
        if (strcmp(sim_profiles[i].key, name) == 0) {
            return sim_profiles[i].profile;
        }
    }
    return nullptr;
}

// Web server callbacks (must be static free functions)
static void web_on_setpoint(float sp) {
    if (g_model) {
        g_model->setpoint = sp;
        printf("[WEB] Setpoint changed to %.0f\n", sp);
    }
}

static void web_on_alarm(const char* probe, float target) {
    if (strcmp(probe, "meat1") == 0) {
        g_meat1_target = target;
        ui_update_meat1_target(target);
        printf("[WEB] Meat1 target set to %.0f\n", target);
    } else if (strcmp(probe, "meat2") == 0) {
        g_meat2_target = target;
        ui_update_meat2_target(target);
        printf("[WEB] Meat2 target set to %.0f\n", target);
    }
    g_alarm_active = false;
    g_alarm_acked = false;
    g_alarm_type = 0;
}

static void web_on_fan_mode(const char* mode) {
    strncpy(g_fan_mode, mode, sizeof(g_fan_mode) - 1);
    g_fan_mode[sizeof(g_fan_mode) - 1] = '\0';
    if (g_model) g_model->setFanMode(mode);
    ui_update_settings_state(g_is_fahrenheit, g_fan_mode);
    printf("[WEB] Fan mode changed to %s\n", mode);
}

static void web_on_new_session() {
    reset_session();
    printf("[WEB] New session started via web UI\n");
}

static void print_usage(const char* prog) {
    printf("Pit Claw LVGL Simulator\n\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --speed N      Time acceleration factor (default: 5)\n");
    printf("  --profile NAME Cook profile (default: normal)\n");
    printf("  --port N       Web server port (default: 3000)\n");
    printf("  --wizard       Force setup wizard (resets saved setup state)\n");
    printf("\nAvailable profiles:\n");
    for (int i = 0; i < sim_profile_count; i++) {
        printf("  %-18s %s\n", sim_profiles[i].key, sim_profiles[i].profile->name);
    }
}

// --------------------------------------------------------------------------
// Alarm simulation
// --------------------------------------------------------------------------

static void check_alarms(const SimResult& result) {
    if (g_alarm_acked) return;

    uint8_t newAlarm = 0;

    // Check meat1 target
    if (g_meat1_target > 0 && result.meat1Connected && result.meat1Temp >= g_meat1_target) {
        newAlarm = 3; // MEAT1_DONE
    }
    // Check meat2 target
    if (g_meat2_target > 0 && result.meat2Connected && result.meat2Temp >= g_meat2_target) {
        newAlarm = 4; // MEAT2_DONE
    }

    if (newAlarm && !g_alarm_active) {
        g_alarm_active = true;
        g_alarm_type = newAlarm;
        printf("[SIM] ALARM: %s\n", newAlarm == 3 ? "Meat 1 done!" : "Meat 2 done!");
    }
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    int speed = 5;
    int webPort = 3000;
    const char* profileName = "normal";
    bool forceWizard = false;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--speed") == 0 && i + 1 < argc) {
            speed = atoi(argv[++i]);
            if (speed < 1) speed = 1;
        } else if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
            profileName = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            webPort = atoi(argv[++i]);
            if (webPort < 1 || webPort > 65535) webPort = 3000;
        } else if (strcmp(argv[i], "--wizard") == 0) {
            forceWizard = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    SimProfile* profile = find_profile(profileName);
    if (!profile) {
        fprintf(stderr, "Unknown profile: %s\n", profileName);
        print_usage(argv[0]);
        return 1;
    }

    // Determine wizard mode: --wizard forces it, otherwise check persistent state
    if (forceWizard) sim_clear_setup();
    bool wizardMode = forceWizard || !sim_is_setup_complete();

    g_activeProfile = profile;

    printf("Pit Claw Simulator - Profile: %s, Speed: %dx%s\n",
           profile->name, speed, wizardMode ? ", Wizard mode" : "");
    printf("  Web UI: http://localhost:%d\n", webPort);

    ui_init();

    // Wire up dashboard callbacks
    ui_set_callbacks(on_setpoint, on_meat_target, on_alarm_ack);
    ui_set_settings_callbacks(on_units, on_fan_mode, on_new_session, on_factory_reset);
    ui_set_wifi_callback(on_wifi_action);

    // Initialize thermal model
    SimThermalModel model;
    model.init(*profile);
    model.setFanMode(g_fan_mode);
    g_model = &model;

    // Set initial meat target from profile
    g_meat1_target = profile->meat1Target;
    g_meat2_target = profile->meat2Target;

    // Set initial UI state
    ui_update_setpoint(model.setpoint);
    ui_update_meat1_target(g_meat1_target);
    ui_update_meat2_target(g_meat2_target);
    ui_update_settings_state(true, "fan_and_damper");

    // Initialize web server for browser-based UI
    SimWebServer webServer;
    g_webServer = &webServer;
    webServer.begin(webPort, "data");
    webServer.onSetpoint(web_on_setpoint);
    webServer.onAlarm(web_on_alarm);
    webServer.onNewSession(web_on_new_session);
    webServer.onFanMode(web_on_fan_mode);
    webServer.setState(model.setpoint, g_meat1_target, g_meat2_target);

    // Boot phase: wizard mode starts with splash, normal mode goes straight to running
    SimPhase simPhase = wizardMode ? SimPhase::SPLASH : SimPhase::RUNNING;
    uint32_t wizardDoneMs = 0;

    if (wizardMode) {
        ui_boot_splash_init();
        printf("[SIM] Showing boot splash (hold 10s for factory reset, or wait 2s)\n");
    }

    // Main loop timing
    g_simStartTs = (uint32_t)time(nullptr); // real clock at sim start
    uint32_t lastUpdate = SDL_GetTicks();
    uint32_t lastGraph = SDL_GetTicks();
    bool running = true;

    while (running) {
        uint32_t now = SDL_GetTicks();

        // --- Splash phase: wait for auto-dismiss or factory reset hold ---
        if (simPhase == SimPhase::SPLASH) {
            ui_boot_splash_update();
            if (!ui_boot_splash_is_active()) {
                if (ui_boot_splash_factory_reset_triggered()) {
                    sim_clear_setup();
                    printf("[SIM] Factory reset triggered from splash\n");
                }
                ui_boot_splash_cleanup();
                // Start the setup wizard
                ui_wizard_init();
                ui_wizard_set_callbacks(wiz_sim_fan_test, wiz_sim_servo_test,
                                        wiz_sim_buzzer_test, wiz_sim_units, wiz_sim_complete);
                simPhase = SimPhase::WIZARD;
                printf("[SIM] Setup wizard started\n");
            }
        }
        // --- Wizard phase: run wizard, show simulated probe readings ---
        else if (simPhase == SimPhase::WIZARD) {
            if (ui_wizard_is_active()) {
                // Update probe readings at ~1Hz with initial thermal model temps
                if (now - lastUpdate >= 1000) {
                    lastUpdate = now;
                    ui_wizard_update_probes(
                        display_temp(model.pitTemp),
                        display_temp(model.meat1Temp),
                        display_temp(model.meat2Temp),
                        true, model.meat1Connected, model.meat2Connected);
                }
            } else {
                // Wizard finished — transition to done
                wizardDoneMs = now;
                simPhase = SimPhase::WIZARD_DONE;
                printf("[SIM] Wizard complete, showing Done screen for 2s\n");
            }
        }
        // --- Wizard done: wait 2 seconds then switch to dashboard ---
        else if (simPhase == SimPhase::WIZARD_DONE) {
            if (now - wizardDoneMs >= 2000) {
                ui_switch_screen(Screen::DASHBOARD);
                simPhase = SimPhase::RUNNING;
                // Reset timing so thermal sim starts fresh
                lastUpdate = now;
                lastGraph = now;
                g_simStartTs = (uint32_t)time(nullptr);
                printf("[SIM] Entering normal simulation mode\n");
            }
        }
        // --- Running phase: normal thermal simulation ---
        else {
            // Handle factory reset request from settings screen
            if (g_factory_reset_requested) {
                g_factory_reset_requested = false;
                ui_boot_splash_init();
                simPhase = SimPhase::SPLASH;
                printf("[SIM] Showing boot splash (hold 10s for factory reset, or wait 2s)\n");
                continue;
            }

            // Update thermal model and UI every second of real time
            if (now - lastUpdate >= 1000) {
                float dt = (float)speed;
                SimResult result = model.update(dt);

                // Dashboard temperatures (converted to display units)
                ui_update_temps(display_temp(result.pitTemp),
                                display_temp(result.meat1Temp),
                                display_temp(result.meat2Temp),
                                true, result.meat1Connected, result.meat2Connected);

                // Output bars — model already applies split-range logic
                ui_update_output_bars(result.fanPercent, result.damperPercent);

                // Setpoint (may have changed via events)
                ui_update_setpoint(display_temp(model.setpoint));

                // Cook timer (session-relative)
                ui_update_cook_timer(0, (uint32_t)(model.simTime - g_sessionStartSimTime), 0);

                // WiFi (simulated as connected in simulator)
                ui_update_wifi(true);
                {
                    WifiInfo winfo;
                    winfo.connected = true;
                    winfo.apMode = false;
                    winfo.ssid = "Simulator";
                    winfo.ip = "localhost";
                    winfo.rssi = -42;
                    ui_update_wifi_info(winfo);
                }

                // Check and display alarms
                check_alarms(result);
                ui_update_alerts(
                    g_alarm_active ? g_alarm_type : 0,
                    result.lidOpen,
                    result.fireOut,
                    0  // no probe errors in basic sim
                );

                // Broadcast data to web clients and accumulate history
                webServer.setState(model.setpoint, g_meat1_target, g_meat2_target);
                {
                    bbq_protocol::DataPayload payload;
                    memset(&payload, 0, sizeof(payload));
                    payload.ts = g_simStartTs + (uint32_t)(model.simTime - g_sessionStartSimTime);
                    payload.pit   = result.pitTemp;
                    payload.meat1 = result.meat1Connected ? result.meat1Temp : NAN;
                    payload.meat2 = result.meat2Connected ? result.meat2Temp : NAN;
                    payload.fan   = (uint8_t)result.fanPercent;
                    payload.damper = (uint8_t)result.damperPercent;
                    payload.sp    = model.setpoint;
                    payload.lid   = result.lidOpen;
                    payload.meat1Target = g_meat1_target;
                    payload.meat2Target = g_meat2_target;
                    payload.est   = 0;
                    payload.fanMode = g_fan_mode;
                    payload.errorCount = 0;
                    webServer.broadcastData(payload);

                    // Accumulate for history replay
                    bbq_protocol::HistoryPoint hp;
                    hp.ts     = payload.ts;
                    hp.pit    = payload.pit;
                    hp.meat1  = payload.meat1;
                    hp.meat2  = payload.meat2;
                    hp.fan    = payload.fan;
                    hp.damper = payload.damper;
                    hp.sp     = payload.sp;
                    hp.lid    = payload.lid;
                    webServer.addHistoryPoint(hp);
                }

                lastUpdate = now;
            }

            // Update graph less frequently (every 5 real seconds)
            if (now - lastGraph >= 5000) {
                ui_graph_add_point(display_temp(model.pitTemp),
                                   display_temp(model.meat1Temp),
                                   display_temp(model.meat2Temp),
                                   display_temp(model.setpoint),
                                   false, !model.meat1Connected, !model.meat2Connected);
                lastGraph = now;
            }
        }

        // LVGL tick + timer handler (always, regardless of phase)
        lv_tick_inc(5);
        lv_timer_handler();

        // Tick web server (non-blocking)
        webServer.tick();

        // Check if SDL window was closed
        if (!lv_display_get_default()) {
            running = false;
        }

        SDL_Delay(5);
    }

    g_model = nullptr;
    g_webServer = nullptr;
    printf("Simulator exited.\n");
    return 0;
}

#endif // SIMULATOR_BUILD
