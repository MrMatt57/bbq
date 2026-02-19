# Firmware Development

Guide for building, flashing, testing, and understanding the Pit Claw firmware.

## Prerequisites

### Quick Setup (Windows)

```powershell
# Run from an elevated (Admin) PowerShell in the repo root
powershell -ExecutionPolicy Bypass -File scripts\setup-dev.ps1
```

This installs Git, Python, Node.js, PlatformIO CLI, and OpenSCAD. The script is idempotent — safe to run again anytime.

### Manual Install

If you prefer to install tools yourself (or you're not on Windows):

1. [Git](https://git-scm.com/)
2. [Python 3.12+](https://www.python.org/)
3. [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation.html) — `pip install platformio`
4. [VS Code](https://code.visualstudio.com/) + [PlatformIO extension](https://platformio.org/install/ide?install=vscode) (recommended)

## Build Commands

All commands run from the `firmware/` directory.

```bash
# Build firmware
pio run -e wt32_sc01_plus

# Flash firmware via USB
pio run -e wt32_sc01_plus --target upload

# Upload web UI files to LittleFS
pio run -e wt32_sc01_plus --target uploadfs

# Build simulator (desktop, no hardware needed)
pio run -e simulator

# Clean build artifacts
pio run --target clean

# Serial monitor
pio device monitor --baud 115200
```

## Testing

```bash
# Desktop unit tests (PID, prediction, alarms — no hardware)
pio test -e native

# Single test suite
pio test -e native --filter test_desktop/test_pid

# On-device integration tests (requires connected hardware)
pio test -e wt32_sc01_plus
```

Tests use the Unity framework with two environments:
- **`native`** — runs on desktop, tests pure logic (PID, prediction, alarms, fan logic, temperature conversion)
- **`wt32_sc01_plus`** — runs on device, tests hardware integration (ADC, fan PWM, servo, buzzer, I2C)

## OTA Updates

After the initial USB flash, firmware can be updated over Wi-Fi:

1. Open `http://bbq.local/update` in a browser
2. Upload the `.bin` file from `.pio/build/wt32_sc01_plus/firmware.bin`
3. The device reboots with the new firmware

The LittleFS partition (config, session data, web UI files) is preserved across firmware updates.

## Architecture

### Source Layout

```
firmware/
  src/
    main.cpp                    # Setup + main loop (Arduino setup/loop pattern)
    config.h                    # Pin assignments, constants, defaults
    config_manager.h/.cpp       # Load/save config.json on LittleFS
    wifi_manager.h/.cpp         # WiFiManager captive portal, mDNS, auto-reconnect
    ota_manager.h/.cpp          # Web-based OTA firmware update endpoint
    pid_controller.h/.cpp       # PID wrapper (QuickPID + lid-open, startup, split-range)
    temp_manager.h/.cpp         # ADS1115 reading, Steinhart-Hart, EMA filtering
    temp_predictor.h/.cpp       # Rolling linear regression for done-time prediction
    fan_controller.h/.cpp       # PWM output with kick-start, long-pulse, min-speed
    servo_controller.h/.cpp     # Damper servo control
    alarm_manager.h/.cpp        # Threshold logic, hysteresis, buzzer + web triggers
    cook_session.h/.cpp         # Session state, circular buffer, LittleFS persistence
    error_manager.h/.cpp        # Probe disconnect/short, fan stall, fire-out detection
    web_protocol.h/.cpp         # Shared WebSocket protocol (message building/parsing)
    web_server.h/.cpp           # ESPAsyncWebServer, REST + WebSocket handlers
    split_range.h               # Fan + damper coordination from PID output
    units.h                     # Temperature unit conversion utilities
    display/
      ui_init.h/.cpp            # LVGL screen setup (dashboard, graph, settings)
      ui_update.h/.cpp          # Real-time widget updates
      ui_setup_wizard.h/.cpp    # First-boot setup wizard screens
      ui_colors.h               # Shared LVGL color constants
      graph_history.h/.cpp      # Adaptive-condensing graph history buffer
    simulator/                  # Desktop simulator (see web-development.md)
      sim_main.cpp              # SDL2 + mongoose main loop
      sim_thermal.h/.cpp        # Charcoal smoker physics simulation
      sim_profiles.h            # Pre-built cook profiles
      sim_web_server.h/.cpp     # Mongoose HTTP + WebSocket server
      mongoose.h/.c             # Mongoose embedded web server library
  data/                         # Web UI files (uploaded to LittleFS)
  test/
    test_desktop/               # Native tests
    test_embedded/              # On-device tests
  platformio.ini
```

### Key Modules

**PID Controller** (`pid_controller.h/.cpp`) — wraps QuickPID with BBQ-specific features: proportional-on-measurement, derivative-on-measurement, integral anti-windup conditioning. Includes lid-open detection (6% drop below setpoint) and startup mode.

**Temperature Manager** (`temp_manager.h/.cpp`) — reads ADS1115 ADC via I2C, converts raw ADC counts to temperature using Steinhart-Hart equation, applies EMA (exponential moving average) filtering, and supports per-probe calibration offsets.

**Fan + Damper Split-Range** (`split_range.h`) — the PID produces a single 0-100% output mapped to both actuators:
- Damper: linearly maps full PID range (0% = closed, 100% = open)
- Fan: activates above configurable threshold (default 30%), scales within its own min-max range
- Three modes: fan-only, fan+damper coordinated, damper-primary with fan boost

**Cook Session** (`cook_session.h/.cpp`) — stores the current cook as a circular buffer in RAM (600 samples, ~50 min at 5s intervals), flushed to LittleFS every 60 seconds for power-loss recovery. Only one session stored on device — web UI provides CSV/JSON download.

**Error Manager** (`error_manager.h/.cpp`) — detects probe disconnect (ADC max/open circuit), probe short (ADC zero), fire-out (pit declining >2°F/min for 10+ min at full fan), and Wi-Fi loss.

### Configuration

All user settings stored in `config.json` on LittleFS. Survives reboots and firmware OTA updates.

```json
{
  "wifi": { "ssid": "", "password": "" },
  "units": "F",
  "pid": { "p": 4.0, "i": 0.02, "d": 5.0 },
  "fan": { "mode": "fan_and_damper", "minSpeed": 15, "fanOnThreshold": 30 },
  "probes": {
    "pit":   { "name": "Pit",    "a": 7.3431401e-04, "b": 2.1574370e-04, "c": 9.5156860e-08, "offset": 0.0 },
    "meat1": { "name": "Meat 1", "a": 7.3431401e-04, "b": 2.1574370e-04, "c": 9.5156860e-08, "offset": 0.0 },
    "meat2": { "name": "Meat 2", "a": 7.3431401e-04, "b": 2.1574370e-04, "c": 9.5156860e-08, "offset": 0.0 }
  },
  "alarms": {
    "pitBand": 15,
    "pushover": { "enabled": false, "userKey": "", "apiToken": "" }
  },
  "setupComplete": false
}
```

### Technical Constants

**Thermoworks Pro-Series Steinhart-Hart Coefficients:**
- A = 7.3431401e-04, B = 2.1574370e-04, C = 9.5156860e-08
- Reference resistor: 10K ohm (1% metal film)
- 0.1uF ceramic filter cap on each ADC input

**PID Defaults (HeaterMeter-derived):**
- P = 4.0, I = 0.02, D = 5.0
- Temp sampling: 1s interval, 4-reading average
- PID compute: every 4 seconds
- Lid-open threshold: 6% drop below setpoint

**Fan Control:**
- PWM frequency: 25 kHz
- Kick-start: 75% for 500ms
- Long-pulse mode below 10% (10s cycle)
- Min sustained speed: 15%
