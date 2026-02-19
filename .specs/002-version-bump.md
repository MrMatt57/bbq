# 002: Firmware Version Bump

**Branch**: `feature/version-bump`
**Created**: 2026-02-18

## Summary

Bump the firmware version string from `"0.1.0"` to `"0.2.0"` to reflect the updated lifecycle commands and workflow improvements.

## Requirements

- Change `FIRMWARE_VERSION` from `"0.1.0"` to `"0.2.0"` in `firmware/src/config.h`

## Design

Single-line edit in the `#define FIRMWARE_VERSION` macro (line ~106 of `firmware/src/config.h`).

## Files to Modify

| File | Change |
|------|--------|
| `firmware/src/config.h` | Change `#define FIRMWARE_VERSION "0.1.0"` to `#define FIRMWARE_VERSION "0.2.0"` |

## Test Plan

- [ ] `firmware/src/config.h` contains `#define FIRMWARE_VERSION "0.2.0"`
