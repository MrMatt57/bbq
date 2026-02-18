#include "ui_update.h"

#if !defined(NATIVE_BUILD) || defined(SIMULATOR_BUILD)

#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "ui_colors.h"
#include "graph_history.h"

// --------------------------------------------------------------------------
// External widget references (defined in ui_init.cpp)
// --------------------------------------------------------------------------

// Dashboard — top bar
extern lv_obj_t* lbl_wifi_icon;
extern lv_obj_t* lbl_start_time;
extern lv_obj_t* lbl_elapsed;
extern lv_obj_t* lbl_done_time;

// Dashboard — output bars
extern lv_obj_t* bar_fan;
extern lv_obj_t* lbl_fan_bar;
extern lv_obj_t* bar_damper;
extern lv_obj_t* lbl_damper_bar;

// Dashboard — pit card
extern lv_obj_t* lbl_pit_temp;
extern lv_obj_t* lbl_setpoint;

// Dashboard — meat cards
extern lv_obj_t* lbl_meat1_temp;
extern lv_obj_t* lbl_meat1_target;
extern lv_obj_t* lbl_meat1_est;
extern lv_obj_t* lbl_meat2_temp;
extern lv_obj_t* lbl_meat2_target;
extern lv_obj_t* lbl_meat2_est;

// Dashboard — alert banner
extern lv_obj_t* alert_banner;
extern lv_obj_t* lbl_alert_text;

// Graph
extern lv_obj_t* chart_temps;
extern lv_chart_series_t* ser_pit;
extern lv_chart_series_t* ser_meat1;
extern lv_chart_series_t* ser_meat2;
extern lv_chart_series_t* ser_setpoint;
extern lv_obj_t* graph_y_labels[5];

// Settings
extern lv_obj_t* btn_units_f;
extern lv_obj_t* btn_units_c;
extern lv_obj_t* btn_fan_only;
extern lv_obj_t* btn_fan_damper;
extern lv_obj_t* btn_damper_pri;

// Settings — Wi-Fi info
extern lv_obj_t* lbl_wifi_status;
extern lv_obj_t* lbl_wifi_ssid;
extern lv_obj_t* lbl_wifi_ip;
extern lv_obj_t* lbl_wifi_signal;
extern lv_obj_t* btn_wifi_action;
extern lv_obj_t* lbl_wifi_action;

// --------------------------------------------------------------------------
// Unit state
// --------------------------------------------------------------------------

static bool s_fahrenheit = true;

void ui_set_units(bool fahrenheit) {
    s_fahrenheit = fahrenheit;
}

static const char* unit_suffix() {
    return s_fahrenheit ? "F" : "C";
}

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

void ui_update_temps(float pit, float meat1, float meat2,
                     bool pitConn, bool meat1Conn, bool meat2Conn) {
    if (!lbl_pit_temp) return;

    char buf[16];

    if (pitConn) {
        snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", pit);
        lv_label_set_text(lbl_pit_temp, buf);
        lv_obj_set_style_text_color(lbl_pit_temp, COLOR_ORANGE, 0);
    } else {
        lv_label_set_text(lbl_pit_temp, "---");
        lv_obj_set_style_text_color(lbl_pit_temp, COLOR_TEXT_DIM, 0);
    }

    if (lbl_meat1_temp) {
        if (meat1Conn) {
            snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", meat1);
            lv_label_set_text(lbl_meat1_temp, buf);
            lv_obj_set_style_text_color(lbl_meat1_temp, COLOR_RED, 0);
        } else {
            lv_label_set_text(lbl_meat1_temp, "---");
            lv_obj_set_style_text_color(lbl_meat1_temp, COLOR_TEXT_DIM, 0);
        }
    }

    if (lbl_meat2_temp) {
        if (meat2Conn) {
            snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", meat2);
            lv_label_set_text(lbl_meat2_temp, buf);
            lv_obj_set_style_text_color(lbl_meat2_temp, COLOR_BLUE, 0);
        } else {
            lv_label_set_text(lbl_meat2_temp, "---");
            lv_obj_set_style_text_color(lbl_meat2_temp, COLOR_TEXT_DIM, 0);
        }
    }
}

void ui_update_setpoint(float sp) {
    if (!lbl_setpoint) return;

    char buf[24];
    snprintf(buf, sizeof(buf), "Set: %.0f\xC2\xB0%s", sp, unit_suffix());
    lv_label_set_text(lbl_setpoint, buf);
}

void ui_update_cook_timer(uint32_t startEpoch, uint32_t elapsedSec, uint32_t estDoneEpoch) {
    // Elapsed time (center, hero element)
    if (lbl_elapsed) {
        uint32_t h = elapsedSec / 3600;
        uint32_t m = (elapsedSec % 3600) / 60;
        uint32_t s = elapsedSec % 60;
        char buf[16];
        snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
                 (unsigned long)h, (unsigned long)m, (unsigned long)s);
        lv_label_set_text(lbl_elapsed, buf);
    }

    // Start time
    if (lbl_start_time) {
        if (startEpoch > 0) {
            time_t t = (time_t)startEpoch;
            struct tm* tm = localtime(&t);
            if (tm) {
                char buf[24];
                snprintf(buf, sizeof(buf), "Start %02d:%02d", tm->tm_hour, tm->tm_min);
                lv_label_set_text(lbl_start_time, buf);
            }
        } else {
            lv_label_set_text(lbl_start_time, "");
        }
    }

    // Estimated done time
    if (lbl_done_time) {
        if (estDoneEpoch > 0) {
            time_t t = (time_t)estDoneEpoch;
            struct tm* tm = localtime(&t);
            if (tm) {
                char buf[24];
                snprintf(buf, sizeof(buf), "Done ~%d:%02d", tm->tm_hour, tm->tm_min);
                lv_label_set_text(lbl_done_time, buf);
            }
        } else {
            lv_label_set_text(lbl_done_time, "");
        }
    }
}

void ui_update_meat1_target(float target) {
    if (!lbl_meat1_target) return;

    if (target > 0) {
        char buf[24];
        snprintf(buf, sizeof(buf), "Target: %.0f\xC2\xB0%s", target, unit_suffix());
        lv_label_set_text(lbl_meat1_target, buf);
    } else {
        lv_label_set_text(lbl_meat1_target, "Target: ---");
    }
}

void ui_update_meat2_target(float target) {
    if (!lbl_meat2_target) return;

    if (target > 0) {
        char buf[24];
        snprintf(buf, sizeof(buf), "Target: %.0f\xC2\xB0%s", target, unit_suffix());
        lv_label_set_text(lbl_meat2_target, buf);
    } else {
        lv_label_set_text(lbl_meat2_target, "Target: ---");
    }
}

void ui_update_meat1_estimate(uint32_t estEpoch) {
    if (!lbl_meat1_est) return;

    if (estEpoch > 0) {
        time_t t = (time_t)estEpoch;
        struct tm* tm = localtime(&t);
        if (tm) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Est: %d:%02d PM", tm->tm_hour % 12 ? tm->tm_hour % 12 : 12, tm->tm_min);
            lv_label_set_text(lbl_meat1_est, buf);
        }
    } else {
        lv_label_set_text(lbl_meat1_est, "");
    }
}

void ui_update_meat2_estimate(uint32_t estEpoch) {
    if (!lbl_meat2_est) return;

    if (estEpoch > 0) {
        time_t t = (time_t)estEpoch;
        struct tm* tm = localtime(&t);
        if (tm) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Est: %d:%02d PM", tm->tm_hour % 12 ? tm->tm_hour % 12 : 12, tm->tm_min);
            lv_label_set_text(lbl_meat2_est, buf);
        }
    } else {
        lv_label_set_text(lbl_meat2_est, "");
    }
}

void ui_update_alerts(uint8_t alarmType, bool lidOpen, bool fireOut, uint8_t probeErrors) {
    if (!alert_banner || !lbl_alert_text) return;

    // Priority: Alarm > Fire > Lid > Probe errors
    // alarmType: 0=none, 1=pit_high, 2=pit_low, 3=meat1_done, 4=meat2_done
    if (alarmType == 3) {
        lv_label_set_text(lbl_alert_text, "MEAT 1 DONE - Tap to silence");
        lv_obj_set_style_bg_color(alert_banner, COLOR_RED, 0);
        lv_obj_remove_flag(alert_banner, LV_OBJ_FLAG_HIDDEN);
    } else if (alarmType == 4) {
        lv_label_set_text(lbl_alert_text, "MEAT 2 DONE - Tap to silence");
        lv_obj_set_style_bg_color(alert_banner, COLOR_RED, 0);
        lv_obj_remove_flag(alert_banner, LV_OBJ_FLAG_HIDDEN);
    } else if (alarmType == 1) {
        lv_label_set_text(lbl_alert_text, "PIT HIGH - Tap to silence");
        lv_obj_set_style_bg_color(alert_banner, COLOR_RED, 0);
        lv_obj_remove_flag(alert_banner, LV_OBJ_FLAG_HIDDEN);
    } else if (alarmType == 2) {
        lv_label_set_text(lbl_alert_text, "PIT LOW - Tap to silence");
        lv_obj_set_style_bg_color(alert_banner, COLOR_RED, 0);
        lv_obj_remove_flag(alert_banner, LV_OBJ_FLAG_HIDDEN);
    } else if (fireOut) {
        lv_label_set_text(lbl_alert_text, "FIRE MAY BE OUT");
        lv_obj_set_style_bg_color(alert_banner, COLOR_RED, 0);
        lv_obj_remove_flag(alert_banner, LV_OBJ_FLAG_HIDDEN);
    } else if (lidOpen) {
        lv_label_set_text(lbl_alert_text, "LID OPEN");
        lv_obj_set_style_bg_color(alert_banner, COLOR_ORANGE, 0);
        lv_obj_remove_flag(alert_banner, LV_OBJ_FLAG_HIDDEN);
    } else if (probeErrors) {
        char buf[40] = "PROBE ERROR:";
        if (probeErrors & 0x01) strcat(buf, " Pit");
        if (probeErrors & 0x02) strcat(buf, " Meat1");
        if (probeErrors & 0x04) strcat(buf, " Meat2");
        lv_label_set_text(lbl_alert_text, buf);
        lv_obj_set_style_bg_color(alert_banner, COLOR_ORANGE, 0);
        lv_obj_remove_flag(alert_banner, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(alert_banner, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_update_output_bars(float fanPct, float damperPct) {
    if (lbl_fan_bar) {
        char buf[16];
        snprintf(buf, sizeof(buf), "FAN %.0f%%", fanPct);
        lv_label_set_text(lbl_fan_bar, buf);
    }
    if (bar_fan) {
        lv_bar_set_value(bar_fan, (int32_t)(fanPct + 0.5f), LV_ANIM_ON);
    }
    if (lbl_damper_bar) {
        char buf[16];
        snprintf(buf, sizeof(buf), "DAMPER %.0f%%", damperPct);
        lv_label_set_text(lbl_damper_bar, buf);
    }
    if (bar_damper) {
        lv_bar_set_value(bar_damper, (int32_t)(damperPct + 0.5f), LV_ANIM_ON);
    }
}

void ui_update_wifi(bool connected) {
    if (lbl_wifi_icon) {
        lv_obj_set_style_text_color(lbl_wifi_icon,
            connected ? COLOR_GREEN : COLOR_RED, 0);
    }
}

// --------------------------------------------------------------------------
// Graph — adaptive condensing with external arrays
// --------------------------------------------------------------------------

static GraphHistory s_history;
static int32_t s_pit_arr[GRAPH_HISTORY_SIZE];
static int32_t s_meat1_arr[GRAPH_HISTORY_SIZE];
static int32_t s_meat2_arr[GRAPH_HISTORY_SIZE];
static int32_t s_sp_arr[GRAPH_HISTORY_SIZE];

// Sync GraphHistory buffer → LVGL external arrays, auto-scale Y axis
static void sync_graph_arrays() {
    if (!chart_temps) return;

    uint16_t count = s_history.getCount();

    // Dynamically set point count so data spans the full chart width.
    // Min 2 to avoid division-by-zero in LVGL's x-position math.
    uint16_t displayCount = count < 2 ? 2 : count;
    lv_chart_set_point_count(chart_temps, displayCount);

    // Clear only the used portion of arrays
    for (uint16_t i = 0; i < displayCount; i++) {
        s_pit_arr[i]   = LV_CHART_POINT_NONE;
        s_meat1_arr[i] = LV_CHART_POINT_NONE;
        s_meat2_arr[i] = LV_CHART_POINT_NONE;
        s_sp_arr[i]    = LV_CHART_POINT_NONE;
    }

    // Left-align data: index 0 = oldest point
    float yMinF = 9999.0f, yMaxF = -9999.0f;

    for (uint16_t i = 0; i < count; i++) {
        const GraphSlot& slot = s_history.getSlot(i);

        if (slot.pitValid) {
            s_pit_arr[i] = (int32_t)(slot.pit + 0.5f);
            if (slot.pit < yMinF) yMinF = slot.pit;
            if (slot.pit > yMaxF) yMaxF = slot.pit;
        }
        if (slot.meat1Valid) {
            s_meat1_arr[i] = (int32_t)(slot.meat1 + 0.5f);
            if (slot.meat1 < yMinF) yMinF = slot.meat1;
            if (slot.meat1 > yMaxF) yMaxF = slot.meat1;
        }
        if (slot.meat2Valid) {
            s_meat2_arr[i] = (int32_t)(slot.meat2 + 0.5f);
            if (slot.meat2 < yMinF) yMinF = slot.meat2;
            if (slot.meat2 > yMaxF) yMaxF = slot.meat2;
        }
        // Setpoint is always valid
        s_sp_arr[i] = (int32_t)(slot.setpoint + 0.5f);
        if (slot.setpoint < yMinF) yMinF = slot.setpoint;
        if (slot.setpoint > yMaxF) yMaxF = slot.setpoint;
    }

    // Auto-scale Y axis with 15-degree padding, rounded to 25-degree steps
    if (count > 0 && yMinF < 9000.0f) {
        int32_t yMin = (int32_t)(floorf((yMinF - 15.0f) / 25.0f)) * 25;
        int32_t yMax = (int32_t)(ceilf((yMaxF + 15.0f) / 25.0f)) * 25;
        if (yMax - yMin < 150) yMax = yMin + 150;  // minimum 150-degree range
        if (yMin < 0) yMin = 0;

        lv_chart_set_range(chart_temps, LV_CHART_AXIS_PRIMARY_Y, yMin, yMax);

        // Update Y-axis labels at each of the 5 division line positions
        for (int i = 0; i < 5; i++) {
            if (graph_y_labels[i]) {
                int temp = yMax - (yMax - yMin) * (i + 1) / 6;
                char buf[8];
                snprintf(buf, sizeof(buf), "%d", temp);
                lv_label_set_text(graph_y_labels[i], buf);
            }
        }
    }

    lv_chart_refresh(chart_temps);
}

void ui_graph_init() {
    if (!chart_temps) return;

    // Initialize arrays to LV_CHART_POINT_NONE
    for (int i = 0; i < GRAPH_HISTORY_SIZE; i++) {
        s_pit_arr[i]   = LV_CHART_POINT_NONE;
        s_meat1_arr[i] = LV_CHART_POINT_NONE;
        s_meat2_arr[i] = LV_CHART_POINT_NONE;
        s_sp_arr[i]    = LV_CHART_POINT_NONE;
    }

    // Bind external arrays to chart series
    lv_chart_set_ext_y_array(chart_temps, ser_pit,      s_pit_arr);
    lv_chart_set_ext_y_array(chart_temps, ser_meat1,    s_meat1_arr);
    lv_chart_set_ext_y_array(chart_temps, ser_meat2,    s_meat2_arr);
    lv_chart_set_ext_y_array(chart_temps, ser_setpoint, s_sp_arr);
}

void ui_graph_add_point(float pit, float meat1, float meat2, float setpoint,
                        bool pitDisc, bool meat1Disc, bool meat2Disc) {
    s_history.addPoint(pit, meat1, meat2, setpoint, pitDisc, meat1Disc, meat2Disc);
    sync_graph_arrays();
}

void ui_graph_clear() {
    s_history.clear();
    sync_graph_arrays();
}

static const char* rssi_quality(int rssi) {
    if (rssi == 0)    return "N/A";
    if (rssi >= -50)  return "Excellent";
    if (rssi >= -60)  return "Good";
    if (rssi >= -70)  return "Fair";
    return "Weak";
}

void ui_update_wifi_info(const WifiInfo& info) {
    if (lbl_wifi_status) {
        if (info.connected) {
            lv_label_set_text(lbl_wifi_status, "Connected");
            lv_obj_set_style_text_color(lbl_wifi_status, COLOR_GREEN, 0);
        } else if (info.apMode) {
            lv_label_set_text(lbl_wifi_status, "AP Mode");
            lv_obj_set_style_text_color(lbl_wifi_status, COLOR_ORANGE, 0);
        } else {
            lv_label_set_text(lbl_wifi_status, "Disconnected");
            lv_obj_set_style_text_color(lbl_wifi_status, COLOR_RED, 0);
        }
    }

    if (lbl_wifi_ssid) {
        char buf[48];
        if (info.ssid && info.ssid[0]) {
            snprintf(buf, sizeof(buf), "SSID: %s", info.ssid);
        } else {
            snprintf(buf, sizeof(buf), "SSID: ---");
        }
        lv_label_set_text(lbl_wifi_ssid, buf);
    }

    if (lbl_wifi_ip) {
        char buf[64];
        if (info.connected && !info.apMode) {
            snprintf(buf, sizeof(buf), "IP: %s  (bbq.local)", info.ip ? info.ip : "---");
        } else if (info.ip && info.ip[0]) {
            snprintf(buf, sizeof(buf), "IP: %s", info.ip);
        } else {
            snprintf(buf, sizeof(buf), "IP: ---");
        }
        lv_label_set_text(lbl_wifi_ip, buf);
    }

    if (lbl_wifi_signal) {
        char buf[32];
        if (info.connected && !info.apMode && info.rssi != 0) {
            snprintf(buf, sizeof(buf), "Signal: %d dBm (%s)", info.rssi, rssi_quality(info.rssi));
        } else {
            snprintf(buf, sizeof(buf), "Signal: ---");
        }
        lv_label_set_text(lbl_wifi_signal, buf);
    }

    // Toggle button label between Disconnect and Reconnect
    if (lbl_wifi_action) {
        if (info.connected || info.apMode) {
            lv_label_set_text(lbl_wifi_action, "Disconnect");
        } else {
            lv_label_set_text(lbl_wifi_action, "Reconnect");
        }
    }
}

void ui_update_settings_state(bool isFahrenheit, const char* fanMode) {
    if (btn_units_f && btn_units_c) {
        lv_obj_set_style_bg_color(btn_units_f, isFahrenheit ? COLOR_ORANGE : COLOR_BAR_BG, 0);
        lv_obj_set_style_bg_color(btn_units_c, isFahrenheit ? COLOR_BAR_BG : COLOR_ORANGE, 0);
    }

    if (btn_fan_only && btn_fan_damper && btn_damper_pri && fanMode) {
        lv_obj_set_style_bg_color(btn_fan_only, COLOR_BAR_BG, 0);
        lv_obj_set_style_bg_color(btn_fan_damper, COLOR_BAR_BG, 0);
        lv_obj_set_style_bg_color(btn_damper_pri, COLOR_BAR_BG, 0);

        if (strcmp(fanMode, "fan_only") == 0) {
            lv_obj_set_style_bg_color(btn_fan_only, COLOR_ORANGE, 0);
        } else if (strcmp(fanMode, "fan_and_damper") == 0) {
            lv_obj_set_style_bg_color(btn_fan_damper, COLOR_ORANGE, 0);
        } else if (strcmp(fanMode, "damper_primary") == 0) {
            lv_obj_set_style_bg_color(btn_damper_pri, COLOR_ORANGE, 0);
        }
    }
}

#else // NATIVE_BUILD && !SIMULATOR_BUILD
// Native test stubs
void ui_update_temps(float, float, float, bool, bool, bool) {}
void ui_update_setpoint(float) {}
void ui_update_cook_timer(uint32_t, uint32_t, uint32_t) {}
void ui_update_meat1_target(float) {}
void ui_update_meat2_target(float) {}
void ui_update_meat1_estimate(uint32_t) {}
void ui_update_meat2_estimate(uint32_t) {}
void ui_update_alerts(uint8_t, bool, bool, uint8_t) {}
void ui_update_output_bars(float, float) {}
void ui_update_wifi(bool) {}
void ui_update_wifi_info(const WifiInfo&) {}
void ui_graph_init() {}
void ui_graph_add_point(float, float, float, float, bool, bool, bool) {}
void ui_graph_clear() {}
void ui_update_settings_state(bool, const char*) {}
void ui_set_units(bool) {}
#endif
