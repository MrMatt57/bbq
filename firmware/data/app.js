// BBQ Controller - app.js
// Vanilla JavaScript, no build step required.

(function () {
  'use strict';

  // ---------------------------------------------------------------------------
  // Constants
  // ---------------------------------------------------------------------------
  var WS_MAX_BACKOFF = 30000;
  var DEBOUNCE_MS = 300;
  var CHART_WINDOW_SEC = 2 * 60 * 60; // 2 hours visible window
  var PREDICTION_WINDOW_SEC = 30 * 60; // 30 min of history for regression
  var MIN_PREDICTION_POINTS = 10; // ~5 min at 30s interval

  // ---------------------------------------------------------------------------
  // State
  // ---------------------------------------------------------------------------
  var ws = null;
  var wsBackoff = 1000;
  var reconnectTimer = null;
  var connected = false;

  var chart = null;
  var chartData = [[], [], [], [], [], [], []]; // [timestamps, pit, meat1, meat2, fan, damper, setpoint]
  var predictionData = { meat1: null, meat2: null }; // { times: [], temps: [] } for each

  var pitSetpoint = 225;   // always stored in °F
  var meat1Target = null;  // always stored in °F
  var meat2Target = null;  // always stored in °F

  var currentUnits = 'F';        // 'F' or 'C' — display only
  var currentTimeFormat = '12h'; // '12h' or '24h'

  var cookTimerStart = null;  // server timestamp (seconds) when cook started
  var cookTimerInterval = null;
  var latestServerTs = null;  // most recent msg.ts from server (seconds)

  var debounceTimers = {};

  // ---------------------------------------------------------------------------
  // DOM References
  // ---------------------------------------------------------------------------
  var dom = {};

  function cacheDom() {
    dom.connIndicator = document.getElementById('connIndicator');
    dom.connText = document.getElementById('connText');
    dom.pitTemp = document.getElementById('pitTemp');
    dom.pitSetpoint = document.getElementById('pitSetpoint');
    dom.meat1Temp = document.getElementById('meat1Temp');
    dom.meat1Target = document.getElementById('meat1Target');
    dom.meat2Temp = document.getElementById('meat2Temp');
    dom.meat2Target = document.getElementById('meat2Target');
    dom.meat1Prediction = document.getElementById('meat1Prediction');
    dom.meat2Prediction = document.getElementById('meat2Prediction');
    dom.fanBar = document.getElementById('fanBar');
    dom.fanValue = document.getElementById('fanValue');
    dom.damperBar = document.getElementById('damperBar');
    dom.damperValue = document.getElementById('damperValue');
    dom.cookTimer = document.getElementById('cookTimer');
    dom.cookEstimate = document.getElementById('cookEstimate');
    dom.chartContainer = document.getElementById('chartContainer');
    dom.pitSpInput = document.getElementById('pitSpInput');
    dom.pitSpDown = document.getElementById('pitSpDown');
    dom.pitSpUp = document.getElementById('pitSpUp');
    dom.meat1TargetInput = document.getElementById('meat1TargetInput');
    dom.meat2TargetInput = document.getElementById('meat2TargetInput');
    dom.btnNewSession = document.getElementById('btnNewSession');
    dom.btnDownloadCSV = document.getElementById('btnDownloadCSV');
    dom.btnToggleUnits = document.getElementById('btnToggleUnits');
    dom.btnToggleTime = document.getElementById('btnToggleTime');
  }

  // ---------------------------------------------------------------------------
  // Utilities
  // ---------------------------------------------------------------------------
  function debounce(key, fn, delay) {
    if (debounceTimers[key]) clearTimeout(debounceTimers[key]);
    debounceTimers[key] = setTimeout(fn, delay || DEBOUNCE_MS);
  }

  function formatTime(seconds) {
    var h = Math.floor(seconds / 3600);
    var m = Math.floor((seconds % 3600) / 60);
    var s = Math.floor(seconds % 60);
    return (
      String(h).padStart(2, '0') + ':' +
      String(m).padStart(2, '0') + ':' +
      String(s).padStart(2, '0')
    );
  }

  function formatClockTime(date) {
    var h = date.getHours();
    var m = date.getMinutes();
    if (currentTimeFormat === '24h') {
      return String(h).padStart(2, '0') + ':' + String(m).padStart(2, '0');
    }
    var ampm = h >= 12 ? 'PM' : 'AM';
    h = h % 12;
    if (h === 0) h = 12;
    return h + ':' + String(m).padStart(2, '0') + ' ' + ampm;
  }

  // ---------------------------------------------------------------------------
  // Temperature Conversion
  // ---------------------------------------------------------------------------
  function fToC(f) { return (f - 32) * 5 / 9; }
  function cToF(c) { return c * 9 / 5 + 32; }

  function displayTemp(fValue) {
    if (fValue === null || fValue === undefined) return null;
    if (fValue === -1 || fValue === 'ERR') return fValue;
    return currentUnits === 'C' ? Math.round(fToC(fValue)) : Math.round(fValue);
  }

  function displayTempFromInput(displayValue) {
    if (currentUnits === 'C') return Math.round(cToF(displayValue));
    return displayValue;
  }

  function unitLabel() { return '\u00B0' + currentUnits; }

  function formatTemp(value) {
    if (value === null || value === undefined) return '---';
    if (value === -1 || value === 'ERR') return 'ERR';
    return displayTemp(value).toString();
  }

  // ---------------------------------------------------------------------------
  // Preferences (localStorage)
  // ---------------------------------------------------------------------------
  function detectDefaults() {
    var units = 'C';
    var timeFormat = '24h';

    // US and a few other locales use Fahrenheit
    var lang = navigator.language || '';
    if (/^en-(US|BS|KY|LR|PW|FM|MH)/i.test(lang)) {
      units = 'F';
    }

    // Detect 12h/24h from OS settings via Intl
    try {
      var hourCycle = new Intl.DateTimeFormat(undefined, { hour: 'numeric' })
        .resolvedOptions().hourCycle;
      timeFormat = (hourCycle === 'h11' || hourCycle === 'h12') ? '12h' : '24h';
    } catch (e) {
      if (units === 'F') timeFormat = '12h';
    }

    return { units: units, timeFormat: timeFormat };
  }

  function loadPrefs() {
    try {
      var stored = localStorage.getItem('bbq_prefs');
      if (stored) {
        var prefs = JSON.parse(stored);
        currentUnits = prefs.units === 'C' ? 'C' : 'F';
        currentTimeFormat = prefs.timeFormat === '24h' ? '24h' : '12h';
        return;
      }
    } catch (e) { /* fall through to defaults */ }
    var defaults = detectDefaults();
    currentUnits = defaults.units;
    currentTimeFormat = defaults.timeFormat;
  }

  function savePrefs() {
    try {
      localStorage.setItem('bbq_prefs', JSON.stringify({
        units: currentUnits,
        timeFormat: currentTimeFormat
      }));
    } catch (e) { /* silently fail */ }
  }

  // ---------------------------------------------------------------------------
  // WebSocket Connection
  // ---------------------------------------------------------------------------
  function wsConnect() {
    if (ws && (ws.readyState === WebSocket.CONNECTING || ws.readyState === WebSocket.OPEN)) {
      return;
    }

    var protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
    var url = protocol + '//' + location.host + '/ws';

    try {
      ws = new WebSocket(url);
    } catch (e) {
      console.error('WebSocket creation failed:', e);
      scheduleReconnect();
      return;
    }

    ws.onopen = function () {
      console.log('WebSocket connected');
      connected = true;
      wsBackoff = 1000;
      updateConnectionStatus(true);
    };

    ws.onmessage = function (evt) {
      try {
        var msg = JSON.parse(evt.data);
        handleMessage(msg);
      } catch (e) {
        console.warn('Failed to parse WS message:', e);
      }
    };

    ws.onclose = function () {
      console.log('WebSocket closed');
      connected = false;
      updateConnectionStatus(false);
      scheduleReconnect();
    };

    ws.onerror = function (err) {
      console.error('WebSocket error:', err);
      ws.close();
    };
  }

  function scheduleReconnect() {
    if (reconnectTimer) return;
    console.log('Reconnecting in ' + wsBackoff + 'ms...');
    reconnectTimer = setTimeout(function () {
      reconnectTimer = null;
      wsConnect();
    }, wsBackoff);
    wsBackoff = Math.min(wsBackoff * 2, WS_MAX_BACKOFF);
  }

  function wsSend(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(obj));
    }
  }

  function updateConnectionStatus(isConnected) {
    if (isConnected) {
      dom.connIndicator.classList.add('connected');
      dom.connIndicator.classList.remove('disconnected');
      dom.connText.textContent = 'Connected';
      dom.connIndicator.title = 'Connected';
    } else {
      dom.connIndicator.classList.remove('connected');
      dom.connIndicator.classList.add('disconnected');
      dom.connText.textContent = 'Disconnected';
      dom.connIndicator.title = 'Disconnected';
    }
  }

  // ---------------------------------------------------------------------------
  // Message Handling
  // ---------------------------------------------------------------------------
  function handleMessage(msg) {
    if (msg.type === 'data') {
      updateTemperatures(msg);
      updateOutputs(msg);
      appendChartData(msg);
      updateCookTimer(msg);
      updatePredictions();
    } else if (msg.type === 'history') {
      loadHistory(msg);
    } else if (msg.type === 'session' && msg.action === 'reset') {
      handleSessionReset(msg);
    }
  }

  function handleSessionReset(msg) {
    // Reset cook timer and chart state once the server confirms the reset.
    // Doing this here (instead of optimistically on button click) avoids a
    // race where stale data messages from the old session sneak in between
    // the local clear and the server-side reset, leaving cookTimerStart
    // pointing at an old-session timestamp and causing negative elapsed time.
    resetCookTimer();
    latestServerTs = null;

    for (var i = 0; i < chartData.length; i++) {
      chartData[i] = [];
    }
    if (chart) {
      chart.setData(chartData);
    }

    // Restore setpoint from the server's reset defaults
    if (msg.sp !== undefined) {
      pitSetpoint = msg.sp;
      dom.pitSetpoint.textContent = displayTemp(msg.sp);
      dom.pitSpInput.value = displayTemp(msg.sp);
    }
  }

  function loadHistory(msg) {
    // Restore alarm targets and setpoint (server values are always °F)
    if (msg.sp !== undefined) {
      pitSetpoint = msg.sp;
      dom.pitSetpoint.textContent = displayTemp(msg.sp);
      dom.pitSpInput.value = displayTemp(msg.sp);
    }
    if (msg.meat1Target !== undefined && msg.meat1Target !== null) {
      meat1Target = msg.meat1Target;
      dom.meat1Target.textContent = displayTemp(msg.meat1Target);
      dom.meat1TargetInput.value = displayTemp(msg.meat1Target);
    }
    if (msg.meat2Target !== undefined && msg.meat2Target !== null) {
      meat2Target = msg.meat2Target;
      dom.meat2Target.textContent = displayTemp(msg.meat2Target);
      dom.meat2TargetInput.value = displayTemp(msg.meat2Target);
    }

    // Populate chart data from history (stored as °F)
    if (!msg.data || !msg.data.length) return;

    // Reset chart arrays
    for (var i = 0; i < chartData.length; i++) {
      chartData[i] = [];
    }

    for (var j = 0; j < msg.data.length; j++) {
      var d = msg.data[j];
      chartData[0].push(d.ts);
      chartData[1].push(d.pit !== null && d.pit !== undefined && d.pit !== -1 ? d.pit : null);
      chartData[2].push(d.meat1 !== null && d.meat1 !== undefined && d.meat1 !== -1 ? d.meat1 : null);
      chartData[3].push(d.meat2 !== null && d.meat2 !== undefined && d.meat2 !== -1 ? d.meat2 : null);
      chartData[4].push(d.fan !== undefined ? d.fan : null);
      chartData[5].push(d.damper !== undefined ? d.damper : null);
      chartData[6].push(d.sp !== undefined ? d.sp : pitSetpoint);
    }

    // Update display with the latest point
    var last = msg.data[msg.data.length - 1];
    latestServerTs = last.ts;
    updateTemperatures(last);
    updateOutputs(last);

    // Trigger cook timer from history (first valid meat reading)
    for (var k = 0; k < msg.data.length; k++) {
      updateCookTimer(msg.data[k]);
      if (cookTimerStart) break;
    }

    if (chart) {
      chart.setData(buildChartDataWithPrediction());
    }
    updatePredictions();
  }

  function updateTemperatures(msg) {
    // Temperature values from server are always °F; formatTemp converts for display
    dom.pitTemp.textContent = formatTemp(msg.pit);
    dom.meat1Temp.textContent = formatTemp(msg.meat1);
    dom.meat2Temp.textContent = formatTemp(msg.meat2);

    if (msg.sp !== undefined) {
      pitSetpoint = msg.sp;
      dom.pitSetpoint.textContent = displayTemp(msg.sp);
      dom.pitSpInput.value = displayTemp(msg.sp);
    }

    if (msg.meat1Target !== undefined && msg.meat1Target !== null) {
      meat1Target = msg.meat1Target;
      dom.meat1Target.textContent = displayTemp(msg.meat1Target);
      dom.meat1TargetInput.value = displayTemp(msg.meat1Target);
    }

    if (msg.meat2Target !== undefined && msg.meat2Target !== null) {
      meat2Target = msg.meat2Target;
      dom.meat2Target.textContent = displayTemp(msg.meat2Target);
      dom.meat2TargetInput.value = displayTemp(msg.meat2Target);
    }
  }

  function updateOutputs(msg) {
    var fan = msg.fan !== undefined ? msg.fan : 0;
    var damper = msg.damper !== undefined ? msg.damper : 0;

    dom.fanBar.style.width = fan + '%';
    dom.fanValue.textContent = Math.round(fan) + '%';
    dom.damperBar.style.width = damper + '%';
    dom.damperValue.textContent = Math.round(damper) + '%';
  }

  // ---------------------------------------------------------------------------
  // Cook Timer
  // ---------------------------------------------------------------------------
  function updateCookTimer(msg) {
    if (cookTimerStart) return; // already started
    var meat1Valid = msg.meat1 !== null && msg.meat1 !== undefined && msg.meat1 !== -1;
    var meat2Valid = msg.meat2 !== null && msg.meat2 !== undefined && msg.meat2 !== -1;
    if (meat1Valid || meat2Valid) {
      cookTimerStart = msg.ts || Math.floor(Date.now() / 1000);
      localStorage.setItem('bbq_cook_timer_start', cookTimerStart.toString());
    }
  }

  function tickCookTimer() {
    if (!cookTimerStart || !latestServerTs) {
      dom.cookTimer.textContent = '00:00:00';
      dom.cookEstimate.textContent = 'Est. --:--:--';
      return;
    }
    var elapsed = latestServerTs - cookTimerStart;
    dom.cookTimer.textContent = formatTime(elapsed);

    // Estimated total cook time = max predicted done time - cook start
    var est1 = predictDoneTime(2, meat1Target);
    var est2 = predictDoneTime(3, meat2Target);
    var maxDone = null;
    if (est1) maxDone = est1.doneTime;
    if (est2 && (!maxDone || est2.doneTime > maxDone)) maxDone = est2.doneTime;

    if (maxDone) {
      var totalSec = maxDone - cookTimerStart;
      dom.cookEstimate.textContent = 'Est. ' + formatTime(totalSec);
    } else {
      dom.cookEstimate.textContent = 'Est. --:--:--';
    }
  }

  function restoreCookTimer() {
    var stored = localStorage.getItem('bbq_cook_timer_start');
    if (stored) {
      cookTimerStart = parseInt(stored, 10);
    }
  }

  function resetCookTimer() {
    cookTimerStart = null;
    localStorage.removeItem('bbq_cook_timer_start');
    dom.cookTimer.textContent = '00:00:00';
    dom.cookEstimate.textContent = 'Est. --:--:--';
  }

  // ---------------------------------------------------------------------------
  // Chart
  // ---------------------------------------------------------------------------
  function buildChartOpts() {
    var container = dom.chartContainer;
    var width = container.clientWidth;
    var height = Math.max(250, Math.min(400, window.innerHeight * 0.35));

    return {
      width: width,
      height: height,
      tzDate: function (ts) { return new Date(ts * 1000); },
      cursor: {
        drag: { x: true, y: false }
      },
      scales: {
        x: { time: true },
        temp: { auto: true },
        pct: { auto: false, range: [0, 100] }
      },
      axes: [
        {
          // X axis — uses formatClockTime for consistent 12h/24h
          stroke: '#666',
          grid: { stroke: 'rgba(255,255,255,0.06)' },
          ticks: { stroke: 'rgba(255,255,255,0.06)' },
          values: function (u, vals) {
            return vals.map(function (v) {
              return formatClockTime(new Date(v * 1000));
            });
          }
        },
        {
          // Left Y axis — Temperature (data stays °F, labels convert)
          scale: 'temp',
          label: 'Temperature (' + unitLabel() + ')',
          stroke: '#e0e0e0',
          grid: { stroke: 'rgba(255,255,255,0.06)' },
          ticks: { stroke: 'rgba(255,255,255,0.06)' },
          values: function (u, vals) {
            return vals.map(function (v) {
              var display = currentUnits === 'C' ? Math.round(fToC(v)) : v;
              return display + '\u00B0';
            });
          }
        },
        {
          // Right Y axis — Percent
          scale: 'pct',
          side: 1,
          label: 'Output %',
          stroke: '#e0e0e0',
          grid: { show: false },
          ticks: { stroke: 'rgba(255,255,255,0.06)' },
          values: function (u, vals) {
            return vals.map(function (v) { return v + '%'; });
          }
        }
      ],
      series: [
        { label: 'Time' },
        {
          label: 'Pit',
          scale: 'temp',
          stroke: '#f59e0b',
          width: 2,
          value: function (u, v) { return v == null ? '--' : displayTemp(v) + '\u00B0'; }
        },
        {
          label: 'Meat 1',
          scale: 'temp',
          stroke: '#ef4444',
          width: 2,
          value: function (u, v) { return v == null ? '--' : displayTemp(v) + '\u00B0'; }
        },
        {
          label: 'Meat 2',
          scale: 'temp',
          stroke: '#3b82f6',
          width: 2,
          value: function (u, v) { return v == null ? '--' : displayTemp(v) + '\u00B0'; }
        },
        {
          label: 'Fan %',
          scale: 'pct',
          stroke: '#10b981',
          width: 1.5,
          dash: [6, 3],
          value: function (u, v) { return v == null ? '--' : Math.round(v) + '%'; }
        },
        {
          label: 'Damper %',
          scale: 'pct',
          stroke: '#8b5cf6',
          width: 1.5,
          dash: [6, 3],
          value: function (u, v) { return v == null ? '--' : Math.round(v) + '%'; }
        },
        {
          label: 'Setpoint',
          scale: 'temp',
          stroke: '#f59e0b',
          width: 1.5,
          dash: [8, 4],
          value: function (u, v) { return v == null ? '--' : displayTemp(v) + '\u00B0'; }
        }
      ]
    };
  }

  function initChart() {
    if (typeof uPlot === 'undefined') {
      console.warn('uPlot not loaded, retrying in 500ms');
      setTimeout(initChart, 500);
      return;
    }

    chart = new uPlot(buildChartOpts(), chartData, dom.chartContainer);
  }

  function recreateChart() {
    if (chart) {
      chart.destroy();
      chart = null;
    }
    dom.chartContainer.innerHTML = '';
    initChart();
    if (chartData[0].length > 0 && chart) {
      chart.setData(buildChartDataWithPrediction());
    }
  }

  function appendChartData(msg) {
    var now = msg.ts || Math.floor(Date.now() / 1000);
    latestServerTs = now;

    chartData[0].push(now);
    chartData[1].push(msg.pit !== null && msg.pit !== undefined && msg.pit !== -1 ? msg.pit : null);
    chartData[2].push(msg.meat1 !== null && msg.meat1 !== undefined && msg.meat1 !== -1 ? msg.meat1 : null);
    chartData[3].push(msg.meat2 !== null && msg.meat2 !== undefined && msg.meat2 !== -1 ? msg.meat2 : null);
    chartData[4].push(msg.fan !== undefined ? msg.fan : null);
    chartData[5].push(msg.damper !== undefined ? msg.damper : null);
    chartData[6].push(msg.sp !== undefined ? msg.sp : pitSetpoint);

    // Trim data older than 4 hours (keep extra buffer beyond 2h visible window)
    var cutoff = now - 4 * 60 * 60;
    while (chartData[0].length > 0 && chartData[0][0] < cutoff) {
      for (var i = 0; i < chartData.length; i++) {
        chartData[i].shift();
      }
    }

    if (chart) {
      chart.setData(buildChartDataWithPrediction());
    }
  }

  function buildChartDataWithPrediction() {
    // Base data (copy)
    var data = [];
    for (var i = 0; i < chartData.length; i++) {
      data.push(chartData[i].slice());
    }
    return data;
  }

  function resizeChart() {
    if (!chart || !dom.chartContainer) return;
    var width = dom.chartContainer.clientWidth;
    var height = Math.max(250, Math.min(400, window.innerHeight * 0.35));
    chart.setSize({ width: width, height: height });
  }

  // ---------------------------------------------------------------------------
  // Predictions (Linear Regression)
  // ---------------------------------------------------------------------------
  function linearRegression(xs, ys) {
    var n = xs.length;
    if (n < 2) return null;
    var sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
    for (var i = 0; i < n; i++) {
      sumX += xs[i];
      sumY += ys[i];
      sumXY += xs[i] * ys[i];
      sumXX += xs[i] * xs[i];
    }
    var denom = n * sumXX - sumX * sumX;
    if (denom === 0) return null;
    var slope = (n * sumXY - sumX * sumY) / denom;
    var intercept = (sumY - slope * sumX) / n;
    return { slope: slope, intercept: intercept };
  }

  function predictDoneTime(seriesIndex, target) {
    if (!target || chartData[0].length < MIN_PREDICTION_POINTS) return null;

    var now = chartData[0][chartData[0].length - 1];
    var windowStart = now - PREDICTION_WINDOW_SEC;

    // Collect recent valid data points
    var xs = [];
    var ys = [];
    for (var i = 0; i < chartData[0].length; i++) {
      if (chartData[0][i] >= windowStart && chartData[seriesIndex][i] !== null) {
        xs.push(chartData[0][i]);
        ys.push(chartData[seriesIndex][i]);
      }
    }

    if (xs.length < MIN_PREDICTION_POINTS) return null;

    var currentTemp = ys[ys.length - 1];
    if (currentTemp >= target) return null; // Already at or above target

    var reg = linearRegression(xs, ys);
    if (!reg || reg.slope <= 0) return null; // Not rising

    // Time when regression line hits target
    var doneTimeSec = (target - reg.intercept) / reg.slope;
    if (doneTimeSec <= now) return null; // Already passed (shouldn't happen if slope > 0 and temp < target)
    if (doneTimeSec - now > 24 * 60 * 60) return null; // More than 24h away, probably bad data

    return {
      doneTime: doneTimeSec,
      currentTemp: currentTemp,
      slope: reg.slope,
      intercept: reg.intercept
    };
  }

  function updatePredictions() {
    updateSinglePrediction(2, meat1Target, dom.meat1Prediction);
    updateSinglePrediction(3, meat2Target, dom.meat2Prediction);
  }

  function updateSinglePrediction(seriesIndex, target, domElement) {
    if (!target) {
      domElement.textContent = '';
      return;
    }

    if (chartData[0].length < MIN_PREDICTION_POINTS) {
      domElement.textContent = 'Est. done: Calculating...';
      return;
    }

    var pred = predictDoneTime(seriesIndex, target);
    if (!pred) {
      // Check if already at target
      var last = chartData[seriesIndex][chartData[seriesIndex].length - 1];
      if (last !== null && last >= target) {
        domElement.textContent = 'Target reached!';
      } else {
        domElement.textContent = 'Est. done: Calculating...';
      }
      return;
    }

    var doneDate = new Date(pred.doneTime * 1000);
    domElement.textContent = 'Est. done: ' + formatClockTime(doneDate);
  }

  // ---------------------------------------------------------------------------
  // Toggle Mechanics
  // ---------------------------------------------------------------------------
  function toggleUnits() {
    currentUnits = currentUnits === 'F' ? 'C' : 'F';
    savePrefs();
    updateToggleButtons();
    updateUnitLabels();
    refreshAllDisplayValues();
    recreateChart();
  }

  function toggleTimeFormat() {
    currentTimeFormat = currentTimeFormat === '12h' ? '24h' : '12h';
    savePrefs();
    updateToggleButtons();
    recreateChart();
    updatePredictions();
  }

  function updateToggleButtons() {
    dom.btnToggleUnits.textContent = '\u00B0' + currentUnits;
    dom.btnToggleTime.textContent = currentTimeFormat;
  }

  function updateUnitLabels() {
    var label = unitLabel();

    // Update all unit spans in temperature cards
    var unitSpans = document.querySelectorAll('.temp-unit');
    for (var i = 0; i < unitSpans.length; i++) {
      unitSpans[i].textContent = label;
    }

    // Update control labels
    var pitLabel = document.querySelector('label[for="pitSpInput"]');
    if (pitLabel) pitLabel.textContent = 'Pit Setpoint (' + label + ')';
    var m1Label = document.querySelector('label[for="meat1TargetInput"]');
    if (m1Label) m1Label.textContent = 'Meat 1 Target (' + label + ')';
    var m2Label = document.querySelector('label[for="meat2TargetInput"]');
    if (m2Label) m2Label.textContent = 'Meat 2 Target (' + label + ')';

    // Update input constraints and +/- button text
    if (currentUnits === 'C') {
      dom.pitSpInput.min = 38;
      dom.pitSpInput.max = 260;
      dom.pitSpInput.step = 3;
      dom.pitSpDown.textContent = '-3';
      dom.pitSpDown.setAttribute('aria-label', 'Decrease setpoint by 3');
      dom.pitSpUp.textContent = '+3';
      dom.pitSpUp.setAttribute('aria-label', 'Increase setpoint by 3');
      dom.meat1TargetInput.min = 38;
      dom.meat1TargetInput.max = 100;
      dom.meat1TargetInput.placeholder = 'e.g. 95';
      dom.meat2TargetInput.min = 38;
      dom.meat2TargetInput.max = 100;
      dom.meat2TargetInput.placeholder = 'e.g. 91';
    } else {
      dom.pitSpInput.min = 100;
      dom.pitSpInput.max = 500;
      dom.pitSpInput.step = 5;
      dom.pitSpDown.textContent = '-5';
      dom.pitSpDown.setAttribute('aria-label', 'Decrease setpoint by 5');
      dom.pitSpUp.textContent = '+5';
      dom.pitSpUp.setAttribute('aria-label', 'Increase setpoint by 5');
      dom.meat1TargetInput.min = 100;
      dom.meat1TargetInput.max = 212;
      dom.meat1TargetInput.placeholder = 'e.g. 203';
      dom.meat2TargetInput.min = 100;
      dom.meat2TargetInput.max = 212;
      dom.meat2TargetInput.placeholder = 'e.g. 195';
    }
  }

  function refreshAllDisplayValues() {
    // Re-display all temps from internal °F state
    var len = chartData[0].length;
    if (len > 0) {
      dom.pitTemp.textContent = formatTemp(chartData[1][len - 1]);
      dom.meat1Temp.textContent = formatTemp(chartData[2][len - 1]);
      dom.meat2Temp.textContent = formatTemp(chartData[3][len - 1]);
    }

    dom.pitSetpoint.textContent = displayTemp(pitSetpoint);
    dom.pitSpInput.value = displayTemp(pitSetpoint);

    if (meat1Target !== null) {
      dom.meat1Target.textContent = displayTemp(meat1Target);
      dom.meat1TargetInput.value = displayTemp(meat1Target);
    }
    if (meat2Target !== null) {
      dom.meat2Target.textContent = displayTemp(meat2Target);
      dom.meat2TargetInput.value = displayTemp(meat2Target);
    }

    updatePredictions();
  }

  // ---------------------------------------------------------------------------
  // Controls
  // ---------------------------------------------------------------------------
  function initControls() {
    // Pit setpoint +/- buttons (step 5°F or 3°C)
    dom.pitSpDown.addEventListener('click', function () {
      var displayVal = parseInt(dom.pitSpInput.value, 10) || displayTemp(225);
      var step = currentUnits === 'C' ? 3 : 5;
      var min = currentUnits === 'C' ? 38 : 100;
      displayVal = Math.max(min, displayVal - step);
      dom.pitSpInput.value = displayVal;
      sendSetpoint(displayTempFromInput(displayVal));
    });

    dom.pitSpUp.addEventListener('click', function () {
      var displayVal = parseInt(dom.pitSpInput.value, 10) || displayTemp(225);
      var step = currentUnits === 'C' ? 3 : 5;
      var max = currentUnits === 'C' ? 260 : 500;
      displayVal = Math.min(max, displayVal + step);
      dom.pitSpInput.value = displayVal;
      sendSetpoint(displayTempFromInput(displayVal));
    });

    dom.pitSpInput.addEventListener('change', function () {
      var displayVal = parseInt(this.value, 10);
      var min = currentUnits === 'C' ? 38 : 100;
      var max = currentUnits === 'C' ? 260 : 500;
      if (!isNaN(displayVal) && displayVal >= min && displayVal <= max) {
        sendSetpoint(displayTempFromInput(displayVal));
      }
    });

    // Meat targets (user enters display unit, convert to °F for server)
    dom.meat1TargetInput.addEventListener('change', function () {
      var displayVal = parseInt(this.value, 10);
      var min = currentUnits === 'C' ? 38 : 100;
      var max = currentUnits === 'C' ? 100 : 212;
      if (!isNaN(displayVal) && displayVal >= min && displayVal <= max) {
        var fVal = displayTempFromInput(displayVal);
        meat1Target = fVal;
        dom.meat1Target.textContent = displayVal;
        debounce('meat1Target', function () {
          wsSend({ type: 'alarm', meat1Target: fVal });
        });
      }
    });

    dom.meat2TargetInput.addEventListener('change', function () {
      var displayVal = parseInt(this.value, 10);
      var min = currentUnits === 'C' ? 38 : 100;
      var max = currentUnits === 'C' ? 100 : 212;
      if (!isNaN(displayVal) && displayVal >= min && displayVal <= max) {
        var fVal = displayTempFromInput(displayVal);
        meat2Target = fVal;
        dom.meat2Target.textContent = displayVal;
        debounce('meat2Target', function () {
          wsSend({ type: 'alarm', meat2Target: fVal });
        });
      }
    });

    // Session controls
    dom.btnNewSession.addEventListener('click', function () {
      if (confirm('Start a new session? This will reset the cook timer and begin a new data log.')) {
        wsSend({ type: 'session', action: 'new' });
        // Cleanup happens in handleSessionReset when the server confirms
      }
    });

    dom.btnDownloadCSV.addEventListener('click', function () {
      wsSend({ type: 'session', action: 'download', format: 'csv' });
    });
  }

  function sendSetpoint(val) {
    pitSetpoint = val; // always stored as °F
    dom.pitSetpoint.textContent = displayTemp(val);
    debounce('setpoint', function () {
      wsSend({ type: 'set', sp: val });
    });
  }

  // ---------------------------------------------------------------------------
  // Resize Handler
  // ---------------------------------------------------------------------------
  var resizeTimeout;
  function onResize() {
    clearTimeout(resizeTimeout);
    resizeTimeout = setTimeout(resizeChart, 150);
  }

  // ---------------------------------------------------------------------------
  // Init
  // ---------------------------------------------------------------------------
  function init() {
    cacheDom();
    loadPrefs();
    updateToggleButtons();
    updateUnitLabels();
    restoreCookTimer();
    initControls();
    initChart();
    wsConnect();

    // Toggle button listeners
    dom.btnToggleUnits.addEventListener('click', toggleUnits);
    dom.btnToggleTime.addEventListener('click', toggleTimeFormat);

    // Cook timer tick
    cookTimerInterval = setInterval(tickCookTimer, 1000);

    // Window resize
    window.addEventListener('resize', onResize);
  }

  // Wait for DOM
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

})();
