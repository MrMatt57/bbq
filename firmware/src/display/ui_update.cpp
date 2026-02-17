#include "ui_update.h"

#if !defined(NATIVE_BUILD) || defined(SIMULATOR_BUILD)

#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ui_colors.h"

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

// Settings
extern lv_obj_t* btn_units_f;
extern lv_obj_t* btn_units_c;
extern lv_obj_t* btn_fan_only;
extern lv_obj_t* btn_fan_damper;
extern lv_obj_t* btn_damper_pri;

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
        lv_label_set_text(lbl_alert_text, "MEAT 1 DONE — Tap to silence");
        lv_obj_set_style_bg_color(alert_banner, COLOR_RED, 0);
        lv_obj_remove_flag(alert_banner, LV_OBJ_FLAG_HIDDEN);
    } else if (alarmType == 4) {
        lv_label_set_text(lbl_alert_text, "MEAT 2 DONE — Tap to silence");
        lv_obj_set_style_bg_color(alert_banner, COLOR_RED, 0);
        lv_obj_remove_flag(alert_banner, LV_OBJ_FLAG_HIDDEN);
    } else if (alarmType == 1) {
        lv_label_set_text(lbl_alert_text, "PIT HIGH — Tap to silence");
        lv_obj_set_style_bg_color(alert_banner, COLOR_RED, 0);
        lv_obj_remove_flag(alert_banner, LV_OBJ_FLAG_HIDDEN);
    } else if (alarmType == 2) {
        lv_label_set_text(lbl_alert_text, "PIT LOW — Tap to silence");
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
        snprintf(buf, sizeof(buf), "DMPR %.0f%%", damperPct);
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

void ui_update_graph(float pit, float meat1, float meat2) {
    if (!chart_temps || !ser_pit) return;

    lv_chart_set_next_value(chart_temps, ser_pit, (int32_t)(pit + 0.5f));
    lv_chart_set_next_value(chart_temps, ser_meat1, (int32_t)(meat1 + 0.5f));
    lv_chart_set_next_value(chart_temps, ser_meat2, (int32_t)(meat2 + 0.5f));
}

void ui_update_graph_setpoint(float sp) {
    if (!chart_temps || !ser_setpoint) return;

    // Add the setpoint as a constant value to the setpoint series
    lv_chart_set_next_value(chart_temps, ser_setpoint, (int32_t)(sp + 0.5f));
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
void ui_update_graph(float, float, float) {}
void ui_update_graph_setpoint(float) {}
void ui_update_settings_state(bool, const char*) {}
void ui_set_units(bool) {}
#endif
