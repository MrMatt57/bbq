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
  var chartData = [[], [], [], [], [], []]; // [timestamps, pit, meat1, meat2, fan, damper]
  var predictionData = { meat1: null, meat2: null }; // { times: [], temps: [] } for each

  var pitSetpoint = 225;
  var meat1Target = null;
  var meat2Target = null;

  var cookTimerStart = null;
  var cookTimerInterval = null;

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
    var ampm = h >= 12 ? 'PM' : 'AM';
    h = h % 12;
    if (h === 0) h = 12;
    return h + ':' + String(m).padStart(2, '0') + ' ' + ampm;
  }

  function formatTemp(value) {
    if (value === null || value === undefined) return '---';
    if (value === -1 || value === 'ERR') return 'ERR';
    return Math.round(value).toString();
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
    }
  }

  function updateTemperatures(msg) {
    dom.pitTemp.textContent = formatTemp(msg.pit);
    dom.meat1Temp.textContent = formatTemp(msg.meat1);
    dom.meat2Temp.textContent = formatTemp(msg.meat2);

    if (msg.sp !== undefined) {
      pitSetpoint = msg.sp;
      dom.pitSetpoint.textContent = msg.sp;
      dom.pitSpInput.value = msg.sp;
    }

    if (msg.meat1Target !== undefined && msg.meat1Target !== null) {
      meat1Target = msg.meat1Target;
      dom.meat1Target.textContent = msg.meat1Target;
      dom.meat1TargetInput.value = msg.meat1Target;
    }

    if (msg.meat2Target !== undefined && msg.meat2Target !== null) {
      meat2Target = msg.meat2Target;
      dom.meat2Target.textContent = msg.meat2Target;
      dom.meat2TargetInput.value = msg.meat2Target;
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
      cookTimerStart = Date.now();
      localStorage.setItem('bbq_cook_timer_start', cookTimerStart.toString());
    }
  }

  function tickCookTimer() {
    if (!cookTimerStart) {
      dom.cookTimer.textContent = '00:00:00';
      dom.cookEstimate.textContent = 'Est. --:--:--';
      return;
    }
    var elapsed = (Date.now() - cookTimerStart) / 1000;
    dom.cookTimer.textContent = formatTime(elapsed);

    // Estimated total cook time = max predicted done time - cook start
    var est1 = predictDoneTime(2, meat1Target);
    var est2 = predictDoneTime(3, meat2Target);
    var maxDone = null;
    if (est1) maxDone = est1.doneTime;
    if (est2 && (!maxDone || est2.doneTime > maxDone)) maxDone = est2.doneTime;

    if (maxDone) {
      var totalSec = maxDone - cookTimerStart / 1000;
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
  function initChart() {
    if (typeof uPlot === 'undefined') {
      console.warn('uPlot not loaded, retrying in 500ms');
      setTimeout(initChart, 500);
      return;
    }

    var container = dom.chartContainer;
    var width = container.clientWidth;
    var height = Math.max(250, Math.min(400, window.innerHeight * 0.35));

    var opts = {
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
          // X axis
          stroke: '#666',
          grid: { stroke: 'rgba(255,255,255,0.06)' },
          ticks: { stroke: 'rgba(255,255,255,0.06)' },
          values: function (u, vals) {
            return vals.map(function (v) {
              var d = new Date(v * 1000);
              var h = d.getHours();
              var m = d.getMinutes();
              return String(h).padStart(2, '0') + ':' + String(m).padStart(2, '0');
            });
          }
        },
        {
          // Left Y axis - Temperature
          scale: 'temp',
          label: 'Temperature (F)',
          stroke: '#e0e0e0',
          grid: { stroke: 'rgba(255,255,255,0.06)' },
          ticks: { stroke: 'rgba(255,255,255,0.06)' },
          values: function (u, vals) {
            return vals.map(function (v) { return v + '\u00B0'; });
          }
        },
        {
          // Right Y axis - Percent
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
          width: 2
        },
        {
          label: 'Meat 1',
          scale: 'temp',
          stroke: '#ef4444',
          width: 2
        },
        {
          label: 'Meat 2',
          scale: 'temp',
          stroke: '#3b82f6',
          width: 2
        },
        {
          label: 'Fan %',
          scale: 'pct',
          stroke: '#10b981',
          width: 1.5,
          dash: [6, 3]
        },
        {
          label: 'Damper %',
          scale: 'pct',
          stroke: '#8b5cf6',
          width: 1.5,
          dash: [6, 3]
        }
      ]
    };

    chart = new uPlot(opts, chartData, container);
  }

  function appendChartData(msg) {
    var now = Math.floor(Date.now() / 1000);

    chartData[0].push(now);
    chartData[1].push(msg.pit !== null && msg.pit !== undefined && msg.pit !== -1 ? msg.pit : null);
    chartData[2].push(msg.meat1 !== null && msg.meat1 !== undefined && msg.meat1 !== -1 ? msg.meat1 : null);
    chartData[3].push(msg.meat2 !== null && msg.meat2 !== undefined && msg.meat2 !== -1 ? msg.meat2 : null);
    chartData[4].push(msg.fan !== undefined ? msg.fan : null);
    chartData[5].push(msg.damper !== undefined ? msg.damper : null);

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
  // Controls
  // ---------------------------------------------------------------------------
  function initControls() {
    // Pit setpoint +/- buttons
    dom.pitSpDown.addEventListener('click', function () {
      var val = parseInt(dom.pitSpInput.value, 10) || 225;
      val = Math.max(100, val - 5);
      dom.pitSpInput.value = val;
      sendSetpoint(val);
    });

    dom.pitSpUp.addEventListener('click', function () {
      var val = parseInt(dom.pitSpInput.value, 10) || 225;
      val = Math.min(500, val + 5);
      dom.pitSpInput.value = val;
      sendSetpoint(val);
    });

    dom.pitSpInput.addEventListener('change', function () {
      var val = parseInt(this.value, 10);
      if (!isNaN(val) && val >= 100 && val <= 500) {
        sendSetpoint(val);
      }
    });

    // Meat targets
    dom.meat1TargetInput.addEventListener('change', function () {
      var val = parseInt(this.value, 10);
      if (!isNaN(val) && val >= 100 && val <= 212) {
        meat1Target = val;
        dom.meat1Target.textContent = val;
        debounce('meat1Target', function () {
          wsSend({ type: 'alarm', meat1Target: val });
        });
      }
    });

    dom.meat2TargetInput.addEventListener('change', function () {
      var val = parseInt(this.value, 10);
      if (!isNaN(val) && val >= 100 && val <= 212) {
        meat2Target = val;
        dom.meat2Target.textContent = val;
        debounce('meat2Target', function () {
          wsSend({ type: 'alarm', meat2Target: val });
        });
      }
    });

    // Session controls
    dom.btnNewSession.addEventListener('click', function () {
      if (confirm('Start a new session? This will reset the cook timer and begin a new data log.')) {
        wsSend({ type: 'session', action: 'new' });
        resetCookTimer();
        // Clear chart data
        for (var i = 0; i < chartData.length; i++) {
          chartData[i] = [];
        }
        if (chart) {
          chart.setData(chartData);
        }
      }
    });

    dom.btnDownloadCSV.addEventListener('click', function () {
      wsSend({ type: 'session', action: 'download', format: 'csv' });
    });
  }

  function sendSetpoint(val) {
    pitSetpoint = val;
    dom.pitSetpoint.textContent = val;
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
    restoreCookTimer();
    initControls();
    initChart();
    wsConnect();

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
