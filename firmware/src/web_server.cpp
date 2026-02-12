#include "web_server.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <time.h>

#include "temp_manager.h"
#include "pid_controller.h"
#include "fan_controller.h"
#include "servo_controller.h"
#include "config_manager.h"
#include "cook_session.h"
#include "alarm_manager.h"
#include "error_manager.h"
#endif

WebServer::WebServer()
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
{
}

void WebServer::begin() {
#ifndef NATIVE_BUILD
    _server = new AsyncWebServer(WEB_PORT);
    _ws = new AsyncWebSocket(WS_PATH);

    // WebSocket event handler
    _ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                        AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->onWsEvent(server, client, type, arg, data, len);
    });

    _server->addHandler(_ws);

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

void WebServer::update() {
#ifndef NATIVE_BUILD
    unsigned long now = millis();

    // Periodic broadcast to all connected clients
    if (now - _lastBroadcastMs >= WS_SEND_INTERVAL) {
        _lastBroadcastMs = now;

        if (_ws && _ws->count() > 0) {
            String msg = buildDataMessage();
            _ws->textAll(msg);
        }
    }

    // Clean up disconnected clients
    if (_ws) {
        _ws->cleanupClients(WS_MAX_CLIENTS);
    }
#endif
}

void WebServer::setModules(TempManager* temp, PidController* pid, FanController* fan,
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

void WebServer::broadcastNow() {
#ifndef NATIVE_BUILD
    if (_ws && _ws->count() > 0) {
        String msg = buildDataMessage();
        _ws->textAll(msg);
    }
#endif
}

uint8_t WebServer::getClientCount() const {
#ifndef NATIVE_BUILD
    if (_ws) return _ws->count();
#endif
    return 0;
}

String WebServer::buildDataMessage() {
    String json;
    json.reserve(256);

#ifndef NATIVE_BUILD
    JsonDocument doc;
    doc["type"] = "data";

    // Timestamp
    time_t now;
    time(&now);
    doc["ts"] = (uint32_t)now;

    // Temperatures
    if (_temp) {
        if (_temp->isConnected(PROBE_PIT)) {
            doc["pit"] = serialized(String(_temp->getPitTemp(), 1));
        } else {
            doc["pit"] = (const char*)nullptr;  // JSON null
        }
        if (_temp->isConnected(PROBE_MEAT1)) {
            doc["meat1"] = serialized(String(_temp->getMeat1Temp(), 1));
        } else {
            doc["meat1"] = (const char*)nullptr;
        }
        if (_temp->isConnected(PROBE_MEAT2)) {
            doc["meat2"] = serialized(String(_temp->getMeat2Temp(), 1));
        } else {
            doc["meat2"] = (const char*)nullptr;
        }
    }

    // Fan and damper
    if (_fan) {
        doc["fan"] = (int)_fan->getCurrentSpeedPct();
    }
    if (_servo) {
        doc["damper"] = (int)_servo->getCurrentPositionPct();
    }

    // Setpoint
    doc["sp"] = (int)_setpoint;

    // Lid-open
    if (_pid) {
        doc["lid"] = _pid->isLidOpen();
    } else {
        doc["lid"] = false;
    }

    // Estimated done time
    if (_estimatedTime > 0) {
        doc["est"] = _estimatedTime;
    } else {
        doc["est"] = (const char*)nullptr;
    }

    // Errors
    JsonArray errors = doc["errors"].to<JsonArray>();
    if (_error) {
        auto activeErrors = _error->getErrors();
        for (size_t i = 0; i < activeErrors.size(); i++) {
            errors.add(activeErrors[i].message);
        }
    }

    serializeJson(doc, json);
#endif

    return json;
}

void WebServer::handleWebSocketMessage(uint8_t clientId, const char* data, size_t len) {
#ifndef NATIVE_BUILD
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        Serial.printf("[WS] JSON parse error from client %u: %s\n", clientId, err.c_str());
        return;
    }

    const char* type = doc["type"] | "";

    if (strcmp(type, "set") == 0) {
        // Setpoint change: {"type":"set","sp":250}
        if (doc["sp"].is<float>()) {
            float sp = doc["sp"].as<float>();
            _setpoint = sp;
            if (_onSetpoint) _onSetpoint(sp);
            Serial.printf("[WS] Client %u set setpoint to %.0f\n", clientId, sp);
        }
    }
    else if (strcmp(type, "alarm") == 0) {
        // Alarm target: {"type":"alarm","meat1Target":203}
        if (doc["meat1Target"].is<float>()) {
            if (_onAlarm) _onAlarm("meat1", doc["meat1Target"].as<float>());
        }
        if (doc["meat2Target"].is<float>()) {
            if (_onAlarm) _onAlarm("meat2", doc["meat2Target"].as<float>());
        }
        if (doc["pitBand"].is<float>()) {
            if (_onAlarm) _onAlarm("pitBand", doc["pitBand"].as<float>());
        }
    }
    else if (strcmp(type, "session") == 0) {
        // Session control: {"type":"session","action":"new"} or
        //                  {"type":"session","action":"download","format":"csv"}
        const char* action = doc["action"] | "";
        const char* format = doc["format"] | "csv";

        if (_onSession) _onSession(action, format);

        // Handle download request directly
        if (strcmp(action, "download") == 0 && _session) {
            String sessionData;
            if (strcmp(format, "json") == 0) {
                sessionData = _session->toJSON();
            } else {
                sessionData = _session->toCSV();
            }
            // Send session data to the requesting client
            _ws->text(clientId, sessionData);
        }
    }
    else {
        Serial.printf("[WS] Unknown message type from client %u: %s\n", clientId, type);
    }
#endif
}

void WebServer::onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                           AwsEventType type, void* arg, uint8_t* data, size_t len) {
#ifndef NATIVE_BUILD
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WS] Client #%u connected from %s\n",
                          client->id(), client->remoteIP().toString().c_str());
            // Send current state immediately on connect
            {
                String msg = buildDataMessage();
                client->text(msg);
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
