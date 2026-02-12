#pragma once

#include "../config.h"
#include <stdint.h>

#ifndef NATIVE_BUILD
#include <lvgl.h>
#endif

// Screen IDs
enum class Screen : uint8_t {
    DASHBOARD = 0,
    GRAPH,
    SETTINGS
};

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
