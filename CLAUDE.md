# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

BBQ Temperature Monitor & Controller — an ESP32-S3 based device with 3.5" touchscreen for controlling smoker/grill temperature via PID-controlled fan and optional servo damper. Reads up to 3 Thermoworks-compatible NTC thermistor probes (1 pit + 2 meat). Provides a web UI (PWA) over Wi-Fi for remote monitoring, graphing, and alarms. Touchscreen UI is fully functional without Wi-Fi.

## Development Workflow

This project uses a `/feature` → implement → `/ship` lifecycle for parallel development:

1. **`/feature {description}`** — Run from the main repo on `main`. Claude explores the codebase, asks clarifying questions, then creates a numbered spec in `.specs/`, a git worktree, and a feature branch. The spec captures all context so a fresh session can implement independently.

2. **Implement** — Open a new terminal in the worktree, start Claude, and tell it to implement the spec. The spec file in `.specs/` has everything needed: requirements, design, files to modify, and test plan.

3. **`/ship`** — Run from the feature worktree when implementation is complete. Claude commits, pushes, creates a PR (with summary and test plan from the spec), and enables auto-merge. CI passes → auto-merges → branch auto-deleted.

### Spec Files (`.specs/`)

- Naming: `NNN-branch-name.md` (e.g., `001-ota-updates.md`)
- Numbered sequentially, starting at `001`
- Each spec is the single source of truth for its feature — detailed enough for a fresh Claude session to implement without prior context

### Worktree Detection

When the working directory is a git worktree (not the main repo at `C:\dev\bbq\bbq`), check `.specs/` for a feature spec matching the current branch. If found, offer to implement it.

### PR Format

- **Title**: Short, under 70 characters
- **Body**: `## Summary` (bullet points) + `## Test plan` (checklist)

## Build & Test Commands

```bash
# === Simulator (touchscreen + web UI, no hardware) ===
pio run -e simulator                                # Build simulator
.pio/build/simulator/program                        # Run at http://localhost:3000
.pio/build/simulator/program --speed 50             # 50x time acceleration
.pio/build/simulator/program --profile stall        # brisket stall scenario
.pio/build/simulator/program --port 8080            # custom web port

# === Firmware ===
pio run -e wt32_sc01_plus                          # Build firmware
pio run -e wt32_sc01_plus --target upload           # Flash firmware via USB
pio run -e wt32_sc01_plus --target uploadfs         # Upload web UI to LittleFS

# === Tests ===
pio test -e native                                  # Desktop unit tests (no hardware)
pio test -e native --filter test_desktop/test_pid    # Single test suite
pio test -e wt32_sc01_plus                          # On-device integration tests

# === Utilities ===
pio device monitor --baud 115200                    # Serial monitor
pio run --target clean                              # Clean build
```

## Architecture

### Hardware Target

- **MCU Board**: WT32-SC01 Plus (ESP32-S3, 3.5" 480x320 capacitive touchscreen, 16MB flash, 2MB PSRAM, 92x60x10.8mm)
- **Carrier Board**: 50x70mm perfboard consolidating all electronics behind the display
- **ADC**: ADS1115 16-bit I2C (4 channels: 3 probes + 1 spare) — socketed on carrier board
- **Probes**: Thermoworks Pro-Series NTC thermistors via panel-mount 2.5mm mono jacks
- **Fan**: 5015 12V blower via IRLZ44N logic-level MOSFET on carrier board (PWM on GPIO12)
- **Damper**: MG90S servo (PWM on GPIO13)
- **Buzzer**: Piezo buzzer soldered to carrier board (GPIO14)
- **Power**: 12V barrel jack (panel-mount) → MP1584EN mini buck converter (22x17mm) → 5V rail
- **I2C Bus**: GPIO10 (SDA), GPIO11 (SCL)
- **Enclosure target**: ~100x70x40mm (deck-of-cards form factor)

### Carrier Board Layout (50x70mm perfboard)

All internal electronics consolidated onto a single board mounted behind the WT32-SC01 Plus:

```
┌──────────────────────────────────────────────────┐
│  MP1584EN        ADS1115 (socketed)              │
│  buck conv       ┌─────────┐                     │
│  ┌──────┐        │ A0 A1 A2│ A3(spare)           │
│  │12V→5V│        └─────────┘                     │
│  └──────┘                                        │
│                  R1  R2  R3   (10K 1% resistors)  │
│  IRLZ44N        C1  C2  C3   (0.1uF caps)       │
│  ┌──┐  10K                                       │
│  │FET│  pulldown  [BUZZER]                       │
│  └──┘                                            │
│                                                  │
│  Connectors: [12V in] [Fan] [Servo] [WT32 ext]  │
│              [Probe1] [Probe2] [Probe3]          │
└──────────────────────────────────────────────────┘
```

Components on the carrier board (~20 solder joints total):
- MP1584EN mini buck converter (12V→5V, 22x17mm module)
- ADS1115 breakout (pin-header socketed for replacement)
- IRLZ44N MOSFET (TO-220, logic-level, fully on at 3.3V for <1A loads) + 10K gate pulldown resistor
- 3x 10K 1% metal film resistors (voltage dividers)
- 3x 0.1uF ceramic capacitors (ADC input filters)
- Piezo buzzer (soldered directly)
- Power distribution traces (12V, 5V, 3.3V, GND rails)

Connections off the carrier board (short wire runs):
- 8-pin ribbon to WT32-SC01 Plus extension connector (2.0mm pitch, cable included with board)
- Wire pairs to panel-mount 2.5mm probe jacks (soldered to jack lugs)
- Wire pair to panel-mount DC barrel jack
- 2-pin to fan (JST-XH or direct solder)
- 3-pin to servo (standard servo connector)

### Pin Assignments (WT32-SC01 Plus Extension Connector)

| GPIO | Function |
|------|----------|
| 10   | I2C SDA (ADS1115) |
| 11   | I2C SCL (ADS1115) |
| 12   | Fan PWM (via MOSFET) |
| 13   | Servo PWM |
| 14   | Buzzer |
| 21   | Spare |

### Software Stack

- **Framework**: PlatformIO + Arduino
- **Display UI**: LVGL v9 (parallel 8-bit interface, TFT_eSPI driver)
- **Web Server**: ESPAsyncWebServer + AsyncWebSocket (JSON protocol)
- **Web UI**: Vanilla JS PWA served gzipped from LittleFS, uPlot for charting (all times in browser local timezone)
- **Wi-Fi Provisioning**: WiFiManager (captive portal with auto-redirect)
- **Network Discovery**: mDNS — device accessible at `http://bbq.local`
- **OTA Updates**: Web-based firmware upload via `/update` endpoint (ArduinoOTA + ElegantOTA)
- **PID**: QuickPID library (pOnMeas, dOnMeas, iAwCondition anti-windup)
- **File System**: LittleFS (cook session, web assets, config.json)
- **Testing**: Unity framework, dual envs (native for logic, embedded for hardware)
- **Simulator**: SDL2 + mongoose desktop build for touchscreen + web UI development without hardware

### Source Layout

```
firmware/
  src/
    main.cpp                    # Setup + main loop (Arduino setup/loop pattern)
    config.h                    # Pin assignments, constants, defaults
    config_manager.h/.cpp       # Load/save config.json on LittleFS, defaults, factory reset
    wifi_manager.h/.cpp         # WiFiManager captive portal, mDNS, auto-reconnect
    ota_manager.h/.cpp          # Web-based OTA firmware update endpoint
    pid_controller.h/.cpp       # BBQ-specific PID wrapper (QuickPID + lid-open, startup, split-range)
    temp_manager.h/.cpp         # ADS1115 reading, Steinhart-Hart, EMA filtering, calibration
    temp_predictor.h/.cpp       # Rolling linear regression for meat done-time prediction
    fan_controller.h/.cpp       # PWM output with kick-start, long-pulse, min-speed clamping
    servo_controller.h/.cpp     # Damper servo control
    alarm_manager.h/.cpp        # Threshold logic, hysteresis, buzzer + web alarm triggers
    cook_session.h/.cpp         # Session state, circular buffer, LittleFS persistence
    error_manager.h/.cpp        # Probe disconnect/short, fan stall, fire-out detection
    web_protocol.h/.cpp         # Shared WebSocket protocol (message building/parsing)
    web_server.h/.cpp           # ESPAsyncWebServer setup, REST + WebSocket handlers
    display/
      ui_init.h/.cpp            # LVGL screen setup (main dashboard, graph, settings)
      ui_update.h/.cpp          # Real-time widget updates
      ui_setup_wizard.h/.cpp    # First-boot setup wizard screens
      ui_colors.h               # Shared LVGL color constants
      graph_history.h/.cpp      # Adaptive-condensing graph history buffer
    simulator/                  # Desktop simulator (PIO native build)
      sim_main.cpp              # SDL2 + mongoose main loop
      sim_thermal.h/.cpp        # Charcoal smoker physics simulation
      sim_profiles.h            # Pre-built cook profiles
      sim_web_server.h/.cpp     # Mongoose HTTP + WebSocket server
      mongoose.h/.c             # Mongoose embedded web server library
  data/                         # Web UI files (uploaded to LittleFS)
    index.html
    app.js
    style.css
    manifest.json               # PWA manifest (icon, name, theme color)
    favicon.svg
    sw.js                       # Service worker for PWA offline shell
  sdl2_setup.py                 # PlatformIO extra script for SDL2 simulator build
  test/
    test_desktop/               # Native tests (PID, predictor, alarm, fan_logic, temp_conversion)
    test_embedded/              # On-device tests (ADC, fan_pwm, servo, buzzer, i2c)
  platformio.ini
enclosure/
  bbq-case.scad                 # Controller enclosure (front bezel, rear shell, kickstand)
  bbq-fan-assembly.scad         # Fan + damper + UDS adapter
  README.md                     # Print settings, assembly notes, hardware (screws, inserts, clamps)
  stl/                          # Export STLs here from OpenSCAD
docs/                           # Project media and documentation assets
scripts/                        # Developer setup scripts (setup-dev.ps1)
.github/workflows/              # CI (ci.yml) and release (release.yml) workflows
```

### Cook Simulator (Desktop)

PlatformIO native build that runs the LVGL touchscreen UI in an SDL2 window and serves the web UI via an embedded web server (mongoose). Uses the same C++ thermal model and shared WebSocket protocol as the firmware — one source of truth.

```bash
# Build and run
pio run -e simulator
.pio/build/simulator/program                    # SDL2 window + http://localhost:3000

# With options
.pio/build/simulator/program --speed 10         # 10x speed
.pio/build/simulator/program --speed 50         # 50x speed (12-hour cook in ~15 minutes)
.pio/build/simulator/program --profile stall    # brisket stall scenario
.pio/build/simulator/program --port 8080        # custom web server port
```

**How it works:**
- SDL2 window renders the LVGL touchscreen UI (same code as firmware)
- Embedded mongoose HTTP server serves web UI files from `firmware/data/`
- WebSocket on the same port sends simulated data using the shared protocol (`web_protocol.cpp`)
- Shared C++ thermal model (`sim_thermal.cpp`) drives both the touchscreen and web UIs
- Edit `app.js` or `style.css` → refresh browser manually (no compile needed for web UI changes)

**Thermal model** simulates:
- Charcoal fire with thermal mass and heat decay
- PID-like response to fan/damper output (simplified, not exact PID — just realistic curves)
- Meat temperature rise with configurable thermal properties
- Setpoint changes from either UI (touchscreen or web) affect the simulated pit temp

**Pre-built cook profiles** (`sim_profiles.h`):

| Profile | Description | Simulated duration (real time at 1x) |
|---------|-------------|--------------------------------------|
| `normal` | 225F pit, pork butt to 203F, smooth rise | ~10 hours |
| `stall` | 225F pit, brisket with 150-170F stall plateau | ~14 hours |
| `hot-fast` | 300F pit, chicken thighs to 185F, fast cook | ~2 hours |
| `temp-change` | Start at 225F, bump to 275F mid-cook | ~8 hours |
| `lid-open` | Normal cook with periodic lid-open temp drops | ~10 hours |
| `fire-out` | Normal cook, fire dies at 4 hours (pit temp declines) | ~6 hours |
| `probe-disconnect` | Normal cook, meat1 probe disconnects at 3 hours | ~10 hours |

**Key constraint**: The web UI code in `firmware/data/` must work identically on both the simulator and the real ESP32. No simulator-specific code in the web UI — the abstraction boundary is the WebSocket protocol (`web_protocol.h/.cpp`).

### Configuration Persistence

All user settings stored in `config.json` on LittleFS. Survives reboots and firmware OTA updates (LittleFS partition is not overwritten by firmware flash).

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

### First-Boot Setup Wizard (Touchscreen)

On first power-on (or after factory reset), the touchscreen walks the user through:
1. **Welcome** — project name, version
2. **Units** — °F or °C
3. **Wi-Fi** — displays QR code that auto-connects phone to AP and opens captive portal; WiFiManager handles credential entry
4. **Probe check** — plug in probes, verify live readings on screen
5. **Hardware test** — fan spins briefly, servo sweeps, buzzer beeps
6. **Done** — sets `setupComplete: true` in config, shows dashboard

### Wi-Fi & Network

- **Provisioning**: WiFiManager library creates captive portal AP (`BBQ-Setup`). Phone auto-redirects to config page on connect — no IP to remember.
- **QR code on screen**: In AP mode, touchscreen displays a QR code encoding a WiFi network config (`WIFI:T:WPA;S:BBQ-Setup;P:bbqsetup;;`). Scanning connects the phone to the AP automatically, then captive portal opens.
- **mDNS**: After joining home network, device registers `bbq.local`. Access web UI at `http://bbq.local`.
- **Auto-reconnect**: If Wi-Fi drops, device retries in background. Touchscreen UI remains fully functional — Wi-Fi is a convenience layer, not a dependency.
- **Fallback AP**: If stored Wi-Fi credentials fail 3 times on boot, device falls back to AP mode for reconfiguration.

### OTA Firmware Updates

- Web-based upload at `http://bbq.local/update` (ElegantOTA or AsyncElegantOTA)
- Upload a `.bin` file from the browser — no USB needed after initial flash
- LittleFS partition preserved across firmware updates (config and session data safe)
- Firmware version displayed on touchscreen settings page and web UI footer

### Error Detection

| Error | Detection | Response |
|-------|-----------|----------|
| Probe disconnected | ADC reads max (open circuit / infinite resistance) | Show "---" for temp, disable alarms for that probe |
| Probe shorted | ADC reads 0 (zero resistance) | Show "ERR" for temp, disable alarms for that probe |
| Fire out | Pit temp declining >2°F/min for 10+ min despite fan at 100% | Alarm: buzzer + push notification "Fire may be out" |
| Fan stall | Future: tachometer feedback. V1: inferred from pit not responding to fan | Informational warning on screen |
| Wi-Fi lost | WiFi.status() != connected | Auto-reconnect loop, status icon on touchscreen, all local functions continue |

### Factory Reset

Hold finger on touchscreen for 10 seconds during the boot splash screen. Device wipes `config.json`, restarts, and enters setup wizard. Cook session data is also cleared.

### Enclosure (OpenSCAD Parametric Design)

Single `bbq-case.scad` file with all parts. Open in OpenSCAD to preview, render, and export STLs.

**Design goals**: Sturdy, professional, rounded corners, basic but clean. Not trying for tight component-hugging tolerances — generous internal space to nestle everything comfortably.

**Three printable parts:**
1. **Front bezel** — holds the WT32-SC01 Plus display, bezel lip frames the screen, screw bosses
2. **Rear shell** — houses carrier board, panel-mount jacks, barrel jack, cable pass-throughs, ventilation
3. **Kickstand** — detachable flip-out stand (~30° viewing angle), press-fit hinge

**Key dimensions (parametric variables in the .scad file):**

```
// Board dimensions
wt32_pcb_w    = 60;      // WT32-SC01 Plus width (mm)
wt32_pcb_h    = 92;      // WT32-SC01 Plus height (mm)
wt32_pcb_d    = 10.8;    // WT32-SC01 Plus depth (mm)
display_w     = 50;      // Active display area width (mm)
display_h     = 74;      // Active display area height (mm)

carrier_w     = 50;      // Carrier perfboard width (mm)
carrier_h     = 70;      // Carrier perfboard height (mm)
carrier_d     = 15;      // Carrier board component height (mm)

// Enclosure
wall          = 2.5;     // Wall thickness (mm)
corner_r      = 4;       // Corner radius (mm)
tolerance     = 0.5;     // Fit tolerance per side (mm)
screw_d       = 3.0;     // M3 screw hole diameter (mm)

// Panel mount holes
probe_jack_d  = 6.2;     // 2.5mm mono jack mounting hole (mm)
barrel_jack_d = 12.2;    // DC barrel jack mounting hole (mm)
usb_c_w       = 10;      // USB-C cutout width (mm)
usb_c_h       = 4;       // USB-C cutout height (mm)
```

**Assembly:**
- 4x M3x8mm screws join front bezel to rear shell (into heat-set threaded inserts or self-tapping into PETG)
- WT32-SC01 Plus held by its 4x M3 mounting holes on standoffs in the front bezel
- Carrier board rests on standoffs in the rear shell
- Panel-mount jacks thread through holes and secure with their own nuts
- Kickstand clips into a hinge slot on the rear shell

**Panel layout (rear shell bottom edge, landscape orientation):**

```
┌───────────────────────────────────────────────┐
│                  REAR SHELL                   │
│                                               │
│  [vent slots]              [vent slots]       │
│                                               │
│  Carrier board standoffs                      │
│                                               │
├───────────────────────────────────────────────┤
│ BOTTOM EDGE                                   │
│ [USB-C]  [Probe1] [Probe2] [Probe3] [DC 12V] │
└───────────────────────────────────────────────┘

SIDE EDGE (right):
  [Fan cable slot]  [Servo cable slot]
```

**Print settings:**
- Material: PETG
- Layer height: 0.2mm
- Walls: 4 perimeters
- Infill: 30%
- No supports needed (designed for supportless printing)
- Print front bezel face-down, rear shell open-side-up

### Fan + Damper Assembly (Separate OpenSCAD file, UDS-specific)

Separate from the controller enclosure. A `bbq-fan-assembly.scad` file for the blower + servo damper unit that mounts to the smoker.

**Target smoker**: Ugly Drum Smoker (UDS) with standard 3/4" NPT pipe nipple intakes. Cap all intakes except one; this assembly mounts to the open nipple and controls all airflow.

**Two printable parts:**
1. **Blower housing** — integrated unit holding:
   - 5015 blower fan (50x50x15mm cavity with screw mounts)
   - MG90S servo mount
   - Butterfly damper plate on the servo arm at the fan intake side
   - Round output duct (~26mm ID to match 3/4" NPT pipe OD)
2. **UDS pipe adapter** — slides/friction-fits over the 3/4" NPT pipe nipple:
   - Inner diameter: ~27mm (3/4" NPT OD is ~26.7mm, with tolerance)
   - Hose-clamp groove for secure attachment
   - Mates to the blower housing output duct
   - Gasket groove for high-temp gasket tape seal

**Material note**: The fan assembly is near the smoker but not in the fire. The pipe adapter is close to the drum which can be hot. Print the adapter in **PETG minimum** (80°C glass transition). The drum skin near the bottom vent is typically 100-150°F during a 225°F cook — well within PETG range. If concerned, ASA is an option (105°C glass transition).

**Key dimensions:**
```
// Fan
fan_w         = 50;      // 5015 blower width (mm)
fan_h         = 50;      // 5015 blower height (mm)
fan_d         = 15;      // 5015 blower depth (mm)
fan_outlet_d  = 20;      // Fan outlet inner diameter (mm)

// Servo
servo_w       = 23;      // MG90S width (mm)
servo_h       = 12.2;    // MG90S height (mm)
servo_d       = 28.5;    // MG90S depth with mounting tabs (mm)

// UDS adapter
npt_od        = 26.7;    // 3/4" NPT pipe outer diameter (mm)
adapter_id    = 27.2;    // Adapter inner diameter with tolerance (mm)
adapter_len   = 30;      // Adapter length for grip (mm)
clamp_groove  = 2;       // Hose clamp groove depth (mm)
```

**Airflow path:**
```
Outside air → [butterfly damper] → [5015 blower fan] → [duct] → [UDS pipe adapter] → drum
              (servo controlled)    (PWM speed)         (sealed)  (clamped to pipe nipple)
```

**Assembly:**
- Fan screws into housing (M3 or M4, fan-specific)
- Servo press-fits or screws into servo mount pocket
- Damper plate attached to servo horn with a small screw
- Housing duct press-fits into UDS adapter (or secured with a small hose clamp)
- UDS adapter slides over 3/4" pipe nipple and secured with a small hose clamp

**Future adapters**: The blower housing output duct is a standard round interface. Community can design adapter plates for other smokers (Weber WSM, kamado, offset) that mate to the same duct opening.

### Key Technical Constants

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

**Data Storage (single-session model):**
- Only the current/last cook session is stored on device — no multi-session archive
- Cook data point: 13 bytes (timestamp + 3 temps + fan% + damper% + flags)
- RAM buffer: 600 samples (~50 min at 5s intervals)
- LittleFS flush: every 60 seconds (power-loss recovery — lose at most 60s of data)
- 12-hour cook: ~110 KB on flash
- Starting a new session requires confirmation and overwrites previous session
- Web UI provides CSV/JSON download before overwrite — the phone/laptop is the archive

### Fan + Damper Coordination (Split-Range)

The PID produces a single 0-100% output mapped to both actuators:
- **Damper**: Linearly maps full PID range (0% = closed, 100% = open)
- **Fan**: Activates above configurable threshold (default 30%), scales within its own min-max range
- Three modes: fan-only, fan+damper coordinated, damper-primary with fan boost

### Predictive Curve (Web UI Chart)

The chart always shows a projected curve for each meat probe from the current temperature to the target:
- Actual data renders as a solid line on the left (historical)
- A dashed projection line extends to the right showing the predicted path to target
- The predicted done time (in browser local timezone) is displayed at the end of the projection
- As the cook progresses, actual data fills in and the projection continuously recalculates
- The chart X-axis extends to the predicted done time so you always see the full picture
- Prediction uses rolling window linear regression (stall-aware in future iteration)
- All timestamps from the ESP32 are UTC epoch seconds; the browser converts to local timezone for display

### Alarm Thresholds

- Meat target alarm: triggers when probe reaches set temperature
- Pit alarm: no alerts during ramp-up; once setpoint reached, alerts on deviation beyond configurable band (recommend ±15°F default based on normal oscillation)
- Three alarm layers: local buzzer, Web Audio API (browser open), Pushover notifications (phone in pocket)

### Web UI as PWA

- `manifest.json` with app name, icon, theme color, `display: standalone`
- Service worker (`sw.js`) caches the app shell (HTML/JS/CSS) for offline loading — data still requires WebSocket connection
- "Add to Home Screen" prompt on first visit (or manual via browser menu)
- On iOS: must be added to home screen for push notification support (iOS 16.4+)

### WebSocket Protocol

```json
// Server → Client: periodic data (every 1-2s)
{"type":"data","ts":1707600000,"pit":225.5,"meat1":145.2,"meat2":98.7,"fan":45,"damper":80,"sp":225,"lid":false,"est":1707614400,"meat1Target":203,"meat2Target":null,"errors":[]}

// Server → Client: history dump (on connect)
{"type":"history","sp":225,"meat1Target":203,"meat2Target":null,"data":[[ts,pit,meat1,meat2,fan,damper],...]}

// Server → Client: session reset confirmation
{"type":"session","action":"reset","sp":225}

// Server → Client: CSV download response
{"type":"session","action":"download","format":"csv","data":"timestamp,pit,meat1,...\n..."}

// Client → Server
{"type":"set","sp":250}
{"type":"alarm","meat1Target":203,"meat2Target":185,"pitBand":15}
{"type":"session","action":"new"}
{"type":"session","action":"download","format":"csv"}
```
