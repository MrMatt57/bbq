#include "ui_boot_splash.h"

#if !defined(NATIVE_BUILD) || defined(SIMULATOR_BUILD)

#include <lvgl.h>
#include "ui_colors.h"

// --------------------------------------------------------------------------
// State
// --------------------------------------------------------------------------

static bool     splash_active  = false;
static bool     factory_reset  = false;
static uint32_t splash_start_ms = 0;  // When splash was shown (resets on release)
static uint32_t press_start_ms  = 0;  // When current press began (0 = not pressed)

static lv_obj_t* scr_splash    = nullptr;
static lv_obj_t* bar_progress  = nullptr;
static lv_obj_t* lbl_hold_hint = nullptr;

// --------------------------------------------------------------------------
// LVGL event callbacks for press tracking
// --------------------------------------------------------------------------

static void splash_pressed_cb(lv_event_t* e) {
    (void)e;
    press_start_ms = lv_tick_get();
    if (bar_progress) lv_obj_remove_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
}

static void splash_released_cb(lv_event_t* e) {
    (void)e;
    press_start_ms = 0;
    // Reset auto-dismiss timer so it counts 2s from release
    splash_start_ms = lv_tick_get();
    if (bar_progress) {
        lv_bar_set_value(bar_progress, 0, LV_ANIM_OFF);
        lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
    }
}

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

void ui_boot_splash_init() {
    splash_active   = true;
    factory_reset   = false;
    splash_start_ms = lv_tick_get();
    press_start_ms  = 0;

    scr_splash = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_splash, COLOR_BG, 0);

    // Make the whole screen clickable for factory reset hold detection
    lv_obj_add_flag(scr_splash, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr_splash, splash_pressed_cb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(scr_splash, splash_released_cb, LV_EVENT_RELEASED, nullptr);

    // "Pit Claw" title
    lv_obj_t* lbl = lv_label_create(scr_splash);
    lv_label_set_text(lbl, "Pit Claw");
    lv_obj_set_style_text_color(lbl, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_36, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -30);

    // Version
    lbl = lv_label_create(scr_splash);
    lv_label_set_text_fmt(lbl, "v%s", FIRMWARE_VERSION);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 10);

    // Factory reset progress bar (hidden until screen is pressed)
    bar_progress = lv_bar_create(scr_splash);
    lv_obj_set_size(bar_progress, 300, 8);
    lv_obj_align(bar_progress, LV_ALIGN_CENTER, 0, 60);
    lv_bar_set_range(bar_progress, 0, 100);
    lv_bar_set_value(bar_progress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_progress, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_progress, COLOR_RED, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_progress, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_progress, 4, LV_PART_INDICATOR);
    lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);

    // Hint text at bottom
    lbl_hold_hint = lv_label_create(scr_splash);
    lv_label_set_text(lbl_hold_hint, "Hold screen 10s for factory reset");
    lv_obj_set_style_text_color(lbl_hold_hint, COLOR_TEXT_VDIM, 0);
    lv_obj_set_style_text_font(lbl_hold_hint, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_hold_hint, LV_ALIGN_BOTTOM_MID, 0, -16);

    lv_screen_load(scr_splash);
}

bool ui_boot_splash_is_active() {
    return splash_active;
}

void ui_boot_splash_update() {
    if (!splash_active) return;

    uint32_t now = lv_tick_get();

    if (press_start_ms > 0) {
        // Screen is being pressed — update progress bar
        uint32_t held_ms = now - press_start_ms;
        float progress = (float)held_ms / 10000.0f;
        if (progress > 1.0f) progress = 1.0f;

        if (bar_progress) {
            lv_bar_set_value(bar_progress, (int)(progress * 100), LV_ANIM_OFF);
        }

        // Change hint text as progress increases
        if (lbl_hold_hint && held_ms >= 3000) {
            lv_label_set_text(lbl_hold_hint, "Keep holding for factory reset...");
            lv_obj_set_style_text_color(lbl_hold_hint, COLOR_RED, 0);
        }

        if (held_ms >= 10000) {
            factory_reset = true;
            splash_active = false;
        }
    } else {
        // Not pressed — auto-dismiss after 2 seconds
        if ((now - splash_start_ms) >= 2000) {
            splash_active = false;
        }
    }
}

bool ui_boot_splash_factory_reset_triggered() {
    return factory_reset;
}

void ui_boot_splash_cleanup() {
    if (scr_splash) {
        lv_obj_delete(scr_splash);
        scr_splash = nullptr;
        bar_progress = nullptr;
        lbl_hold_hint = nullptr;
    }
}

#else
// Native test stubs
void ui_boot_splash_init() {}
bool ui_boot_splash_is_active() { return false; }
void ui_boot_splash_update() {}
bool ui_boot_splash_factory_reset_triggered() { return false; }
void ui_boot_splash_cleanup() {}
#endif
