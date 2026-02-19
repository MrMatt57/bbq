# Web UI Development

Guide for developing the Pit Claw web interface and using the desktop simulator.

## Simulator

The simulator runs the LVGL touchscreen UI in an SDL2 window and serves the web UI via an embedded web server — no ESP32 hardware needed.

### Build and Run

```bash
cd firmware

# Build
pio run -e simulator

# Run (opens SDL2 window + web server at http://localhost:3000)
.pio/build/simulator/program
```

### Command-Line Options

```bash
.pio/build/simulator/program --speed 10         # 10x time acceleration
.pio/build/simulator/program --speed 50         # 50x speed (12-hour cook in ~15 min)
.pio/build/simulator/program --profile stall    # brisket stall scenario
.pio/build/simulator/program --port 8080        # custom web server port
```

### Cook Profiles

| Profile | Description | Duration (real time at 1x) |
|---------|-------------|----------------------------|
| `normal` | 225°F pit, pork butt to 203°F, smooth rise | ~10 hours |
| `stall` | 225°F pit, brisket with 150-170°F stall plateau | ~14 hours |
| `hot-fast` | 300°F pit, chicken thighs to 185°F | ~2 hours |
| `temp-change` | Start at 225°F, bump to 275°F mid-cook | ~8 hours |
| `lid-open` | Normal cook with periodic lid-open temp drops | ~10 hours |
| `fire-out` | Normal cook, fire dies at 4 hours | ~6 hours |
| `probe-disconnect` | Normal cook, meat1 probe disconnects at 3 hours | ~10 hours |

### How It Works

- **SDL2 window** renders the LVGL touchscreen UI (same code as firmware)
- **Mongoose HTTP server** serves web UI files from `firmware/data/`
- **WebSocket** on the same port sends simulated data using the shared protocol (`web_protocol.cpp`)
- **Thermal model** (`sim_thermal.cpp`) simulates charcoal fire physics, heat decay, meat temp rise, and PID response

Both the touchscreen and web UI are driven by the same thermal model and shared WebSocket protocol — one source of truth.

### Editing the Web UI

Web UI files live in `firmware/data/`:

```
firmware/data/
  index.html          # Main page
  app.js              # Application logic
  style.css           # Styles
  manifest.json       # PWA manifest
  favicon.svg         # App icon
  sw.js               # Service worker (offline shell caching)
```

Edit these files and refresh your browser — no recompile needed. Only the simulator itself (C++ code) requires rebuilding.

**Key constraint:** The web UI code must work identically on both the simulator and the real ESP32. No simulator-specific code in the web UI — the abstraction boundary is the WebSocket protocol.

## Web UI Features

- **Live temperatures** — pit and both meat probes with set points
- **Temperature graph** — uPlot time-series chart with pit temp, meat temps, fan %, and damper %
- **Predictive curve** — dashed projection from current meat temp to target, with predicted done time
- **Controls** — set pit target temperature, meat target temperatures, alarm thresholds
- **Cook timer** — starts when pit reaches set point; tracks total cook time
- **Session export** — download cook data as CSV/JSON (device stores only current/last session)
- **OTA update** — flash new firmware at Settings → Update
- **Alarms** — configure pit deviation band, meat targets, Pushover notifications

### Graph

The timeline focuses on the active cook — ramp-up time is shown compressed on the left, with the main view starting from when the pit first reaches set temperature. The chart extends to the right with a dashed predictive curve showing the projected path to the meat target temperature, with the predicted done time displayed at the end. All times shown in the browser's local timezone.

### PWA

The web UI is a Progressive Web App:

- `manifest.json` defines the app name, icon, theme color, and `display: standalone`
- `sw.js` caches the app shell (HTML/JS/CSS) for offline loading — data still requires a WebSocket connection
- "Add to Home Screen" for a native-app experience on phones:
  - **iOS**: Safari → Share → Add to Home Screen
  - **Android**: Chrome → Menu → Add to Home Screen

## WebSocket Protocol

All communication between the web UI and device (or simulator) uses JSON over WebSocket.

### Server → Client

**Periodic data** (every 1-2 seconds):
```json
{
  "type": "data",
  "ts": 1707600000,
  "pit": 225.5,
  "meat1": 145.2,
  "meat2": 98.7,
  "fan": 45,
  "damper": 80,
  "sp": 225,
  "lid": false,
  "est": 1707614400,
  "meat1Target": 203,
  "meat2Target": null,
  "errors": []
}
```

**History dump** (on connect):
```json
{
  "type": "history",
  "sp": 225,
  "meat1Target": 203,
  "meat2Target": null,
  "data": [[ts, pit, meat1, meat2, fan, damper], ...]
}
```

**Session events:**
```json
{"type": "session", "action": "reset", "sp": 225}
{"type": "session", "action": "download", "format": "csv", "data": "timestamp,pit,meat1,...\n..."}
```

### Client → Server

```json
{"type": "set", "sp": 250}
{"type": "alarm", "meat1Target": 203, "meat2Target": 185, "pitBand": 15}
{"type": "session", "action": "new"}
{"type": "session", "action": "download", "format": "csv"}
```

### Timestamps

All timestamps from the ESP32 are UTC epoch seconds. The browser converts to local timezone for display. The chart library (uPlot) handles timezone-aware axis labels.
