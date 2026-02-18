#include "web_protocol.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace bbq_protocol {

// ---------------------------------------------------------------------------
// buildDataMessage — periodic data broadcast
// ---------------------------------------------------------------------------
// Helper: format float with 1 decimal place into a small buffer
static void fmtTemp(char* out, size_t sz, float val) {
    snprintf(out, sz, "%.1f", val);
}

size_t buildDataMessage(char* buf, size_t bufSize, const DataPayload& d) {
    JsonDocument doc;
    doc["type"] = "data";
    doc["ts"] = d.ts;

    // Temperatures: NAN → null, -1 → -1 (shorted), else value with 1 decimal
    // Use separate buffers since serialized() stores a pointer until serializeJson
    char pitStr[16], m1Str[16], m2Str[16];

    if (std::isnan(d.pit))    doc["pit"] = (const char*)nullptr;
    else if (d.pit == -1.0f)  doc["pit"] = -1;
    else { fmtTemp(pitStr, sizeof(pitStr), d.pit); doc["pit"] = serialized(pitStr); }

    if (std::isnan(d.meat1))   doc["meat1"] = (const char*)nullptr;
    else if (d.meat1 == -1.0f) doc["meat1"] = -1;
    else { fmtTemp(m1Str, sizeof(m1Str), d.meat1); doc["meat1"] = serialized(m1Str); }

    if (std::isnan(d.meat2))   doc["meat2"] = (const char*)nullptr;
    else if (d.meat2 == -1.0f) doc["meat2"] = -1;
    else { fmtTemp(m2Str, sizeof(m2Str), d.meat2); doc["meat2"] = serialized(m2Str); }

    doc["fan"] = (int)d.fan;
    doc["damper"] = (int)d.damper;
    doc["sp"] = (int)d.sp;
    doc["lid"] = d.lid;
    if (d.fanMode) doc["fanMode"] = d.fanMode;

    // Meat targets: 0 → null
    if (d.meat1Target > 0)  doc["meat1Target"] = (int)d.meat1Target;
    else                    doc["meat1Target"] = (const char*)nullptr;

    if (d.meat2Target > 0)  doc["meat2Target"] = (int)d.meat2Target;
    else                    doc["meat2Target"] = (const char*)nullptr;

    // Estimated done time
    if (d.est > 0)  doc["est"] = d.est;
    else            doc["est"] = (const char*)nullptr;

    // Errors array
    JsonArray errors = doc["errors"].to<JsonArray>();
    for (uint8_t i = 0; i < d.errorCount && i < 8; i++) {
        errors.add(d.errors[i]);
    }

    return serializeJson(doc, buf, bufSize);
}

// ---------------------------------------------------------------------------
// buildSessionReset — server confirms new session
// ---------------------------------------------------------------------------
size_t buildSessionReset(char* buf, size_t bufSize, float setpoint) {
    JsonDocument doc;
    doc["type"] = "session";
    doc["action"] = "reset";
    doc["sp"] = (int)setpoint;
    return serializeJson(doc, buf, bufSize);
}

// ---------------------------------------------------------------------------
// buildHistoryMessage — replay session data on connect
//
// Builds JSON incrementally with snprintf to avoid ArduinoJson overhead
// for potentially hundreds of data points (~110 bytes per point).
// ---------------------------------------------------------------------------
char* buildHistoryMessage(const HistoryPoint* points, size_t count,
                          float sp, float meat1Target, float meat2Target,
                          size_t* outLen) {
    // Estimate buffer size: envelope ~128 bytes, ~120 bytes per point
    size_t estSize = 256 + count * 130;
    char* buf = (char*)malloc(estSize);
    if (!buf) {
        if (outLen) *outLen = 0;
        return nullptr;
    }

    size_t pos = 0;

    // Header
    pos += snprintf(buf + pos, estSize - pos,
        "{\"type\":\"history\",\"sp\":%d", (int)sp);

    if (meat1Target > 0)
        pos += snprintf(buf + pos, estSize - pos, ",\"meat1Target\":%d", (int)meat1Target);
    else
        pos += snprintf(buf + pos, estSize - pos, ",\"meat1Target\":null");

    if (meat2Target > 0)
        pos += snprintf(buf + pos, estSize - pos, ",\"meat2Target\":%d", (int)meat2Target);
    else
        pos += snprintf(buf + pos, estSize - pos, ",\"meat2Target\":null");

    pos += snprintf(buf + pos, estSize - pos, ",\"data\":[");

    // Data points
    for (size_t i = 0; i < count; i++) {
        const HistoryPoint& p = points[i];
        if (i > 0) buf[pos++] = ',';

        // Ensure buffer space (realloc if needed)
        if (pos + 200 > estSize) {
            estSize *= 2;
            char* newBuf = (char*)realloc(buf, estSize);
            if (!newBuf) { free(buf); if (outLen) *outLen = 0; return nullptr; }
            buf = newBuf;
        }

        pos += snprintf(buf + pos, estSize - pos, "{\"ts\":%u", (unsigned)p.ts);

        // Temperatures: NAN → null
        if (std::isnan(p.pit))   pos += snprintf(buf + pos, estSize - pos, ",\"pit\":null");
        else                     pos += snprintf(buf + pos, estSize - pos, ",\"pit\":%.1f", p.pit);

        if (std::isnan(p.meat1)) pos += snprintf(buf + pos, estSize - pos, ",\"meat1\":null");
        else                     pos += snprintf(buf + pos, estSize - pos, ",\"meat1\":%.1f", p.meat1);

        if (std::isnan(p.meat2)) pos += snprintf(buf + pos, estSize - pos, ",\"meat2\":null");
        else                     pos += snprintf(buf + pos, estSize - pos, ",\"meat2\":%.1f", p.meat2);

        pos += snprintf(buf + pos, estSize - pos,
            ",\"fan\":%u,\"damper\":%u,\"sp\":%d,\"lid\":%s}",
            (unsigned)p.fan, (unsigned)p.damper, (int)p.sp,
            p.lid ? "true" : "false");
    }

    pos += snprintf(buf + pos, estSize - pos, "]}");

    if (outLen) *outLen = pos;
    return buf;
}

// ---------------------------------------------------------------------------
// buildCSVDownloadEnvelope — wrap CSV data in JSON for WebSocket delivery
// ---------------------------------------------------------------------------
char* buildCSVDownloadEnvelope(const char* csvData, size_t csvLen, size_t* outLen) {
    // Worst case: every char needs escaping (unlikely), plus envelope
    size_t estSize = 128 + csvLen * 2;
    char* buf = (char*)malloc(estSize);
    if (!buf) {
        if (outLen) *outLen = 0;
        return nullptr;
    }

    size_t pos = 0;
    pos += snprintf(buf + pos, estSize - pos,
        "{\"type\":\"session\",\"action\":\"download\",\"format\":\"csv\",\"data\":\"");

    // Escape the CSV data for JSON string
    for (size_t i = 0; i < csvLen; i++) {
        if (pos + 10 > estSize) {
            estSize *= 2;
            char* newBuf = (char*)realloc(buf, estSize);
            if (!newBuf) { free(buf); if (outLen) *outLen = 0; return nullptr; }
            buf = newBuf;
        }

        char c = csvData[i];
        switch (c) {
            case '"':  buf[pos++] = '\\'; buf[pos++] = '"';  break;
            case '\\': buf[pos++] = '\\'; buf[pos++] = '\\'; break;
            case '\n': buf[pos++] = '\\'; buf[pos++] = 'n';  break;
            case '\r': buf[pos++] = '\\'; buf[pos++] = 'r';  break;
            case '\t': buf[pos++] = '\\'; buf[pos++] = 't';  break;
            default:   buf[pos++] = c;                        break;
        }
    }

    pos += snprintf(buf + pos, estSize - pos, "\"}");

    if (outLen) *outLen = pos;
    return buf;
}

// ---------------------------------------------------------------------------
// parseCommand — parse incoming JSON command from client
// ---------------------------------------------------------------------------
ParsedCommand parseCommand(const char* data, size_t len) {
    ParsedCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = CmdType::UNKNOWN;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) return cmd;

    const char* type = doc["type"] | "";

    if (strcmp(type, "set") == 0) {
        cmd.type = CmdType::SET_SP;
        cmd.setpoint = doc["sp"].as<float>();
    }
    else if (strcmp(type, "alarm") == 0) {
        cmd.type = CmdType::ALARM;

        if (doc["meat1Target"].is<float>()) {
            cmd.hasMeat1Target = true;
            cmd.meat1Target = doc["meat1Target"].as<float>();
        } else if (doc["meat1Target"].isNull()) {
            cmd.hasMeat1Target = true;
            cmd.meat1Target = 0;
        }

        if (doc["meat2Target"].is<float>()) {
            cmd.hasMeat2Target = true;
            cmd.meat2Target = doc["meat2Target"].as<float>();
        } else if (doc["meat2Target"].isNull()) {
            cmd.hasMeat2Target = true;
            cmd.meat2Target = 0;
        }

        if (doc["pitBand"].is<float>()) {
            cmd.hasPitBand = true;
            cmd.pitBand = doc["pitBand"].as<float>();
        }
    }
    else if (strcmp(type, "config") == 0) {
        const char* fm = doc["fanMode"] | "";
        if (fm[0] != '\0') {
            cmd.type = CmdType::SET_FAN_MODE;
            strncpy(cmd.fanMode, fm, sizeof(cmd.fanMode) - 1);
            cmd.fanMode[sizeof(cmd.fanMode) - 1] = '\0';
        }
    }
    else if (strcmp(type, "session") == 0) {
        const char* action = doc["action"] | "";
        if (strcmp(action, "new") == 0) {
            cmd.type = CmdType::SESSION_NEW;
        } else if (strcmp(action, "download") == 0) {
            cmd.type = CmdType::SESSION_DOWNLOAD;
            const char* fmt = doc["format"] | "csv";
            strncpy(cmd.format, fmt, sizeof(cmd.format) - 1);
            cmd.format[sizeof(cmd.format) - 1] = '\0';
        }
    }

    return cmd;
}

} // namespace bbq_protocol
