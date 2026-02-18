// Pit Claw - app.js
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
  var GITHUB_REPO = 'MrMatt57/pitclaw';
  var OTA_CHUNK_SIZE = 4096;

  // ---------------------------------------------------------------------------
  // State
  // ---------------------------------------------------------------------------
  var ws = null;
  var wsBackoff = 1000;
  var reconnectTimer = null;
  var connected = false;

  var chart = null;
  var chartData = [[], [], [], [], [], [], [], [], []]; // [timestamps, pit, meat1, meat2, fan, damper, setpoint, meat1Target, meat2Target]
  var predictionData = { meat1: null, meat2: null }; // { times: [], temps: [] } for each

  var pitSetpoint = 225;   // always stored in °F
  var meat1Target = null;  // always stored in °F
  var meat2Target = null;  // always stored in °F

  var currentUnits = 'F';        // 'F' or 'C' — display only
  var currentTimeFormat = '12h'; // '12h' or '24h'
  var currentFanMode = 'fan_and_damper'; // 'fan_only', 'fan_and_damper', 'damper_primary'

  var cookTimerStart = null;  // server timestamp (seconds) when cook started
  var cookTimerInterval = null;
  var latestServerTs = null;  // most recent msg.ts from server (seconds)

  var debounceTimers = {};
  var seriesShow = null; // persists toggle state across chart recreation

  var notifyEnabled = false;
  var notifyMeat1Fired = false;  // true once meat1 target notification sent
  var notifyMeat2Fired = false;  // true once meat2 target notification sent
  var audioCtx = null;           // Web Audio API context (created on user gesture)

  var firmwareVersion = null;    // current firmware version string
  var latestRelease = null;      // cached GitHub release JSON

  // ---------------------------------------------------------------------------
  // DOM References
  // ---------------------------------------------------------------------------
  var dom = {};

  function cacheDom() {
    dom.wifiIcon = document.getElementById('wifiIcon');
    dom.btnNotify = document.getElementById('btnNotify');
    dom.btnSettings = document.getElementById('btnSettings');
    dom.btnCloseSettings = document.getElementById('btnCloseSettings');
    dom.settingsPanel = document.getElementById('settingsPanel');
    dom.settingsOverlay = document.getElementById('settingsOverlay');
    dom.pitTemp = document.getElementById('pitTemp');
    dom.pitSetpoint = document.getElementById('pitSetpoint');
    dom.pitPrediction = document.getElementById('pitPrediction');
    dom.meat1Temp = document.getElementById('meat1Temp');
    dom.meat1Target = document.getElementById('meat1Target');
    dom.meat2Temp = document.getElementById('meat2Temp');
    dom.meat2Target = document.getElementById('meat2Target');
    dom.meat1Prediction = document.getElementById('meat1Prediction');
    dom.meat2Prediction = document.getElementById('meat2Prediction');
    dom.fanBar = document.getElementById('fanBar');
    dom.damperBar = document.getElementById('damperBar');
    dom.fanBarRow = document.getElementById('fanBarRow');
    dom.damperBarRow = document.getElementById('damperBarRow');
    dom.fanBarValue = document.getElementById('fanBarValue');
    dom.damperBarValue = document.getElementById('damperBarValue');
    dom.btnFanOnly = document.getElementById('btnFanOnly');
    dom.btnFanAndDamper = document.getElementById('btnFanAndDamper');
    dom.btnDamperPrimary = document.getElementById('btnDamperPrimary');
    dom.cookStart = document.getElementById('cookStart');
    dom.cookTimer = document.getElementById('cookTimer');
    dom.cookDone = document.getElementById('cookDone');
    dom.chartContainer = document.getElementById('chartContainer');
    dom.pitSpInput = document.getElementById('pitSpInput');
    dom.pitSpDown = document.getElementById('pitSpDown');
    dom.pitSpUp = document.getElementById('pitSpUp');
    dom.meat1TargetInput = document.getElementById('meat1TargetInput');
    dom.meat1TargetDown = document.getElementById('meat1TargetDown');
    dom.meat1TargetUp = document.getElementById('meat1TargetUp');
    dom.meat2TargetInput = document.getElementById('meat2TargetInput');
    dom.meat2TargetDown = document.getElementById('meat2TargetDown');
    dom.meat2TargetUp = document.getElementById('meat2TargetUp');
    dom.btnNewSession = document.getElementById('btnNewSession');
    dom.btnDownloadCSV = document.getElementById('btnDownloadCSV');
    dom.btnToggleUnits = document.getElementById('btnToggleUnits');
    dom.btnToggleTime = document.getElementById('btnToggleTime');
    dom.legTime = document.getElementById('legTime');
    dom.legPit = document.getElementById('legPit');
    dom.legMeat1 = document.getElementById('legMeat1');
    dom.legMeat2 = document.getElementById('legMeat2');
    dom.legSP = document.getElementById('legSP');
    dom.legM1Tgt = document.getElementById('legM1Tgt');
    dom.legM2Tgt = document.getElementById('legM2Tgt');
    dom.legFan = document.getElementById('legFan');
    dom.legDamper = document.getElementById('legDamper');
    dom.meat1Card = document.querySelector('.meat1-card');
    dom.meat2Card = document.querySelector('.meat2-card');
    dom.fwVersion = document.getElementById('fwVersion');
    dom.settingsVersion = document.getElementById('settingsVersion');
    dom.updateBanner = document.getElementById('updateBanner');
    dom.updateMessage = document.getElementById('updateMessage');
    dom.btnUpdate = document.getElementById('btnUpdate');
    dom.btnDismissUpdate = document.getElementById('btnDismissUpdate');
    dom.updateOverlay = document.getElementById('updateOverlay');
    dom.updateProgress = document.getElementById('updateProgress');
    dom.updateStatus = document.getElementById('updateStatus');
    dom.btnCheckUpdate = document.getElementById('btnCheckUpdate');
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

  function formatRemaining(seconds) {
    if (seconds <= 0) return '0m';
    var h = Math.floor(seconds / 3600);
    var m = Math.floor((seconds % 3600) / 60);
    if (h > 0) return h + 'h ' + m + 'm';
    if (m > 0) return m + 'm';
    return '<1m';
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
        notifyEnabled = !!prefs.notify && ('Notification' in window) && Notification.permission === 'granted';
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
        timeFormat: currentTimeFormat,
        notify: notifyEnabled
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
      dom.wifiIcon.classList.add('connected');
      dom.wifiIcon.classList.remove('disconnected');
    } else {
      dom.wifiIcon.classList.remove('connected');
      dom.wifiIcon.classList.add('disconnected');
    }
  }

  // ---------------------------------------------------------------------------
  // Fan Mode
  // ---------------------------------------------------------------------------
  function applyFanMode(mode) {
    currentFanMode = mode;

    // Toggle active class on mode buttons
    dom.btnFanOnly.classList.toggle('active', mode === 'fan_only');
    dom.btnFanAndDamper.classList.toggle('active', mode === 'fan_and_damper');
    dom.btnDamperPrimary.classList.toggle('active', mode === 'damper_primary');

    // Show/hide bar rows
    dom.fanBarRow.classList.toggle('hidden', mode === 'damper_primary');
    dom.damperBarRow.classList.toggle('hidden', mode === 'fan_only');

    // Show/hide chart legend items for fan/damper
    var legFanItem = dom.legFan.closest('.legend-item');
    var legDamperItem = dom.legDamper.closest('.legend-item');
    if (legFanItem) legFanItem.style.display = mode === 'damper_primary' ? 'none' : '';
    if (legDamperItem) legDamperItem.style.display = mode === 'fan_only' ? 'none' : '';
  }

  // ---------------------------------------------------------------------------
  // Message Handling
  // ---------------------------------------------------------------------------
  function handleMessage(msg) {
    if (msg.type === 'data') {
      if (msg.fanMode && msg.fanMode !== currentFanMode) {
        applyFanMode(msg.fanMode);
      }
      updateTemperatures(msg);
      updateOutputs(msg);
      appendChartData(msg);
      updateCookTimer(msg);
      updatePredictions();
      checkTargetNotifications(msg);
    } else if (msg.type === 'history') {
      loadHistory(msg);
    } else if (msg.type === 'session' && msg.action === 'reset') {
      handleSessionReset(msg);
    } else if (msg.type === 'session' && msg.action === 'download') {
      handleSessionDownload(msg);
    }
  }

  function handleSessionReset(msg) {
    // Reset cook timer and chart state once the server confirms the reset.
    // Doing this here (instead of optimistically on button click) avoids a
    // race where stale data messages from the old session sneak in between
    // the local clear and the server-side reset, leaving cookTimerStart
    // pointing at an old-session timestamp and causing negative elapsed time.
    closeSettings();
    resetCookTimer();
    latestServerTs = null;

    for (var i = 0; i < chartData.length; i++) {
      chartData[i] = [];
    }
    if (chart) {
      chart.setData(chartData);
    }
    updateLegendValues(null);

    // Clear stale targets
    hideMeatTarget(1);
    hideMeatTarget(2);

    // Restore setpoint from the server's reset defaults
    if (msg.sp !== undefined) {
      pitSetpoint = msg.sp;
      dom.pitSetpoint.textContent = displayTemp(msg.sp);
      dom.pitSpInput.value = displayTemp(msg.sp);
    }
  }

  function handleSessionDownload(msg) {
    if (!msg.data) return;
    var blob = new Blob([msg.data], { type: 'text/csv' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = 'cook-session.csv';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  function loadHistory(msg) {
    // Restore alarm targets and setpoint (server values are always °F)
    if (msg.sp !== undefined) {
      pitSetpoint = msg.sp;
      dom.pitSetpoint.textContent = displayTemp(msg.sp);
      dom.pitSpInput.value = displayTemp(msg.sp);
    }
    if (msg.meat1Target !== undefined) {
      if (msg.meat1Target !== null && msg.meat1Target > 0) {
        showMeatTarget(1, msg.meat1Target);
      } else {
        hideMeatTarget(1);
      }
    }
    if (msg.meat2Target !== undefined) {
      if (msg.meat2Target !== null && msg.meat2Target > 0) {
        showMeatTarget(2, msg.meat2Target);
      } else {
        hideMeatTarget(2);
      }
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
      chartData[7].push(meat1Target);
      chartData[8].push(meat2Target);
    }

    // Update display with the latest point
    var last = msg.data[msg.data.length - 1];
    latestServerTs = last.ts;
    updateTemperatures(last);
    updateOutputs(last);

    // Reset cook timer and re-derive from history (server is source of truth)
    resetCookTimer();
    for (var k = 0; k < msg.data.length; k++) {
      updateCookTimer(msg.data[k]);
      if (cookTimerStart) break;
    }

    if (chart) {
      chart.setData(buildChartDataWithPrediction());
    }
    updatePredictions();
    updateLegendValues(null);
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

    if (msg.meat1Target !== undefined) {
      if (msg.meat1Target !== null && msg.meat1Target > 0) {
        showMeatTarget(1, msg.meat1Target);
      } else {
        hideMeatTarget(1);
      }
    }

    if (msg.meat2Target !== undefined) {
      if (msg.meat2Target !== null && msg.meat2Target > 0) {
        showMeatTarget(2, msg.meat2Target);
      } else {
        hideMeatTarget(2);
      }
    }
  }

  function updateOutputs(msg) {
    var fan = msg.fan !== undefined ? msg.fan : 0;
    var damper = msg.damper !== undefined ? msg.damper : 0;

    dom.fanBar.style.width = fan + '%';
    dom.damperBar.style.width = damper + '%';
    dom.fanBarValue.textContent = Math.round(fan) + '%';
    dom.damperBarValue.textContent = Math.round(damper) + '%';
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
      dom.cookStart.textContent = '--:--';
      dom.cookTimer.textContent = '00:00:00';
      dom.cookDone.textContent = '--:--';
      return;
    }

    // Start time (clock)
    dom.cookStart.textContent = formatClockTime(new Date(cookTimerStart * 1000));

    // Elapsed (guard against stale/mismatched timestamps)
    var elapsed = latestServerTs - cookTimerStart;
    if (elapsed < 0) elapsed = 0;
    dom.cookTimer.textContent = formatTime(elapsed);

    // Done time — average of predictions, or single if only one is set
    var est1 = predictDoneTime(2, meat1Target);
    var est2 = predictDoneTime(3, meat2Target);
    var doneTime = null;

    if (est1 && est2) {
      doneTime = (est1.doneTime + est2.doneTime) / 2;
    } else if (est1) {
      doneTime = est1.doneTime;
    } else if (est2) {
      doneTime = est2.doneTime;
    }

    if (doneTime) {
      var remaining = doneTime - latestServerTs;
      var remStr = remaining > 0 ? ' (' + formatRemaining(remaining) + ')' : ' (Done)';
      dom.cookDone.textContent = formatClockTime(new Date(doneTime * 1000)) + remStr;
    } else {
      dom.cookDone.textContent = '--:--';
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
    dom.cookStart.textContent = '--:--';
    dom.cookTimer.textContent = '00:00:00';
    dom.cookDone.textContent = '--:--';
  }

  // ---------------------------------------------------------------------------
  // Chart
  // ---------------------------------------------------------------------------
  function buildChartOpts() {
    var container = dom.chartContainer;
    var width = container.clientWidth;
    var height = Math.max(280, Math.min(500, window.innerHeight * 0.45));

    return {
      width: width,
      height: height,
      tzDate: function (ts) { return new Date(ts * 1000); },
      legend: { show: false },
      cursor: {
        drag: { x: true, y: false }
      },
      hooks: {
        setCursor: [function (u) { updateLegendValues(u.cursor.idx); }]
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
          points: { show: false },
          value: function (u, v) { return v == null ? '--' : displayTemp(v) + '\u00B0'; }
        },
        {
          label: 'Meat 1',
          scale: 'temp',
          stroke: '#ef4444',
          width: 2,
          points: { show: false },
          value: function (u, v) { return v == null ? '--' : displayTemp(v) + '\u00B0'; }
        },
        {
          label: 'Meat 2',
          scale: 'temp',
          stroke: '#3b82f6',
          width: 2,
          points: { show: false },
          value: function (u, v) { return v == null ? '--' : displayTemp(v) + '\u00B0'; }
        },
        {
          label: 'Fan %',
          scale: 'pct',
          stroke: '#10b981',
          width: 1.5,
          dash: [6, 3],
          show: false,
          points: { show: false },
          value: function (u, v) { return v == null ? '--' : Math.round(v) + '%'; }
        },
        {
          label: 'Damper %',
          scale: 'pct',
          stroke: '#8b5cf6',
          width: 1.5,
          dash: [6, 3],
          show: false,
          points: { show: false },
          value: function (u, v) { return v == null ? '--' : Math.round(v) + '%'; }
        },
        {
          label: 'Setpoint',
          scale: 'temp',
          stroke: '#f59e0b',
          width: 1.5,
          dash: [8, 4],
          points: { show: false },
          value: function (u, v) { return v == null ? '--' : displayTemp(v) + '\u00B0'; }
        },
        {
          label: 'Meat 1 Target',
          scale: 'temp',
          stroke: '#ef4444',
          width: 1.5,
          dash: [8, 4],
          points: { show: false },
          value: function (u, v) { return v == null ? '--' : displayTemp(v) + '\u00B0'; }
        },
        {
          label: 'Meat 2 Target',
          scale: 'temp',
          stroke: '#3b82f6',
          width: 1.5,
          dash: [8, 4],
          points: { show: false },
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
      seriesShow = chart.series.map(function (s) { return s.show; });
      chart.destroy();
      chart = null;
    }
    dom.chartContainer.innerHTML = '';
    initChart();
    if (chart && seriesShow) {
      for (var i = 1; i < seriesShow.length && i < chart.series.length; i++) {
        chart.setSeries(i, { show: seriesShow[i] });
      }
    }
    if (chartData[0].length > 0 && chart) {
      chart.setData(buildChartDataWithPrediction());
    }
    syncLegendClasses();
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
    chartData[7].push(meat1Target);
    chartData[8].push(meat2Target);

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
    updateLegendValues(null);
  }

  function clearTargetFromChart(seriesIndex) {
    for (var i = 0; i < chartData[seriesIndex].length; i++) {
      chartData[seriesIndex][i] = null;
    }
    if (chart) {
      chart.setData(buildChartDataWithPrediction());
    }
  }

  function restoreTargetInChart(seriesIndex, fVal) {
    for (var i = 0; i < chartData[seriesIndex].length; i++) {
      chartData[seriesIndex][i] = fVal;
    }
    if (chart) {
      chart.setData(buildChartDataWithPrediction());
    }
  }

  function showMeatTarget(probe, fVal) {
    if (probe === 1) {
      meat1Target = fVal;
      dom.meat1Target.textContent = displayTemp(fVal);
      dom.meat1TargetInput.value = displayTemp(fVal);
      dom.meat1Card.classList.remove('no-target');
      restoreTargetInChart(7, fVal);
    } else {
      meat2Target = fVal;
      dom.meat2Target.textContent = displayTemp(fVal);
      dom.meat2TargetInput.value = displayTemp(fVal);
      dom.meat2Card.classList.remove('no-target');
      restoreTargetInChart(8, fVal);
    }
  }

  function hideMeatTarget(probe) {
    if (probe === 1) {
      meat1Target = null;
      dom.meat1Target.textContent = '---';
      dom.meat1TargetInput.value = '';
      dom.meat1Prediction.textContent = '';
      dom.meat1Card.classList.add('no-target');
      clearTargetFromChart(7);
    } else {
      meat2Target = null;
      dom.meat2Target.textContent = '---';
      dom.meat2TargetInput.value = '';
      dom.meat2Prediction.textContent = '';
      dom.meat2Card.classList.add('no-target');
      clearTargetFromChart(8);
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
    var height = Math.max(280, Math.min(500, window.innerHeight * 0.45));
    chart.setSize({ width: width, height: height });
  }

  function initLegendToggles() {
    var items = document.querySelectorAll('.legend-item[data-series]');
    for (var i = 0; i < items.length; i++) {
      items[i].addEventListener('click', function () {
        var idx = parseInt(this.getAttribute('data-series'), 10);
        if (!chart || isNaN(idx) || idx < 1 || idx >= chart.series.length) return;
        var show = !chart.series[idx].show;
        chart.setSeries(idx, { show: show });
        syncLegendClasses();
      });
    }
  }

  function syncLegendClasses() {
    if (!chart) return;
    var items = document.querySelectorAll('.legend-item[data-series]');
    for (var i = 0; i < items.length; i++) {
      var idx = parseInt(items[i].getAttribute('data-series'), 10);
      if (!isNaN(idx) && idx < chart.series.length) {
        items[i].classList.toggle('legend-off', !chart.series[idx].show);
      }
    }
  }

  function updateLegendValues(idx) {
    var len = chartData[0].length;
    if (idx == null || idx < 0 || idx >= len) idx = len - 1;
    if (len === 0 || idx < 0) {
      dom.legTime.textContent = '--:--';
      dom.legPit.textContent = '--';
      dom.legMeat1.textContent = '--';
      dom.legMeat2.textContent = '--';
      dom.legSP.textContent = '--';
      dom.legM1Tgt.textContent = '--';
      dom.legM2Tgt.textContent = '--';
      dom.legFan.textContent = '--';
      dom.legDamper.textContent = '--';
      return;
    }
    dom.legTime.textContent = formatClockTime(new Date(chartData[0][idx] * 1000));
    var v;
    v = chartData[1][idx]; dom.legPit.textContent = v == null ? '--' : displayTemp(v) + '\u00B0';
    v = chartData[2][idx]; dom.legMeat1.textContent = v == null ? '--' : displayTemp(v) + '\u00B0';
    v = chartData[3][idx]; dom.legMeat2.textContent = v == null ? '--' : displayTemp(v) + '\u00B0';
    v = chartData[4][idx]; dom.legFan.textContent = v == null ? '--' : Math.round(v) + '%';
    v = chartData[5][idx]; dom.legDamper.textContent = v == null ? '--' : Math.round(v) + '%';
    v = chartData[6][idx]; dom.legSP.textContent = v == null ? '--' : displayTemp(v) + '\u00B0';
    v = chartData[7][idx]; dom.legM1Tgt.textContent = v == null ? '--' : displayTemp(v) + '\u00B0';
    v = chartData[8][idx]; dom.legM2Tgt.textContent = v == null ? '--' : displayTemp(v) + '\u00B0';
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

  function updatePitPrediction() {
    if (!pitSetpoint || chartData[0].length < MIN_PREDICTION_POINTS) {
      dom.pitPrediction.textContent = chartData[0].length > 0 ? 'Calculating...' : '';
      return;
    }

    var last = chartData[1][chartData[1].length - 1];
    if (last !== null && last >= pitSetpoint) {
      dom.pitPrediction.textContent = '';
      return;
    }

    var pred = predictDoneTime(1, pitSetpoint);
    if (!pred) {
      dom.pitPrediction.textContent = 'Calculating...';
      return;
    }

    var doneDate = new Date(pred.doneTime * 1000);
    var remaining = pred.doneTime - (latestServerTs || Math.floor(Date.now() / 1000));
    var remStr = remaining > 0 ? formatRemaining(remaining) : 'Done';
    dom.pitPrediction.textContent = formatClockTime(doneDate) + ' (' + remStr + ')';
  }

  function updatePredictions() {
    updatePitPrediction();
    updateSinglePrediction(2, meat1Target, dom.meat1Prediction);
    updateSinglePrediction(3, meat2Target, dom.meat2Prediction);
  }

  function updateSinglePrediction(seriesIndex, target, domElement) {
    if (!target) {
      domElement.textContent = '';
      return;
    }

    if (chartData[0].length < MIN_PREDICTION_POINTS) {
      domElement.textContent = 'Calculating...';
      return;
    }

    var pred = predictDoneTime(seriesIndex, target);
    if (!pred) {
      // Check if already at target
      var last = chartData[seriesIndex][chartData[seriesIndex].length - 1];
      if (last !== null && last >= target) {
        domElement.textContent = 'Target reached!';
      } else {
        domElement.textContent = 'Calculating...';
      }
      return;
    }

    var doneDate = new Date(pred.doneTime * 1000);
    var remaining = pred.doneTime - (latestServerTs || Math.floor(Date.now() / 1000));
    var remStr = remaining > 0 ? formatRemaining(remaining) : 'Done';
    domElement.textContent = formatClockTime(doneDate) + ' (' + remStr + ')';
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

    // Update unit spans in settings panel controls
    var controlUnitSpans = document.querySelectorAll('.temp-control-unit');
    for (var j = 0; j < controlUnitSpans.length; j++) {
      controlUnitSpans[j].textContent = label;
    }

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
      dom.meat1TargetInput.placeholder = '---';
      dom.meat1TargetDown.textContent = '-3';
      dom.meat1TargetDown.setAttribute('aria-label', 'Decrease target by 3');
      dom.meat1TargetUp.textContent = '+3';
      dom.meat1TargetUp.setAttribute('aria-label', 'Increase target by 3');
      dom.meat2TargetInput.min = 38;
      dom.meat2TargetInput.max = 100;
      dom.meat2TargetInput.placeholder = '---';
      dom.meat2TargetDown.textContent = '-3';
      dom.meat2TargetDown.setAttribute('aria-label', 'Decrease target by 3');
      dom.meat2TargetUp.textContent = '+3';
      dom.meat2TargetUp.setAttribute('aria-label', 'Increase target by 3');
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
      dom.meat1TargetInput.placeholder = '---';
      dom.meat1TargetDown.textContent = '-5';
      dom.meat1TargetDown.setAttribute('aria-label', 'Decrease target by 5');
      dom.meat1TargetUp.textContent = '+5';
      dom.meat1TargetUp.setAttribute('aria-label', 'Increase target by 5');
      dom.meat2TargetInput.min = 100;
      dom.meat2TargetInput.max = 212;
      dom.meat2TargetInput.placeholder = '---';
      dom.meat2TargetDown.textContent = '-5';
      dom.meat2TargetDown.setAttribute('aria-label', 'Decrease target by 5');
      dom.meat2TargetUp.textContent = '+5';
      dom.meat2TargetUp.setAttribute('aria-label', 'Increase target by 5');
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
      dom.meat1Card.classList.remove('no-target');
    } else {
      dom.meat1Target.textContent = '---';
      dom.meat1TargetInput.value = '';
      dom.meat1Card.classList.add('no-target');
    }
    if (meat2Target !== null) {
      dom.meat2Target.textContent = displayTemp(meat2Target);
      dom.meat2TargetInput.value = displayTemp(meat2Target);
      dom.meat2Card.classList.remove('no-target');
    } else {
      dom.meat2Target.textContent = '---';
      dom.meat2TargetInput.value = '';
      dom.meat2Card.classList.add('no-target');
    }

    updatePredictions();
    updateLegendValues(null);
  }

  // ---------------------------------------------------------------------------
  // Settings Panel
  // ---------------------------------------------------------------------------
  function openSettings() {
    dom.settingsPanel.classList.add('open');
    dom.settingsOverlay.classList.add('active');
    document.body.style.overflow = 'hidden';
  }

  function closeSettings() {
    dom.settingsPanel.classList.remove('open');
    dom.settingsOverlay.classList.remove('active');
    document.body.style.overflow = '';
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
      notifyMeat1Fired = false;
      var raw = this.value.trim();
      if (raw === '') {
        meat1Target = null;
        dom.meat1Target.textContent = '---';
        dom.meat1Prediction.textContent = '';
        dom.meat1Card.classList.add('no-target');
        clearTargetFromChart(7);
        debounce('meat1Target', function () {
          wsSend({ type: 'alarm', meat1Target: null });
        });
        return;
      }
      var displayVal = parseInt(raw, 10);
      var min = currentUnits === 'C' ? 38 : 100;
      var max = currentUnits === 'C' ? 100 : 212;
      if (!isNaN(displayVal) && displayVal >= min && displayVal <= max) {
        var fVal = displayTempFromInput(displayVal);
        meat1Target = fVal;
        dom.meat1Target.textContent = displayVal;
        dom.meat1Card.classList.remove('no-target');
        restoreTargetInChart(7, fVal);
        debounce('meat1Target', function () {
          wsSend({ type: 'alarm', meat1Target: fVal });
        });
      }
    });

    dom.meat2TargetInput.addEventListener('change', function () {
      notifyMeat2Fired = false;
      var raw = this.value.trim();
      if (raw === '') {
        meat2Target = null;
        dom.meat2Target.textContent = '---';
        dom.meat2Prediction.textContent = '';
        dom.meat2Card.classList.add('no-target');
        clearTargetFromChart(8);
        debounce('meat2Target', function () {
          wsSend({ type: 'alarm', meat2Target: null });
        });
        return;
      }
      var displayVal = parseInt(raw, 10);
      var min = currentUnits === 'C' ? 38 : 100;
      var max = currentUnits === 'C' ? 100 : 212;
      if (!isNaN(displayVal) && displayVal >= min && displayVal <= max) {
        var fVal = displayTempFromInput(displayVal);
        meat2Target = fVal;
        dom.meat2Target.textContent = displayVal;
        dom.meat2Card.classList.remove('no-target');
        restoreTargetInChart(8, fVal);
        debounce('meat2Target', function () {
          wsSend({ type: 'alarm', meat2Target: fVal });
        });
      }
    });

    // Meat 1 Target +/- buttons
    dom.meat1TargetDown.addEventListener('click', function () {
      var raw = dom.meat1TargetInput.value.trim();
      if (raw === '') return;
      var displayVal = parseInt(raw, 10);
      var step = currentUnits === 'C' ? 3 : 5;
      var min = currentUnits === 'C' ? 38 : 100;
      displayVal = Math.max(min, displayVal - step);
      dom.meat1TargetInput.value = displayVal;
      dom.meat1TargetInput.dispatchEvent(new Event('change'));
    });

    dom.meat1TargetUp.addEventListener('click', function () {
      var raw = dom.meat1TargetInput.value.trim();
      var step = currentUnits === 'C' ? 3 : 5;
      var max = currentUnits === 'C' ? 100 : 212;
      var displayVal;
      if (raw === '') {
        displayVal = currentUnits === 'C' ? 90 : 195;
      } else {
        displayVal = Math.min(max, parseInt(raw, 10) + step);
      }
      dom.meat1TargetInput.value = displayVal;
      dom.meat1TargetInput.dispatchEvent(new Event('change'));
    });

    // Meat 2 Target +/- buttons
    dom.meat2TargetDown.addEventListener('click', function () {
      var raw = dom.meat2TargetInput.value.trim();
      if (raw === '') return;
      var displayVal = parseInt(raw, 10);
      var step = currentUnits === 'C' ? 3 : 5;
      var min = currentUnits === 'C' ? 38 : 100;
      displayVal = Math.max(min, displayVal - step);
      dom.meat2TargetInput.value = displayVal;
      dom.meat2TargetInput.dispatchEvent(new Event('change'));
    });

    dom.meat2TargetUp.addEventListener('click', function () {
      var raw = dom.meat2TargetInput.value.trim();
      var step = currentUnits === 'C' ? 3 : 5;
      var max = currentUnits === 'C' ? 100 : 212;
      var displayVal;
      if (raw === '') {
        displayVal = currentUnits === 'C' ? 90 : 195;
      } else {
        displayVal = Math.min(max, parseInt(raw, 10) + step);
      }
      dom.meat2TargetInput.value = displayVal;
      dom.meat2TargetInput.dispatchEvent(new Event('change'));
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

    // Fan mode buttons
    var fanModeButtons = [dom.btnFanOnly, dom.btnFanAndDamper, dom.btnDamperPrimary];
    fanModeButtons.forEach(function (btn) {
      btn.addEventListener('click', function () {
        var mode = this.getAttribute('data-mode');
        applyFanMode(mode);
        wsSend({ type: 'config', fanMode: mode });
      });
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
  // Notifications
  // ---------------------------------------------------------------------------
  function toggleNotifications() {
    // Create/resume AudioContext during this user gesture so beeps work later
    ensureAudioContext();

    if (!('Notification' in window)) {
      alert('This browser does not support notifications.');
      return;
    }

    if (notifyEnabled) {
      notifyEnabled = false;
      savePrefs();
      syncNotifyButton();
      return;
    }

    if (Notification.permission === 'granted') {
      notifyEnabled = true;
      notifyMeat1Fired = false;
      notifyMeat2Fired = false;
      savePrefs();
      syncNotifyButton();
      playAlarmBeep();
      sendNotification('Notifications enabled', 'You will be notified when meat targets are reached.');
    } else if (Notification.permission === 'denied') {
      alert('Notifications are blocked. Please enable them in your browser settings.');
      syncNotifyButton();
    } else {
      Notification.requestPermission().then(function (perm) {
        if (perm === 'granted') {
          notifyEnabled = true;
          notifyMeat1Fired = false;
          notifyMeat2Fired = false;
          savePrefs();
          playAlarmBeep();
          sendNotification('Notifications enabled', 'You will be notified when meat targets are reached.');
        }
        syncNotifyButton();
      });
    }
  }

  function syncNotifyButton() {
    dom.btnNotify.classList.remove('notify-on', 'notify-denied');
    if (!('Notification' in window) || Notification.permission === 'denied') {
      dom.btnNotify.classList.add('notify-denied');
    } else if (notifyEnabled) {
      dom.btnNotify.classList.add('notify-on');
    }
  }

  function ensureAudioContext() {
    if (!audioCtx) {
      try { audioCtx = new (window.AudioContext || window.webkitAudioContext)(); } catch (e) {}
    }
    if (audioCtx && audioCtx.state === 'suspended') {
      audioCtx.resume();
    }
  }

  function playAlarmBeep() {
    if (!audioCtx) return;
    if (audioCtx.state === 'suspended') { audioCtx.resume(); }
    var now = audioCtx.currentTime;
    for (var i = 0; i < 3; i++) {
      var start = now + i * 0.25;
      var osc = audioCtx.createOscillator();
      var gain = audioCtx.createGain();
      osc.connect(gain);
      gain.connect(audioCtx.destination);
      osc.frequency.value = 880;
      osc.type = 'sine';
      gain.gain.setValueAtTime(0.3, start);
      gain.gain.exponentialRampToValueAtTime(0.01, start + 0.15);
      osc.start(start);
      osc.stop(start + 0.15);
    }
  }

  function sendNotification(title, body) {
    if (!notifyEnabled || !('Notification' in window) || Notification.permission !== 'granted') return;
    var opts = {
      body: body,
      icon: '/favicon.svg',
      tag: 'pitclaw-' + title.replace(/\s+/g, '-').toLowerCase()
    };
    // Always try the service worker path first via .ready (resolves when SW is
    // active, regardless of whether .controller is set yet on this page load).
    if (navigator.serviceWorker) {
      navigator.serviceWorker.ready.then(function (reg) {
        return reg.showNotification(title, opts);
      }).catch(function () {
        try { new Notification(title, opts); } catch (e) {}
      });
    } else {
      try { new Notification(title, opts); } catch (e) {}
    }
  }

  function checkTargetNotifications(msg) {
    if (!notifyEnabled) return;

    var meat1Valid = msg.meat1 !== null && msg.meat1 !== undefined && msg.meat1 !== -1;
    var meat2Valid = msg.meat2 !== null && msg.meat2 !== undefined && msg.meat2 !== -1;

    if (meat1Target && meat1Valid && !notifyMeat1Fired && msg.meat1 >= meat1Target) {
      notifyMeat1Fired = true;
      playAlarmBeep();
      sendNotification('Meat 1 Target Reached', 'Meat 1 is at ' + formatTemp(msg.meat1) + unitLabel() + ' (target: ' + formatTemp(meat1Target) + unitLabel() + ')');
    }

    if (meat2Target && meat2Valid && !notifyMeat2Fired && msg.meat2 >= meat2Target) {
      notifyMeat2Fired = true;
      playAlarmBeep();
      sendNotification('Meat 2 Target Reached', 'Meat 2 is at ' + formatTemp(msg.meat2) + unitLabel() + ' (target: ' + formatTemp(meat2Target) + unitLabel() + ')');
    }
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
  // Firmware Version & OTA Update
  // ---------------------------------------------------------------------------
  function fetchVersion() {
    fetch('/api/version')
      .then(function (r) { return r.json(); })
      .then(function (data) {
        firmwareVersion = data.version;
        dom.fwVersion.textContent = 'Pit Claw v' + firmwareVersion;
        dom.settingsVersion.textContent = 'v' + firmwareVersion;
        checkForUpdate();
      })
      .catch(function (err) {
        console.warn('Failed to fetch version:', err);
      });
  }

  function compareVersions(a, b) {
    var pa = a.split('.').map(Number);
    var pb = b.split('.').map(Number);
    for (var i = 0; i < 3; i++) {
      var va = pa[i] || 0;
      var vb = pb[i] || 0;
      if (va < vb) return -1;
      if (va > vb) return 1;
    }
    return 0;
  }

  function checkForUpdate() {
    if (!firmwareVersion || firmwareVersion === 'dev') return;

    fetch('https://api.github.com/repos/' + GITHUB_REPO + '/releases/latest')
      .then(function (r) {
        if (!r.ok) throw new Error('GitHub API ' + r.status);
        return r.json();
      })
      .then(function (release) {
        latestRelease = release;
        var remoteVersion = release.tag_name.replace(/^v/, '');
        if (compareVersions(firmwareVersion, remoteVersion) < 0) {
          dom.updateMessage.textContent = 'Update available: v' + remoteVersion;
          dom.updateBanner.style.display = '';
        }
      })
      .catch(function (err) {
        console.warn('Update check failed:', err);
      });
  }

  function performUpdate() {
    if (!latestRelease) return;

    // Find firmware .bin asset
    var asset = null;
    for (var i = 0; i < latestRelease.assets.length; i++) {
      if (/pitclaw-firmware-.*\.bin$/.test(latestRelease.assets[i].name)) {
        asset = latestRelease.assets[i];
        break;
      }
    }
    if (!asset) {
      alert('No firmware binary found in release assets.');
      return;
    }

    dom.updateBanner.style.display = 'none';
    dom.updateOverlay.style.display = '';
    dom.updateStatus.textContent = 'Downloading firmware...';
    dom.updateProgress.style.width = '0%';

    // Download .bin from GitHub
    fetch(asset.browser_download_url)
      .then(function (r) {
        if (!r.ok) throw new Error('Download failed: ' + r.status);
        var contentLength = +r.headers.get('Content-Length') || 0;
        if (!r.body || !contentLength) return r.arrayBuffer();

        // Stream download with progress
        var reader = r.body.getReader();
        var received = 0;
        var chunks = [];
        function pump() {
          return reader.read().then(function (result) {
            if (result.done) {
              var blob = new Uint8Array(received);
              var pos = 0;
              for (var j = 0; j < chunks.length; j++) {
                blob.set(chunks[j], pos);
                pos += chunks[j].length;
              }
              return blob.buffer;
            }
            chunks.push(result.value);
            received += result.value.length;
            var pct = Math.round((received / contentLength) * 40);
            dom.updateProgress.style.width = pct + '%';
            dom.updateStatus.textContent = 'Downloading... ' + Math.round((received / contentLength) * 100) + '%';
            return pump();
          });
        }
        return pump();
      })
      .then(function (buffer) {
        dom.updateProgress.style.width = '40%';
        dom.updateStatus.textContent = 'Starting upload to device...';
        return uploadFirmwareOTA(buffer);
      })
      .then(function () {
        dom.updateProgress.style.width = '100%';
        dom.updateStatus.textContent = 'Update complete! Rebooting...';
        setTimeout(function () { window.location.reload(); }, 15000);
      })
      .catch(function (err) {
        console.error('Update failed:', err);
        dom.updateOverlay.style.display = 'none';
        alert('Update failed: ' + err.message);
      });
  }

  function uploadFirmwareOTA(buffer) {
    var size = buffer.byteLength;

    // ElegantOTA v3 protocol: start session, then upload chunks
    return fetch('/ota/start?mode=fr&hash=0&size=' + size)
      .then(function (r) {
        if (!r.ok) throw new Error('OTA start failed: ' + r.status);
        // Upload in chunks
        var offset = 0;
        function sendChunk() {
          if (offset >= size) return Promise.resolve();
          var end = Math.min(offset + OTA_CHUNK_SIZE, size);
          var chunk = buffer.slice(offset, end);
          return fetch('/ota/upload', {
            method: 'POST',
            body: chunk,
            headers: { 'Content-Type': 'application/octet-stream' }
          }).then(function (r) {
            if (!r.ok) throw new Error('Chunk upload failed at ' + offset);
            offset = end;
            var pct = 40 + Math.round((offset / size) * 55);
            dom.updateProgress.style.width = pct + '%';
            dom.updateStatus.textContent = 'Uploading... ' + Math.round((offset / size) * 100) + '%';
            return sendChunk();
          });
        }
        return sendChunk();
      })
      .catch(function (err) {
        // On reboot during final chunk, the connection drops — that's expected
        if (err.name === 'TypeError' && err.message.indexOf('Failed to fetch') !== -1) {
          return; // device is rebooting
        }
        throw err;
      });
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
    initLegendToggles();
    syncLegendClasses();

    // Hide target rows until server provides state
    dom.meat1Card.classList.add('no-target');
    dom.meat2Card.classList.add('no-target');

    wsConnect();

    // Notification bell
    dom.btnNotify.addEventListener('click', toggleNotifications);
    syncNotifyButton();

    // If notifications were restored from prefs, pre-create AudioContext on
    // first user interaction so the alarm beep works even without re-clicking
    // the bell.  Browsers require a user gesture to start audio playback.
    if (notifyEnabled) {
      var unlockAudio = function () {
        ensureAudioContext();
        document.removeEventListener('click', unlockAudio, true);
        document.removeEventListener('touchstart', unlockAudio, true);
      };
      document.addEventListener('click', unlockAudio, true);
      document.addEventListener('touchstart', unlockAudio, true);
    }

    // Toggle button listeners
    dom.btnToggleUnits.addEventListener('click', toggleUnits);
    dom.btnToggleTime.addEventListener('click', toggleTimeFormat);

    // Settings panel listeners
    dom.btnSettings.addEventListener('click', openSettings);
    dom.btnCloseSettings.addEventListener('click', closeSettings);
    dom.settingsOverlay.addEventListener('click', closeSettings);

    // Cook timer tick
    cookTimerInterval = setInterval(tickCookTimer, 1000);

    // Window resize
    window.addEventListener('resize', onResize);

    // Firmware version and OTA update
    fetchVersion();
    dom.btnUpdate.addEventListener('click', performUpdate);
    dom.btnDismissUpdate.addEventListener('click', function () {
      dom.updateBanner.style.display = 'none';
    });
    dom.btnCheckUpdate.addEventListener('click', function () {
      dom.btnCheckUpdate.textContent = 'Checking...';
      latestRelease = null;
      fetch('https://api.github.com/repos/' + GITHUB_REPO + '/releases/latest')
        .then(function (r) {
          if (!r.ok) throw new Error('GitHub API ' + r.status);
          return r.json();
        })
        .then(function (release) {
          latestRelease = release;
          var remoteVersion = release.tag_name.replace(/^v/, '');
          if (!firmwareVersion || firmwareVersion === 'dev') {
            dom.btnCheckUpdate.textContent = 'Dev build';
          } else if (compareVersions(firmwareVersion, remoteVersion) < 0) {
            dom.updateMessage.textContent = 'Update available: v' + remoteVersion;
            dom.updateBanner.style.display = '';
            dom.btnCheckUpdate.textContent = 'v' + remoteVersion + ' available';
          } else {
            dom.btnCheckUpdate.textContent = 'Up to date';
          }
        })
        .catch(function (err) {
          console.warn('Update check failed:', err);
          dom.btnCheckUpdate.textContent = 'Check failed';
        });
    });
  }

  // Wait for DOM
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

})();
