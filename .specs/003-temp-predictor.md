# 003: Temperature Predictor

**Branch**: `feature/mvp-scaffold`
**Created**: 2026-02-18
**Status**: Implemented

## Summary

Rolling linear regression predictor that estimates when each meat probe will reach its target temperature. Collects a 5-minute window of temperature samples, computes the rate of change (°/min), and projects the estimated done time as a UTC epoch timestamp. Used by both the touchscreen UI and web UI to display predicted completion times.

## Requirements

- Per-probe rolling window of 60 samples at 5-second intervals (5 minutes of data)
- Minimum 12 samples (1 minute) required before producing a prediction
- Linear regression slope calculation: rate = (temp[n] - temp[0]) / (ts[n] - ts[0])
- Estimated done time: current_ts + (target - current_temp) / rate
- Predictions capped at 24 hours maximum
- Independent tracking for meat1 and meat2 probes
- Graceful handling of edge cases: insufficient data, decreasing temperature, no target set, probe disconnected
- Reset capability (per-probe or all)

## Design

### Algorithm

```
1. Sample meat temperature every 5 seconds into circular buffer
2. After 12+ samples accumulated:
   a. Compute linear regression slope across window
   b. If slope > 0 and target is set:
      - remaining = target - current_temp
      - time_to_done = remaining / slope
      - est_epoch = now + time_to_done
   c. Clamp to 24h max
3. Return 0 if prediction unavailable
```

### Data Structure

Each probe maintains:
- Circular sample buffer: `{timestamp, temperature}` × 60 slots
- Head pointer and count for circular tracking
- Target temperature (set by user)

### Integration Points

- Called from `main.cpp` loop after temperature readings
- Estimates consumed by `web_server.cpp` (WebSocket broadcast) and `ui_update.cpp` (touchscreen display)
- Targets set via web UI alarm commands or touchscreen controls

## Files to Modify

| File | Change |
|------|--------|
| `firmware/src/temp_predictor.h/.cpp` | Rolling window linear regression predictor |

## Test Plan

- [x] Prediction accuracy with known linear slope (test_temp_predictor.cpp, 14 tests)
- [x] Rolling window slides correctly after buffer fills
- [x] Rate calculation returns correct °/min
- [x] Returns 0 with insufficient samples (<12)
- [x] Returns 0 when temperature is decreasing
- [x] Returns 0 when no target is set
- [x] Handles probe disconnect gracefully
- [x] Two probes operate independently
- [x] Desktop tests pass (`pio test -e native`)
