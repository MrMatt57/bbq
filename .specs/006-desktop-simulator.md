# 006: Desktop Simulator

**Branch**: `feature/simulator-ui`
**Created**: 2026-02-18
**Status**: Implemented

## Summary

Desktop build that runs the full LVGL touchscreen UI in an SDL2 window and serves the web UI via an embedded mongoose HTTP server. Uses a shared C++ charcoal smoker thermal model to simulate realistic temperature curves, PID-like fan/damper response, and timed events (lid-open, fire-out, probe disconnect). Enables development and testing of both UIs without hardware.

## Requirements

- PlatformIO native build (`pio run -e simulator`) producing a desktop executable
- SDL2 window rendering the LVGL touchscreen UI (same code as firmware)
- Embedded mongoose HTTP + WebSocket server serving web UI from `firmware/data/`
- Shared thermal model driving both touchscreen and web UI
- Time acceleration via `--speed N` CLI argument (e.g., `--speed 50` = 12-hour cook in ~15 minutes)
- Custom web server port via `--port N` (default 3000)
- Seven pre-built cook profiles selectable via `--profile NAME`:
  - `normal`: 225°F pit, pork butt to 203°F (~10 hours)
  - `stall`: 225°F pit, brisket with 150-170°F stall plateau (~14 hours)
  - `hot-fast`: 300°F pit, chicken thighs to 185°F (~2 hours)
  - `temp-change`: Start at 225°F, bump to 275°F mid-cook (~8 hours)
  - `lid-open`: Normal cook with periodic lid-open temp drops (~10 hours)
  - `fire-out`: Normal cook, fire dies at 4 hours (~6 hours)
  - `probe-disconnect`: Normal cook, meat1 probe disconnects at 3 hours (~10 hours)
- Touch input simulation (mouse clicks in SDL2 window)
- UI callbacks synchronized: setpoint/target/alarm changes from either UI affect shared state
- Web UI files editable without recompiling (hot reload via browser refresh)

## Design

### Architecture

```
sim_main.cpp (entry point)
  ├── SDL2 window + LVGL rendering
  ├── SimThermalModel (physics simulation)
  ├── SimWebServer (mongoose HTTP + WebSocket)
  └── Shared state (temps, setpoint, targets, alarms)
```

### Thermal Model (sim_thermal.h/.cpp)

Simulates a charcoal smoker with:
- **Fire energy**: Abstract 0-100 scale, decays over time, increases with fan (oxygen)
- **Pit temperature**: Rises toward fire energy limit with cooling loss toward ambient
- **Meat temperature**: Rises toward pit with thermal lag (slower than pit)
- **Lid-open**: Temperature drop of ~5-10% when detected, recovery when closed
- **Stall plateau**: Configurable brisket-like stall at 150-170°F for given duration
- **Fire-out**: Fire energy decays to zero, pit cools toward ambient
- **PID-like response**: Simplified error-based output (not exact QuickPID, but realistic curves)

### Cook Profiles (sim_profiles.h)

Each `SimProfile` struct defines:
- Setpoint, meat targets, ambient temperature
- Stall temperature range and duration
- Fire-out scenario flag
- Timed events array (lid-open at time T, probe disconnect at time T)

### SDL2 + LVGL Integration

- SDL2 provides the window and mouse input
- LVGL renders to an SDL2 texture
- Same `ui_init.cpp` and `ui_update.cpp` code as firmware
- Simulator-specific: `#ifdef SIMULATOR` guards for SDL2 init and mongoose server

### Build Configuration

PlatformIO `[env:simulator]` with:
- `sdl2_setup.py` extra script to install SDL2 library
- Selective source filter: `simulator/`, `display/`, `web_protocol.cpp`
- Mongoose compiled from source (`simulator/mongoose.c`, ~28K lines)

## Files to Modify

| File | Change |
|------|--------|
| `firmware/src/simulator/sim_main.cpp` | SDL2 event loop, LVGL init, thermal model integration, web server startup |
| `firmware/src/simulator/sim_thermal.h/.cpp` | Charcoal smoker physics simulation |
| `firmware/src/simulator/sim_profiles.h` | Seven pre-built cook profile definitions |
| `firmware/src/simulator/sim_web_server.h/.cpp` | Mongoose HTTP + WebSocket server |
| `firmware/src/simulator/mongoose.h/.c` | Mongoose embedded web server library |
| `firmware/sdl2_setup.py` | PlatformIO extra script for SDL2 build setup |
| `firmware/platformio.ini` | `[env:simulator]` build configuration |

## Test Plan

- [x] Simulator builds (`pio run -e simulator`)
- [x] SDL2 window opens and renders LVGL touchscreen UI
- [x] Web UI accessible at http://localhost:3000
- [x] WebSocket streams simulated data to web UI
- [x] Time acceleration works (`--speed 50`)
- [x] All 7 cook profiles produce realistic temperature curves
- [x] Setpoint changes from touchscreen reflect in web UI and vice versa
- [x] Touch input works in SDL2 window (mouse click)
