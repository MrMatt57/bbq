#pragma once

#include <cstdint>
#include <cstddef>

namespace bbq_protocol {

// Data for building a periodic data message
struct DataPayload {
    uint32_t ts;
    float pit, meat1, meat2;       // NAN = disconnected, -1 = shorted
    uint8_t fan, damper;
    float sp;
    bool lid;
    float meat1Target, meat2Target; // 0 = not set
    uint32_t est;                   // 0 = not available
    const char* fanMode;            // "fan_only", "fan_and_damper", "damper_primary"
    const char* errors[8];
    uint8_t errorCount;
};

// Single point for history replay
struct HistoryPoint {
    uint32_t ts;
    float pit, meat1, meat2;       // NAN = disconnected
    uint8_t fan, damper;
    float sp;
    bool lid;
};

// Parsed incoming command
enum class CmdType { SET_SP, ALARM, SESSION_NEW, SESSION_DOWNLOAD, SET_FAN_MODE, UNKNOWN };
struct ParsedCommand {
    CmdType type;
    float setpoint;
    float meat1Target, meat2Target, pitBand;
    bool hasMeat1Target, hasMeat2Target, hasPitBand;
    char format[8]; // "csv" or "json"
    char fanMode[20]; // "fan_only", "fan_and_damper", "damper_primary"
};

// Returns bytes written to buf (excluding null terminator)
size_t buildDataMessage(char* buf, size_t bufSize, const DataPayload& d);
size_t buildSessionReset(char* buf, size_t bufSize, float setpoint);

// Dynamic allocation — caller must free() returned pointer
char* buildHistoryMessage(const HistoryPoint* points, size_t count,
                          float sp, float meat1Target, float meat2Target,
                          size_t* outLen);
char* buildCSVDownloadEnvelope(const char* csvData, size_t csvLen, size_t* outLen);

// Parse an incoming JSON command
ParsedCommand parseCommand(const char* data, size_t len);

} // namespace bbq_protocol
