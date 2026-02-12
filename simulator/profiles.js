// Pre-built cook profiles for the BBQ simulator.
// Each profile defines initial conditions and scheduled events that
// produce realistic temperature curves for different cooking scenarios.

const profiles = {
  normal: {
    name: 'Normal',
    description: '225°F pit, pork butt to 203°F, smooth rise (~10 hr)',
    initialPitTemp: 70,
    targetPitTemp: 225,
    meat1Start: 40,
    meat2Start: 40,
    meat1Target: 203,
    meat2Target: 180,
    events: []
  },

  stall: {
    name: 'Brisket Stall',
    description: '225°F pit, brisket with 150-170°F stall plateau (~14 hr)',
    initialPitTemp: 70,
    targetPitTemp: 225,
    meat1Start: 38,
    meat2Start: 40,
    meat1Target: 203,
    meat2Target: 185,
    // The stall is modeled in thermal-model.js based on this flag.
    // Meat1 will plateau around 155-165°F for several simulated hours
    // before resuming its rise, simulating evaporative cooling.
    stallEnabled: true,
    stallTempLow: 150,
    stallTempHigh: 170,
    stallDurationHours: 4,
    events: []
  },

  'hot-fast': {
    name: 'Hot & Fast',
    description: '300°F pit, chicken to 185°F (~2 hr)',
    initialPitTemp: 70,
    targetPitTemp: 300,
    meat1Start: 40,
    meat2Start: 42,
    meat1Target: 185,
    meat2Target: 185,
    events: []
  },

  'temp-change': {
    name: 'Temperature Change',
    description: 'Start 225°F, bump to 275°F at 4 hr (~8 hr)',
    initialPitTemp: 70,
    targetPitTemp: 225,
    meat1Start: 40,
    meat2Start: 40,
    meat1Target: 203,
    meat2Target: 185,
    events: [
      {
        time: 4 * 3600,       // 4 hours in seconds
        type: 'setpoint',
        params: { sp: 275 }
      }
    ]
  },

  'lid-open': {
    name: 'Lid Opens',
    description: 'Normal cook with lid opens at 2 hr, 5 hr, 8 hr (~10 hr)',
    initialPitTemp: 70,
    targetPitTemp: 225,
    meat1Start: 40,
    meat2Start: 40,
    meat1Target: 203,
    meat2Target: 180,
    events: [
      {
        time: 2 * 3600,       // 2 hours
        type: 'lid-open',
        params: { duration: 60 }   // lid open for 60 seconds
      },
      {
        time: 5 * 3600,       // 5 hours
        type: 'lid-open',
        params: { duration: 90 }
      },
      {
        time: 8 * 3600,       // 8 hours
        type: 'lid-open',
        params: { duration: 60 }
      }
    ]
  },

  'fire-out': {
    name: 'Fire Out',
    description: 'Normal cook, fire dies at 4 hr (~6 hr)',
    initialPitTemp: 70,
    targetPitTemp: 225,
    meat1Start: 40,
    meat2Start: 40,
    meat1Target: 203,
    meat2Target: 180,
    events: [
      {
        time: 4 * 3600,       // 4 hours
        type: 'fire-out',
        params: {}
      }
    ]
  },

  'probe-disconnect': {
    name: 'Probe Disconnect',
    description: 'Normal cook, meat1 probe disconnects at 3 hr (~10 hr)',
    initialPitTemp: 70,
    targetPitTemp: 225,
    meat1Start: 40,
    meat2Start: 40,
    meat1Target: 203,
    meat2Target: 180,
    events: [
      {
        time: 3 * 3600,       // 3 hours
        type: 'probe-disconnect',
        params: { probe: 'meat1' }
      }
    ]
  }
};

module.exports = profiles;
