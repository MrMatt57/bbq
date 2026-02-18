#pragma once

#include "../config.h"
#include <stdint.h>

// Update temperature displays on the dashboard.
// Shows "---" for disconnected probes.
void ui_update_temps(float pit, float meat1, float meat2,
                     bool pitConn, bool meat1Conn, bool meat2Conn);

// Update the setpoint display (dashboard card + modal initial value).
void ui_update_setpoint(float sp);

// Update cook timer: top bar start time, elapsed, and estimated done.
// startEpoch=0 means no NTP. estDoneEpoch=0 means no estimate.
void ui_update_cook_timer(uint32_t startEpoch, uint32_t elapsedSec, uint32_t estDoneEpoch);

// Update meat target display on dashboard cards. target=0 means no target.
void ui_update_meat1_target(float target);
void ui_update_meat2_target(float target);

// Update per-probe estimated done times on cards. estEpoch=0 means no estimate.
void ui_update_meat1_estimate(uint32_t estEpoch);
void ui_update_meat2_estimate(uint32_t estEpoch);

// Update alert banner. alarmType is a uint8_t cast of AlarmType enum.
// probeErrors is a bitmask: bit 0=pit, bit 1=meat1, bit 2=meat2.
void ui_update_alerts(uint8_t alarmType, bool lidOpen, bool fireOut, uint8_t probeErrors);

// Update thin output bars (fan and damper percentage).
void ui_update_output_bars(float fanPct, float damperPct);

// Update WiFi connection status icon.
void ui_update_wifi(bool connected);

// Wi-Fi info for settings screen display
struct WifiInfo {
    bool connected;
    bool apMode;
    const char* ssid;   // "MyNetwork", "BBQ-Setup", or "Simulator"
    const char* ip;     // "192.168.1.42", "192.168.4.1", or "localhost"
    int rssi;           // dBm, 0 if unknown
};

// Update settings screen Wi-Fi info card with current connection details.
void ui_update_wifi_info(const WifiInfo& info);

// Initialize graph external arrays. Call once after chart/series creation.
void ui_graph_init();

// Add a data point to the graph with adaptive condensing.
// Disconnected probes are marked invalid (pass true for disconnected).
void ui_graph_add_point(float pit, float meat1, float meat2, float setpoint,
                        bool pitDisc, bool meat1Disc, bool meat2Disc);

// Clear graph history (e.g., on new session).
void ui_graph_clear();

// Update settings screen state to reflect current values.
void ui_update_settings_state(bool isFahrenheit, const char* fanMode);

// Set the display units (affects temperature labels like °F / °C).
void ui_set_units(bool fahrenheit);
