#pragma once

#include "../config.h"

// Initialize and display the boot splash screen.
// Call after ui_init() — splash loads on top of the dashboard.
void ui_boot_splash_init();

// Returns true while the splash is still showing.
bool ui_boot_splash_is_active();

// Call from the main loop to update hold detection and auto-dismiss timing.
void ui_boot_splash_update();

// Returns true if the user held the screen for 10 seconds (factory reset).
bool ui_boot_splash_factory_reset_triggered();

// Delete splash screen objects to free memory. Call after transitioning away.
void ui_boot_splash_cleanup();
