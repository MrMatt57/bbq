// BBQ Simulator Server
// Express serves the web UI from firmware/data/, WebSocket sends simulated
// temperature data using the same protocol as the real ESP32 firmware.

const fs = require('fs');
const http = require('http');
const path = require('path');
const express = require('express');
const { WebSocketServer } = require('ws');
const ThermalModel = require('./thermal-model');
const profiles = require('./profiles');

// Session persistence file (.dat so nodemon ignores it — it only watches .js/.json)
const SESSION_FILE = path.join(__dirname, 'session.dat');

// ---------------------------------------------------------------------------
// CLI argument parsing (no extra dependencies)
// ---------------------------------------------------------------------------
function parseArgs(argv) {
  const args = {
    speed: parseFloat(process.env.SPEED) || 1,
    profile: process.env.PROFILE || 'normal'
  };
  for (let i = 2; i < argv.length; i++) {
    if (argv[i] === '--speed' && argv[i + 1]) {
      args.speed = parseFloat(argv[++i]) || 1;
    } else if (argv[i] === '--profile' && argv[i + 1]) {
      args.profile = argv[++i];
    }
  }
  return args;
}

const args = parseArgs(process.argv);
const PORT = parseInt(process.env.PORT, 10) || 3000;
const SPEED = Math.max(0.1, args.speed);
const PROFILE_NAME = args.profile;

// Validate profile
if (!profiles[PROFILE_NAME]) {
  console.error(`Unknown profile: "${PROFILE_NAME}"`);
  console.error(`Available profiles: ${Object.keys(profiles).join(', ')}`);
  process.exit(1);
}

const profile = profiles[PROFILE_NAME];

// ---------------------------------------------------------------------------
// Express app — serve static web UI files from firmware/data/
// ---------------------------------------------------------------------------
const app = express();
const dataDir = path.resolve(__dirname, '..', 'firmware', 'data');
app.use(express.static(dataDir));

const server = http.createServer(app);

// ---------------------------------------------------------------------------
// Thermal model
// ---------------------------------------------------------------------------
let model = new ThermalModel(profile);

// Session data: array of {ts, pit, meat1, meat2, fan, damper, sp, lid} objects
let sessionData = [];
let sessionStartTs = Math.floor(Date.now() / 1000);

// Current setpoint (can be changed by client)
let currentSetpoint = profile.targetPitTemp;

// Alarm targets (tracked for client reference; model doesn't use these)
let alarmTargets = {
  meat1Target: profile.meat1Target || null,
  meat2Target: profile.meat2Target || null
};

// ---------------------------------------------------------------------------
// Session persistence (survives nodemon restarts)
// ---------------------------------------------------------------------------
function saveSession() {
  const state = {
    sessionData,
    sessionStartTs,
    currentSetpoint,
    alarmTargets,
    modelState: model.serialize()
  };
  try {
    fs.writeFileSync(SESSION_FILE, JSON.stringify(state));
  } catch (e) {
    // Silently ignore write errors
  }
}

function restoreSession() {
  try {
    if (!fs.existsSync(SESSION_FILE)) return false;
    const raw = fs.readFileSync(SESSION_FILE, 'utf8');
    const state = JSON.parse(raw);
    sessionData = state.sessionData || [];
    sessionStartTs = state.sessionStartTs || sessionStartTs;
    currentSetpoint = state.currentSetpoint || currentSetpoint;
    if (state.alarmTargets) alarmTargets = state.alarmTargets;
    if (state.modelState) model.deserialize(state.modelState);
    console.log(`  Restored session: ${sessionData.length} data points`);
    return true;
  } catch (e) {
    console.log('  No session to restore (starting fresh)');
    return false;
  }
}

// ---------------------------------------------------------------------------
// Estimated done time calculation
// Uses simple linear extrapolation from recent meat1 temperature rise rate.
// ---------------------------------------------------------------------------
function estimateDoneTime() {
  if (!alarmTargets.meat1Target) return null;

  // Need at least 2 data points spanning some time
  const recent = sessionData.slice(-60); // last ~60 data points
  if (recent.length < 10) return null;

  const first = recent[0];
  const last = recent[recent.length - 1];

  if (first.meat1 === null || last.meat1 === null) return null;

  const tempRate = (last.meat1 - first.meat1) / (last.ts - first.ts); // degrees per second
  if (tempRate <= 0.00001) return null; // not rising, can't estimate

  const remaining = alarmTargets.meat1Target - last.meat1;
  if (remaining <= 0) return last.ts; // already done

  const secondsRemaining = remaining / tempRate;
  return Math.floor(last.ts + secondsRemaining);
}

// ---------------------------------------------------------------------------
// WebSocket server
// ---------------------------------------------------------------------------
const wss = new WebSocketServer({ server });

// Broadcast a message to all connected clients
function broadcast(data) {
  const msg = JSON.stringify(data);
  wss.clients.forEach(client => {
    if (client.readyState === 1) { // WebSocket.OPEN
      client.send(msg);
    }
  });
}

// Handle incoming messages from clients
function handleMessage(ws, raw) {
  let msg;
  try {
    msg = JSON.parse(raw);
  } catch (e) {
    console.error('Invalid JSON from client:', raw);
    return;
  }

  switch (msg.type) {
    case 'set':
      if (msg.sp !== undefined) {
        currentSetpoint = msg.sp;
        model.setpoint = msg.sp;
        model.hasReachedSetpoint = false;
        model.pidIntegral = 0;
        console.log(`Client set setpoint to ${msg.sp}°F`);
      }
      break;

    case 'alarm':
      if (msg.meat1Target !== undefined) {
        alarmTargets.meat1Target = msg.meat1Target;
        console.log(`Client set meat1 alarm target to ${msg.meat1Target}°F`);
      }
      if (msg.meat2Target !== undefined) {
        alarmTargets.meat2Target = msg.meat2Target;
        console.log(`Client set meat2 alarm target to ${msg.meat2Target}°F`);
      }
      break;

    case 'session':
      if (msg.action === 'new') {
        console.log('Client requested new session — resetting model');
        model.reset(profile);
        sessionData = [];
        sessionStartTs = Math.floor(Date.now() / 1000);
        currentSetpoint = profile.targetPitTemp;
        try { fs.unlinkSync(SESSION_FILE); } catch (e) {}
        // Notify all clients that the session has been reset
        broadcast({ type: 'session', action: 'reset', sp: currentSetpoint });
      } else if (msg.action === 'download' && msg.format === 'csv') {
        const csv = generateCSV();
        ws.send(JSON.stringify({
          type: 'session',
          action: 'download',
          format: 'csv',
          data: csv
        }));
        console.log(`Sent CSV download (${sessionData.length} data points)`);
      }
      break;

    default:
      console.log('Unknown message type:', msg.type);
  }
}

// Generate CSV from session data
function generateCSV() {
  const header = 'timestamp,pit,meat1,meat2,fan,damper,setpoint,lid\n';
  const rows = sessionData.map(d =>
    `${d.ts},${d.pit},${d.meat1 !== null ? d.meat1 : ''},${d.meat2 !== null ? d.meat2 : ''},${d.fan},${d.damper},${d.sp},${d.lid}`
  ).join('\n');
  return header + rows;
}

wss.on('connection', (ws) => {
  console.log('WebSocket client connected');

  // Send session history so the client can populate the chart on refresh
  if (sessionData.length > 0) {
    ws.send(JSON.stringify({
      type: 'history',
      data: sessionData,
      sp: currentSetpoint,
      meat1Target: alarmTargets.meat1Target,
      meat2Target: alarmTargets.meat2Target
    }));
    console.log(`Sent ${sessionData.length} history points to new client`);
  }

  ws.on('message', (raw) => handleMessage(ws, raw.toString()));
  ws.on('close', () => console.log('WebSocket client disconnected'));
  ws.on('error', (err) => console.error('WebSocket error:', err.message));
});

// ---------------------------------------------------------------------------
// Simulation loop
// Runs at real-time intervals (1-2 seconds), but advances simulation time
// by dt * SPEED each tick.
// ---------------------------------------------------------------------------
const REAL_INTERVAL_MS = 1000; // real-time interval: 1 second
const SIM_DT = 5;             // simulated seconds per tick at 1x speed

let simInterval = null;

function startSimulation() {
  simInterval = setInterval(() => {
    const dt = SIM_DT * SPEED;

    // Advance the thermal model
    const state = model.update(dt, null, null, currentSetpoint);

    // Build the data message (same protocol as ESP32 firmware)
    const ts = sessionStartTs + Math.floor(model.simTime);
    const dataPoint = {
      ts,
      pit: state.pitTemp,
      meat1: state.meat1Temp,
      meat2: state.meat2Temp,
      fan: state.fanPercent,
      damper: state.damperPercent,
      sp: currentSetpoint,
      lid: state.lidOpen
    };

    // Store in session history
    sessionData.push(dataPoint);

    // Persist every 30 data points (~30s real time)
    if (sessionData.length % 30 === 0) saveSession();

    // Compute estimated done time
    const est = estimateDoneTime();

    // Build and broadcast the message
    const msg = {
      type: 'data',
      ts: dataPoint.ts,
      pit: dataPoint.pit,
      meat1: dataPoint.meat1,
      meat2: dataPoint.meat2,
      fan: dataPoint.fan,
      damper: dataPoint.damper,
      sp: dataPoint.sp,
      lid: dataPoint.lid,
      est: est,
      errors: state.errors
    };

    broadcast(msg);
  }, REAL_INTERVAL_MS);
}

// ---------------------------------------------------------------------------
// Start server
// ---------------------------------------------------------------------------
server.listen(PORT, () => {
  console.log('');
  console.log('=== BBQ Simulator ===');
  console.log(`  Port:    ${PORT}`);
  console.log(`  Profile: ${PROFILE_NAME} — ${profile.description}`);
  console.log(`  Speed:   ${SPEED}x (${SIM_DT * SPEED} simulated seconds per tick)`);
  console.log(`  Serving: ${dataDir}`);
  console.log('');
  console.log(`  Open http://localhost:${PORT} in your browser`);
  console.log('');

  restoreSession();
  startSimulation();
});

// Graceful shutdown
process.on('SIGINT', () => {
  console.log('\nShutting down simulator...');
  saveSession();
  clearInterval(simInterval);
  wss.close();
  server.close();
  process.exit(0);
});

process.on('SIGTERM', () => {
  saveSession();
  clearInterval(simInterval);
  wss.close();
  server.close();
  process.exit(0);
});
