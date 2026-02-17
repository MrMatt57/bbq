#pragma once

// Shared color constants for LVGL touchscreen UI.
// Included by ui_init.cpp, ui_update.cpp, and ui_setup_wizard.cpp.

#if !defined(NATIVE_BUILD) || defined(SIMULATOR_BUILD)
#include <lvgl.h>

#define COLOR_BG          lv_color_hex(0x1A1A1A)
#define COLOR_CARD_BG     lv_color_hex(0x2A2A2A)
#define COLOR_NAV_BG      lv_color_hex(0x111111)
#define COLOR_TEXT         lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_DIM     lv_color_hex(0x888888)
#define COLOR_TEXT_VDIM    lv_color_hex(0x555555)
#define COLOR_ORANGE       lv_color_hex(0xFF6600)
#define COLOR_RED          lv_color_hex(0xFF3333)
#define COLOR_BLUE         lv_color_hex(0x3399FF)
#define COLOR_GREEN        lv_color_hex(0x33CC33)
#define COLOR_PURPLE       lv_color_hex(0x8855FF)
#define COLOR_BAR_BG      lv_color_hex(0x333333)

#endif
