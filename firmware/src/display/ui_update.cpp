#include "ui_update.h"

#ifndef NATIVE_BUILD

#include <lvgl.h>
#include <stdio.h>
#include <time.h>

// --------------------------------------------------------------------------
// External widget references (defined in ui_init.cpp)
// --------------------------------------------------------------------------

extern lv_obj_t* lbl_pit_temp;
extern lv_obj_t* lbl_setpoint;
extern lv_obj_t* lbl_meat1_temp;
extern lv_obj_t* lbl_meat2_temp;
extern lv_obj_t* lbl_fan_pct;
extern lv_obj_t* lbl_damper_pct;
extern lv_obj_t* lbl_timer;
extern lv_obj_t* lbl_est_time;
extern lv_obj_t* lbl_wifi_icon;
extern lv_obj_t* bar_pid_output;
extern lv_obj_t* lbl_status;

extern lv_obj_t* chart_temps;
extern lv_chart_series_t* ser_pit;
extern lv_chart_series_t* ser_meat1;
extern lv_chart_series_t* ser_meat2;

// --------------------------------------------------------------------------
// Color constants (must match ui_init.cpp)
// --------------------------------------------------------------------------

#define COLOR_TEXT        lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_DIM    lv_color_hex(0x888888)
#define COLOR_ORANGE      lv_color_hex(0xFF6600)
#define COLOR_RED         lv_color_hex(0xFF3333)
#define COLOR_GREEN       lv_color_hex(0x33CC33)

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

void ui_update_temps(float pit, float meat1, float meat2,
                     bool pitConn, bool meat1Conn, bool meat2Conn) {
    if (!lbl_pit_temp) return;

    char buf[16];

    if (pitConn) {
        snprintf(buf, sizeof(buf), "%.0f", pit);
        lv_label_set_text(lbl_pit_temp, buf);
        lv_obj_set_style_text_color(lbl_pit_temp, COLOR_ORANGE, 0);
    } else {
        lv_label_set_text(lbl_pit_temp, "---");
        lv_obj_set_style_text_color(lbl_pit_temp, COLOR_TEXT_DIM, 0);
    }

    if (meat1Conn) {
        snprintf(buf, sizeof(buf), "%.0f", meat1);
        lv_label_set_text(lbl_meat1_temp, buf);
        lv_obj_set_style_text_color(lbl_meat1_temp, COLOR_TEXT, 0);
    } else {
        lv_label_set_text(lbl_meat1_temp, "---");
        lv_obj_set_style_text_color(lbl_meat1_temp, COLOR_TEXT_DIM, 0);
    }

    if (meat2Conn) {
        snprintf(buf, sizeof(buf), "%.0f", meat2);
        lv_label_set_text(lbl_meat2_temp, buf);
        lv_obj_set_style_text_color(lbl_meat2_temp, COLOR_TEXT, 0);
    } else {
        lv_label_set_text(lbl_meat2_temp, "---");
        lv_obj_set_style_text_color(lbl_meat2_temp, COLOR_TEXT_DIM, 0);
    }
}

void ui_update_fan(float fanPct, float damperPct) {
    if (!lbl_fan_pct) return;

    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f%%", fanPct);
    lv_label_set_text(lbl_fan_pct, buf);

    snprintf(buf, sizeof(buf), "%.0f%%", damperPct);
    lv_label_set_text(lbl_damper_pct, buf);
}

void ui_update_setpoint(float sp) {
    if (!lbl_setpoint) return;

    char buf[24];
    snprintf(buf, sizeof(buf), "Set: %.0f", sp);
    lv_label_set_text(lbl_setpoint, buf);
}

void ui_update_pid(float output) {
    if (!bar_pid_output) return;

    int val = (int)(output + 0.5f);
    if (val < 0) val = 0;
    if (val > 100) val = 100;
    lv_bar_set_value(bar_pid_output, val, LV_ANIM_ON);
}

void ui_update_status(bool lidOpen, bool fireOut, bool wifiConnected, bool alarming) {
    if (lbl_wifi_icon) {
        lv_obj_set_style_text_color(lbl_wifi_icon,
            wifiConnected ? COLOR_GREEN : COLOR_RED, 0);
    }

    if (!lbl_status) return;

    if (fireOut) {
        lv_label_set_text(lbl_status, "FIRE!");
        lv_obj_set_style_text_color(lbl_status, COLOR_RED, 0);
    } else if (lidOpen) {
        lv_label_set_text(lbl_status, "LID");
        lv_obj_set_style_text_color(lbl_status, COLOR_ORANGE, 0);
    } else if (alarming) {
        lv_label_set_text(lbl_status, "ALARM");
        lv_obj_set_style_text_color(lbl_status, COLOR_RED, 0);
    } else {
        lv_label_set_text(lbl_status, "OK");
        lv_obj_set_style_text_color(lbl_status, COLOR_GREEN, 0);
    }
}

void ui_update_timer(uint32_t elapsedSec) {
    if (!lbl_timer) return;

    uint32_t h = elapsedSec / 3600;
    uint32_t m = (elapsedSec % 3600) / 60;
    uint32_t s = elapsedSec % 60;

    char buf[16];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
             (unsigned long)h, (unsigned long)m, (unsigned long)s);
    lv_label_set_text(lbl_timer, buf);
}

void ui_update_graph(float pit, float meat1, float meat2) {
    if (!chart_temps || !ser_pit) return;

    lv_chart_set_next_value(chart_temps, ser_pit, (int32_t)(pit + 0.5f));
    lv_chart_set_next_value(chart_temps, ser_meat1, (int32_t)(meat1 + 0.5f));
    lv_chart_set_next_value(chart_temps, ser_meat2, (int32_t)(meat2 + 0.5f));
}

void ui_update_estimated_time(uint32_t estEpoch) {
    if (!lbl_est_time) return;

    if (estEpoch == 0) {
        lv_label_set_text(lbl_est_time, "");
        return;
    }

    // Convert epoch to local time for display
    time_t t = (time_t)estEpoch;
    struct tm* tmInfo = localtime(&t);

    if (tmInfo) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Est: %02d:%02d",
                 tmInfo->tm_hour, tmInfo->tm_min);
        lv_label_set_text(lbl_est_time, buf);
    }
}

#else
// Native build stubs
void ui_update_temps(float, float, float, bool, bool, bool) {}
void ui_update_fan(float, float) {}
void ui_update_setpoint(float) {}
void ui_update_pid(float) {}
void ui_update_status(bool, bool, bool, bool) {}
void ui_update_timer(uint32_t) {}
void ui_update_graph(float, float, float) {}
void ui_update_estimated_time(uint32_t) {}
#endif
