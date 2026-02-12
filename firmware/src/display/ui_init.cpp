#include "ui_init.h"
#include "ui_update.h"
#include "ui_setup_wizard.h"

#ifndef NATIVE_BUILD

#include <lvgl.h>
#include <TFT_eSPI.h>

// --------------------------------------------------------------------------
// Display driver
// --------------------------------------------------------------------------

static TFT_eSPI tft = TFT_eSPI();

// LVGL draw buffer
static lv_color_t draw_buf1[DISPLAY_WIDTH * 40];
static lv_color_t draw_buf2[DISPLAY_WIDTH * 40];

// LVGL display flush callback
static void disp_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)px_map, w * h, true);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

// --------------------------------------------------------------------------
// Touch input driver (WT32-SC01 Plus uses FT5x06 capacitive touch via I2C)
// --------------------------------------------------------------------------

static void touchpad_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    uint16_t touchX, touchY;
    bool touched = tft.getTouch(&touchX, &touchY);

    if (touched) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touchX;
        data->point.y = touchY;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// --------------------------------------------------------------------------
// Color constants
// --------------------------------------------------------------------------

#define COLOR_BG         lv_color_hex(0x1A1A1A)
#define COLOR_CARD_BG    lv_color_hex(0x2A2A2A)
#define COLOR_TEXT        lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_DIM    lv_color_hex(0x888888)
#define COLOR_ORANGE      lv_color_hex(0xFF6600)
#define COLOR_RED         lv_color_hex(0xFF3333)
#define COLOR_GREEN       lv_color_hex(0x33CC33)
#define COLOR_BLUE        lv_color_hex(0x3399FF)

// --------------------------------------------------------------------------
// Screens and key widgets (accessed by ui_update)
// --------------------------------------------------------------------------

static lv_obj_t* scr_dashboard = nullptr;
static lv_obj_t* scr_graph     = nullptr;
static lv_obj_t* scr_settings  = nullptr;

// Dashboard widgets
lv_obj_t* lbl_pit_temp     = nullptr;
lv_obj_t* lbl_pit_label    = nullptr;
lv_obj_t* lbl_setpoint     = nullptr;
lv_obj_t* lbl_meat1_temp   = nullptr;
lv_obj_t* lbl_meat1_label  = nullptr;
lv_obj_t* lbl_meat2_temp   = nullptr;
lv_obj_t* lbl_meat2_label  = nullptr;
lv_obj_t* lbl_fan_pct      = nullptr;
lv_obj_t* lbl_damper_pct   = nullptr;
lv_obj_t* lbl_timer        = nullptr;
lv_obj_t* lbl_est_time     = nullptr;
lv_obj_t* lbl_wifi_icon    = nullptr;
lv_obj_t* lbl_version      = nullptr;
lv_obj_t* bar_pid_output   = nullptr;
lv_obj_t* lbl_status       = nullptr;

// Graph widgets
lv_obj_t* chart_temps      = nullptr;
lv_chart_series_t* ser_pit   = nullptr;
lv_chart_series_t* ser_meat1 = nullptr;
lv_chart_series_t* ser_meat2 = nullptr;

// Settings widgets
lv_obj_t* spinbox_setpoint = nullptr;

// Navigation tab buttons
static lv_obj_t* btn_nav_dash  = nullptr;
static lv_obj_t* btn_nav_graph = nullptr;
static lv_obj_t* btn_nav_set   = nullptr;

static Screen current_screen = Screen::DASHBOARD;

// --------------------------------------------------------------------------
// Navigation event handler
// --------------------------------------------------------------------------

static void nav_event_cb(lv_event_t* e) {
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    if (target == btn_nav_dash) {
        ui_switch_screen(Screen::DASHBOARD);
    } else if (target == btn_nav_graph) {
        ui_switch_screen(Screen::GRAPH);
    } else if (target == btn_nav_set) {
        ui_switch_screen(Screen::SETTINGS);
    }
}

// --------------------------------------------------------------------------
// Helper: create a navigation bar at the bottom of a screen
// --------------------------------------------------------------------------

static void create_nav_bar(lv_obj_t* parent) {
    lv_obj_t* nav = lv_obj_create(parent);
    lv_obj_set_size(nav, DISPLAY_WIDTH, 40);
    lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(nav, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(nav, 0, 0);
    lv_obj_set_style_pad_all(nav, 4, 0);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Dashboard button
    btn_nav_dash = lv_btn_create(nav);
    lv_obj_set_size(btn_nav_dash, 100, 32);
    lv_obj_set_style_bg_color(btn_nav_dash, COLOR_ORANGE, 0);
    lv_obj_add_event_cb(btn_nav_dash, nav_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl = lv_label_create(btn_nav_dash);
    lv_label_set_text(lbl, "Home");
    lv_obj_center(lbl);

    // Graph button
    btn_nav_graph = lv_btn_create(nav);
    lv_obj_set_size(btn_nav_graph, 100, 32);
    lv_obj_set_style_bg_color(btn_nav_graph, COLOR_CARD_BG, 0);
    lv_obj_add_event_cb(btn_nav_graph, nav_event_cb, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_nav_graph);
    lv_label_set_text(lbl, "Graph");
    lv_obj_center(lbl);

    // Settings button
    btn_nav_set = lv_btn_create(nav);
    lv_obj_set_size(btn_nav_set, 100, 32);
    lv_obj_set_style_bg_color(btn_nav_set, COLOR_CARD_BG, 0);
    lv_obj_add_event_cb(btn_nav_set, nav_event_cb, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_nav_set);
    lv_label_set_text(lbl, "Settings");
    lv_obj_center(lbl);
}

// --------------------------------------------------------------------------
// Dashboard screen
// --------------------------------------------------------------------------

static void create_dashboard_screen() {
    scr_dashboard = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_dashboard, COLOR_BG, 0);

    // --- Top bar ---
    lv_obj_t* top_bar = lv_obj_create(scr_dashboard);
    lv_obj_set_size(top_bar, DISPLAY_WIDTH, 30);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 4, 0);

    lbl_wifi_icon = lv_label_create(top_bar);
    lv_label_set_text(lbl_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(lbl_wifi_icon, COLOR_GREEN, 0);
    lv_obj_align(lbl_wifi_icon, LV_ALIGN_LEFT_MID, 4, 0);

    lbl_timer = lv_label_create(top_bar);
    lv_label_set_text(lbl_timer, "00:00:00");
    lv_obj_set_style_text_color(lbl_timer, COLOR_TEXT, 0);
    lv_obj_align(lbl_timer, LV_ALIGN_CENTER, 0, 0);

    lbl_version = lv_label_create(top_bar);
    lv_label_set_text(lbl_version, "v" FIRMWARE_VERSION);
    lv_obj_set_style_text_color(lbl_version, COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl_version, LV_ALIGN_RIGHT_MID, -4, 0);

    // --- Pit temperature (large, center-left) ---
    lv_obj_t* pit_card = lv_obj_create(scr_dashboard);
    lv_obj_set_size(pit_card, 220, 160);
    lv_obj_set_pos(pit_card, 10, 38);
    lv_obj_set_style_bg_color(pit_card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(pit_card, 0, 0);
    lv_obj_set_style_radius(pit_card, 8, 0);

    lbl_pit_label = lv_label_create(pit_card);
    lv_label_set_text(lbl_pit_label, "PIT");
    lv_obj_set_style_text_color(lbl_pit_label, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(lbl_pit_label, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_pit_label, LV_ALIGN_TOP_MID, 0, 8);

    lbl_pit_temp = lv_label_create(pit_card);
    lv_label_set_text(lbl_pit_temp, "---");
    lv_obj_set_style_text_color(lbl_pit_temp, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(lbl_pit_temp, &lv_font_montserrat_48, 0);
    lv_obj_align(lbl_pit_temp, LV_ALIGN_CENTER, 0, 0);

    lbl_setpoint = lv_label_create(pit_card);
    lv_label_set_text(lbl_setpoint, "Set: 225");
    lv_obj_set_style_text_color(lbl_setpoint, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_setpoint, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_setpoint, LV_ALIGN_BOTTOM_MID, 0, -8);

    // --- Meat 1 card (right, top) ---
    lv_obj_t* meat1_card = lv_obj_create(scr_dashboard);
    lv_obj_set_size(meat1_card, 220, 72);
    lv_obj_set_pos(meat1_card, 245, 38);
    lv_obj_set_style_bg_color(meat1_card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(meat1_card, 0, 0);
    lv_obj_set_style_radius(meat1_card, 8, 0);

    lbl_meat1_label = lv_label_create(meat1_card);
    lv_label_set_text(lbl_meat1_label, "MEAT 1");
    lv_obj_set_style_text_color(lbl_meat1_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_meat1_label, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_meat1_label, LV_ALIGN_LEFT_MID, 12, 0);

    lbl_meat1_temp = lv_label_create(meat1_card);
    lv_label_set_text(lbl_meat1_temp, "---");
    lv_obj_set_style_text_color(lbl_meat1_temp, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl_meat1_temp, &lv_font_montserrat_36, 0);
    lv_obj_align(lbl_meat1_temp, LV_ALIGN_RIGHT_MID, -12, 0);

    // --- Meat 2 card (right, bottom) ---
    lv_obj_t* meat2_card = lv_obj_create(scr_dashboard);
    lv_obj_set_size(meat2_card, 220, 72);
    lv_obj_set_pos(meat2_card, 245, 118);
    lv_obj_set_style_bg_color(meat2_card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(meat2_card, 0, 0);
    lv_obj_set_style_radius(meat2_card, 8, 0);

    lbl_meat2_label = lv_label_create(meat2_card);
    lv_label_set_text(lbl_meat2_label, "MEAT 2");
    lv_obj_set_style_text_color(lbl_meat2_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_meat2_label, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_meat2_label, LV_ALIGN_LEFT_MID, 12, 0);

    lbl_meat2_temp = lv_label_create(meat2_card);
    lv_label_set_text(lbl_meat2_temp, "---");
    lv_obj_set_style_text_color(lbl_meat2_temp, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl_meat2_temp, &lv_font_montserrat_36, 0);
    lv_obj_align(lbl_meat2_temp, LV_ALIGN_RIGHT_MID, -12, 0);

    // --- Bottom info bar (fan, damper, PID, status) ---
    lv_obj_t* info_bar = lv_obj_create(scr_dashboard);
    lv_obj_set_size(info_bar, DISPLAY_WIDTH - 20, 70);
    lv_obj_set_pos(info_bar, 10, 205);
    lv_obj_set_style_bg_color(info_bar, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(info_bar, 0, 0);
    lv_obj_set_style_radius(info_bar, 8, 0);
    lv_obj_set_style_pad_all(info_bar, 8, 0);
    lv_obj_set_flex_flow(info_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(info_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Fan
    lv_obj_t* fan_col = lv_obj_create(info_bar);
    lv_obj_set_size(fan_col, 80, 50);
    lv_obj_set_style_bg_opa(fan_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fan_col, 0, 0);
    lv_obj_set_style_pad_all(fan_col, 0, 0);

    lv_obj_t* lbl = lv_label_create(fan_col);
    lv_label_set_text(lbl, "FAN");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 0);

    lbl_fan_pct = lv_label_create(fan_col);
    lv_label_set_text(lbl_fan_pct, "0%");
    lv_obj_set_style_text_color(lbl_fan_pct, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl_fan_pct, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_fan_pct, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Damper
    lv_obj_t* dmp_col = lv_obj_create(info_bar);
    lv_obj_set_size(dmp_col, 80, 50);
    lv_obj_set_style_bg_opa(dmp_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dmp_col, 0, 0);
    lv_obj_set_style_pad_all(dmp_col, 0, 0);

    lbl = lv_label_create(dmp_col);
    lv_label_set_text(lbl, "DAMPER");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 0);

    lbl_damper_pct = lv_label_create(dmp_col);
    lv_label_set_text(lbl_damper_pct, "0%");
    lv_obj_set_style_text_color(lbl_damper_pct, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl_damper_pct, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_damper_pct, LV_ALIGN_BOTTOM_MID, 0, 0);

    // PID output bar
    lv_obj_t* pid_col = lv_obj_create(info_bar);
    lv_obj_set_size(pid_col, 140, 50);
    lv_obj_set_style_bg_opa(pid_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pid_col, 0, 0);
    lv_obj_set_style_pad_all(pid_col, 0, 0);

    lbl = lv_label_create(pid_col);
    lv_label_set_text(lbl, "PID OUTPUT");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 0);

    bar_pid_output = lv_bar_create(pid_col);
    lv_obj_set_size(bar_pid_output, 120, 16);
    lv_bar_set_range(bar_pid_output, 0, 100);
    lv_bar_set_value(bar_pid_output, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_pid_output, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_pid_output, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_obj_align(bar_pid_output, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Status (lid open, fire out, etc.)
    lv_obj_t* stat_col = lv_obj_create(info_bar);
    lv_obj_set_size(stat_col, 80, 50);
    lv_obj_set_style_bg_opa(stat_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stat_col, 0, 0);
    lv_obj_set_style_pad_all(stat_col, 0, 0);

    lbl_status = lv_label_create(stat_col);
    lv_label_set_text(lbl_status, "OK");
    lv_obj_set_style_text_color(lbl_status, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, 0);

    // Estimated time
    lbl_est_time = lv_label_create(scr_dashboard);
    lv_label_set_text(lbl_est_time, "");
    lv_obj_set_style_text_color(lbl_est_time, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_est_time, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_est_time, 245, 196);

    // Navigation bar
    create_nav_bar(scr_dashboard);
}

// --------------------------------------------------------------------------
// Graph screen
// --------------------------------------------------------------------------

static void create_graph_screen() {
    scr_graph = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_graph, COLOR_BG, 0);

    lv_obj_t* title = lv_label_create(scr_graph);
    lv_label_set_text(title, "Temperature History");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    // Chart
    chart_temps = lv_chart_create(scr_graph);
    lv_obj_set_size(chart_temps, DISPLAY_WIDTH - 30, 220);
    lv_obj_align(chart_temps, LV_ALIGN_CENTER, 0, -5);
    lv_chart_set_type(chart_temps, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_temps, 120);  // ~10 min of data at 5s intervals
    lv_chart_set_range(chart_temps, LV_CHART_AXIS_PRIMARY_Y, 50, 350);
    lv_chart_set_div_line_count(chart_temps, 5, 8);
    lv_obj_set_style_bg_color(chart_temps, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(chart_temps, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_line_color(chart_temps, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_size(chart_temps, 0, 0, LV_PART_INDICATOR);  // Hide data point dots

    // Series
    ser_pit = lv_chart_add_series(chart_temps, COLOR_ORANGE, LV_CHART_AXIS_PRIMARY_Y);
    ser_meat1 = lv_chart_add_series(chart_temps, COLOR_TEXT, LV_CHART_AXIS_PRIMARY_Y);
    ser_meat2 = lv_chart_add_series(chart_temps, COLOR_BLUE, LV_CHART_AXIS_PRIMARY_Y);

    // Legend
    lv_obj_t* legend = lv_obj_create(scr_graph);
    lv_obj_set_size(legend, DISPLAY_WIDTH - 30, 24);
    lv_obj_align(legend, LV_ALIGN_BOTTOM_MID, 0, -44);
    lv_obj_set_style_bg_opa(legend, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(legend, 0, 0);
    lv_obj_set_style_pad_all(legend, 0, 0);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lbl;

    lbl = lv_label_create(legend);
    lv_label_set_text(lbl, LV_SYMBOL_MINUS " Pit  ");
    lv_obj_set_style_text_color(lbl, COLOR_ORANGE, 0);

    lbl = lv_label_create(legend);
    lv_label_set_text(lbl, LV_SYMBOL_MINUS " Meat1  ");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);

    lbl = lv_label_create(legend);
    lv_label_set_text(lbl, LV_SYMBOL_MINUS " Meat2");
    lv_obj_set_style_text_color(lbl, COLOR_BLUE, 0);

    // Navigation bar
    create_nav_bar(scr_graph);
}

// --------------------------------------------------------------------------
// Settings screen
// --------------------------------------------------------------------------

static void create_settings_screen() {
    scr_settings = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_settings, COLOR_BG, 0);

    lv_obj_t* title = lv_label_create(scr_settings);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    // Scrollable content area
    lv_obj_t* content = lv_obj_create(scr_settings);
    lv_obj_set_size(content, DISPLAY_WIDTH - 20, 220);
    lv_obj_align(content, LV_ALIGN_CENTER, 0, -5);
    lv_obj_set_style_bg_color(content, COLOR_BG, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 8, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content, 12, 0);

    // --- Setpoint ---
    lv_obj_t* row = lv_obj_create(content);
    lv_obj_set_size(row, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(row, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 8, 0);

    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, "Setpoint");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    spinbox_setpoint = lv_spinbox_create(row);
    lv_spinbox_set_range(spinbox_setpoint, 100, 500);
    lv_spinbox_set_value(spinbox_setpoint, 225);
    lv_spinbox_set_digit_format(spinbox_setpoint, 3, 0);
    lv_spinbox_set_step(spinbox_setpoint, 5);
    lv_obj_set_size(spinbox_setpoint, 80, 36);
    lv_obj_align(spinbox_setpoint, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_color(spinbox_setpoint, COLOR_ORANGE, 0);

    // --- Wi-Fi info ---
    row = lv_obj_create(content);
    lv_obj_set_size(row, LV_PCT(100), 40);
    lv_obj_set_style_bg_color(row, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 8, 0);

    lbl = lv_label_create(row);
    lv_label_set_text(lbl, "Wi-Fi: http://bbq.local");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    // --- Firmware version ---
    row = lv_obj_create(content);
    lv_obj_set_size(row, LV_PCT(100), 40);
    lv_obj_set_style_bg_color(row, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 8, 0);

    lbl = lv_label_create(row);
    lv_label_set_text_fmt(lbl, "Firmware: v%s", FIRMWARE_VERSION);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    // --- Factory Reset button ---
    lv_obj_t* btn_reset = lv_btn_create(content);
    lv_obj_set_size(btn_reset, 200, 40);
    lv_obj_set_style_bg_color(btn_reset, COLOR_RED, 0);
    lv_obj_set_style_radius(btn_reset, 6, 0);

    lbl = lv_label_create(btn_reset);
    lv_label_set_text(lbl, "Factory Reset");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    // Navigation bar
    create_nav_bar(scr_settings);
}

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

void ui_init() {
    // Initialize TFT
    tft.begin();
    tft.setRotation(1);  // Landscape
    tft.fillScreen(TFT_BLACK);

    // Initialize LVGL
    lv_init();

    // Create display
    lv_display_t* disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_buffers(disp, draw_buf1, draw_buf2, sizeof(draw_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, disp_flush_cb);

    // Create touch input device
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpad_read_cb);

    // Create all screens
    create_dashboard_screen();
    create_graph_screen();
    create_settings_screen();

    // Load dashboard as default
    lv_screen_load(scr_dashboard);
    current_screen = Screen::DASHBOARD;
}

void ui_switch_screen(Screen screen) {
    lv_obj_t* target = nullptr;
    switch (screen) {
        case Screen::DASHBOARD: target = scr_dashboard; break;
        case Screen::GRAPH:     target = scr_graph;     break;
        case Screen::SETTINGS:  target = scr_settings;  break;
    }
    if (target) {
        lv_screen_load_anim(target, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
        current_screen = screen;
    }
}

Screen ui_get_current_screen() {
    return current_screen;
}

void ui_tick(uint32_t ms) {
    lv_tick_inc(ms);
}

void ui_handler() {
    lv_timer_handler();
}

#else
// Native build stubs
void ui_init() {}
void ui_switch_screen(Screen) {}
Screen ui_get_current_screen() { return Screen::DASHBOARD; }
void ui_tick(uint32_t) {}
void ui_handler() {}
#endif
