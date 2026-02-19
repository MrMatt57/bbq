# 001: Core Firmware Platform

**Branch**: `feature/mvp-scaffold`
**Created**: 2026-02-18
**Status**: Implemented

## Summary

The foundational firmware layer for the BBQ temperature controller. Implements PID-controlled fan and servo damper output, thermistor temperature reading via ADS1115 ADC, alarm detection with buzzer, error monitoring, cook session logging with power-loss recovery, persistent JSON configuration, WiFi provisioning with captive portal, and OTA firmware updates.

## Requirements

- PID controller with lid-open detection, bumpless transfer, and configurable tuning (P=4.0, I=0.02, D=5.0 defaults)
- Read 3 NTC thermistor probes (1 pit + 2 meat) via ADS1115 16-bit I2C ADC with Steinhart-Hart conversion
- EMA-filtered temperature readings (alpha=0.2) sampled at 1s intervals, PID computed every 4s
- PWM fan control (25 kHz) with kick-start (75% for 500ms), long-pulse mode below 10%, minimum 15% sustained speed
- Servo damper control (0-90 degree range, linear 0-100% mapping)
- Split-range fan+damper coordination: damper maps full PID range, fan activates above configurable threshold (default 30%)
- Pit band alarm (±15°F default, only after setpoint reached) and meat target alarms with hysteresis
- Buzzer alarm output (2 kHz tone, 500ms on/off pattern)
- Error detection: probe disconnect (ADC >= 32000), probe short (ADC <= 100), fire-out (2°F/min decline for 10+ min at 95%+ fan)
- Cook session logging: 13-byte data points at 5s intervals, 600-sample circular RAM buffer, LittleFS flush every 60s
- CSV and JSON export of session data
- Persistent configuration via config.json on LittleFS (WiFi, units, PID, fan, probes, alarms, setup state)
- Factory reset support (wipe config.json, restart setup wizard)
- WiFi provisioning via WiFiManager captive portal (AP: "BBQ-Setup" / "bbqsetup"), mDNS at bbq.local
- Auto-reconnect with exponential backoff (5s base, 60s max, 20 attempts)
- Fallback AP mode after 3 boot failures
- OTA firmware updates via ElegantOTA at /update endpoint, progress tracking, LittleFS preserved across updates
- Arduino setup/loop pattern with modular initialization and callback wiring

## Design

### Architecture

Modular design with each subsystem in its own .h/.cpp pair. The main loop orchestrates all modules via callbacks to avoid circular dependencies. Each module exposes a `begin()` for initialization and `update()` for periodic work.

### Control Loop (main.cpp)

```
setup():
  ConfigManager.begin() → load config.json
  TempManager.begin() → init ADS1115
  PidController.begin() → init QuickPID
  FanController.begin() → init LEDC PWM
  ServoController.begin() → attach servo
  AlarmManager.begin() → init buzzer pin
  ErrorManager.begin()
  CookSession.begin() → mount LittleFS, recover session
  WifiManager.begin() → connect or start AP
  OtaManager.begin() → register /update
  WebServer.begin() → start HTTP + WebSocket

loop():
  TempManager.update() → read ADC, convert temps
  PidController.update() → compute output
  FanController.setSpeed(pidOutput) → apply fan
  ServoController.setPosition(pidOutput) → apply damper
  AlarmManager.update() → check thresholds
  ErrorManager.update() → check errors
  CookSession.update() → log data point
  WebServer.update() → broadcast to clients
  WifiManager.update() → handle reconnection
  OtaManager.update() → service OTA
```

### PID Controller

Uses QuickPID library with pOnMeas, dOnMeas, and iAwCondition anti-windup. Lid-open detection triggers when pit drops >6% below setpoint; recovery at 2% below. During lid-open, PID output is zeroed and integrator is reset for bumpless transfer back to normal control.

### Temperature Pipeline

```
ADC raw → voltage divider → resistance → Steinhart-Hart → °C → optional °F → EMA filter
```

Steinhart-Hart coefficients from Thermoworks Pro-Series probes: A=7.3431401e-04, B=2.1574370e-04, C=9.5156860e-08. Reference resistor: 10K ohm (1% metal film). 0.1uF ceramic filter cap on each ADC input.

### Fan Control State Machine

1. **Off (0%)**: PWM=0
2. **Kick-start (0→non-zero)**: 75% duty for 500ms, then target
3. **Long-pulse (<10%)**: Cycle between 15% min and off over 10s period
4. **Normal (10-100%)**: Direct PWM with 15% floor

### Session Data Format

```c
struct DataPoint {  // 13 bytes
    uint32_t timestamp;   // UTC epoch seconds
    int16_t  pitTemp;     // °F × 10 (e.g., 2255 = 225.5°F)
    int16_t  meat1Temp;
    int16_t  meat2Temp;
    uint8_t  fanPct;
    uint8_t  damperPct;
    uint8_t  flags;       // bitmask: lid-open, alarms, errors, probe disconnects
};
```

### Configuration Persistence

All settings stored in `/config.json` on LittleFS via ArduinoJson. Structure mirrors the JSON schema documented in CLAUDE.md. Factory reset deletes the file and restarts.

## Files to Modify

| File | Change |
|------|--------|
| `firmware/src/config.h` | Pin assignments, constants, defaults for all modules |
| `firmware/src/main.cpp` | Setup/loop orchestration, module init, callback wiring |
| `firmware/src/pid_controller.h/.cpp` | QuickPID wrapper with lid-open detection |
| `firmware/src/temp_manager.h/.cpp` | ADS1115 reading, Steinhart-Hart, EMA filtering |
| `firmware/src/fan_controller.h/.cpp` | PWM output, kick-start, long-pulse, min-speed |
| `firmware/src/servo_controller.h/.cpp` | Damper servo 0-90° control |
| `firmware/src/alarm_manager.h/.cpp` | Pit band + meat target alarms, buzzer |
| `firmware/src/error_manager.h/.cpp` | Probe disconnect/short, fire-out detection |
| `firmware/src/cook_session.h/.cpp` | Circular buffer, LittleFS persistence, CSV/JSON export |
| `firmware/src/config_manager.h/.cpp` | JSON config load/save, factory reset |
| `firmware/src/wifi_manager.h/.cpp` | WiFiManager captive portal, mDNS, auto-reconnect |
| `firmware/src/ota_manager.h/.cpp` | ElegantOTA integration |
| `firmware/platformio.ini` | Build configuration, library dependencies |

## Test Plan

- [x] PID controller computes correct output for given error (test_pid.cpp, 18 tests)
- [x] Lid-open detection triggers at 6% drop, recovers at 2% (test_pid.cpp)
- [x] Steinhart-Hart conversion matches known resistance/temperature pairs (test_temp_conversion.cpp, 22 tests)
- [x] Fan kick-start activates at 75% for 500ms on 0→non-zero transition (test_fan_logic.cpp, 20 tests)
- [x] Fan min-speed clamping at 15% (test_fan_logic.cpp)
- [x] Alarm triggers on pit deviation beyond band after setpoint reached (test_alarm.cpp, 32 tests)
- [x] Meat alarm triggers once at target, hysteresis prevents re-trigger (test_alarm.cpp)
- [x] Session circular buffer wraps correctly at 600 samples (test_session.cpp, 27 tests)
- [x] Session CSV/JSON export produces valid output (test_session.cpp)
- [x] Desktop tests pass (`pio test -e native`)
- [x] Firmware builds (`pio run -e wt32_sc01_plus`)
