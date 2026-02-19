# 001: Hello World — Version Bump

**Branch**: `feature/hello-world`
**Created**: 2026-02-18

## Summary

Minimal first feature to validate the `/feature` → implement → `/ship` workflow. Bumps the firmware version string from `"dev"` to `"0.1.0"`.

## Requirements

- Change `FIRMWARE_VERSION` from `"dev"` to `"0.1.0"` in `firmware/src/config.h`
- No other changes needed

## Design

Single-line edit in the `#define FIRMWARE_VERSION` macro. This constant is used by the OTA update page and the web UI footer to display the running firmware version.

## Files to Modify

| File | Change |
|------|--------|
| `firmware/src/config.h` | Change `#define FIRMWARE_VERSION "dev"` to `#define FIRMWARE_VERSION "0.1.0"` (line ~106) |

## Test Plan

- [ ] `firmware/src/config.h` contains `#define FIRMWARE_VERSION "0.1.0"`
- [ ] Firmware builds: `pio run -e wt32_sc01_plus`
- [ ] Simulator builds: `pio run -e simulator`
- [ ] Desktop tests pass: `pio test -e native`
