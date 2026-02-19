# 002: LVGL Touchscreen UI

**Branch**: `feature/simulator-ui`
**Created**: 2026-02-18
**Status**: Implemented

## Summary

Full touchscreen user interface for the WT32-SC01 Plus 3.5" capacitive display using LVGL v9. Provides a three-screen layout (dashboard, graph, settings) with real-time temperature display, setpoint/target adjustment, output bars, cook timer, alerts, and a first-boot setup wizard. The device is fully functional via touchscreen alone without Wi-Fi.

## Requirements

- Three navigable screens: Dashboard, Graph, Settings (swipe or button navigation)
- Dashboard screen: live pit/meat temperatures, setpoint adjustment (± buttons), meat target setting, fan/damper output bars, cook timer, WiFi status icon, alert banner
- Graph screen: historical temperature chart with adaptive-condensing buffer (240 slots), color-coded series (pit=orange, meat1=red, meat2=blue, setpoint=dashed white)
- Settings screen: units toggle (°F/°C), fan mode selection (fan-only, fan+damper, damper-primary), WiFi info display, new session button, firmware version, factory reset
- First-boot setup wizard: Welcome → Units → Wi-Fi (QR code for AP connection) → Probe check → Hardware test → Done
- Real-time widget updates at display refresh rate
- Dark theme with amber/orange accent colors
- Disconnected probes show "---", shorted probes show "ERR"
- Alert banner for active alarms, lid-open, fire-out, probe errors
- LVGL fonts 16-48pt enabled for readability
- Adaptive-condensing graph history: 240 slots, condenses 240→120 by pairwise averaging when full

## Design

### Screen Architecture

LVGL v9 with TFT_eSPI driver for parallel 8-bit interface. Three screen objects created at init, switched with animation. Touch input handled by LVGL's built-in driver.

### Dashboard Layout (480×320 landscape)

```
┌─────────────────────────────────────────────────┐
│ [Alert Banner - conditionally visible]          │
├─────────────────────────────────────────────────┤
│                                                 │
│   PIT          MEAT 1         MEAT 2            │
│   225°F        145°F          98°F              │
│   [−][+]       Target: 203    Target: ---       │
│                                                 │
│   Fan: ████████░░░░ 45%                         │
│   Damp: ██████████░░ 80%                        │
│                                                 │
│   Cook Time: 2:34:15    Est: 5:30 PM            │
│                                                 │
│   [WiFi ●]              [Graph] [Settings]      │
└─────────────────────────────────────────────────┘
```

### Graph History Buffer

The `GraphHistory` class stores up to 240 `GraphSlot` entries. When full, it condenses 240→120 by pairwise averaging (respecting validity flags), then continues appending from slot 120. This allows ~5-6 condensation cycles for a 12-hour cook while keeping memory constant.

```cpp
struct GraphSlot {
    float pit, meat1, meat2, setpoint;
    bool pitValid, meat1Valid, meat2Valid;
};
```

### Setup Wizard Flow

Sequential screens with back/next navigation. QR code on the Wi-Fi screen encodes `WIFI:T:WPA;S:BBQ-Setup;P:bbqsetup;;` for automatic phone connection to the AP. Probe check screen shows live ADC readings. Hardware test briefly spins fan, sweeps servo, beeps buzzer. Completing the wizard sets `setupComplete: true` in config.

### Color Constants (ui_colors.h)

Dark theme: Background #1A1A1A, Card #2A2A2A, Text #FFFFFF, Accent #FF6600, Red #FF3333, Blue #3399FF, Green #33CC33.

## Files to Modify

| File | Change |
|------|--------|
| `firmware/src/display/ui_init.h/.cpp` | LVGL screen creation (dashboard, graph, settings), touch input, screen switching |
| `firmware/src/display/ui_update.h/.cpp` | Real-time widget update functions for all screens |
| `firmware/src/display/ui_setup_wizard.h/.cpp` | First-boot wizard screens (welcome, units, WiFi QR, probe check, hardware test) |
| `firmware/src/display/ui_colors.h` | Shared LVGL color constants |
| `firmware/src/display/graph_history.h/.cpp` | Adaptive-condensing 240-slot graph buffer |
| `firmware/src/main.cpp` | LVGL init, tick, handler calls; UI callback wiring |
| `firmware/platformio.ini` | LVGL build flags (fonts, QR code, TFT_eSPI config) |

## Test Plan

- [x] Dashboard shows live temperatures, setpoint, output bars, cook timer
- [x] Graph screen renders temperature history with color-coded series
- [x] Settings screen toggles units and fan mode
- [x] Setup wizard completes full flow (Welcome → Done)
- [x] Disconnected probes display "---", shorted probes display "ERR"
- [x] Graph history condenses correctly when buffer is full
- [x] Firmware builds (`pio run -e wt32_sc01_plus`)
- [x] Simulator builds and renders UI (`pio run -e simulator`)
