#include "ui_init.h"
#include "ui_update.h"
#include "ui_setup_wizard.h"
#include "ui_colors.h"

#if !defined(NATIVE_BUILD) || defined(SIMULATOR_BUILD)

#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <cstring>

#ifndef SIMULATOR_BUILD
#include <TFT_eSPI.h>

// --------------------------------------------------------------------------
// Display driver (hardware)
// --------------------------------------------------------------------------

static TFT_eSPI tft = TFT_eSPI();

static lv_color_t draw_buf1[DISPLAY_WIDTH * 40];
static lv_color_t draw_buf2[DISPLAY_WIDTH * 40];

static void disp_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)px_map, w * h, true);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

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
#endif // !SIMULATOR_BUILD

// --------------------------------------------------------------------------
// Screens and key widgets (accessed by ui_update)
// --------------------------------------------------------------------------

static lv_obj_t* scr_dashboard = nullptr;
static lv_obj_t* scr_graph     = nullptr;
static lv_obj_t* scr_settings  = nullptr;

// Dashboard — top bar
lv_obj_t* lbl_wifi_icon    = nullptr;
lv_obj_t* lbl_start_time   = nullptr;
lv_obj_t* lbl_elapsed      = nullptr;
lv_obj_t* lbl_done_time    = nullptr;
lv_obj_t* lbl_version      = nullptr;

// Dashboard — output bars
lv_obj_t* bar_fan          = nullptr;
lv_obj_t* lbl_fan_bar      = nullptr;
lv_obj_t* bar_damper       = nullptr;
lv_obj_t* lbl_damper_bar   = nullptr;

// Dashboard — pit card
lv_obj_t* lbl_pit_temp     = nullptr;
lv_obj_t* lbl_pit_label    = nullptr;
lv_obj_t* lbl_setpoint     = nullptr;
lv_obj_t* lbl_pit_hint     = nullptr;

// Dashboard — meat cards
lv_obj_t* lbl_meat1_temp   = nullptr;
lv_obj_t* lbl_meat1_label  = nullptr;
lv_obj_t* lbl_meat1_target = nullptr;
lv_obj_t* lbl_meat1_est    = nullptr;
lv_obj_t* lbl_meat2_temp   = nullptr;
lv_obj_t* lbl_meat2_label  = nullptr;
lv_obj_t* lbl_meat2_target = nullptr;
lv_obj_t* lbl_meat2_est    = nullptr;

// Dashboard — alert banner
lv_obj_t* alert_banner     = nullptr;
lv_obj_t* lbl_alert_text   = nullptr;

// Graph widgets
lv_obj_t* chart_temps      = nullptr;
lv_chart_series_t* ser_pit   = nullptr;
lv_chart_series_t* ser_meat1 = nullptr;
lv_chart_series_t* ser_meat2 = nullptr;
lv_chart_series_t* ser_setpoint = nullptr;
lv_obj_t* graph_y_labels[5] = {};

// Settings widgets
lv_obj_t* btn_units_f       = nullptr;
lv_obj_t* btn_units_c       = nullptr;
lv_obj_t* btn_fan_only      = nullptr;
lv_obj_t* btn_fan_damper    = nullptr;
lv_obj_t* btn_damper_pri    = nullptr;

// Settings — Wi-Fi info widgets
lv_obj_t* lbl_wifi_status   = nullptr;
lv_obj_t* lbl_wifi_ssid     = nullptr;
lv_obj_t* lbl_wifi_ip       = nullptr;
lv_obj_t* lbl_wifi_signal   = nullptr;
lv_obj_t* btn_wifi_action   = nullptr;
lv_obj_t* lbl_wifi_action   = nullptr;

// Nav bar buttons (per screen, for active tab highlighting)
static lv_obj_t* nav_btns[3][3] = {}; // [screen_idx][btn_idx]

// Modals
static lv_obj_t* modal_setpoint   = nullptr;
static lv_obj_t* modal_meat       = nullptr;
static lv_obj_t* modal_confirm    = nullptr;

// Modal state
static float modal_sp_value  = 225;
static float modal_meat_value = 195;
static uint8_t modal_meat_probe = 1; // 1 or 2

// Confirm modal state
static lv_obj_t* lbl_confirm_title   = nullptr;
static lv_obj_t* lbl_confirm_msg     = nullptr;
static void (*confirm_action_cb)()   = nullptr;

// Modal widget refs
static lv_obj_t* lbl_modal_sp_value = nullptr;
static lv_obj_t* lbl_modal_meat_value = nullptr;
static lv_obj_t* lbl_modal_meat_title = nullptr;

static Screen current_screen = Screen::DASHBOARD;

// --------------------------------------------------------------------------
// Callbacks (set by main.cpp or sim_main.cpp)
// --------------------------------------------------------------------------

static UiSetpointCb    cb_setpoint    = nullptr;
static UiMeatTargetCb  cb_meat_target = nullptr;
static UiAlarmAckCb    cb_alarm_ack   = nullptr;
static UiUnitsCb       cb_units       = nullptr;
static UiFanModeCb     cb_fan_mode    = nullptr;
static UiNewSessionCb  cb_new_session = nullptr;
static UiFactoryResetCb cb_factory_reset = nullptr;
static UiWifiActionCb   cb_wifi_action  = nullptr;

void ui_set_callbacks(UiSetpointCb sp, UiMeatTargetCb meat, UiAlarmAckCb ack) {
    cb_setpoint = sp;
    cb_meat_target = meat;
    cb_alarm_ack = ack;
}

void ui_set_settings_callbacks(UiUnitsCb units, UiFanModeCb fan,
                                UiNewSessionCb session, UiFactoryResetCb reset) {
    cb_units = units;
    cb_fan_mode = fan;
    cb_new_session = session;
    cb_factory_reset = reset;
}

void ui_set_wifi_callback(UiWifiActionCb cb) {
    cb_wifi_action = cb;
}

// --------------------------------------------------------------------------
// Navigation
// --------------------------------------------------------------------------

static void update_nav_highlight(Screen screen);

static void nav_event_cb(lv_event_t* e) {
    Screen screen = (Screen)(uintptr_t)lv_event_get_user_data(e);
    ui_switch_screen(screen);
}

static lv_obj_t* create_nav_btn(lv_obj_t* parent, const char* icon, const char* text, Screen target) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 140, LV_PCT(100));
    lv_obj_set_style_bg_color(btn, COLOR_CARD_BG, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_add_event_cb(btn, nav_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)target);

    lv_obj_t* lbl = lv_label_create(btn);
    char buf[32];
    snprintf(buf, sizeof(buf), "%s %s", icon, text);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    return btn;
}

static void create_nav_bar(lv_obj_t* parent, uint8_t screen_idx) {
    lv_obj_t* nav = lv_obj_create(parent);
    lv_obj_set_size(nav, DISPLAY_WIDTH, 50);
    lv_obj_align(nav, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(nav, COLOR_NAV_BG, 0);
    lv_obj_set_style_border_width(nav, 0, 0);
    lv_obj_set_style_pad_all(nav, 4, 0);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    nav_btns[screen_idx][0] = create_nav_btn(nav, LV_SYMBOL_HOME,     "Home",     Screen::DASHBOARD);
    nav_btns[screen_idx][1] = create_nav_btn(nav, LV_SYMBOL_IMAGE,    "Graph",    Screen::GRAPH);
    nav_btns[screen_idx][2] = create_nav_btn(nav, LV_SYMBOL_SETTINGS, "Settings", Screen::SETTINGS);

    // Highlight the active tab
    lv_obj_set_style_bg_color(nav_btns[screen_idx][screen_idx], COLOR_ORANGE, 0);
}

static void update_nav_highlight(Screen screen) {
    uint8_t idx = (uint8_t)screen;
    for (uint8_t s = 0; s < 3; s++) {
        for (uint8_t b = 0; b < 3; b++) {
            if (nav_btns[s][b]) {
                lv_obj_set_style_bg_color(nav_btns[s][b],
                    (b == idx) ? COLOR_ORANGE : COLOR_CARD_BG, 0);
            }
        }
    }
}

// --------------------------------------------------------------------------
// Modal helpers
// --------------------------------------------------------------------------

static lv_obj_t* create_modal_overlay(lv_obj_t* parent) {
    lv_obj_t* overlay = lv_obj_create(parent);
    lv_obj_set_size(overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    return overlay;
}

static void hide_modal(lv_obj_t* modal) {
    if (modal) lv_obj_add_flag(modal, LV_OBJ_FLAG_HIDDEN);
}

static void show_modal(lv_obj_t* modal) {
    if (modal) lv_obj_remove_flag(modal, LV_OBJ_FLAG_HIDDEN);
}

// --------------------------------------------------------------------------
// Setpoint modal
// --------------------------------------------------------------------------

static void update_sp_modal_display() {
    if (lbl_modal_sp_value) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f\xC2\xB0" "F", modal_sp_value);
        lv_label_set_text(lbl_modal_sp_value, buf);
    }
}

static void sp_minus_cb(lv_event_t* e) {
    (void)e;
    modal_sp_value -= 5;
    if (modal_sp_value < 100) modal_sp_value = 100;
    update_sp_modal_display();
}

static void sp_plus_cb(lv_event_t* e) {
    (void)e;
    modal_sp_value += 5;
    if (modal_sp_value > 500) modal_sp_value = 500;
    update_sp_modal_display();
}

static void sp_cancel_cb(lv_event_t* e) {
    (void)e;
    hide_modal(modal_setpoint);
}

static void sp_apply_cb(lv_event_t* e) {
    (void)e;
    hide_modal(modal_setpoint);
    if (cb_setpoint) cb_setpoint(modal_sp_value);
    // Hide "tap to edit" hint after first use
    if (lbl_pit_hint) lv_obj_add_flag(lbl_pit_hint, LV_OBJ_FLAG_HIDDEN);
}

static void pit_card_click_cb(lv_event_t* e) {
    (void)e;
    update_sp_modal_display();
    show_modal(modal_setpoint);
}

static void create_setpoint_modal() {
    modal_setpoint = create_modal_overlay(lv_layer_top());

    lv_obj_t* card = lv_obj_create(modal_setpoint);
    lv_obj_set_size(card, 280, 180);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card, COLOR_ORANGE, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 12, 0);

    // Title
    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "Pit Setpoint");
    lv_obj_set_style_text_color(title, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // -5 button
    lv_obj_t* btn_minus = lv_btn_create(card);
    lv_obj_set_size(btn_minus, 64, 52);
    lv_obj_align(btn_minus, LV_ALIGN_CENTER, -80, -8);
    lv_obj_set_style_bg_color(btn_minus, COLOR_BAR_BG, 0);
    lv_obj_set_style_radius(btn_minus, 8, 0);
    lv_obj_add_event_cb(btn_minus, sp_minus_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl = lv_label_create(btn_minus);
    lv_label_set_text(lbl, "-5");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl);

    // Value display
    lbl_modal_sp_value = lv_label_create(card);
    lv_label_set_text(lbl_modal_sp_value, "225\xC2\xB0" "F");
    lv_obj_set_style_text_color(lbl_modal_sp_value, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(lbl_modal_sp_value, &lv_font_montserrat_36, 0);
    lv_obj_align(lbl_modal_sp_value, LV_ALIGN_CENTER, 0, -8);

    // +5 button
    lv_obj_t* btn_plus = lv_btn_create(card);
    lv_obj_set_size(btn_plus, 64, 52);
    lv_obj_align(btn_plus, LV_ALIGN_CENTER, 80, -8);
    lv_obj_set_style_bg_color(btn_plus, COLOR_BAR_BG, 0);
    lv_obj_set_style_radius(btn_plus, 8, 0);
    lv_obj_add_event_cb(btn_plus, sp_plus_cb, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_plus);
    lv_label_set_text(lbl, "+5");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl);

    // Range hint
    lv_obj_t* hint = lv_label_create(card);
    lv_label_set_text(hint, "100 - 500\xC2\xB0" "F");
    lv_obj_set_style_text_color(hint, COLOR_TEXT_VDIM, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 28);

    // Cancel button
    lv_obj_t* btn_cancel = lv_btn_create(card);
    lv_obj_set_size(btn_cancel, 110, 36);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 8, 0);
    lv_obj_set_style_bg_color(btn_cancel, COLOR_BAR_BG, 0);
    lv_obj_set_style_radius(btn_cancel, 6, 0);
    lv_obj_add_event_cb(btn_cancel, sp_cancel_cb, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_cancel);
    lv_label_set_text(lbl, "Cancel");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    // Apply button
    lv_obj_t* btn_apply = lv_btn_create(card);
    lv_obj_set_size(btn_apply, 110, 36);
    lv_obj_align(btn_apply, LV_ALIGN_BOTTOM_RIGHT, -8, 0);
    lv_obj_set_style_bg_color(btn_apply, COLOR_ORANGE, 0);
    lv_obj_set_style_radius(btn_apply, 6, 0);
    lv_obj_add_event_cb(btn_apply, sp_apply_cb, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_apply);
    lv_label_set_text(lbl, "Apply");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);
}

// --------------------------------------------------------------------------
// Meat target modal
// --------------------------------------------------------------------------

static void update_meat_modal_display() {
    if (lbl_modal_meat_value) {
        char buf[16];
        if (modal_meat_value > 0) {
            snprintf(buf, sizeof(buf), "%.0f\xC2\xB0" "F", modal_meat_value);
        } else {
            snprintf(buf, sizeof(buf), "---");
        }
        lv_label_set_text(lbl_modal_meat_value, buf);
    }
    if (lbl_modal_meat_title) {
        lv_label_set_text(lbl_modal_meat_title,
            modal_meat_probe == 1 ? "Meat 1 Target" : "Meat 2 Target");
        lv_obj_set_style_text_color(lbl_modal_meat_title,
            modal_meat_probe == 1 ? COLOR_RED : COLOR_BLUE, 0);
    }
}

static void meat_minus_cb(lv_event_t* e) {
    (void)e;
    modal_meat_value -= 5;
    if (modal_meat_value < 100) modal_meat_value = 100;
    update_meat_modal_display();
}

static void meat_plus_cb(lv_event_t* e) {
    (void)e;
    modal_meat_value += 5;
    if (modal_meat_value > 212) modal_meat_value = 212;
    update_meat_modal_display();
}

static void meat_cancel_cb(lv_event_t* e) {
    (void)e;
    hide_modal(modal_meat);
}

static void meat_set_cb(lv_event_t* e) {
    (void)e;
    hide_modal(modal_meat);
    if (cb_meat_target) cb_meat_target(modal_meat_probe, modal_meat_value);
}

static void meat_clear_cb(lv_event_t* e) {
    (void)e;
    hide_modal(modal_meat);
    if (cb_meat_target) cb_meat_target(modal_meat_probe, 0);
}

static void meat1_card_click_cb(lv_event_t* e) {
    (void)e;
    modal_meat_probe = 1;
    if (modal_meat_value <= 0) modal_meat_value = 195;
    update_meat_modal_display();
    show_modal(modal_meat);
}

static void meat2_card_click_cb(lv_event_t* e) {
    (void)e;
    modal_meat_probe = 2;
    if (modal_meat_value <= 0) modal_meat_value = 195;
    update_meat_modal_display();
    show_modal(modal_meat);
}

static void create_meat_target_modal() {
    modal_meat = create_modal_overlay(lv_layer_top());

    lv_obj_t* card = lv_obj_create(modal_meat);
    lv_obj_set_size(card, 280, 210);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card, COLOR_RED, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 12, 0);

    // Title
    lbl_modal_meat_title = lv_label_create(card);
    lv_label_set_text(lbl_modal_meat_title, "Meat 1 Target");
    lv_obj_set_style_text_color(lbl_modal_meat_title, COLOR_RED, 0);
    lv_obj_set_style_text_font(lbl_modal_meat_title, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_modal_meat_title, LV_ALIGN_TOP_MID, 0, 0);

    // -5 button
    lv_obj_t* btn_minus = lv_btn_create(card);
    lv_obj_set_size(btn_minus, 64, 52);
    lv_obj_align(btn_minus, LV_ALIGN_CENTER, -80, -14);
    lv_obj_set_style_bg_color(btn_minus, COLOR_BAR_BG, 0);
    lv_obj_set_style_radius(btn_minus, 8, 0);
    lv_obj_add_event_cb(btn_minus, meat_minus_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl = lv_label_create(btn_minus);
    lv_label_set_text(lbl, "-5");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl);

    // Value display
    lbl_modal_meat_value = lv_label_create(card);
    lv_label_set_text(lbl_modal_meat_value, "195\xC2\xB0" "F");
    lv_obj_set_style_text_color(lbl_modal_meat_value, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl_modal_meat_value, &lv_font_montserrat_36, 0);
    lv_obj_align(lbl_modal_meat_value, LV_ALIGN_CENTER, 0, -14);

    // +5 button
    lv_obj_t* btn_plus = lv_btn_create(card);
    lv_obj_set_size(btn_plus, 64, 52);
    lv_obj_align(btn_plus, LV_ALIGN_CENTER, 80, -14);
    lv_obj_set_style_bg_color(btn_plus, COLOR_BAR_BG, 0);
    lv_obj_set_style_radius(btn_plus, 8, 0);
    lv_obj_add_event_cb(btn_plus, meat_plus_cb, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_plus);
    lv_label_set_text(lbl, "+5");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl);

    // Range hint
    lv_obj_t* hint = lv_label_create(card);
    lv_label_set_text(hint, "100 - 212\xC2\xB0" "F");
    lv_obj_set_style_text_color(hint, COLOR_TEXT_VDIM, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 22);

    // Clear button
    lv_obj_t* btn_clear = lv_btn_create(card);
    lv_obj_set_size(btn_clear, 76, 36);
    lv_obj_align(btn_clear, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(btn_clear, COLOR_BAR_BG, 0);
    lv_obj_set_style_radius(btn_clear, 6, 0);
    lv_obj_add_event_cb(btn_clear, meat_clear_cb, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_clear);
    lv_label_set_text(lbl, "Clear");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
    lv_obj_center(lbl);

    // Cancel button
    lv_obj_t* btn_cancel = lv_btn_create(card);
    lv_obj_set_size(btn_cancel, 76, 36);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_cancel, COLOR_BAR_BG, 0);
    lv_obj_set_style_radius(btn_cancel, 6, 0);
    lv_obj_add_event_cb(btn_cancel, meat_cancel_cb, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_cancel);
    lv_label_set_text(lbl, "Cancel");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    // Set button
    lv_obj_t* btn_set = lv_btn_create(card);
    lv_obj_set_size(btn_set, 76, 36);
    lv_obj_align(btn_set, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_set, COLOR_ORANGE, 0);
    lv_obj_set_style_radius(btn_set, 6, 0);
    lv_obj_add_event_cb(btn_set, meat_set_cb, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_set);
    lv_label_set_text(lbl, "Set");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);
}

// --------------------------------------------------------------------------
// Confirmation modal
// --------------------------------------------------------------------------

static void confirm_cancel_cb(lv_event_t* e) {
    (void)e;
    hide_modal(modal_confirm);
}

static void confirm_ok_cb(lv_event_t* e) {
    (void)e;
    hide_modal(modal_confirm);
    if (confirm_action_cb) confirm_action_cb();
}

static void show_confirm(const char* title, const char* msg, void (*action)()) {
    confirm_action_cb = action;
    if (lbl_confirm_title) lv_label_set_text(lbl_confirm_title, title);
    if (lbl_confirm_msg) lv_label_set_text(lbl_confirm_msg, msg);
    show_modal(modal_confirm);
}

static void create_confirm_modal() {
    modal_confirm = create_modal_overlay(lv_layer_top());
    lv_obj_t* card = lv_obj_create(modal_confirm);
    lv_obj_set_size(card, 280, 150);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(card, COLOR_RED, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 12, 0);

    lbl_confirm_title = lv_label_create(card);
    lv_label_set_text(lbl_confirm_title, "Confirm");
    lv_obj_set_style_text_color(lbl_confirm_title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl_confirm_title, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_confirm_title, LV_ALIGN_TOP_MID, 0, 0);

    lbl_confirm_msg = lv_label_create(card);
    lv_label_set_text(lbl_confirm_msg, "Are you sure?");
    lv_obj_set_style_text_color(lbl_confirm_msg, COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl_confirm_msg, LV_ALIGN_CENTER, 0, -4);

    lv_obj_t* btn_cancel = lv_btn_create(card);
    lv_obj_set_size(btn_cancel, 110, 36);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 8, 0);
    lv_obj_set_style_bg_color(btn_cancel, COLOR_BAR_BG, 0);
    lv_obj_set_style_radius(btn_cancel, 6, 0);
    lv_obj_add_event_cb(btn_cancel, confirm_cancel_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl = lv_label_create(btn_cancel);
    lv_label_set_text(lbl, "Cancel");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    lv_obj_t* btn_ok = lv_btn_create(card);
    lv_obj_set_size(btn_ok, 110, 36);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_RIGHT, -8, 0);
    lv_obj_set_style_bg_color(btn_ok, COLOR_RED, 0);
    lv_obj_set_style_radius(btn_ok, 6, 0);
    lv_obj_add_event_cb(btn_ok, confirm_ok_cb, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_ok);
    lv_label_set_text(lbl, "Confirm");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);
}

// --------------------------------------------------------------------------
// Alert banner tap handler
// --------------------------------------------------------------------------

static void alert_tap_cb(lv_event_t* e) {
    (void)e;
    if (cb_alarm_ack) cb_alarm_ack();
}

// --------------------------------------------------------------------------
// Dashboard screen
// --------------------------------------------------------------------------

static void create_dashboard_screen() {
    scr_dashboard = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_dashboard, COLOR_BG, 0);

    // --- Top bar (34px) ---
    lv_obj_t* top_bar = lv_obj_create(scr_dashboard);
    lv_obj_set_size(top_bar, DISPLAY_WIDTH, 34);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar, COLOR_NAV_BG, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 2, 0);

    lbl_wifi_icon = lv_label_create(top_bar);
    lv_label_set_text(lbl_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(lbl_wifi_icon, COLOR_GREEN, 0);
    lv_obj_align(lbl_wifi_icon, LV_ALIGN_LEFT_MID, 4, 0);

    lbl_start_time = lv_label_create(top_bar);
    lv_label_set_text(lbl_start_time, "");
    lv_obj_set_style_text_color(lbl_start_time, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_start_time, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_start_time, LV_ALIGN_LEFT_MID, 30, 0);

    lbl_elapsed = lv_label_create(top_bar);
    lv_label_set_text(lbl_elapsed, "00:00:00");
    lv_obj_set_style_text_color(lbl_elapsed, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl_elapsed, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_elapsed, LV_ALIGN_CENTER, 0, 0);

    lbl_done_time = lv_label_create(top_bar);
    lv_label_set_text(lbl_done_time, "");
    lv_obj_set_style_text_color(lbl_done_time, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_done_time, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_done_time, LV_ALIGN_RIGHT_MID, -60, 0);

    lbl_version = lv_label_create(top_bar);
    lv_label_set_text(lbl_version, "v" FIRMWARE_VERSION);
    lv_obj_set_style_text_color(lbl_version, COLOR_TEXT_VDIM, 0);
    lv_obj_set_style_text_font(lbl_version, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_version, LV_ALIGN_RIGHT_MID, -4, 0);

    // --- Output bars (20px) ---
    lv_obj_t* bar_row = lv_obj_create(scr_dashboard);
    lv_obj_set_size(bar_row, DISPLAY_WIDTH - 20, 20);
    lv_obj_set_pos(bar_row, 10, 34);
    lv_obj_set_style_bg_opa(bar_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar_row, 0, 0);
    lv_obj_set_style_pad_all(bar_row, 0, 0);

    // Fan bar — left half: label + bar
    lbl_fan_bar = lv_label_create(bar_row);
    lv_label_set_text(lbl_fan_bar, "FAN 0%");
    lv_obj_set_style_text_color(lbl_fan_bar, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(lbl_fan_bar, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_fan_bar, 0, 0);

    bar_fan = lv_bar_create(bar_row);
    lv_obj_set_size(bar_fan, 130, 6);
    lv_bar_set_range(bar_fan, 0, 100);
    lv_bar_set_value(bar_fan, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_fan, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_fan, COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_fan, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_fan, 3, LV_PART_INDICATOR);
    lv_obj_set_pos(bar_fan, 82, 5);

    // Damper bar — right half: label + bar
    lbl_damper_bar = lv_label_create(bar_row);
    lv_label_set_text(lbl_damper_bar, "DAMPER 0%");
    lv_obj_set_style_text_color(lbl_damper_bar, COLOR_PURPLE, 0);
    lv_obj_set_style_text_font(lbl_damper_bar, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_damper_bar, 230, 0);

    bar_damper = lv_bar_create(bar_row);
    lv_obj_set_size(bar_damper, 120, 6);
    lv_bar_set_range(bar_damper, 0, 100);
    lv_bar_set_value(bar_damper, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_damper, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_damper, COLOR_PURPLE, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_damper, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_damper, 3, LV_PART_INDICATOR);
    lv_obj_set_pos(bar_damper, 330, 5);

    // --- Pit card (left, tappable) ---
    lv_obj_t* pit_card = lv_obj_create(scr_dashboard);
    lv_obj_set_size(pit_card, 228, 210);
    lv_obj_set_pos(pit_card, 6, 56);
    lv_obj_set_style_bg_color(pit_card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(pit_card, 0, 0);
    lv_obj_set_style_radius(pit_card, 8, 0);
    lv_obj_set_style_border_side(pit_card, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(pit_card, 3, 0);
    lv_obj_set_style_border_color(pit_card, COLOR_ORANGE, 0);
    lv_obj_add_event_cb(pit_card, pit_card_click_cb, LV_EVENT_CLICKED, nullptr);

    lbl_pit_label = lv_label_create(pit_card);
    lv_label_set_text(lbl_pit_label, "PIT");
    lv_obj_set_style_text_color(lbl_pit_label, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(lbl_pit_label, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_pit_label, LV_ALIGN_TOP_MID, 0, 4);

    lbl_pit_temp = lv_label_create(pit_card);
    lv_label_set_text(lbl_pit_temp, "---");
    lv_obj_set_style_text_color(lbl_pit_temp, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(lbl_pit_temp, &lv_font_montserrat_48, 0);
    lv_obj_align(lbl_pit_temp, LV_ALIGN_CENTER, 0, -8);

    lbl_setpoint = lv_label_create(pit_card);
    lv_label_set_text(lbl_setpoint, "Set: 225\xC2\xB0" "F");
    lv_obj_set_style_text_color(lbl_setpoint, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_setpoint, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_setpoint, LV_ALIGN_BOTTOM_MID, 0, -24);

    lbl_pit_hint = lv_label_create(pit_card);
    lv_label_set_text(lbl_pit_hint, "tap to edit");
    lv_obj_set_style_text_color(lbl_pit_hint, COLOR_TEXT_VDIM, 0);
    lv_obj_set_style_text_font(lbl_pit_hint, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_pit_hint, LV_ALIGN_BOTTOM_MID, 0, -6);

    // --- Meat 1 card (right top, tappable) ---
    lv_obj_t* meat1_card = lv_obj_create(scr_dashboard);
    lv_obj_set_size(meat1_card, 228, 103);
    lv_obj_set_pos(meat1_card, 244, 56);
    lv_obj_set_style_bg_color(meat1_card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(meat1_card, 0, 0);
    lv_obj_set_style_radius(meat1_card, 8, 0);
    lv_obj_set_style_border_side(meat1_card, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(meat1_card, 3, 0);
    lv_obj_set_style_border_color(meat1_card, COLOR_RED, 0);
    lv_obj_add_event_cb(meat1_card, meat1_card_click_cb, LV_EVENT_CLICKED, nullptr);

    lbl_meat1_label = lv_label_create(meat1_card);
    lv_label_set_text(lbl_meat1_label, "MEAT 1");
    lv_obj_set_style_text_color(lbl_meat1_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_meat1_label, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_meat1_label, LV_ALIGN_TOP_LEFT, 8, 4);

    lbl_meat1_temp = lv_label_create(meat1_card);
    lv_label_set_text(lbl_meat1_temp, "---");
    lv_obj_set_style_text_color(lbl_meat1_temp, COLOR_RED, 0);
    lv_obj_set_style_text_font(lbl_meat1_temp, &lv_font_montserrat_36, 0);
    lv_obj_align(lbl_meat1_temp, LV_ALIGN_RIGHT_MID, -8, 0);

    lbl_meat1_target = lv_label_create(meat1_card);
    lv_label_set_text(lbl_meat1_target, "Target: ---");
    lv_obj_set_style_text_color(lbl_meat1_target, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_meat1_target, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_meat1_target, LV_ALIGN_LEFT_MID, 8, 4);

    lbl_meat1_est = lv_label_create(meat1_card);
    lv_label_set_text(lbl_meat1_est, "");
    lv_obj_set_style_text_color(lbl_meat1_est, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_meat1_est, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_meat1_est, LV_ALIGN_BOTTOM_LEFT, 8, -4);

    // --- Meat 2 card (right bottom, tappable) ---
    lv_obj_t* meat2_card = lv_obj_create(scr_dashboard);
    lv_obj_set_size(meat2_card, 228, 103);
    lv_obj_set_pos(meat2_card, 244, 163);
    lv_obj_set_style_bg_color(meat2_card, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(meat2_card, 0, 0);
    lv_obj_set_style_radius(meat2_card, 8, 0);
    lv_obj_set_style_border_side(meat2_card, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(meat2_card, 3, 0);
    lv_obj_set_style_border_color(meat2_card, COLOR_BLUE, 0);
    lv_obj_add_event_cb(meat2_card, meat2_card_click_cb, LV_EVENT_CLICKED, nullptr);

    lbl_meat2_label = lv_label_create(meat2_card);
    lv_label_set_text(lbl_meat2_label, "MEAT 2");
    lv_obj_set_style_text_color(lbl_meat2_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_meat2_label, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_meat2_label, LV_ALIGN_TOP_LEFT, 8, 4);

    lbl_meat2_temp = lv_label_create(meat2_card);
    lv_label_set_text(lbl_meat2_temp, "---");
    lv_obj_set_style_text_color(lbl_meat2_temp, COLOR_BLUE, 0);
    lv_obj_set_style_text_font(lbl_meat2_temp, &lv_font_montserrat_36, 0);
    lv_obj_align(lbl_meat2_temp, LV_ALIGN_RIGHT_MID, -8, 0);

    lbl_meat2_target = lv_label_create(meat2_card);
    lv_label_set_text(lbl_meat2_target, "Target: ---");
    lv_obj_set_style_text_color(lbl_meat2_target, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_meat2_target, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_meat2_target, LV_ALIGN_LEFT_MID, 8, 4);

    lbl_meat2_est = lv_label_create(meat2_card);
    lv_label_set_text(lbl_meat2_est, "");
    lv_obj_set_style_text_color(lbl_meat2_est, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_meat2_est, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_meat2_est, LV_ALIGN_BOTTOM_LEFT, 8, -4);

    // --- Alert banner (32px, hidden by default) ---
    alert_banner = lv_obj_create(scr_dashboard);
    lv_obj_set_size(alert_banner, DISPLAY_WIDTH - 12, 28);
    lv_obj_set_pos(alert_banner, 6, 238);
    lv_obj_set_style_bg_color(alert_banner, COLOR_RED, 0);
    lv_obj_set_style_border_width(alert_banner, 0, 0);
    lv_obj_set_style_radius(alert_banner, 6, 0);
    lv_obj_set_style_pad_all(alert_banner, 4, 0);
    lv_obj_add_event_cb(alert_banner, alert_tap_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(alert_banner, LV_OBJ_FLAG_HIDDEN);

    lbl_alert_text = lv_label_create(alert_banner);
    lv_label_set_text(lbl_alert_text, "");
    lv_obj_set_style_text_color(lbl_alert_text, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl_alert_text, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_alert_text);

    // Navigation bar
    create_nav_bar(scr_dashboard, 0);
}

// --------------------------------------------------------------------------
// Graph screen
// --------------------------------------------------------------------------

static void create_graph_screen() {
    scr_graph = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_graph, COLOR_BG, 0);

    // Layout constants
    const int chart_x = 36;
    const int chart_y = 22;
    const int chart_w = DISPLAY_WIDTH - chart_x - 6;  // 438
    const int chart_h = 210;
    // Nav bar top = 270, legend below chart, chart bottom = 232

    // Title
    lv_obj_t* title = lv_label_create(scr_graph);
    lv_label_set_text(title, "Temperature History");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 14, 2);

    // Chart
    chart_temps = lv_chart_create(scr_graph);
    lv_obj_set_size(chart_temps, chart_w, chart_h);
    lv_obj_set_pos(chart_temps, chart_x, chart_y);
    lv_chart_set_type(chart_temps, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_temps, 240);  // ~20 min at 5s intervals
    lv_chart_set_range(chart_temps, LV_CHART_AXIS_PRIMARY_Y, 50, 350);
    lv_chart_set_div_line_count(chart_temps, 5, 8);
    lv_obj_set_style_bg_color(chart_temps, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(chart_temps, COLOR_BAR_BG, 0);
    lv_obj_set_style_border_width(chart_temps, 1, 0);
    lv_obj_set_style_line_color(chart_temps, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_line_opa(chart_temps, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_size(chart_temps, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(chart_temps, 2, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(chart_temps, 6, 0);
    lv_obj_set_style_pad_bottom(chart_temps, 6, 0);
    lv_obj_set_style_pad_left(chart_temps, 2, 0);
    lv_obj_set_style_pad_right(chart_temps, 4, 0);
    lv_obj_set_style_radius(chart_temps, 4, 0);

    // Y-axis labels — aligned with chart division lines
    // Positions match the 5 horizontal div lines; text updated dynamically by auto-scale
    const int content_top = chart_y + 6;
    const int content_h = chart_h - 12;
    const int y_temps[] = {300, 250, 200, 150, 100};
    for (int i = 0; i < 5; i++) {
        graph_y_labels[i] = lv_label_create(scr_graph);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", y_temps[i]);
        lv_label_set_text(graph_y_labels[i], buf);
        lv_obj_set_style_text_color(graph_y_labels[i], COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(graph_y_labels[i], &lv_font_montserrat_14, 0);
        int line_y = content_top + content_h * (i + 1) / 6;
        lv_obj_set_pos(graph_y_labels[i], 2, line_y - 7);  // -7 to center 14px font
    }

    // Series — order matters for legend
    ser_pit      = lv_chart_add_series(chart_temps, COLOR_ORANGE, LV_CHART_AXIS_PRIMARY_Y);
    ser_meat1    = lv_chart_add_series(chart_temps, COLOR_RED, LV_CHART_AXIS_PRIMARY_Y);
    ser_meat2    = lv_chart_add_series(chart_temps, COLOR_BLUE, LV_CHART_AXIS_PRIMARY_Y);
    ser_setpoint = lv_chart_add_series(chart_temps, lv_color_hex(0x999999), LV_CHART_AXIS_PRIMARY_Y);

    // Legend with colored swatches — positioned just below chart
    lv_obj_t* legend = lv_obj_create(scr_graph);
    lv_obj_set_size(legend, chart_w, 24);
    lv_obj_set_pos(legend, chart_x, chart_y + chart_h + 6);
    lv_obj_set_style_bg_opa(legend, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(legend, 0, 0);
    lv_obj_set_style_pad_all(legend, 0, 0);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(legend, 6, 0);

    // Helper: swatch + label
    auto add_legend_item = [&](lv_color_t color, const char* text) {
        lv_obj_t* swatch = lv_obj_create(legend);
        lv_obj_set_size(swatch, 12, 12);
        lv_obj_set_style_bg_color(swatch, color, 0);
        lv_obj_set_style_border_width(swatch, 0, 0);
        lv_obj_set_style_radius(swatch, 2, 0);
        lv_obj_set_style_pad_all(swatch, 0, 0);

        lv_obj_t* lbl = lv_label_create(legend);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    };

    add_legend_item(COLOR_ORANGE,        "Pit");
    add_legend_item(COLOR_RED,           "Meat1");
    add_legend_item(COLOR_BLUE,          "Meat2");
    add_legend_item(lv_color_hex(0x999999), "Set");

    // Navigation bar
    create_nav_bar(scr_graph, 1);
}

// --------------------------------------------------------------------------
// Settings screen
// --------------------------------------------------------------------------

static void units_f_click(lv_event_t* e) {
    (void)e;
    if (cb_units) cb_units(true);
    if (btn_units_f) lv_obj_set_style_bg_color(btn_units_f, COLOR_ORANGE, 0);
    if (btn_units_c) lv_obj_set_style_bg_color(btn_units_c, COLOR_BAR_BG, 0);
}

static void units_c_click(lv_event_t* e) {
    (void)e;
    if (cb_units) cb_units(false);
    if (btn_units_f) lv_obj_set_style_bg_color(btn_units_f, COLOR_BAR_BG, 0);
    if (btn_units_c) lv_obj_set_style_bg_color(btn_units_c, COLOR_ORANGE, 0);
}

static void fan_only_click(lv_event_t* e) {
    (void)e;
    if (cb_fan_mode) cb_fan_mode("fan_only");
    if (btn_fan_only)   lv_obj_set_style_bg_color(btn_fan_only, COLOR_ORANGE, 0);
    if (btn_fan_damper)  lv_obj_set_style_bg_color(btn_fan_damper, COLOR_BAR_BG, 0);
    if (btn_damper_pri)  lv_obj_set_style_bg_color(btn_damper_pri, COLOR_BAR_BG, 0);
}

static void fan_damper_click(lv_event_t* e) {
    (void)e;
    if (cb_fan_mode) cb_fan_mode("fan_and_damper");
    if (btn_fan_only)   lv_obj_set_style_bg_color(btn_fan_only, COLOR_BAR_BG, 0);
    if (btn_fan_damper)  lv_obj_set_style_bg_color(btn_fan_damper, COLOR_ORANGE, 0);
    if (btn_damper_pri)  lv_obj_set_style_bg_color(btn_damper_pri, COLOR_BAR_BG, 0);
}

static void damper_pri_click(lv_event_t* e) {
    (void)e;
    if (cb_fan_mode) cb_fan_mode("damper_primary");
    if (btn_fan_only)   lv_obj_set_style_bg_color(btn_fan_only, COLOR_BAR_BG, 0);
    if (btn_fan_damper)  lv_obj_set_style_bg_color(btn_fan_damper, COLOR_BAR_BG, 0);
    if (btn_damper_pri)  lv_obj_set_style_bg_color(btn_damper_pri, COLOR_ORANGE, 0);
}

static void new_session_click(lv_event_t* e) {
    (void)e;
    show_confirm("New Session",
                 "Start a new cook session?\nCurrent data will be lost.",
                 []() { if (cb_new_session) cb_new_session(); });
}

static void factory_reset_click(lv_event_t* e) {
    (void)e;
    show_confirm("Factory Reset",
                 "Erase all settings and data?\nDevice will restart.",
                 []() { if (cb_factory_reset) cb_factory_reset(); });
}

static void wifi_action_click(lv_event_t* e) {
    (void)e;
    if (!lbl_wifi_action) return;
    const char* text = lv_label_get_text(lbl_wifi_action);
    if (strcmp(text, "Disconnect") == 0) {
        show_confirm("Disconnect Wi-Fi",
                     "Web clients will lose connection.\nDisconnect?",
                     []() { if (cb_wifi_action) cb_wifi_action("disconnect"); });
    } else {
        if (cb_wifi_action) cb_wifi_action("reconnect");
    }
}

static void wifi_setup_click(lv_event_t* e) {
    (void)e;
    show_confirm("Setup Mode",
                 "Start Wi-Fi setup AP?\nCurrent connection will drop.",
                 []() { if (cb_wifi_action) cb_wifi_action("setup_ap"); });
}

static void create_settings_screen() {
    scr_settings = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_settings, COLOR_BG, 0);

    lv_obj_t* title = lv_label_create(scr_settings);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    // Scrollable content area
    lv_obj_t* content = lv_obj_create(scr_settings);
    lv_obj_set_size(content, DISPLAY_WIDTH - 16, 226);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_set_style_bg_color(content, COLOR_BG, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 8, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content, 10, 0);

    // --- Temperature Units ---
    lv_obj_t* row = lv_obj_create(content);
    lv_obj_set_size(row, LV_PCT(100), 44);
    lv_obj_set_style_bg_color(row, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 6, 0);

    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, "Units");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 4, 0);

    btn_units_f = lv_btn_create(row);
    lv_obj_set_size(btn_units_f, 56, 30);
    lv_obj_align(btn_units_f, LV_ALIGN_RIGHT_MID, -64, 0);
    lv_obj_set_style_bg_color(btn_units_f, COLOR_ORANGE, 0);
    lv_obj_set_style_radius(btn_units_f, 4, 0);
    lv_obj_add_event_cb(btn_units_f, units_f_click, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_units_f);
    lv_label_set_text(lbl, "\xC2\xB0" "F");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    btn_units_c = lv_btn_create(row);
    lv_obj_set_size(btn_units_c, 56, 30);
    lv_obj_align(btn_units_c, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_units_c, COLOR_BAR_BG, 0);
    lv_obj_set_style_radius(btn_units_c, 4, 0);
    lv_obj_add_event_cb(btn_units_c, units_c_click, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_units_c);
    lv_label_set_text(lbl, "\xC2\xB0" "C");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    // --- Fan Mode ---
    row = lv_obj_create(content);
    lv_obj_set_size(row, LV_PCT(100), 44);
    lv_obj_set_style_bg_color(row, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 6, 0);

    lbl = lv_label_create(row);
    lv_label_set_text(lbl, "Fan");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 4, 0);

    btn_fan_only = lv_btn_create(row);
    lv_obj_set_size(btn_fan_only, 80, 30);
    lv_obj_align(btn_fan_only, LV_ALIGN_RIGHT_MID, -178, 0);
    lv_obj_set_style_bg_color(btn_fan_only, COLOR_BAR_BG, 0);
    lv_obj_set_style_radius(btn_fan_only, 4, 0);
    lv_obj_add_event_cb(btn_fan_only, fan_only_click, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_fan_only);
    lv_label_set_text(lbl, "Fan");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    btn_fan_damper = lv_btn_create(row);
    lv_obj_set_size(btn_fan_damper, 80, 30);
    lv_obj_align(btn_fan_damper, LV_ALIGN_RIGHT_MID, -90, 0);
    lv_obj_set_style_bg_color(btn_fan_damper, COLOR_ORANGE, 0);
    lv_obj_set_style_radius(btn_fan_damper, 4, 0);
    lv_obj_add_event_cb(btn_fan_damper, fan_damper_click, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_fan_damper);
    lv_label_set_text(lbl, "F+D");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    btn_damper_pri = lv_btn_create(row);
    lv_obj_set_size(btn_damper_pri, 80, 30);
    lv_obj_align(btn_damper_pri, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_damper_pri, COLOR_BAR_BG, 0);
    lv_obj_set_style_radius(btn_damper_pri, 4, 0);
    lv_obj_add_event_cb(btn_damper_pri, damper_pri_click, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_damper_pri);
    lv_label_set_text(lbl, "Damper");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    // --- New Session button ---
    lv_obj_t* btn_session = lv_btn_create(content);
    lv_obj_set_size(btn_session, LV_PCT(100), 40);
    lv_obj_set_style_bg_color(btn_session, COLOR_CARD_BG, 0);
    lv_obj_set_style_radius(btn_session, 6, 0);
    lv_obj_add_event_cb(btn_session, new_session_click, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_session);
    lv_label_set_text(lbl, "New Session");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    // --- Wi-Fi Info Card ---
    row = lv_obj_create(content);
    lv_obj_set_size(row, LV_PCT(100), 80);
    lv_obj_set_style_bg_color(row, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 6, 0);

    // Wi-Fi icon + status label
    lv_obj_t* wifi_hdr = lv_label_create(row);
    lv_label_set_text(wifi_hdr, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_hdr, COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(wifi_hdr, 4, 2);

    lbl_wifi_status = lv_label_create(row);
    lv_label_set_text(lbl_wifi_status, "Disconnected");
    lv_obj_set_style_text_color(lbl_wifi_status, COLOR_RED, 0);
    lv_obj_set_style_text_font(lbl_wifi_status, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_wifi_status, 26, 2);

    lbl_wifi_ssid = lv_label_create(row);
    lv_label_set_text(lbl_wifi_ssid, "SSID: ---");
    lv_obj_set_style_text_color(lbl_wifi_ssid, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_wifi_ssid, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_wifi_ssid, 4, 20);

    lbl_wifi_ip = lv_label_create(row);
    lv_label_set_text(lbl_wifi_ip, "IP: ---");
    lv_obj_set_style_text_color(lbl_wifi_ip, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_wifi_ip, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_wifi_ip, 4, 38);

    lbl_wifi_signal = lv_label_create(row);
    lv_label_set_text(lbl_wifi_signal, "Signal: ---");
    lv_obj_set_style_text_color(lbl_wifi_signal, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_wifi_signal, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_wifi_signal, 4, 56);

    // --- Wi-Fi Actions Row ---
    row = lv_obj_create(content);
    lv_obj_set_size(row, LV_PCT(100), 44);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);

    btn_wifi_action = lv_btn_create(row);
    lv_obj_set_size(btn_wifi_action, 160, 36);
    lv_obj_align(btn_wifi_action, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_wifi_action, COLOR_CARD_BG, 0);
    lv_obj_set_style_radius(btn_wifi_action, 6, 0);
    lv_obj_add_event_cb(btn_wifi_action, wifi_action_click, LV_EVENT_CLICKED, nullptr);
    lbl_wifi_action = lv_label_create(btn_wifi_action);
    lv_label_set_text(lbl_wifi_action, "Disconnect");
    lv_obj_set_style_text_color(lbl_wifi_action, COLOR_TEXT, 0);
    lv_obj_center(lbl_wifi_action);

    lv_obj_t* btn_setup = lv_btn_create(row);
    lv_obj_set_size(btn_setup, 160, 36);
    lv_obj_align(btn_setup, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_setup, COLOR_CARD_BG, 0);
    lv_obj_set_style_radius(btn_setup, 6, 0);
    lv_obj_add_event_cb(btn_setup, wifi_setup_click, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_setup);
    lv_label_set_text(lbl, "Setup Mode");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    // --- Firmware Info ---
    lbl = lv_label_create(content);
    lv_label_set_text_fmt(lbl, "Firmware: v%s", FIRMWARE_VERSION);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

    // --- Factory Reset button ---
    lv_obj_t* btn_reset = lv_btn_create(content);
    lv_obj_set_size(btn_reset, LV_PCT(100), 40);
    lv_obj_set_style_bg_color(btn_reset, COLOR_RED, 0);
    lv_obj_set_style_radius(btn_reset, 6, 0);
    lv_obj_add_event_cb(btn_reset, factory_reset_click, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn_reset);
    lv_label_set_text(lbl, "Factory Reset");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    // Navigation bar
    create_nav_bar(scr_settings, 2);
}

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

void ui_init() {
    lv_init();

#ifdef SIMULATOR_BUILD
    lv_display_t* disp = lv_sdl_window_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_indev_t* mouse = lv_sdl_mouse_create();
    (void)disp;
    (void)mouse;
#else
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    lv_display_t* disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_buffers(disp, draw_buf1, draw_buf2, sizeof(draw_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, disp_flush_cb);

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpad_read_cb);
#endif

    create_dashboard_screen();
    create_graph_screen();
    create_settings_screen();

    create_setpoint_modal();
    create_meat_target_modal();
    create_confirm_modal();

    // Bind external arrays to chart series for adaptive condensing
    ui_graph_init();

    lv_screen_load(scr_dashboard);
    current_screen = Screen::DASHBOARD;

    // Process one tick so the screen load takes effect before first render
    lv_tick_inc(1);
    lv_timer_handler();
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
        update_nav_highlight(screen);
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

#else // NATIVE_BUILD && !SIMULATOR_BUILD
// Native test stubs
void ui_init() {}
void ui_switch_screen(Screen) {}
Screen ui_get_current_screen() { return Screen::DASHBOARD; }
void ui_tick(uint32_t) {}
void ui_handler() {}
void ui_set_callbacks(UiSetpointCb, UiMeatTargetCb, UiAlarmAckCb) {}
void ui_set_settings_callbacks(UiUnitsCb, UiFanModeCb, UiNewSessionCb, UiFactoryResetCb) {}
void ui_set_wifi_callback(UiWifiActionCb) {}
#endif
