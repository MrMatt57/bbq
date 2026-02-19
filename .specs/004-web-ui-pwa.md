# 004: Web UI PWA

**Branch**: `feature/web-ui-overhaul-pitclaw`
**Created**: 2026-02-18
**Status**: Implemented

## Summary

Progressive Web App served from LittleFS over Wi-Fi for remote monitoring and control. Features real-time temperature display, interactive uPlot chart with 2-hour scrolling window, setpoint/target controls, fan mode switching, session management (new cook, CSV download), settings panel with units and time format toggles, browser audio notifications, and firmware update checking. Installable to phone home screen for native app-like experience.

## Requirements

- Vanilla JS (no build step, no frameworks) — edit and refresh workflow
- WebSocket connection with auto-reconnect and exponential backoff (1s→30s)
- Real-time temperature cards: pit (orange), meat1 (red), meat2 (blue) with large font display
- Setpoint and meat target adjustment via ± spinner buttons (debounced)
- Interactive uPlot chart with 2-hour visible window, adaptive zoom, color-coded series
- Fan and damper output bars with percentage labels
- Cook timer in header: start time, elapsed, estimated done
- Settings panel (drawer overlay): units toggle (°F/°C), time format (12h/24h), fan mode buttons
- Session management: new cook button (with confirmation), CSV download
- Browser Web Notification API for alarm alerts
- Web Audio API for alarm tones (works when tab is in background)
- Firmware update checking via GitHub releases API, OTA upload with progress bar
- WiFi status indicator with signal strength
- Cookie-based persistent settings (units, time format)
- Service worker for offline app shell caching
- PWA manifest for home screen installation (standalone display mode)
- All temperatures stored as °F internally, converted for display
- All timestamps from ESP32 are UTC epoch; browser converts to local timezone
- Must work identically on hardware and simulator (no simulator-specific code)

## Design

### Architecture

Single-page app with three layers:
1. **index.html** — PWA shell: header (logo, timer, WiFi, settings), main content (temp cards, chart), settings drawer, update modal
2. **app.js** — Application logic: WebSocket management, chart rendering, controls, notifications, OTA
3. **style.css** — Dark theme styling with amber/red accents, responsive layout

### WebSocket Data Flow

```
Connect → Receive "history" message → Populate chart
        → Receive "data" messages (1.5s) → Update UI
        → Send commands (setpoint, alarms, session, fan mode)
```

### Chart (uPlot)

- 9 data series: timestamp, pit, meat1, meat2, fan, damper, setpoint, meat1Target, meat2Target
- 2-hour visible window with scroll
- Prediction dashed lines extending to estimated done time
- Responsive resize on window change

### Settings Persistence

Units and time format saved to cookies, restored on page load. Fan mode and targets sent to device via WebSocket.

### Color Scheme

- Background: #1a1a2e, Cards: #16213e
- Pit: #f59e0b (amber), Meat1: #ef4444 (red), Meat2: #3b82f6 (blue)
- Fan: #10b981 (green), Damper: #8b5cf6 (purple)
- Accent: #f59e0b, Danger: #ef4444

### PWA Components

- `manifest.json`: App name "Pit Claw", standalone display, amber theme
- `sw.js`: Caches app shell (HTML/JS/CSS) for offline loading; data requires WebSocket
- `favicon.svg`: Lobster claw logo with smoke wisps

## Files to Modify

| File | Change |
|------|--------|
| `firmware/data/index.html` | PWA shell HTML: header, temp cards, chart, settings drawer, update modal |
| `firmware/data/app.js` | WebSocket client, uPlot chart, controls, notifications, OTA, session management |
| `firmware/data/style.css` | Dark theme, responsive layout, card design, progress bars |
| `firmware/data/manifest.json` | PWA metadata (name, icons, display mode, theme color) |
| `firmware/data/sw.js` | Service worker for offline app shell caching |
| `firmware/data/favicon.svg` | Lobster claw logo SVG |

## Test Plan

- [x] WebSocket connects and displays real-time temperature data
- [x] Chart renders historical data with color-coded series
- [x] Setpoint and meat target controls send commands via WebSocket
- [x] Units toggle (°F/°C) converts all displayed temperatures
- [x] Time format toggle (12h/24h) updates cook timer and estimates
- [x] Settings persist across page reloads (cookies)
- [x] CSV download produces valid file
- [x] New session button resets data with confirmation
- [x] PWA installable from browser (manifest + service worker)
- [x] Works identically on simulator and hardware
