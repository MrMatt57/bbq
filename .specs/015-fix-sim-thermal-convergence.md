# 015: Fix Simulator Thermal Model Pit Temperature Convergence

**Branch**: `feature/fix-sim-thermal-convergence`
**Created**: 2026-02-18

## Summary

The simulator's pit temperature never reaches the setpoint (225°F) — it hovers around 216-217°F. The root cause is that `maxAchievable` in `updatePitTemp()` depends on instantaneous airflow, which drops when the PID backs off near setpoint, creating a self-defeating feedback loop. The fix decouples `maxAchievable` from instantaneous airflow by making `fireEnergy` a stateful variable that responds to airflow over time, giving the charcoal fire realistic thermal mass.

## Requirements

- Pit temperature must converge to the setpoint (e.g., 225°F) during a normal cook simulation
- Fire energy should be replenished by airflow (oxygen feeds coals) and consumed by natural decay
- With damper closed, fire should barely sustain (minimal air leakage) — not die instantly, not grow
- Fire-out profile must still work (rapid fire energy decay when `fireOut` flag is set)
- All existing cook profiles (normal, stall, hot-fast, temp-change, lid-open, fire-out, probe-disconnect) must produce realistic behavior
- No changes to the firmware PID, split-range, or any non-simulator code

## Design

### Current Bug (detailed trace)

In `fan_and_damper` mode, when PID output settles around 76% (integral maxed at 2000):
- `damperPct = 76%`, `fanPct ≈ 65.7%`
- `airflow = 0.76 * max(0.15, 0.657) = 0.76 * 0.657 ≈ 0.499`
- `maxAchievable = 70 + 330 * 0.89 * 0.499 ≈ 216.5°F`

The pit asymptotically approaches 216.5°F but can never exceed it.

### Fix: Airflow-Driven Fire Energy Model

**Key insight**: In a real charcoal smoker, the fire has thermal mass. Closing the damper doesn't instantly cool the fire — it slowly starves it of oxygen. The model should reflect this by making `fireEnergy` respond to airflow over time, not having airflow directly cap temperature.

**Changes to `update()` method** — Replace the fire energy decay block with an airflow-aware model:

```cpp
// Compute effective airflow for fire energy model
{
    float naturalDraft = 0.15f;
    float damperOpen = damperPercent / 100.0f;
    float fanFlow = fanPercent / 100.0f;
    float effectiveAirflow = damperOpen * fmaxf(naturalDraft, fanFlow);

    if (fireOut) {
        fireEnergy = fmaxf(0, fireEnergy - 0.0005f * dt);
    } else {
        // Minimal air leakage keeps coals barely smoldering even with damper closed
        float airflow = fmaxf(effectiveAirflow, 0.03f);
        float replenishRate = 0.0001f;
        float netChange = (airflow * replenishRate - fireDecayRate_) * dt;
        fireEnergy = fmaxf(0.05f, fminf(1.0f, fireEnergy + netChange));
    }
}
```

**Changes to `updatePitTemp()` method** — Remove airflow calculation, use fireEnergy directly:

```cpp
float maxFireTemp = 400.0f;
float maxAchievable = ambientTemp + (maxFireTemp - ambientTemp) * fireEnergy;
```

### Tuning Verification

- `fireDecayRate_ = 0.000003f`, `replenishRate = 0.0001f`
- Equilibrium airflow: `0.000003 / 0.0001 = 0.03` (matches minLeakage)
- At damper closed (PID=0): airflow clamped to 0.03 → exact equilibrium, fire barely sustains
- At PID ~30%: airflow ≈ 0.045 → slow fire energy gain
- At PID 100%: airflow = 1.0 → fire energy maxes quickly
- With fireEnergy = 1.0: `maxAchievable = 70 + 330 = 400°F` (well above 225)
- Fire can sustain setpoint as long as `fireEnergy > 0.47`

### Profile Impact

| Profile | Expected behavior |
|---------|-------------------|
| normal | Pit converges to 225°F (FIXED) |
| stall | Pit converges to 225°F, meat stalls as before |
| hot-fast | Pit converges to 300°F (maxAchievable = 400 > 300) |
| temp-change | Pit reaches 225, then 275 after setpoint change |
| lid-open | Pit drops on lid open, recovers after close; fire energy stable |
| fire-out | Fire energy decays rapidly via existing `fireOut` path |
| probe-disconnect | Pit behavior unchanged, probe disconnect still works |

## Files to Modify

| File | Change |
|------|--------|
| `firmware/src/simulator/sim_thermal.cpp` | Replace fire energy decay in `update()` with airflow-driven model; simplify `maxAchievable` in `updatePitTemp()` to use `fireEnergy` only |

## Test Plan

- [x] Simulator builds: `pio run -e simulator`
- [ ] Run simulator with normal profile: pit temp converges to 225°F (not stuck at 216-217)
- [ ] Run simulator with hot-fast profile: pit temp converges to 300°F
- [x] Firmware builds: `pio run -e wt32_sc01_plus`
- [x] Desktop tests pass: `pio test -e native` (130/130 passed)
