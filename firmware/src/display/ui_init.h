#pragma once

#include "../config.h"
#include <stdint.h>

#if !defined(NATIVE_BUILD) || defined(SIMULATOR_BUILD)
#include <lvgl.h>
#endif

// Screen IDs
enum class Screen : uint8_t {
    DASHBOARD = 0,
    GRAPH,
    SETTINGS
};

// Callback typedefs for UI actions
typedef void (*UiSetpointCb)(float setpoint);
typedef void (*UiMeatTargetCb)(uint8_t probe, float target);  // probe 1 or 2, target=0 means clear
typedef void (*UiAlarmAckCb)();
typedef void (*UiUnitsCb)(bool isFahrenheit);
typedef void (*UiFanModeCb)(const char* mode);
typedef void (*UiNewSessionCb)();
typedef void (*UiFactoryResetCb)();
typedef void (*UiWifiActionCb)(const char* action);  // "disconnect", "reconnect", "setup_ap"

// Initialize LVGL display driver, touch input, and create all screens.
// Call once from setup() after all other modules are initialized.
void ui_init();

// Switch to the specified screen with animation
void ui_switch_screen(Screen screen);

// Get the currently active screen
Screen ui_get_current_screen();

// LVGL tick handler — call from a timer interrupt or loop at ~5ms
void ui_tick(uint32_t ms);

// LVGL task handler — call from loop() to process LVGL events
void ui_handler();

// Set callbacks for dashboard interactive elements
void ui_set_callbacks(UiSetpointCb sp, UiMeatTargetCb meat, UiAlarmAckCb ack);

// Set callbacks for settings screen actions
void ui_set_settings_callbacks(UiUnitsCb units, UiFanModeCb fan,
                                UiNewSessionCb session, UiFactoryResetCb reset);

// Set callback for Wi-Fi action buttons (Disconnect/Reconnect/Setup Mode)
void ui_set_wifi_callback(UiWifiActionCb cb);
