# 016: Wire Setup Wizard into Boot Flow + Boot Splash

**Branch**: `feature/wire-setup-wizard`
**Created**: 2026-02-18

## Summary

The setup wizard UI is fully implemented (all 6 steps: Welcome, Units, Wi-Fi QR, Probe Check, Hardware Test, Done) but nothing in `main.cpp` or `sim_main.cpp` calls it. This feature wires the wizard into the boot flow so it runs on first boot (when `setupComplete == false`), adds a boot splash screen with a 10-second hold for factory reset, and adds a `--wizard` CLI flag to the simulator for testing the wizard flow in the SDL2 window.

## Requirements

- On startup, show a boot splash screen for 2 seconds with "Pit Claw" branding and version
- During boot splash, if user holds the touchscreen for 10 continuous seconds, trigger factory reset (wipe config, restart into wizard)
- Show a progress indicator during the hold so the user knows it's working
- After boot splash, check `configManager.isSetupComplete()`:
  - If `false` → launch the setup wizard
  - If `true` → load the dashboard (current behavior)
- Wire all wizard callbacks in `main.cpp`: units selection saves to configManager, hardware test buttons trigger fan/servo/buzzer, completion sets `setupComplete = true` and saves config
- After wizard's Done screen, automatically transition to the dashboard after a brief pause (2 seconds)
- During wizard, update probe readings on the probe-check screen from `tempManager` in the main loop
- Simulator: add `--wizard` flag to force-start the wizard (simulates first-boot)
- Simulator: wire wizard callbacks with simulator-local stubs (print messages for hardware test, update `g_is_fahrenheit` for units)
- Simulator: boot splash shown briefly (1 second) when `--wizard` is used, skipped otherwise

## Design

### Boot Splash Screen (`ui_boot_splash.h/.cpp`)

New LVGL module with a simple splash screen:
- Dark background (`COLOR_BG`)
- "Pit Claw" in large orange text centered
- Version string below in dim text
- Small hint at bottom: "Hold screen 10s for factory reset" in very dim text
- A thin progress bar (hidden by default) that fills over 10 seconds when the screen is pressed

Public API:
```cpp
void ui_boot_splash_init();                    // Create and show splash screen
bool ui_boot_splash_is_active();               // Still in splash phase?
void ui_boot_splash_update(bool pressed);      // Call each loop tick with touch state
bool ui_boot_splash_factory_reset_triggered(); // Was 10s hold completed?
float ui_boot_splash_hold_progress();          // 0.0-1.0 for progress bar
void ui_boot_splash_end();                     // Clean up splash, prepare for next screen
```

The splash screen handles its own timing internally:
- Tracks cumulative press duration (resets if released)
- After 2 seconds with no press, marks itself as done (`is_active` → false)
- If press reaches 10 seconds, sets factory_reset flag and marks done
- The 2-second auto-dismiss timer pauses while the screen is being pressed

### Main.cpp Boot Flow Changes

Add a boot phase state machine:
```cpp
enum class BootPhase { SPLASH, WIZARD, RUNNING };
static BootPhase g_bootPhase = BootPhase::SPLASH;
```

In `setup()`:
1. Init all hardware modules as today (steps 1-12 unchanged)
2. Call `ui_init()` (initializes LVGL, creates no screens yet — we'll show splash first)
3. Call `ui_boot_splash_init()` to show the splash screen
4. Do NOT set up dashboard callbacks yet

In `loop()`:
- **SPLASH phase**: Only run `ui_tick()` + `ui_handler()` + `ui_boot_splash_update()`. Check `ui_boot_splash_is_active()`. When done:
  - If factory reset triggered → call `configManager.factoryReset()` (which restarts)
  - If `!configManager.isSetupComplete()` → init wizard, wire wizard callbacks, switch to WIZARD phase
  - Else → init dashboard, wire dashboard callbacks, switch to RUNNING phase
- **WIZARD phase**: Run `ui_tick()` + `ui_handler()`. Update wizard probe readings at 1Hz. When `!ui_wizard_is_active()`:
  - Init dashboard, wire dashboard callbacks, switch to RUNNING phase
- **RUNNING phase**: Existing loop behavior (all module updates, display updates, etc.)

### Wizard Callbacks in main.cpp

```cpp
static void wiz_fan_test() {
    fanController.setSpeed(75);
    delay(500);
    fanController.setSpeed(0);
}
static void wiz_servo_test() {
    servoController.setPosition(100);
    delay(500);
    servoController.setPosition(0);
}
static void wiz_buzzer_test() {
    alarmManager.beepOnce();  // Need to verify this method exists, or use direct buzzer
}
static void wiz_units(bool isFahrenheit) {
    configManager.setUnits(isFahrenheit ? "F" : "C");
    tempManager.setUseFahrenheit(isFahrenheit);
}
static void wiz_complete() {
    configManager.setSetupComplete(true);
    configManager.save();
}
```

### Wizard → Dashboard Transition

After the wizard's Done screen (step 5), `cb_complete` fires immediately. But we want the "Setup Complete!" screen to show for ~2 seconds before transitioning to the dashboard. Two approaches:

**Chosen approach**: In the WIZARD phase of the main loop, after `ui_wizard_is_active()` returns false, start a 2-second timer. After the timer expires, initialize the dashboard and switch to RUNNING. This keeps the transition logic in main.cpp rather than in the wizard module.

### Simulator Changes (`sim_main.cpp`)

Add `--wizard` CLI flag:
```cpp
bool wizardMode = false;
// In arg parsing:
} else if (strcmp(argv[i], "--wizard") == 0) {
    wizardMode = true;
}
```

When `wizardMode` is true:
1. Show boot splash briefly (1 second delay with LVGL ticking)
2. Call `ui_wizard_init()` and `ui_wizard_set_callbacks()` with simulator stubs
3. In main loop, check `ui_wizard_is_active()` — when done, init dashboard and continue

Simulator wizard callbacks:
- Fan test: `printf("[SIM] Fan test\n")`
- Servo test: `printf("[SIM] Servo test\n")`
- Buzzer test: `printf("[SIM] Buzzer test\n")`
- Units: Update `g_is_fahrenheit` and call `ui_set_units()`
- Complete: Print message, no config save needed

When `wizardMode` is false: skip splash, go straight to dashboard as today.

Update `print_usage()` to list `--wizard` option.

### Existing Wizard Module — No Changes Needed

The wizard code in `ui_setup_wizard.cpp` is complete and doesn't need modification. The `cb_complete` callback + `wizard_active = false` behavior gives main.cpp everything it needs to detect completion and transition.

### Build Filter

The simulator build already includes `+<display/>` which will pick up the new `ui_boot_splash.cpp`. No `platformio.ini` changes needed.

## Files to Modify

| File | Change |
|------|--------|
| `firmware/src/display/ui_boot_splash.h` | **NEW** — Boot splash screen header |
| `firmware/src/display/ui_boot_splash.cpp` | **NEW** — Boot splash screen with hold-to-reset logic |
| `firmware/src/main.cpp` | Add boot phase state machine, wire wizard callbacks, splash → wizard → dashboard flow |
| `firmware/src/simulator/sim_main.cpp` | Add `--wizard` flag, boot splash, wizard callback stubs, wizard → dashboard transition |

## Test Plan

- [ ] Simulator builds (`pio run -e simulator`)
- [ ] Simulator runs normally (no --wizard): skips splash, shows dashboard immediately
- [ ] Simulator with `--wizard`: shows boot splash briefly, then setup wizard with all 6 steps
- [ ] Wizard units step: tapping °F or °C advances and prints unit choice
- [ ] Wizard hardware test: tapping Fan/Servo/Buzzer buttons prints test messages
- [ ] Wizard Done step: shows "Setup Complete!" then auto-transitions to dashboard after ~2s
- [ ] After wizard completes, dashboard is fully functional (thermal sim runs, web UI serves)
- [ ] Firmware builds (`pio run -e wt32_sc01_plus`)
- [ ] Desktop tests pass (`pio test -e native`)
