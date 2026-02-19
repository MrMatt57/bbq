# 005: WebSocket Protocol & Web Server

**Branch**: `feature/web-server-ota`
**Created**: 2026-02-18
**Status**: Implemented

## Summary

Shared WebSocket JSON protocol layer used by both the ESP32 firmware (ESPAsyncWebServer) and the desktop simulator (mongoose). Provides real-time data broadcast, history replay on client connect, and bidirectional command handling. The protocol is the abstraction boundary that allows the web UI to work identically on hardware and simulator.

## Requirements

- JSON-based WebSocket protocol with message types: data, history, session, set, alarm
- Periodic data broadcast every 1.5 seconds to all connected clients (max 4)
- History replay on new client connect (send full session buffer)
- Server→Client message types:
  - `data`: periodic state (timestamp, temps, fan%, damper%, setpoint, lid, targets, estimates, errors)
  - `history`: session replay with data array and current targets
  - `session`: reset confirmation, CSV download envelope
- Client→Server message types:
  - `set`: change setpoint
  - `alarm`: set meat targets and pit band
  - `session`: new session, download CSV/JSON
  - `fan_mode`: change fan control mode
- Disconnected probes serialized as `null`, shorted probes as `-1`
- Temperatures with 1 decimal precision
- Shared C++ implementation (`web_protocol.h/.cpp`) compiled into both firmware and simulator
- ESP32: ESPAsyncWebServer + AsyncWebSocket on port 80
- Simulator: mongoose HTTP + WebSocket on port 3000 (configurable via `--port`)
- HTTP routes: `/` (index.html), `/api/version`, `/update` (OTA), all other files from LittleFS/filesystem
- mDNS discovery at `bbq.local` (firmware only)

## Design

### Protocol Messages

```json
// Server → Client: periodic data (every 1.5s)
{
  "type": "data",
  "ts": 1707600000,
  "pit": 225.5,
  "meat1": 145.2,
  "meat2": null,
  "fan": 45,
  "damper": 80,
  "sp": 225,
  "lid": false,
  "meat1Target": 203,
  "meat2Target": null,
  "est": 1707614400,
  "fanMode": "fan_and_damper",
  "errors": []
}

// Server → Client: history (on connect)
{
  "type": "history",
  "sp": 225,
  "meat1Target": 203,
  "meat2Target": null,
  "data": [{"ts":..., "pit":..., "meat1":..., ...}, ...]
}

// Client → Server: commands
{"type": "set", "sp": 250}
{"type": "alarm", "meat1Target": 203, "meat2Target": 185, "pitBand": 15}
{"type": "session", "action": "new"}
{"type": "session", "action": "download", "format": "csv"}
```

### Shared Protocol Layer

`web_protocol.h/.cpp` provides:
- `DataPayload` struct — all fields for periodic broadcast
- `buildDataMessage()` — serialize to JSON buffer
- `buildHistoryMessage()` — serialize history array (dynamically allocated, caller frees)
- `buildSessionReset()` / `buildCSVDownloadEnvelope()` — session messages
- `parseCommand()` — parse incoming JSON into `ParsedCommand` struct

### ESP32 Web Server

ESPAsyncWebServer handles HTTP and WebSocket on the same port. Static files served gzipped from LittleFS. WebSocket handler routes incoming messages through `parseCommand()` and invokes registered callbacks.

### Simulator Web Server

Mongoose embedded web server serves files from `firmware/data/` directory. WebSocket handler uses the same `web_protocol.cpp` functions. Port configurable via `--port` CLI argument.

## Files to Modify

| File | Change |
|------|--------|
| `firmware/src/web_protocol.h/.cpp` | Shared JSON message building and parsing |
| `firmware/src/web_server.h/.cpp` | ESPAsyncWebServer + AsyncWebSocket setup, REST routes, broadcast loop |
| `firmware/src/simulator/sim_web_server.h/.cpp` | Mongoose HTTP + WebSocket server for simulator |

## Test Plan

- [x] Data messages contain all required fields with correct types
- [x] History replay populates chart on client connect
- [x] Setpoint commands from web UI update device state
- [x] Alarm target commands update alarm manager
- [x] Session new/download commands work correctly
- [x] Disconnected probes serialize as null, shorted as -1
- [x] Protocol works identically on firmware and simulator
- [x] Firmware builds (`pio run -e wt32_sc01_plus`)
- [x] Simulator builds (`pio run -e simulator`)
