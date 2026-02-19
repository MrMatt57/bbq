# 008: Test Infrastructure

**Branch**: `feature/mvp-scaffold`
**Created**: 2026-02-18
**Status**: Implemented

## Summary

Comprehensive test suite using the Unity framework with two test environments: desktop (native) for logic-only unit tests that run without hardware, and embedded for on-device integration tests. Desktop tests cover PID control, temperature prediction, alarm logic, fan control state machine, temperature conversion math, and session data management. Embedded tests verify ADC communication, PWM output, servo positioning, buzzer operation, and I2C bus health.

## Requirements

### Desktop Tests (native environment, no hardware)
- **test_pid** (18 tests): PID computation, enabled/disabled states, tuning parameter storage, lid-open detection state machine (6% drop trigger, 2% recovery), repeated lid-open cycles, begin() reset
- **test_temp_predictor** (14 tests): Linear regression accuracy, rolling window behavior, rate calculation (°/min), edge cases (insufficient samples, decreasing temp, no target, probe disconnected), two-probe independence, reset
- **test_alarm** (32 tests): Pit band alarm (triggers only after setpoint reached, ±15°F band), meat target alarm (one-shot), hysteresis (no re-trigger after acknowledge), enable/disable, multiple simultaneous alarms
- **test_fan_logic** (20 tests): Initial state, kick-start detection (75% for 500ms on 0→non-zero), min-speed clamping (15%), long-pulse mode (<10%), speed bounds (0-100%), manual duty override
- **test_temp_conversion** (22 tests): Steinhart-Hart accuracy against known resistance/temperature pairs (33K=124°F, 10K=184°F, 5K=220°F, 3K=257°F), ADC-to-resistance conversion, C-to-F formula, monotonicity, edge cases
- **test_session** (27 tests): DataPoint encoding (int16 × 10 precision), circular buffer wrapping at 600 samples, CSV/JSON generation, session lifecycle

### Embedded Tests (on-device, hardware required)
- **test_adc** (5 tests): ADS1115 I2C device detection, channel read range (0-32767), open-circuit detection (>32000), channel independence, read rate (<100ms for 3 channels)
- **test_fan_pwm** (6 tests): PWM initialization, 0%/100% duty, kick-start activation, min-speed clamping, PWM frequency verification (25 kHz ±10%)
- **test_servo** (5 tests): Initialization at closed position, position accuracy at 0%/50%/100%, sweep monotonicity
- **test_buzzer** (3 tests): Pin configuration, tone generation, noTone() cleanup
- **test_i2c** (3 tests): Wire.begin() init, bus scan (find ADS1115 at 0x48), 10 consecutive read/write cycles

## Design

### Test Architecture

PlatformIO's Unity test framework with two environments:

```ini
[env:native]
platform = native
test_framework = unity
test_filter = test_desktop/*

[env:wt32_sc01_plus]
# Embedded tests run on device via USB
test_filter = test_embedded/*
```

### Desktop Test Strategy

Tests instantiate module classes directly, calling public methods and asserting state. Hardware dependencies (ADC, GPIO, WiFi) are not available on native — tests cover pure logic only. Each test file includes the module's `.cpp` source directly or links against it.

### Embedded Test Strategy

Tests run on the actual ESP32-S3 board via PlatformIO's test runner. Each test verifies hardware communication and output. Prerequisites documented in test file headers (e.g., "ADS1115 must be connected on I2C").

### Test Organization

```
firmware/test/
  test_desktop/
    test_pid/test_pid.cpp              # 18 tests
    test_temp_predictor/test_temp_predictor.cpp  # 14 tests
    test_alarm/test_alarm.cpp          # 32 tests
    test_fan_logic/test_fan_logic.cpp  # 20 tests
    test_temp_conversion/test_temp_conversion.cpp  # 22 tests
    test_session/test_session.cpp      # 27 tests
  test_embedded/
    test_adc/test_adc.cpp              # 5 tests
    test_fan_pwm/test_fan_pwm.cpp      # 6 tests
    test_servo/test_servo.cpp          # 5 tests
    test_buzzer/test_buzzer.cpp        # 3 tests
    test_i2c/test_i2c.cpp             # 3 tests
```

**Total: 133 desktop tests + 22 embedded tests = 155 tests**

## Files to Modify

| File | Change |
|------|--------|
| `firmware/test/test_desktop/test_pid/test_pid.cpp` | PID controller unit tests |
| `firmware/test/test_desktop/test_temp_predictor/test_temp_predictor.cpp` | Temperature predictor unit tests |
| `firmware/test/test_desktop/test_alarm/test_alarm.cpp` | Alarm manager unit tests |
| `firmware/test/test_desktop/test_fan_logic/test_fan_logic.cpp` | Fan controller unit tests |
| `firmware/test/test_desktop/test_temp_conversion/test_temp_conversion.cpp` | Steinhart-Hart conversion tests |
| `firmware/test/test_desktop/test_session/test_session.cpp` | Cook session unit tests |
| `firmware/test/test_embedded/test_adc/test_adc.cpp` | ADS1115 ADC integration tests |
| `firmware/test/test_embedded/test_fan_pwm/test_fan_pwm.cpp` | Fan PWM output tests |
| `firmware/test/test_embedded/test_servo/test_servo.cpp` | Servo positioning tests |
| `firmware/test/test_embedded/test_buzzer/test_buzzer.cpp` | Buzzer GPIO tests |
| `firmware/test/test_embedded/test_i2c/test_i2c.cpp` | I2C bus communication tests |
| `firmware/platformio.ini` | Test environment configuration |

## Test Plan

- [x] All 133 desktop tests pass (`pio test -e native`)
- [x] Desktop tests run in CI (GitHub Actions)
- [x] Test files cover all core firmware modules
- [x] Each test file is self-contained with clear assertions
