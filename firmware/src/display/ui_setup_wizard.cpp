#include "ui_setup_wizard.h"

#ifndef NATIVE_BUILD

#include <lvgl.h>
#include <stdio.h>

// --------------------------------------------------------------------------
// Color constants
// --------------------------------------------------------------------------

#define COLOR_BG         lv_color_hex(0x1A1A1A)
#define COLOR_CARD_BG    lv_color_hex(0x2A2A2A)
#define COLOR_TEXT        lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_DIM    lv_color_hex(0x888888)
#define COLOR_ORANGE      lv_color_hex(0xFF6600)
#define COLOR_GREEN       lv_color_hex(0x33CC33)

// --------------------------------------------------------------------------
// State
// --------------------------------------------------------------------------

static bool wizard_active = false;
static uint8_t wizard_step = 0;  // 0=welcome, 1=units, 2=wifi, 3=probes, 4=hwtest, 5=done

// Wizard screens (one per step)
static lv_obj_t* wiz_screens[6] = { nullptr };

// Probe check labels (step 3)
static lv_obj_t* lbl_wiz_pit   = nullptr;
static lv_obj_t* lbl_wiz_meat1 = nullptr;
static lv_obj_t* lbl_wiz_meat2 = nullptr;

// Callbacks
static WizardFanTestCb    cb_fan_test    = nullptr;
static WizardServoTestCb  cb_servo_test  = nullptr;
static WizardBuzzerTestCb cb_buzzer_test = nullptr;
static WizardUnitsCb      cb_units       = nullptr;
static WizardCompleteCb   cb_complete    = nullptr;

// --------------------------------------------------------------------------
// Forward declarations
// --------------------------------------------------------------------------

static void create_step_welcome();
static void create_step_units();
static void create_step_wifi();
static void create_step_probes();
static void create_step_hwtest();
static void create_step_done();
static void go_to_step(uint8_t step);

// --------------------------------------------------------------------------
// Navigation helpers
// --------------------------------------------------------------------------

static void next_btn_cb(lv_event_t* e) {
    (void)e;
    if (wizard_step < 5) {
        go_to_step(wizard_step + 1);
    }
}

static void go_to_step(uint8_t step) {
    if (step > 5) step = 5;
    wizard_step = step;

    if (step == 5) {
        // Done step — mark wizard complete
        wizard_active = false;
        if (cb_complete) cb_complete();
    }

    if (wiz_screens[step]) {
        lv_screen_load_anim(wiz_screens[step], LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
    }
}

// --------------------------------------------------------------------------
// Helper: create a "Next" button at the bottom of a screen
// --------------------------------------------------------------------------

static lv_obj_t* add_next_button(lv_obj_t* parent, const char* text) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 140, 44);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_bg_color(btn, COLOR_ORANGE, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, next_btn_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl);

    return btn;
}

// --------------------------------------------------------------------------
// Step 0: Welcome
// --------------------------------------------------------------------------

static void create_step_welcome() {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    wiz_screens[0] = scr;

    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "BBQ Controller");
    lv_obj_set_style_text_color(lbl, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_36, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -40);

    lbl = lv_label_create(scr);
    lv_label_set_text_fmt(lbl, "v%s", FIRMWARE_VERSION);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 10);

    lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Let's get set up!");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 50);

    add_next_button(scr, "Start");
}

// --------------------------------------------------------------------------
// Step 1: Units
// --------------------------------------------------------------------------

static void units_f_cb(lv_event_t* e) {
    (void)e;
    if (cb_units) cb_units(true);
    go_to_step(2);
}

static void units_c_cb(lv_event_t* e) {
    (void)e;
    if (cb_units) cb_units(false);
    go_to_step(2);
}

static void create_step_units() {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    wiz_screens[1] = scr;

    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Temperature Units");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 30);

    // Fahrenheit button
    lv_obj_t* btn_f = lv_btn_create(scr);
    lv_obj_set_size(btn_f, 180, 80);
    lv_obj_align(btn_f, LV_ALIGN_CENTER, -100, 20);
    lv_obj_set_style_bg_color(btn_f, COLOR_ORANGE, 0);
    lv_obj_set_style_radius(btn_f, 12, 0);
    lv_obj_add_event_cb(btn_f, units_f_cb, LV_EVENT_CLICKED, nullptr);

    lbl = lv_label_create(btn_f);
    lv_label_set_text(lbl, LV_SYMBOL_OK " \xC2\xB0" "F");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_36, 0);
    lv_obj_center(lbl);

    // Celsius button
    lv_obj_t* btn_c = lv_btn_create(scr);
    lv_obj_set_size(btn_c, 180, 80);
    lv_obj_align(btn_c, LV_ALIGN_CENTER, 100, 20);
    lv_obj_set_style_bg_color(btn_c, COLOR_CARD_BG, 0);
    lv_obj_set_style_radius(btn_c, 12, 0);
    lv_obj_add_event_cb(btn_c, units_c_cb, LV_EVENT_CLICKED, nullptr);

    lbl = lv_label_create(btn_c);
    lv_label_set_text(lbl, "\xC2\xB0" "C");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_36, 0);
    lv_obj_center(lbl);
}

// --------------------------------------------------------------------------
// Step 2: Wi-Fi (QR code + instructions)
// --------------------------------------------------------------------------

static void create_step_wifi() {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    wiz_screens[2] = scr;

    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Wi-Fi Setup");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 10);

    // QR code for Wi-Fi auto-connect
    // LVGL v9 qrcode: lv_qrcode_create returns an image canvas
    lv_obj_t* qr = lv_qrcode_create(scr);
    lv_qrcode_set_size(qr, 140);
    lv_qrcode_set_dark_color(qr, COLOR_TEXT);
    lv_qrcode_set_light_color(qr, COLOR_BG);
    lv_qrcode_update(qr, "WIFI:T:WPA;S:" AP_SSID ";P:" AP_PASSWORD ";;",
                     strlen("WIFI:T:WPA;S:" AP_SSID ";P:" AP_PASSWORD ";;"));
    lv_obj_align(qr, LV_ALIGN_LEFT_MID, 20, 10);

    // Instructions
    lv_obj_t* instr = lv_label_create(scr);
    lv_label_set_text(instr,
        "1. Scan QR code with\n"
        "   your phone\n\n"
        "2. Connect to\n"
        "   \"" AP_SSID "\" network\n\n"
        "3. Enter your Wi-Fi\n"
        "   credentials in the\n"
        "   portal that opens");
    lv_obj_set_style_text_color(instr, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(instr, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_line_space(instr, 2, 0);
    lv_obj_align(instr, LV_ALIGN_RIGHT_MID, -20, 10);

    add_next_button(scr, "Next");
}

// --------------------------------------------------------------------------
// Step 3: Probe Check
// --------------------------------------------------------------------------

static void create_step_probes() {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    wiz_screens[3] = scr;

    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Probe Check");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 10);

    lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Plug in probes to see live readings:");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 44);

    // Probe readings
    int y_start = 80;
    int y_spacing = 50;

    // Pit
    lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Pit:");
    lv_obj_set_style_text_color(lbl, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(lbl, 60, y_start);

    lbl_wiz_pit = lv_label_create(scr);
    lv_label_set_text(lbl_wiz_pit, "---");
    lv_obj_set_style_text_color(lbl_wiz_pit, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(lbl_wiz_pit, &lv_font_montserrat_36, 0);
    lv_obj_set_pos(lbl_wiz_pit, 250, y_start - 6);

    // Meat 1
    lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Meat 1:");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(lbl, 60, y_start + y_spacing);

    lbl_wiz_meat1 = lv_label_create(scr);
    lv_label_set_text(lbl_wiz_meat1, "---");
    lv_obj_set_style_text_color(lbl_wiz_meat1, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl_wiz_meat1, &lv_font_montserrat_36, 0);
    lv_obj_set_pos(lbl_wiz_meat1, 250, y_start + y_spacing - 6);

    // Meat 2
    lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Meat 2:");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(lbl, 60, y_start + 2 * y_spacing);

    lbl_wiz_meat2 = lv_label_create(scr);
    lv_label_set_text(lbl_wiz_meat2, "---");
    lv_obj_set_style_text_color(lbl_wiz_meat2, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl_wiz_meat2, &lv_font_montserrat_36, 0);
    lv_obj_set_pos(lbl_wiz_meat2, 250, y_start + 2 * y_spacing - 6);

    add_next_button(scr, "Next");
}

// --------------------------------------------------------------------------
// Step 4: Hardware Test
// --------------------------------------------------------------------------

static void fan_test_cb(lv_event_t* e) {
    (void)e;
    if (cb_fan_test) cb_fan_test();
}

static void servo_test_cb(lv_event_t* e) {
    (void)e;
    if (cb_servo_test) cb_servo_test();
}

static void buzzer_test_cb(lv_event_t* e) {
    (void)e;
    if (cb_buzzer_test) cb_buzzer_test();
}

static void create_step_hwtest() {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    wiz_screens[4] = scr;

    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Hardware Test");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 10);

    lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Tap each button to test:");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 44);

    // Test buttons in a row
    int btn_y = 100;
    int btn_w = 120;
    int btn_h = 60;

    // Fan test
    lv_obj_t* btn = lv_btn_create(scr);
    lv_obj_set_size(btn, btn_w, btn_h);
    lv_obj_set_pos(btn, 30, btn_y);
    lv_obj_set_style_bg_color(btn, COLOR_CARD_BG, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, fan_test_cb, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_REFRESH "\nFan");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);

    // Servo test
    btn = lv_btn_create(scr);
    lv_obj_set_size(btn, btn_w, btn_h);
    lv_obj_set_pos(btn, 175, btn_y);
    lv_obj_set_style_bg_color(btn, COLOR_CARD_BG, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, servo_test_cb, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_SETTINGS "\nServo");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);

    // Buzzer test
    btn = lv_btn_create(scr);
    lv_obj_set_size(btn, btn_w, btn_h);
    lv_obj_set_pos(btn, 320, btn_y);
    lv_obj_set_style_bg_color(btn, COLOR_CARD_BG, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, buzzer_test_cb, LV_EVENT_CLICKED, nullptr);
    lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_BELL "\nBuzzer");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);

    add_next_button(scr, "Finish");
}

// --------------------------------------------------------------------------
// Step 5: Done
// --------------------------------------------------------------------------

static void create_step_done() {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    wiz_screens[5] = scr;

    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(lbl, COLOR_GREEN, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_48, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -40);

    lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Setup Complete!");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 20);

    lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Your BBQ controller is ready.");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 55);
}

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

void ui_wizard_init() {
    wizard_active = true;
    wizard_step = 0;

    create_step_welcome();
    create_step_units();
    create_step_wifi();
    create_step_probes();
    create_step_hwtest();
    create_step_done();

    // Show the welcome screen
    lv_screen_load(wiz_screens[0]);
}

bool ui_wizard_is_active() {
    return wizard_active;
}

void ui_wizard_set_callbacks(WizardFanTestCb fanCb,
                              WizardServoTestCb servoCb,
                              WizardBuzzerTestCb buzzerCb,
                              WizardUnitsCb unitsCb,
                              WizardCompleteCb completeCb) {
    cb_fan_test    = fanCb;
    cb_servo_test  = servoCb;
    cb_buzzer_test = buzzerCb;
    cb_units       = unitsCb;
    cb_complete    = completeCb;
}

void ui_wizard_update_probes(float pit, float meat1, float meat2,
                              bool pitConn, bool meat1Conn, bool meat2Conn) {
    if (!wizard_active || wizard_step != 3) return;

    char buf[16];

    if (lbl_wiz_pit) {
        if (pitConn) {
            snprintf(buf, sizeof(buf), "%.0f", pit);
            lv_label_set_text(lbl_wiz_pit, buf);
            lv_obj_set_style_text_color(lbl_wiz_pit, COLOR_GREEN, 0);
        } else {
            lv_label_set_text(lbl_wiz_pit, "---");
            lv_obj_set_style_text_color(lbl_wiz_pit, COLOR_TEXT_DIM, 0);
        }
    }

    if (lbl_wiz_meat1) {
        if (meat1Conn) {
            snprintf(buf, sizeof(buf), "%.0f", meat1);
            lv_label_set_text(lbl_wiz_meat1, buf);
            lv_obj_set_style_text_color(lbl_wiz_meat1, COLOR_GREEN, 0);
        } else {
            lv_label_set_text(lbl_wiz_meat1, "---");
            lv_obj_set_style_text_color(lbl_wiz_meat1, COLOR_TEXT_DIM, 0);
        }
    }

    if (lbl_wiz_meat2) {
        if (meat2Conn) {
            snprintf(buf, sizeof(buf), "%.0f", meat2);
            lv_label_set_text(lbl_wiz_meat2, buf);
            lv_obj_set_style_text_color(lbl_wiz_meat2, COLOR_GREEN, 0);
        } else {
            lv_label_set_text(lbl_wiz_meat2, "---");
            lv_obj_set_style_text_color(lbl_wiz_meat2, COLOR_TEXT_DIM, 0);
        }
    }
}

#else
// Native build stubs
void ui_wizard_init() {}
bool ui_wizard_is_active() { return false; }
void ui_wizard_set_callbacks(WizardFanTestCb, WizardServoTestCb,
                              WizardBuzzerTestCb, WizardUnitsCb, WizardCompleteCb) {}
void ui_wizard_update_probes(float, float, float, bool, bool, bool) {}
#endif
