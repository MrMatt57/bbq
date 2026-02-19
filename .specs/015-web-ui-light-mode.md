# 015: Web UI Light Mode

**Branch**: `feature/web-ui-light-mode`
**Created**: 2026-02-18

## Summary

Add a light/dark theme toggle to the web UI settings panel. The theme preference is persisted on the ESP32 firmware in `config.json` and sent to all connected web clients via the WebSocket protocol (following the same pattern as `fanMode`). When a client connects, it receives the current theme and applies it immediately.

## Requirements

- Add a light mode CSS theme using CSS custom properties and a `[data-theme="light"]` selector
- Keep the current dark theme as the default
- Add a Dark/Light toggle button in the settings panel "Display" section (alongside °F/°C and 12h/24h toggles)
- Persist the theme preference on the firmware in `config.json` under a `"theme"` key (`"dark"` or `"light"`)
- Send the theme to web clients via the WebSocket data message (like `fanMode`)
- Include the theme in the history message header so clients get it immediately on connect
- Handle `{"type":"config","theme":"light"}` from clients to update the theme
- On theme change, broadcast to all connected clients so they sync
- Chart axis/grid colors must update when theme changes (recreate chart)
- Logo SVG background and crab eye fills must adapt to theme
- All `rgba(255,255,255,X)` semi-transparent patterns need light-mode counterparts (`rgba(0,0,0,X)`)
- Simulator must handle theme messages (store locally, no persistence needed)

## Design

### CSS Architecture

The existing `:root` block contains all CSS custom properties for the dark theme. Add a `[data-theme="light"]` block that overrides these variables:

```css
[data-theme="light"] {
  --bg-primary: #f5f5f5;
  --bg-card: #ffffff;
  --bg-input: #e8e8e8;
  --text-primary: #1a1a2e;
  --text-secondary: #4b5563;
  --text-muted: #9ca3af;
  --shadow: 0 4px 12px rgba(0, 0, 0, 0.08);
  /* accent colors unchanged */
}
```

Also override all `rgba(255,255,255,X)` patterns (borders, hover states, scrollbar) with appropriate light-mode counterparts.

Apply by setting `document.documentElement.setAttribute('data-theme', 'light')` or removing the attribute for dark.

### Protocol Changes

Follow the `fanMode` pattern exactly:

1. **DataPayload**: Add `const char* theme` field
2. **buildDataMessage**: Include `"theme":"dark"` or `"theme":"light"` in every data broadcast
3. **buildHistoryMessage**: Include `"theme":"dark"` or `"theme":"light"` in the history header
4. **ParsedCommand**: Add `char theme[8]` field and `SET_THEME` command type
5. **parseCommand**: Handle `{"type":"config","theme":"..."}` alongside existing fanMode handling

### Config Changes

1. Add `char theme[8]` to `AppConfig` struct (default: `"dark"`)
2. Add `getTheme()` / `setTheme()` methods to `ConfigManager`
3. Serialize/deserialize in `toJson`/`fromJson`

### Web Server Changes

1. Add `ThemeCallback` type and `_onTheme` callback
2. Handle `SET_THEME` command: call callback, broadcast immediately
3. In `buildDataPayload`: read theme from config manager
4. In `main.cpp`: wire up callback to save theme to config

### Simulator Changes

1. Add `g_theme` global variable (default: `"dark"`)
2. Handle theme in `web_on_theme` callback and `sim_main.cpp`
3. Include theme in data payloads
4. Add `onTheme` callback to `SimWebServer`

### JavaScript Changes

1. Add `currentTheme` state variable
2. In `handleMessage` for `data` type: check `msg.theme` and apply if changed
3. In `loadHistory`: read `msg.theme` and apply
4. Add toggle button handler that sends `{"type":"config","theme":"..."}` via WebSocket
5. `applyTheme(theme)` function: sets data attribute, updates chart colors, recreates chart
6. Chart opts must use a function to return theme-aware axis/grid colors
7. Save theme to localStorage as fallback (for when firmware is unreachable)

### HTML Changes

Add a theme toggle button in the Display `prefs-row`:
```html
<button class="btn btn-toggle" id="btnToggleTheme" title="Toggle Dark / Light">Dark</button>
```

## Files to Modify

| File | Change |
|------|--------|
| `firmware/data/style.css` | Add `[data-theme="light"]` CSS custom property overrides and light-mode specific rules |
| `firmware/data/app.js` | Add theme state, toggle, sync with firmware, theme-aware chart colors |
| `firmware/data/index.html` | Add theme toggle button in settings Display section |
| `firmware/src/config_manager.h` | Add `theme` field to `AppConfig`, getter/setter |
| `firmware/src/config_manager.cpp` | Serialize/deserialize theme, apply default |
| `firmware/src/web_protocol.h` | Add `theme` to `DataPayload`, `SET_THEME` command |
| `firmware/src/web_protocol.cpp` | Include theme in data/history messages, parse config.theme |
| `firmware/src/web_server.h` | Add `ThemeCallback`, `onTheme()` setter |
| `firmware/src/web_server.cpp` | Handle SET_THEME, include theme in payload, broadcast |
| `firmware/src/main.cpp` | Wire up theme callback to config save |
| `firmware/src/simulator/sim_web_server.h` | Add `onTheme` callback |
| `firmware/src/simulator/sim_web_server.cpp` | Handle theme messages |
| `firmware/src/simulator/sim_main.cpp` | Add g_theme, wire up web_on_theme callback, include in payloads |

## Test Plan

- [ ] Simulator builds (`pio run -e simulator`)
- [ ] Firmware builds (`pio run -e wt32_sc01_plus`)
- [ ] Desktop tests pass (`pio test -e native`)
- [ ] Run simulator: web UI loads in dark mode by default
- [ ] Toggle to Light mode via settings: UI switches to light theme (backgrounds, text, borders, chart)
- [ ] Refresh browser: light mode persists (received from firmware/simulator on reconnect)
- [ ] Toggle back to Dark: UI switches back correctly
- [ ] Chart axis labels, grid lines, and legend update appropriately for each theme
- [ ] Logo SVG renders correctly in both themes (circle background + eye fills match bg)
- [ ] Settings panel, buttons, inputs, scrollbars all look correct in light mode
- [ ] Multiple browser tabs sync theme when one changes it
