// LVGL Desktop Simulator for Pit Claw BBQ Controller
//
// Renders the touchscreen UI in an SDL2 window with simulated cook data.
// Usage:
//   pio run -e simulator && .pio/build/simulator/program
//   .pio/build/simulator/program --speed 10       # 10x time acceleration
//   .pio/build/simulator/program --profile stall  # brisket stall scenario

#ifdef SIMULATOR_BUILD

#include <SDL2/SDL.h>
#include <lvgl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "../display/ui_init.h"
#include "../display/ui_update.h"
#include "sim_thermal.h"
#include "sim_profiles.h"

// Simulator-local state
static SimThermalModel* g_model = nullptr;
static float g_meat1_target = 203;
static float g_meat2_target = 0;
static bool  g_alarm_active = false;
static uint8_t g_alarm_type = 0;
static bool  g_alarm_acked = false;
static bool  g_is_fahrenheit = true;
static char  g_fan_mode[20] = "fan_and_damper";

static float f_to_c(float f) {
    return (f - 32.0f) * 5.0f / 9.0f;
}

static float display_temp(float f) {
    return g_is_fahrenheit ? f : f_to_c(f);
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
    printf("[SIM] Fan mode changed to %s\n", mode);
}

static void on_new_session() {
    printf("[SIM] New session requested (simulator restart recommended)\n");
}

static void on_factory_reset() {
    printf("[SIM] Factory reset requested (simulator restart recommended)\n");
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

static void print_usage(const char* prog) {
    printf("Pit Claw LVGL Simulator\n\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --speed N      Time acceleration factor (default: 5)\n");
    printf("  --profile NAME Cook profile (default: normal)\n");
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
    const char* profileName = "normal";

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--speed") == 0 && i + 1 < argc) {
            speed = atoi(argv[++i]);
            if (speed < 1) speed = 1;
        } else if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
            profileName = argv[++i];
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

    printf("Pit Claw Simulator - Profile: %s, Speed: %dx\n", profile->name, speed);

    ui_init();

    // Wire up callbacks
    ui_set_callbacks(on_setpoint, on_meat_target, on_alarm_ack);
    ui_set_settings_callbacks(on_units, on_fan_mode, on_new_session, on_factory_reset);

    // Initialize thermal model
    SimThermalModel model;
    model.init(*profile);
    g_model = &model;

    // Set initial meat target from profile
    g_meat1_target = profile->meat1Target;
    g_meat2_target = profile->meat2Target;

    // Set initial UI state
    ui_update_setpoint(model.setpoint);
    ui_update_meat1_target(g_meat1_target);
    ui_update_meat2_target(g_meat2_target);
    ui_update_settings_state(true, "fan_and_damper");

    // Main loop timing
    uint32_t lastUpdate = SDL_GetTicks();
    uint32_t lastGraph = SDL_GetTicks();
    bool running = true;

    while (running) {
        uint32_t now = SDL_GetTicks();

        // Update thermal model and UI every second of real time
        if (now - lastUpdate >= 1000) {
            float dt = (float)speed;
            SimResult result = model.update(dt);

            // Dashboard temperatures (converted to display units)
            ui_update_temps(display_temp(result.pitTemp),
                            display_temp(result.meat1Temp),
                            display_temp(result.meat2Temp),
                            true, result.meat1Connected, result.meat2Connected);

            // Output bars — apply fan mode
            float displayFan = result.fanPercent;
            float displayDamper = result.damperPercent;
            if (strcmp(g_fan_mode, "fan_only") == 0) {
                displayDamper = 0;
            } else if (strcmp(g_fan_mode, "damper_primary") == 0) {
                displayFan = result.fanPercent > 30 ? result.fanPercent : 0;
            }
            ui_update_output_bars(displayFan, displayDamper);

            // Setpoint (may have changed via events)
            ui_update_setpoint(display_temp(model.setpoint));

            // Cook timer
            ui_update_cook_timer(0, (uint32_t)model.simTime, 0);

            // WiFi (simulated as disconnected in simulator)
            ui_update_wifi(false);

            // Check and display alarms
            check_alarms(result);
            ui_update_alerts(
                g_alarm_active ? g_alarm_type : 0,
                result.lidOpen,
                result.fireOut,
                0  // no probe errors in basic sim
            );

            lastUpdate = now;
        }

        // Update graph less frequently (every 5 real seconds)
        if (now - lastGraph >= 5000) {
            ui_update_graph(display_temp(model.pitTemp),
                            display_temp(model.meat1Temp),
                            display_temp(model.meat2Temp));
            ui_update_graph_setpoint(display_temp(model.setpoint));
            lastGraph = now;
        }

        // LVGL tick + timer handler
        lv_tick_inc(5);
        lv_timer_handler();

        // Check if SDL window was closed
        if (!lv_display_get_default()) {
            running = false;
        }

        SDL_Delay(5);
    }

    g_model = nullptr;
    printf("Simulator exited.\n");
    return 0;
}

#endif // SIMULATOR_BUILD
