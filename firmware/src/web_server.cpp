#include "web_server.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <time.h>
#include <cmath>

#include "temp_manager.h"
#include "pid_controller.h"
#include "fan_controller.h"
#include "servo_controller.h"
#include "config_manager.h"
#include "cook_session.h"
#include "alarm_manager.h"
#include "error_manager.h"
#endif

BBQWebServer::BBQWebServer()
    :
#ifndef NATIVE_BUILD
      _server(nullptr)
    , _ws(nullptr)
    ,
#endif
      _temp(nullptr)
    , _pid(nullptr)
    , _fan(nullptr)
    , _servo(nullptr)
    , _config(nullptr)
    , _session(nullptr)
    , _alarm(nullptr)
    , _error(nullptr)
    , _setpoint(225.0f)
    , _estimatedTime(0)
    , _lastBroadcastMs(0)
    , _onSetpoint(nullptr)
    , _onAlarm(nullptr)
    , _onSession(nullptr)
    , _onFanMode(nullptr)
{
}

void BBQWebServer::begin() {
#ifndef NATIVE_BUILD
    _server = new AsyncWebServer(WEB_PORT);
    _ws = new AsyncWebSocket(WS_PATH);

    // WebSocket event handler
    _ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                        AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->onWsEvent(server, client, type, arg, data, len);
    });

    _server->addHandler(_ws);

    // Version API endpoint
    _server->on("/api/version", HTTP_GET, [](AsyncWebServerRequest* request) {
        char json[128];
        snprintf(json, sizeof(json),
                 "{\"version\":\"%s\",\"board\":\"wt32_sc01_plus\"}", FIRMWARE_VERSION);
        request->send(200, "application/json", json);
    });

    // Serve static files from LittleFS (web UI)
    _server->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // Fallback 404
    _server->onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not Found");
    });

    _server->begin();
    _lastBroadcastMs = millis();

    Serial.printf("[WEB] Server started on port %d, WebSocket at %s\n", WEB_PORT, WS_PATH);
#endif
}

void BBQWebServer::update() {
#ifndef NATIVE_BUILD
    unsigned long now = millis();

    // Periodic broadcast to all connected clients
    if (now - _lastBroadcastMs >= WS_SEND_INTERVAL) {
        _lastBroadcastMs = now;

        if (_ws && _ws->count() > 0) {
            bbq_protocol::DataPayload payload = buildDataPayload();
            char buf[512];
            size_t len = bbq_protocol::buildDataMessage(buf, sizeof(buf), payload);
            _ws->textAll(buf, len);
        }
    }

    // Clean up disconnected clients
    if (_ws) {
        _ws->cleanupClients(WS_MAX_CLIENTS);
    }
#endif
}

void BBQWebServer::setModules(TempManager* temp, PidController* pid, FanController* fan,
                            ServoController* servo, ConfigManager* config, CookSession* session,
                            AlarmManager* alarm, ErrorManager* error) {
    _temp   = temp;
    _pid    = pid;
    _fan    = fan;
    _servo  = servo;
    _config = config;
    _session = session;
    _alarm  = alarm;
    _error  = error;
}

void BBQWebServer::broadcastNow() {
#ifndef NATIVE_BUILD
    if (_ws && _ws->count() > 0) {
        bbq_protocol::DataPayload payload = buildDataPayload();
        char buf[512];
        size_t len = bbq_protocol::buildDataMessage(buf, sizeof(buf), payload);
        _ws->textAll(buf, len);
    }
#endif
}

uint8_t BBQWebServer::getClientCount() const {
#ifndef NATIVE_BUILD
    if (_ws) return _ws->count();
#endif
    return 0;
}

bbq_protocol::DataPayload BBQWebServer::buildDataPayload() {
    bbq_protocol::DataPayload payload;
    memset(&payload, 0, sizeof(payload));

#ifndef NATIVE_BUILD
    // Timestamp
    time_t now;
    time(&now);
    payload.ts = (uint32_t)now;

    // Temperatures
    if (_temp) {
        payload.pit   = _temp->isConnected(PROBE_PIT)   ? _temp->getPitTemp()   : NAN;
        payload.meat1 = _temp->isConnected(PROBE_MEAT1)  ? _temp->getMeat1Temp() : NAN;
        payload.meat2 = _temp->isConnected(PROBE_MEAT2)  ? _temp->getMeat2Temp() : NAN;
    } else {
        payload.pit = payload.meat1 = payload.meat2 = NAN;
    }

    // Fan and damper
    payload.fan    = _fan    ? (uint8_t)_fan->getCurrentSpeedPct()        : 0;
    payload.damper = _servo  ? (uint8_t)_servo->getCurrentPositionPct()   : 0;

    // Setpoint
    payload.sp = _setpoint;

    // Lid-open
    payload.lid = _pid ? _pid->isLidOpen() : false;

    // Meat targets from alarm manager
    payload.meat1Target = _alarm ? _alarm->getMeat1Target() : 0;
    payload.meat2Target = _alarm ? _alarm->getMeat2Target() : 0;

    // Fan mode
    payload.fanMode = _config ? _config->getFanMode() : "fan_and_damper";

    // Estimated done time
    payload.est = _estimatedTime;

    // Errors
    payload.errorCount = 0;
    if (_error) {
        auto activeErrors = _error->getErrors();
        for (size_t i = 0; i < activeErrors.size() && payload.errorCount < 8; i++) {
            payload.errors[payload.errorCount++] = activeErrors[i].message;
        }
    }
#endif

    return payload;
}

void BBQWebServer::sendHistory(uint8_t clientId) {
#ifndef NATIVE_BUILD
    if (!_session || !_ws) return;

    uint32_t count = _session->getPointCount();
    if (count == 0) return;

    // Build array of HistoryPoints from session data
    bbq_protocol::HistoryPoint* points =
        (bbq_protocol::HistoryPoint*)malloc(count * sizeof(bbq_protocol::HistoryPoint));
    if (!points) return;

    for (uint32_t i = 0; i < count; i++) {
        const DataPoint* dp = _session->getPoint(i);
        if (!dp) continue;

        points[i].ts = dp->timestamp;

        // Convert int16 temps (x10) back to float, check disconnect flags
        if (dp->flags & DP_FLAG_PIT_DISC)   points[i].pit = NAN;
        else                                 points[i].pit = dp->pitTemp / 10.0f;

        if (dp->flags & DP_FLAG_MEAT1_DISC) points[i].meat1 = NAN;
        else                                 points[i].meat1 = dp->meat1Temp / 10.0f;

        if (dp->flags & DP_FLAG_MEAT2_DISC) points[i].meat2 = NAN;
        else                                 points[i].meat2 = dp->meat2Temp / 10.0f;

        points[i].fan    = dp->fanPct;
        points[i].damper = dp->damperPct;
        points[i].sp     = _setpoint; // current setpoint (per-point sp not stored)
        points[i].lid    = (dp->flags & DP_FLAG_LID_OPEN) != 0;
    }

    float m1t = _alarm ? _alarm->getMeat1Target() : 0;
    float m2t = _alarm ? _alarm->getMeat2Target() : 0;

    size_t msgLen = 0;
    char* msg = bbq_protocol::buildHistoryMessage(points, count, _setpoint, m1t, m2t, &msgLen);
    free(points);

    if (msg) {
        _ws->text(clientId, msg, msgLen);
        free(msg);
    }
#endif
}

void BBQWebServer::handleWebSocketMessage(uint8_t clientId, const char* data, size_t len) {
#ifndef NATIVE_BUILD
    bbq_protocol::ParsedCommand cmd = bbq_protocol::parseCommand(data, len);

    switch (cmd.type) {
        case bbq_protocol::CmdType::SET_SP:
            _setpoint = cmd.setpoint;
            if (_onSetpoint) _onSetpoint(cmd.setpoint);
            Serial.printf("[WS] Client %u set setpoint to %.0f\n", clientId, cmd.setpoint);
            break;

        case bbq_protocol::CmdType::ALARM:
            if (cmd.hasMeat1Target && _onAlarm) _onAlarm("meat1", cmd.meat1Target);
            if (cmd.hasMeat2Target && _onAlarm) _onAlarm("meat2", cmd.meat2Target);
            if (cmd.hasPitBand && _onAlarm)     _onAlarm("pitBand", cmd.pitBand);
            break;

        case bbq_protocol::CmdType::SESSION_NEW:
            if (_onSession) _onSession("new", "");
            // Broadcast session reset to all clients
            {
                char buf[128];
                size_t n = bbq_protocol::buildSessionReset(buf, sizeof(buf), _setpoint);
                _ws->textAll(buf, n);
            }
            break;

        case bbq_protocol::CmdType::SET_FAN_MODE:
            if (_onFanMode) _onFanMode(cmd.fanMode);
            Serial.printf("[WS] Client %u set fan mode to %s\n", clientId, cmd.fanMode);
            broadcastNow();
            break;

        case bbq_protocol::CmdType::SESSION_DOWNLOAD:
            if (_session) {
                String csvData = _session->toCSV();
                size_t envLen = 0;
                char* envelope = bbq_protocol::buildCSVDownloadEnvelope(
                    csvData.c_str(), csvData.length(), &envLen);
                if (envelope) {
                    _ws->text(clientId, envelope, envLen);
                    free(envelope);
                }
            }
            break;

        default:
            Serial.printf("[WS] Unknown message type from client %u\n", clientId);
            break;
    }
#endif
}

void BBQWebServer::onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                           AwsEventType type, void* arg, uint8_t* data, size_t len) {
#ifndef NATIVE_BUILD
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WS] Client #%u connected from %s\n",
                          client->id(), client->remoteIP().toString().c_str());
            // Send history if session has data, otherwise send current snapshot
            if (_session && _session->getPointCount() > 0) {
                sendHistory(client->id());
            } else {
                bbq_protocol::DataPayload payload = buildDataPayload();
                char buf[512];
                size_t n = bbq_protocol::buildDataMessage(buf, sizeof(buf), payload);
                _ws->text(client->id(), buf, n);
            }
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("[WS] Client #%u disconnected.\n", client->id());
            break;

        case WS_EVT_DATA:
            {
                AwsFrameInfo* info = (AwsFrameInfo*)arg;
                if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                    // Complete text message received
                    data[len] = '\0';  // Null-terminate
                    handleWebSocketMessage(client->id(), (char*)data, len);
                }
            }
            break;

        case WS_EVT_ERROR:
            Serial.printf("[WS] Client #%u error.\n", client->id());
            break;

        case WS_EVT_PONG:
            break;
    }
#endif
}
