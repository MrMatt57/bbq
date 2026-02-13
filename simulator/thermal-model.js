// Simplified charcoal smoker physics simulation.
// Produces realistic-looking temperature curves for BBQ cooking simulation
// using thermal mass, Newton's law of cooling/heating, and event injection.

class ThermalModel {
  constructor(profile) {
    // Core temperatures
    this.pitTemp = profile.initialPitTemp || 70;
    this.meat1Temp = profile.meat1Start || 40;
    this.meat2Temp = profile.meat2Start || 40;
    this.ambientTemp = profile.initialPitTemp || 70;
    this.setpoint = profile.targetPitTemp || 225;

    // Actuator state (output of the simulated PID)
    this.fanPercent = 0;
    this.damperPercent = 0;

    // Fire model
    this.fireEnergy = 1.0;          // 1.0 = fully lit, decays over time
    this.fireDecayRate = 0.000003;  // per second of sim time — very slow natural decay
    this.fireOut = false;

    // Lid state
    this.lidOpen = false;
    this.lidOpenTimer = 0;          // seconds remaining for lid-open event
    this.lidDropApplied = false;
    this.preLidPitTemp = 0;

    // Probe states (null means disconnected)
    this.meat1Connected = true;
    this.meat2Connected = true;

    // Error tracking
    this.errors = [];

    // Stall model (brisket evaporative cooling)
    this.stallEnabled = profile.stallEnabled || false;
    this.stallTempLow = profile.stallTempLow || 150;
    this.stallTempHigh = profile.stallTempHigh || 170;
    this.stallDurationHours = profile.stallDurationHours || 4;
    this.stallTimeAccumulated = 0;  // sim seconds spent in stall zone
    this.stallDurationSeconds = (this.stallDurationHours || 4) * 3600;
    this.inStall = false;

    // Timing
    this.simTime = 0;               // total simulated seconds elapsed

    // Profile events (will be processed as sim time advances)
    this.events = (profile.events || []).map(e => ({ ...e, fired: false }));

    // PID simulation state for pit temperature control
    this.pidIntegral = 0;
    this.pidPrevError = 0;

    // Noise seeds (for realistic variation)
    this._noisePhase = Math.random() * Math.PI * 2;

    // Track initial ramp — overshoot behavior
    this.hasReachedSetpoint = false;
    this.overshootRemaining = 0;
  }

  /**
   * Advance the simulation by dt seconds of simulated time.
   * fanPercent and damperPercent are computed internally by a simplified PID.
   * The caller passes the setpoint; the model manages the rest.
   */
  update(dt, fanPercentOverride, damperPercentOverride, setpoint) {
    if (setpoint !== undefined) {
      this.setpoint = setpoint;
    }

    this.simTime += dt;

    // --- Process scheduled profile events ---
    this._processEvents();

    // --- Lid open timer ---
    if (this.lidOpenTimer > 0) {
      this.lidOpenTimer -= dt;
      if (this.lidOpenTimer <= 0) {
        this.lidOpen = false;
        this.lidOpenTimer = 0;
        this.lidDropApplied = false;
      }
    }

    // --- Simplified PID to compute fan/damper from pit error ---
    const pidOutput = this._computePID(dt);
    this.fanPercent = Math.max(0, Math.min(100, pidOutput));
    this.damperPercent = Math.max(0, Math.min(100, pidOutput));

    // If fire is out, fan runs at 100% (controller tries to recover) but has no effect
    if (this.fireOut) {
      this.fanPercent = 100;
      this.damperPercent = 100;
    }

    // --- Fire energy model ---
    if (this.fireOut) {
      // Rapid decay once fire is out
      this.fireEnergy = Math.max(0, this.fireEnergy - 0.0005 * dt);
    } else {
      // Slow natural decay
      this.fireEnergy = Math.max(0.05, this.fireEnergy - this.fireDecayRate * dt);
    }

    // --- Pit temperature model ---
    this._updatePitTemp(dt);

    // --- Meat temperature model ---
    this._updateMeatTemps(dt);

    // --- Build error list ---
    this.errors = [];
    if (this.fireOut) {
      this.errors.push('fire_out');
    }
    if (!this.meat1Connected) {
      this.errors.push('meat1_disconnect');
    }
    if (!this.meat2Connected) {
      this.errors.push('meat2_disconnect');
    }

    return {
      pitTemp: this._addNoise(this.pitTemp, 0.8),
      meat1Temp: this.meat1Connected ? this._addNoise(this.meat1Temp, 0.3) : null,
      meat2Temp: this.meat2Connected ? this._addNoise(this.meat2Temp, 0.3) : null,
      fanPercent: Math.round(this.fanPercent),
      damperPercent: Math.round(this.damperPercent),
      lidOpen: this.lidOpen,
      errors: [...this.errors]
    };
  }

  // --- Private methods ---

  /**
   * Simplified PID controller for the simulated pit temperature.
   * This mimics what the real firmware does, but simplified.
   */
  _computePID(dt) {
    const Kp = 4.0;
    const Ki = 0.04;
    const Kd = 3.0;

    const error = this.setpoint - this.pitTemp;

    // Integral with anti-windup (generous clamp so integral can eliminate steady-state error)
    this.pidIntegral += error * dt;
    this.pidIntegral = Math.max(-2000, Math.min(2000, this.pidIntegral));

    // Derivative
    const derivative = dt > 0 ? (error - this.pidPrevError) / dt : 0;
    this.pidPrevError = error;

    let output = Kp * error + Ki * this.pidIntegral + Kd * derivative;

    // If lid is open, back off — real controller detects lid open
    if (this.lidOpen) {
      output = 0;
    }

    return Math.max(0, Math.min(100, output));
  }

  /**
   * Update pit temperature based on fire energy, airflow, and thermal mass.
   */
  _updatePitTemp(dt) {
    const PIT_TAU = 300; // thermal time constant in seconds

    // Lid open: rapid heat loss
    if (this.lidOpen) {
      if (!this.lidDropApplied) {
        this.preLidPitTemp = this.pitTemp;
        this.lidDropApplied = true;
      }
      const targetTemp = this.ambientTemp + 20;
      const lidTau = 60; // fast heat loss with lid open
      const alpha = 1 - Math.exp(-dt / lidTau);
      this.pitTemp += (targetTemp - this.pitTemp) * alpha;
      return;
    }

    // Fire limits the max achievable pit temperature
    const maxFireTemp = 400;
    const maxAchievable = this.ambientTemp + (maxFireTemp - this.ambientTemp) * this.fireEnergy;

    // Pit approaches the setpoint directly, capped by what the fire can achieve.
    // The PID still runs separately to produce realistic fan/damper display values.
    const targetTemp = Math.min(this.setpoint, maxAchievable);

    // Normal exponential approach to target
    const alpha = 1 - Math.exp(-dt / PIT_TAU);
    this.pitTemp += (targetTemp - this.pitTemp) * alpha;

    // Overshoot on initial ramp-up
    if (!this.hasReachedSetpoint && this.pitTemp >= this.setpoint * 0.95) {
      this.hasReachedSetpoint = true;
      // Inject some overshoot energy
      this.overshootRemaining = (this.setpoint - this.ambientTemp) * 0.05;
    }
    if (this.overshootRemaining > 0) {
      const overshootDecay = 1 - Math.exp(-dt / 180);
      const applied = this.overshootRemaining * overshootDecay;
      this.pitTemp += applied;
      this.overshootRemaining -= applied;
      if (this.overshootRemaining < 0.5) {
        this.overshootRemaining = 0;
      }
    }

    // Natural cooling toward ambient when fire energy is very low
    if (this.fireEnergy < 0.1) {
      const coolingAlpha = 1 - Math.exp(-dt / 600);
      this.pitTemp += (this.ambientTemp - this.pitTemp) * coolingAlpha;
    }
  }

  /**
   * Update meat temperatures using Newton's law of heating.
   * Meat is a large thermal mass that slowly absorbs heat from the pit.
   */
  _updateMeatTemps(dt) {
    const MEAT_TAU = 1800; // thermal time constant for meat (slow)

    // --- Meat 1 ---
    if (this.meat1Connected) {
      let meat1Alpha = 1 - Math.exp(-dt / MEAT_TAU);

      // Stall model: when in the stall zone, dramatically reduce the heating rate
      if (this.stallEnabled) {
        if (this.meat1Temp >= this.stallTempLow && this.stallTimeAccumulated < this.stallDurationSeconds) {
          this.inStall = true;
          this.stallTimeAccumulated += dt;

          // During stall, evaporative cooling nearly cancels heat input.
          // The meat temp creeps up very slowly or even plateaus.
          const stallProgress = this.stallTimeAccumulated / this.stallDurationSeconds;
          // Stall factor: starts near 0 (strong stall), gradually releases toward 1
          // Uses a sigmoid-like curve so the stall breaks gradually, not abruptly
          const stallFactor = 0.02 + 0.98 * Math.pow(stallProgress, 3);
          meat1Alpha *= stallFactor;

          // Clamp meat temp within the stall band during deep stall
          if (stallProgress < 0.5 && this.meat1Temp > this.stallTempHigh) {
            this.meat1Temp = this.stallTempHigh;
          }
        } else if (this.stallTimeAccumulated >= this.stallDurationSeconds) {
          this.inStall = false;
        }
      }

      // Heat transfer: meat approaches pit temp
      const meat1Delta = (this.pitTemp - this.meat1Temp) * meat1Alpha;
      this.meat1Temp += meat1Delta;
    }

    // --- Meat 2 ---
    if (this.meat2Connected) {
      // Meat 2 has slightly different thermal properties (smaller cut, heats faster)
      const meat2Tau = MEAT_TAU * 0.75;
      const meat2Alpha = 1 - Math.exp(-dt / meat2Tau);
      const meat2Delta = (this.pitTemp - this.meat2Temp) * meat2Alpha;
      this.meat2Temp += meat2Delta;
    }
  }

  /**
   * Process scheduled profile events based on elapsed simulation time.
   */
  _processEvents() {
    for (const event of this.events) {
      if (event.fired) continue;
      if (this.simTime >= event.time) {
        event.fired = true;
        switch (event.type) {
          case 'setpoint':
            this.setpoint = event.params.sp;
            this.hasReachedSetpoint = false; // allow new overshoot on ramp
            this.pidIntegral = 0;           // reset integral for clean response
            console.log(`[${this._formatTime()}] Event: setpoint changed to ${event.params.sp}°F`);
            break;

          case 'lid-open':
            this.lidOpen = true;
            this.lidOpenTimer = event.params.duration || 60;
            console.log(`[${this._formatTime()}] Event: lid opened for ${this.lidOpenTimer}s`);
            break;

          case 'fire-out':
            this.fireOut = true;
            console.log(`[${this._formatTime()}] Event: fire out!`);
            break;

          case 'probe-disconnect':
            if (event.params.probe === 'meat1') {
              this.meat1Connected = false;
              console.log(`[${this._formatTime()}] Event: meat1 probe disconnected`);
            } else if (event.params.probe === 'meat2') {
              this.meat2Connected = false;
              console.log(`[${this._formatTime()}] Event: meat2 probe disconnected`);
            }
            break;

          default:
            console.log(`[${this._formatTime()}] Unknown event type: ${event.type}`);
        }
      }
    }
  }

  /**
   * Add small realistic noise to a temperature reading.
   */
  _addNoise(temp, magnitude) {
    this._noisePhase += 0.01;
    const noise = Math.sin(this._noisePhase * 7.3) * magnitude * 0.5
                + Math.sin(this._noisePhase * 13.1) * magnitude * 0.3
                + (Math.random() - 0.5) * magnitude * 0.4;
    return Math.round((temp + noise) * 10) / 10;
  }

  /**
   * Format simulated time as HH:MM:SS for log messages.
   */
  _formatTime() {
    const h = Math.floor(this.simTime / 3600);
    const m = Math.floor((this.simTime % 3600) / 60);
    const s = Math.floor(this.simTime % 60);
    return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
  }

  /**
   * Serialize model state for persistence across restarts.
   */
  serialize() {
    return {
      pitTemp: this.pitTemp,
      meat1Temp: this.meat1Temp,
      meat2Temp: this.meat2Temp,
      ambientTemp: this.ambientTemp,
      setpoint: this.setpoint,
      fanPercent: this.fanPercent,
      damperPercent: this.damperPercent,
      fireEnergy: this.fireEnergy,
      fireOut: this.fireOut,
      lidOpen: this.lidOpen,
      lidOpenTimer: this.lidOpenTimer,
      lidDropApplied: this.lidDropApplied,
      preLidPitTemp: this.preLidPitTemp,
      meat1Connected: this.meat1Connected,
      meat2Connected: this.meat2Connected,
      stallEnabled: this.stallEnabled,
      stallTimeAccumulated: this.stallTimeAccumulated,
      stallDurationSeconds: this.stallDurationSeconds,
      inStall: this.inStall,
      simTime: this.simTime,
      pidIntegral: this.pidIntegral,
      pidPrevError: this.pidPrevError,
      hasReachedSetpoint: this.hasReachedSetpoint,
      overshootRemaining: this.overshootRemaining,
      events: this.events,
      _noisePhase: this._noisePhase
    };
  }

  /**
   * Restore model state from a serialized object.
   */
  deserialize(s) {
    Object.assign(this, s);
  }

  /**
   * Reset the model to initial conditions (for new session).
   */
  reset(profile) {
    this.pitTemp = profile.initialPitTemp || 70;
    this.meat1Temp = profile.meat1Start || 40;
    this.meat2Temp = profile.meat2Start || 40;
    this.setpoint = profile.targetPitTemp || 225;
    this.fireEnergy = 1.0;
    this.fireOut = false;
    this.lidOpen = false;
    this.lidOpenTimer = 0;
    this.lidDropApplied = false;
    this.meat1Connected = true;
    this.meat2Connected = true;
    this.errors = [];
    this.stallTimeAccumulated = 0;
    this.inStall = false;
    this.simTime = 0;
    this.pidIntegral = 0;
    this.pidPrevError = 0;
    this.hasReachedSetpoint = false;
    this.overshootRemaining = 0;
    this.events = (profile.events || []).map(e => ({ ...e, fired: false }));
  }
}

module.exports = ThermalModel;
