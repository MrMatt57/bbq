#ifdef SIMULATOR_BUILD

#include "sim_web_server.h"
#include "mongoose.h"
#include "../config.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

SimWebServer* g_simWebServer = nullptr;

SimWebServer::SimWebServer()
    : _mgr(nullptr)
    , _port(3000)
    , _setpoint(225)
    , _meat1Target(0)
    , _meat2Target(0)
    , _onSetpoint(nullptr)
    , _onAlarm(nullptr)
    , _onNewSession(nullptr)
    , _onSessionDownload(nullptr)
    , _onFanMode(nullptr)
{
    memset(_staticDir, 0, sizeof(_staticDir));
}

SimWebServer::~SimWebServer() {
    if (_mgr) {
        mg_mgr_free(_mgr);
        free(_mgr);
        _mgr = nullptr;
    }
    if (g_simWebServer == this) g_simWebServer = nullptr;
}

void SimWebServer::begin(int port, const char* staticDir) {
    _port = port;
    strncpy(_staticDir, staticDir, sizeof(_staticDir) - 1);

    g_simWebServer = this;

    _mgr = (struct mg_mgr*)malloc(sizeof(struct mg_mgr));
    mg_mgr_init(_mgr);

    char listenUrl[64];
    snprintf(listenUrl, sizeof(listenUrl), "http://0.0.0.0:%d", port);

    mg_http_listen(_mgr, listenUrl, eventHandler, nullptr);
}

void SimWebServer::tick() {
    if (_mgr) {
        mg_mgr_poll(_mgr, 0);
    }
}

void SimWebServer::broadcastData(const bbq_protocol::DataPayload& data) {
    if (!_mgr) return;

    char buf[512];
    size_t len = bbq_protocol::buildDataMessage(buf, sizeof(buf), data);

    // Iterate all connections, send to WebSocket ones
    for (struct mg_connection* c = _mgr->conns; c != nullptr; c = c->next) {
        if (c->is_websocket) {
            mg_ws_send(c, buf, len, WEBSOCKET_OP_TEXT);
        }
    }
}

void SimWebServer::addHistoryPoint(const bbq_protocol::HistoryPoint& point) {
    _history.push_back(point);
}

void SimWebServer::clearHistory() {
    _history.clear();
}

void SimWebServer::resetSession() {
    clearHistory();
    char buf[128];
    size_t n = bbq_protocol::buildSessionReset(buf, sizeof(buf), _setpoint);
    for (struct mg_connection* conn = _mgr->conns; conn != nullptr; conn = conn->next) {
        if (conn->is_websocket) {
            mg_ws_send(conn, buf, n, WEBSOCKET_OP_TEXT);
        }
    }
}

void SimWebServer::setState(float setpoint, float meat1Target, float meat2Target) {
    _setpoint = setpoint;
    _meat1Target = meat1Target;
    _meat2Target = meat2Target;
}

void SimWebServer::onSetpoint(void (*cb)(float))       { _onSetpoint = cb; }
void SimWebServer::onAlarm(void (*cb)(const char*, float)) { _onAlarm = cb; }
void SimWebServer::onNewSession(void (*cb)())           { _onNewSession = cb; }
void SimWebServer::onSessionDownload(void (*cb)())      { _onSessionDownload = cb; }
void SimWebServer::onFanMode(void (*cb)(const char*))   { _onFanMode = cb; }

int SimWebServer::getClientCount() const {
    if (!_mgr) return 0;
    int count = 0;
    for (struct mg_connection* c = _mgr->conns; c != nullptr; c = c->next) {
        if (c->is_websocket) count++;
    }
    return count;
}

void SimWebServer::sendHistory(struct mg_connection* c) {
    if (_history.empty()) return;

    size_t msgLen = 0;
    char* msg = bbq_protocol::buildHistoryMessage(
        _history.data(), _history.size(),
        _setpoint, _meat1Target, _meat2Target, &msgLen);

    if (msg) {
        mg_ws_send(c, msg, msgLen, WEBSOCKET_OP_TEXT);
        free(msg);
    }
}

void SimWebServer::sendCSVDownload(struct mg_connection* c) {
    // Build CSV from history data
    // Header
    std::string csv = "timestamp,pit,meat1,meat2,fan,damper,setpoint,lid\n";
    char line[256];

    for (size_t i = 0; i < _history.size(); i++) {
        const bbq_protocol::HistoryPoint& p = _history[i];
        int pos = snprintf(line, sizeof(line), "%u,", (unsigned)p.ts);

        if (std::isnan(p.pit))   pos += snprintf(line + pos, sizeof(line) - pos, ",");
        else                     pos += snprintf(line + pos, sizeof(line) - pos, "%.1f,", p.pit);

        if (std::isnan(p.meat1)) pos += snprintf(line + pos, sizeof(line) - pos, ",");
        else                     pos += snprintf(line + pos, sizeof(line) - pos, "%.1f,", p.meat1);

        if (std::isnan(p.meat2)) pos += snprintf(line + pos, sizeof(line) - pos, ",");
        else                     pos += snprintf(line + pos, sizeof(line) - pos, "%.1f,", p.meat2);

        pos += snprintf(line + pos, sizeof(line) - pos, "%u,%u,%d,%s\n",
            (unsigned)p.fan, (unsigned)p.damper, (int)p.sp,
            p.lid ? "true" : "false");

        csv += line;
    }

    size_t envLen = 0;
    char* envelope = bbq_protocol::buildCSVDownloadEnvelope(csv.c_str(), csv.length(), &envLen);
    if (envelope) {
        mg_ws_send(c, envelope, envLen, WEBSOCKET_OP_TEXT);
        free(envelope);
    }
}

void SimWebServer::handleMessage(struct mg_connection* c, const char* data, size_t len) {
    bbq_protocol::ParsedCommand cmd = bbq_protocol::parseCommand(data, len);

    switch (cmd.type) {
        case bbq_protocol::CmdType::SET_SP:
            if (_onSetpoint) _onSetpoint(cmd.setpoint);
            _setpoint = cmd.setpoint;
            printf("[WEB] Setpoint changed to %.0f via web UI\n", cmd.setpoint);
            break;

        case bbq_protocol::CmdType::ALARM:
            if (cmd.hasMeat1Target) {
                _meat1Target = cmd.meat1Target;
                if (_onAlarm) _onAlarm("meat1", cmd.meat1Target);
                printf("[WEB] Meat1 target set to %.0f\n", cmd.meat1Target);
            }
            if (cmd.hasMeat2Target) {
                _meat2Target = cmd.meat2Target;
                if (_onAlarm) _onAlarm("meat2", cmd.meat2Target);
                printf("[WEB] Meat2 target set to %.0f\n", cmd.meat2Target);
            }
            if (cmd.hasPitBand && _onAlarm) {
                _onAlarm("pitBand", cmd.pitBand);
            }
            break;

        case bbq_protocol::CmdType::SESSION_NEW:
            printf("[WEB] New session requested\n");
            if (_onNewSession) _onNewSession();
            resetSession();
            break;

        case bbq_protocol::CmdType::SESSION_DOWNLOAD:
            printf("[WEB] CSV download requested\n");
            sendCSVDownload(c);
            break;

        case bbq_protocol::CmdType::SET_FAN_MODE:
            printf("[WEB] Fan mode changed to %s\n", cmd.fanMode);
            if (_onFanMode) _onFanMode(cmd.fanMode);
            break;

        default:
            break;
    }
}

// Static mongoose event handler
void SimWebServer::eventHandler(struct mg_connection* c, int ev, void* ev_data) {
    SimWebServer* self = g_simWebServer;
    if (!self) return;

    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message* hm = (struct mg_http_message*)ev_data;

        // WebSocket upgrade on /ws
        if (mg_match(hm->uri, mg_str("/ws"), nullptr)) {
            mg_ws_upgrade(c, hm, nullptr);
            return;
        }

        // Version API endpoint
        if (mg_match(hm->uri, mg_str("/api/version"), nullptr)) {
            mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                          "{\"version\":\"%s\",\"board\":\"simulator\"}", FIRMWARE_VERSION);
            return;
        }

        // Serve static files from firmware/data/
        struct mg_http_serve_opts opts;
        memset(&opts, 0, sizeof(opts));
        opts.root_dir = self->_staticDir;
        opts.ssi_pattern = nullptr;
        mg_http_serve_dir(c, hm, &opts);
    }
    else if (ev == MG_EV_WS_OPEN) {
        printf("[WEB] WebSocket client connected\n");
        // Send history on connect
        self->sendHistory(c);
    }
    else if (ev == MG_EV_WS_MSG) {
        struct mg_ws_message* wm = (struct mg_ws_message*)ev_data;
        // Only handle text frames
        if ((wm->flags & 0x0F) == WEBSOCKET_OP_TEXT) {
            self->handleMessage(c, wm->data.buf, wm->data.len);
        }
    }
    else if (ev == MG_EV_CLOSE) {
        if (c->is_websocket) {
            printf("[WEB] WebSocket client disconnected\n");
        }
    }
}

#endif // SIMULATOR_BUILD
