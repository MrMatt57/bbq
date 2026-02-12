#pragma once

#include "../config.h"
#include <stdint.h>

// Update temperature displays on the dashboard.
// Shows "---" for disconnected probes.
void ui_update_temps(float pit, float meat1, float meat2,
                     bool pitConn, bool meat1Conn, bool meat2Conn);

// Update fan and damper percentage displays.
void ui_update_fan(float fanPct, float damperPct);

// Update the setpoint display.
void ui_update_setpoint(float sp);

// Update the PID output bar indicator.
void ui_update_pid(float output);

// Update status indicators (lid open, fire out, Wi-Fi, alarms).
void ui_update_status(bool lidOpen, bool fireOut, bool wifiConnected, bool alarming);

// Update the cook timer display (elapsed seconds).
void ui_update_timer(uint32_t elapsedSec);

// Add a data point to the graph screen chart.
void ui_update_graph(float pit, float meat1, float meat2);

// Update the estimated done time display.
// Pass 0 to clear. Pass epoch seconds for the predicted completion time.
void ui_update_estimated_time(uint32_t estEpoch);
