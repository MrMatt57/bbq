# 011: Cleanup Simulator Builds

**Branch**: `feature/cleanup-simulator-builds`
**Created**: 2026-02-18

## Summary

Remove the deprecated Node.js simulator (`simulator/`), add the C++ PIO simulator build to CI, and extract duplicated logic (split-range fan/damper coordination, unit conversion) into shared headers so the firmware and simulator share a single source of truth.

## Requirements

- Delete the deprecated `simulator/` directory (Node.js Express/WS simulator) entirely
- Replace the CI `simulator` job (which runs `npm ci` on the deprecated Node.js sim) with a job that builds the C++ PIO simulator (`pio run -e simulator`)
- The CI simulator job must install `libsdl2-dev` on Ubuntu before building
- Extract the split-range fan/damper coordination logic into a shared header (`split_range.h`) that both `main.cpp` and `sim_thermal.cpp` call
- Extract unit conversion (C↔F) into a shared header (`units.h`) that both firmware and simulator can use
- All existing tests must continue to pass (`pio test -e native`)
- Firmware build must continue to succeed (`pio run -e wt32_sc01_plus`)
- Update `.gitignore` if needed (remove `simulator/node_modules/` patterns, `simulator/session.dat`, etc.)

## Design

### 1. Remove Node.js Simulator

Delete the entire `simulator/` directory at the repo root. This contains:
- `server.js`, `thermal-model.js`, `profiles.js` — the deprecated Node.js simulator
- `package.json`, `package-lock.json` — npm dependencies
- `node_modules/` — installed packages
- `session.dat` — runtime state file

### 2. Update CI

In `.github/workflows/ci.yml`, replace the current `simulator` job:

**Current** (deprecated Node.js sim):
```yaml
simulator:
  name: Simulator Dependencies
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - uses: actions/setup-node@v4
      with:
        node-version: 20
        cache: npm
        cache-dependency-path: simulator/package-lock.json
    - name: Install dependencies
      run: npm ci
      working-directory: simulator
```

**New** (C++ PIO simulator):
```yaml
simulator:
  name: Simulator Build
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - uses: actions/setup-python@v5
      with:
        python-version: "3.12"
    - name: Install PlatformIO
      run: pip install platformio
    - name: Install SDL2
      run: sudo apt-get update && sudo apt-get install -y libsdl2-dev
    - name: Cache PlatformIO
      uses: actions/cache@v4
      with:
        path: |
          ~/.platformio
          .pio
        key: pio-sim-${{ hashFiles('firmware/platformio.ini') }}
        restore-keys: pio-sim-
    - name: Build simulator
      run: pio run -e simulator
      working-directory: firmware
```

### 3. Extract Split-Range Logic → `split_range.h`

Create `firmware/src/split_range.h` as a header-only utility:

```cpp
#pragma once
#include <cmath>
#include <cstring>

struct SplitRangeOutput {
    float fanPercent;
    float damperPercent;
};

inline SplitRangeOutput splitRange(float pidOutput, const char* fanMode, float fanOnThreshold) {
    float fanPct = 0.0f;
    float damperPct = 0.0f;

    if (strcmp(fanMode, "fan_only") == 0) {
        fanPct = pidOutput;
        damperPct = 100.0f;
    } else if (strcmp(fanMode, "damper_primary") == 0) {
        float dpThreshold = fmaxf(fanOnThreshold, 50.0f);
        damperPct = pidOutput;
        if (pidOutput > dpThreshold) {
            fanPct = (pidOutput - dpThreshold) / (100.0f - dpThreshold) * 100.0f;
            damperPct = 100.0f;
        }
    } else {
        // fan_and_damper (default)
        damperPct = pidOutput;
        if (pidOutput > fanOnThreshold) {
            fanPct = (pidOutput - fanOnThreshold) / (100.0f - fanOnThreshold) * 100.0f;
        }
    }

    return { fanPct, damperPct };
}
```

Then update `main.cpp` (lines ~296-335) and `sim_thermal.cpp` (lines ~81-104) to call `splitRange()` instead of inlining the logic.

### 4. Extract Unit Conversion → `units.h`

Create `firmware/src/units.h`:

```cpp
#pragma once

inline float celsiusToFahrenheit(float c) { return c * 9.0f / 5.0f + 32.0f; }
inline float fahrenheitToCelsius(float f) { return (f - 32.0f) * 5.0f / 9.0f; }
```

Update `temp_manager.h` (which has `cToF`) and `sim_main.cpp` (which has `f_to_c`) to use these shared functions.

### 5. Update .gitignore

Remove any `simulator/`-specific ignore patterns (e.g., `simulator/node_modules/`, `simulator/session.dat`) since the directory is being deleted.

## Files to Modify

| File | Change |
|------|--------|
| `simulator/` (directory) | **DELETE** — entire deprecated Node.js simulator |
| `.github/workflows/ci.yml` | Replace Node.js simulator job with C++ PIO simulator build |
| `firmware/src/split_range.h` | **CREATE** — shared split-range fan/damper coordination |
| `firmware/src/units.h` | **CREATE** — shared C↔F unit conversion |
| `firmware/src/main.cpp` | Replace inline split-range logic with `splitRange()` call |
| `firmware/src/simulator/sim_thermal.cpp` | Replace inline split-range logic with `splitRange()` call |
| `firmware/src/temp_manager.h` | Replace `cToF()` with `#include "units.h"` usage |
| `firmware/src/simulator/sim_main.cpp` | Replace `f_to_c()` with `#include "units.h"` usage |
| `.gitignore` | Remove `simulator/`-specific patterns if present |

## Test Plan

- [x] Deprecated `simulator/` directory is fully removed
- [x] Desktop tests pass (`pio test -e native`) — 130/130 passed
- [x] Firmware builds (`pio run -e wt32_sc01_plus`)
- [x] Simulator builds (`pio run -e simulator`) — verifies split_range.h and units.h compile correctly in both environments
- [x] CI workflow updated (no Node.js references, simulator job builds C++ PIO simulator)
- [x] `split_range.h` used by both `main.cpp` and `sim_thermal.cpp`
- [x] `units.h` used by both `temp_manager.h` and `sim_main.cpp`
- [x] No remaining references to the Node.js simulator in the codebase
