# <img src="firmware/data/favicon.svg" width="36" height="36" alt="Pit Claw logo"> Pit Claw

[![CI](https://github.com/MrMatt57/pitclaw/actions/workflows/ci.yml/badge.svg)](https://github.com/MrMatt57/pitclaw/actions/workflows/ci.yml)

Open-source BBQ temperature monitor and controller. Uses an ESP32-S3 with a 3.5" touchscreen to maintain precise smoker temperature via PID-controlled fan and optional servo damper. Reads Thermoworks-compatible thermistor probes. Includes a web interface (PWA) for remote monitoring from any phone or computer on Wi-Fi. Touchscreen works fully offline.

## Features

- **3 probe inputs**: 1 pit (ambient) + 2 meat probes (Thermoworks Pro-Series compatible, 2.5mm jack)
- **PID temperature control**: Fan + optional damper hold pit temperature steady (±5°F typical)
- **3.5" capacitive touchscreen**: Real-time dashboard — works fully without Wi-Fi
- **Web UI (PWA)**: Install to home screen, real-time graph, set points, alarms, session export
- **Predictive curve**: Chart shows dashed projection from current meat temp to target with estimated done time
- **Alarms**: Local buzzer + browser audio + Pushover push notifications
- **Cook session logging**: Persists through power loss, download as CSV/JSON before starting next cook
- **Easy setup**: QR code on screen for Wi-Fi, captive portal, mDNS (`bbq.local`)
- **OTA updates**: Flash new firmware from the browser — no USB after initial setup
- **Error detection**: Probe disconnect/short, fire-out warning, Wi-Fi auto-reconnect
- **Calibration**: Per-probe Steinhart-Hart coefficients with offset adjustment
- **3D printable enclosure**: PETG case with snap-fit assembly

## Hardware

### Bill of Materials

All internal electronics mount on a single 50x70mm carrier perfboard behind the display. Requires basic soldering (~20 joints). Enclosure target is ~100x70x40mm.

| # | Component | Product | Qty | Price | Link |
|---|-----------|---------|-----|-------|------|
| 1 | MCU + Display | WT32-SC01 Plus (ESP32-S3, 3.5" 480x320 capacitive touch) | 1 | ~$20-25 | [Amazon](https://www.amazon.com/s?k=WT32-SC01+Plus) / AliExpress |
| 2 | ADC | ADS1115 16-bit I2C breakout (HiLetgo 3-pack) | 1 | ~$4 | [Amazon](https://www.amazon.com/s?k=ADS1115+16+bit) |
| 3 | Blower Fan | GDSTIME 5015 12V dual ball bearing blower (2-pack) | 1 | ~$10-12 | [Amazon](https://www.amazon.com/s?k=GDSTIME+5015+12V+blower) |
| 4 | MOSFET | IRLZ44N logic-level N-channel TO-220 (5-pack) | 1 | ~$7 | [Amazon](https://www.amazon.com/s?k=IRLZ44N+MOSFET) |
| 5 | Servo | Miuzei MG90S metal gear micro servo (2-pack) | 1 | ~$8-10 | [Amazon](https://www.amazon.com/s?k=MG90S+metal+gear+servo) |
| 6 | Power Supply | ALITOVE 12V 5A DC adapter (5.5x2.1mm barrel) | 1 | ~$10-12 | [Amazon](https://www.amazon.com/s?k=12V+5A+power+supply+barrel) |
| 7 | Buck Converter | MP1584EN mini adjustable DC-DC (6-pack, 22x17mm) | 1 | ~$8 | [Amazon](https://www.amazon.com/s?k=MP1584EN+buck+converter) |
| 8 | Power Jack | Panel-mount DC barrel jack 5.5x2.1mm (4-pack) | 1 | ~$6 | [Amazon](https://www.amazon.com/s?k=panel+mount+DC+barrel+jack+5.5x2.1) |
| 9 | Probe Jacks | CESS 2.5mm mono TS panel-mount with nut (4-pack) | 1 | ~$6-8 | [Amazon](https://www.amazon.com/s?k=2.5mm+mono+panel+mount+jack) |
| 10 | Buzzer | Piezo buzzer disc 3-5V (5-pack) | 1 | ~$5 | [Amazon](https://www.amazon.com/s?k=piezo+buzzer+disc+5V) |
| 11 | Carrier Board | 50x70mm perfboard (10-pack) | 1 | ~$6 | [Amazon](https://www.amazon.com/s?k=50x70mm+perfboard) |
| 12 | Resistors | 10K ohm 1% metal film (pack) | 3 | ~$5 | [Amazon](https://www.amazon.com/s?k=10K+1%25+metal+film+resistor) |
| 13 | Capacitors | 0.1uF ceramic (pack) | 3 | ~$5 | [Amazon](https://www.amazon.com/s?k=0.1uF+ceramic+capacitor) |
| 14 | Hookup Wire | 22AWG solid core (assorted colors) | 1 | ~$8 | [Amazon](https://www.amazon.com/s?k=22AWG+solid+core+hookup+wire) |
| | | **Total (excluding probes)** | | **~$95-115** | |

### Temperature Probes (buy separately)

Any Thermoworks Pro-Series probe with 2.5mm mono jack works:
- [TX-1003X-AP](https://www.thermoworks.com/tx-1003x-ap/) — Air/pit probe with grate clip (~$19)
- [TX-1001X-OP](https://www.thermoworks.com/tx-1001x-op/) — Cooking/meat probe (~$21)

Compatible alternatives: Maverick ET-72/73 replacement probes (different Steinhart-Hart coefficients), Inkbird, FireBoard probes.

### 3D Printed Parts

All parametric [OpenSCAD](https://openscad.org/) (free) source files in `enclosure/`. Pre-exported STLs in `enclosure/stl/` if you just want to print.

**Controller enclosure** (`bbq-case.scad`) — three parts:
- **Front bezel** — holds display, bezel lip frames the screen
- **Rear shell** — houses carrier board, panel-mount jacks, cable slots, ventilation
- **Kickstand** — detachable flip-out stand
- Target size: ~100 x 70 x 40mm
- Assembly: 4x M3x8mm screws

**Fan + damper assembly** (`bbq-fan-assembly.scad`) — two parts:
- **Blower housing** — holds 5015 fan, MG90S servo, butterfly damper plate
- **UDS pipe adapter** — friction-fits over 3/4" NPT pipe nipple, secured with hose clamp
- Airflow: outside air → damper → fan → duct → pipe adapter → drum
- The blower housing output is a standard round duct — future adapters for other smokers (WSM, kamado, offset) mate to the same interface

**Print settings** (both):
- Material: PETG (80°C glass transition, resists outdoor heat and grease)
- 4 walls, 30%+ infill, 0.2mm layers, no supports needed

**Additional hardware for assembly:**
- 4x M3x8mm screws + threaded inserts (controller case)
- 2x small hose clamps (fan assembly to pipe, adapter to pipe nipple)
- High-temp gasket tape (seal adapter to pipe nipple)

## Wiring

All components on the carrier perfboard are connected with short soldered traces. External connections use wire runs to panel-mount jacks and the display board.

```
Panel-mount DC barrel jack (12V in)
  │
  └─► Carrier Board 12V rail
       ├─► MP1584EN buck converter IN → OUT (set to 5.0V)
       │     ├─► WT32-SC01 Plus 5V (via extension cable)
       │     └─► Servo VCC
       │
       ├─► IRLZ44N MOSFET drain
       │     └─► Fan +12V (fan GND to MOSFET source)
       │         Gate ◄── GPIO12 (with 10K pulldown to GND)
       │
       └─► GND rail (common ground)

WT32-SC01 Plus Extension Connector (8-pin ribbon cable)
  GPIO10 (SDA) ──► ADS1115 SDA (on carrier board)
  GPIO11 (SCL) ──► ADS1115 SCL (on carrier board)
  GPIO12       ──► IRLZ44N gate (on carrier board)
  GPIO13       ──► Servo signal (wire to servo)
  GPIO14       ──► Buzzer (on carrier board)
  3.3V         ──► ADS1115 VDD + voltage divider supply
  5V           ──► From MP1584EN output
  GND          ──► Common ground rail

Probe voltage dividers (on carrier board, x3)
  3.3V ──[10K 1% resistor]──┬──► ADS1115 AINx
                             │
                       [0.1uF cap]
                             │
    Panel-mount 2.5mm jack ──┴──► GND
    (tip=signal, sleeve=GND, polarity doesn't matter)
```

## Software Setup

### Quick Start (Windows)

One script installs everything you need — Git, Python, Node.js, PlatformIO CLI, and OpenSCAD:

```powershell
# Clone the repo
git clone https://github.com/MrMatt57/pitclaw.git
cd pitclaw

# Run from an elevated (Admin) PowerShell
powershell -ExecutionPolicy Bypass -File scripts\setup-dev.ps1
```

The script is idempotent — safe to run again anytime to verify your setup or after pulling updates that add new dependencies.

### Manual Prerequisites

If you prefer to install tools yourself (or you're not on Windows):

1. [Git](https://git-scm.com/)
2. [Python 3.12+](https://www.python.org/)
3. [Node.js LTS](https://nodejs.org/) (for simulator/web UI development)
4. [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation.html) — `pip install platformio`
5. [VS Code](https://code.visualstudio.com/) + [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
6. [OpenSCAD](https://openscad.org/) (optional, for editing enclosure designs)

### Build & Flash (one time via USB)

```bash
cd firmware

# Build firmware
pio run -e wt32_sc01_plus

# Connect WT32-SC01 Plus via USB-C, then flash
pio run -e wt32_sc01_plus --target upload

# Upload web UI files to flash filesystem
pio run -e wt32_sc01_plus --target uploadfs
```

After this initial flash, all future firmware updates can be done over Wi-Fi at `http://bbq.local/update`.

### First Boot

1. Power on the device — the setup wizard starts on the touchscreen
2. Select your temperature units (°F or °C)
3. A QR code appears on screen — scan it with your phone to connect to the setup Wi-Fi
4. Your phone auto-opens a portal page — enter your home Wi-Fi credentials
5. The device joins your network and registers as `bbq.local`
6. Plug in probes — the wizard shows live readings to verify they work
7. The fan, servo, and buzzer do a quick self-test
8. Setup complete — dashboard appears

### Accessing the Web UI

Open `http://bbq.local` in any browser on the same Wi-Fi network.

For the best experience, add it to your phone's home screen (it's a PWA):
- **iOS**: Safari → Share → Add to Home Screen
- **Android**: Chrome → Menu → Add to Home Screen

### Factory Reset

Hold your finger on the touchscreen for 10 seconds during the boot splash screen.

### Running the Simulator (no hardware needed)

Develop and test the web UI without any ESP32 hardware. The simulator serves the same web UI files and speaks the same WebSocket protocol.

```bash
cd simulator
npm install          # first time only
npm run dev          # http://localhost:3000 — auto-refreshes on file changes
```

Edit files in `firmware/data/` (the web UI) and the browser reloads instantly. Use time acceleration to test long cooks quickly:

```bash
npm run dev -- --speed 50              # 12-hour brisket cook in ~15 minutes
npm run dev -- --profile stall         # test the brisket stall scenario
npm run dev -- --speed 10 --profile fire-out  # test fire-out alarm at 10x
```

Available profiles: `normal`, `stall`, `hot-fast`, `temp-change`, `lid-open`, `fire-out`, `probe-disconnect`.

### Running Tests

```bash
# Desktop unit tests (PID, prediction, alarms — no hardware needed)
pio test -e native

# On-device integration tests
pio test -e wt32_sc01_plus
```

## Web UI

The web interface shows:
- **Live temperatures**: Pit and both meat probes with set points
- **Temperature graph**: uPlot time-series chart with pit temp, meat temps, fan %, and damper %
- **Predictive curve**: Dashed projection from current meat temp to target, with predicted done time
- **Controls**: Set pit target temperature, meat target temperatures, alarm thresholds
- **Cook timer**: Starts when pit reaches set point; tracks total cook time
- **Session export**: Download cook data as CSV/JSON (device stores only current/last session — download before starting a new cook)
- **OTA update**: Flash new firmware at Settings → Update
- **Alarms**: Configure pit deviation band, meat targets, Pushover notifications

The graph timeline focuses on the active cook — ramp-up time is shown compressed on the left, with the main view starting from when the pit first reaches set temperature. The chart extends to the right with a dashed predictive curve showing the projected path to the meat target temperature, with the predicted done time displayed at the end. All times shown in your browser's local timezone.

## License

TBD
