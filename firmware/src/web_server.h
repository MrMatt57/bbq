#pragma once

#include "config.h"
#include "web_protocol.h"
#include <stdint.h>

#ifndef NATIVE_BUILD
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#endif

// Forward declarations for external module references
class TempManager;
class PidController;
class FanController;
class ServoController;
class ConfigManager;
class CookSession;
class AlarmManager;
class ErrorManager;

// Callback types for commands received from WebSocket clients
typedef void (*SetpointCallback)(float setpoint);
typedef void (*AlarmCallback)(const char* probe, float target);
typedef void (*SessionCallback)(const char* action, const char* format);
typedef void (*FanModeCallback)(const char* mode);

class BBQWebServer {
public:
    BBQWebServer();

    // Get the underlying AsyncWebServer pointer (e.g. for OTA registration)
#ifndef NATIVE_BUILD
    AsyncWebServer* getAsyncServer() const { return _server; }
#endif

    // Initialize HTTP server and WebSocket. Call once from setup().
    void begin();

    // Periodic update: broadcast data to all connected WebSocket clients.
    // Respects WS_SEND_INTERVAL internally. Call every loop().
    void update();

    // Set references to other modules for building data messages
    void setModules(TempManager* temp, PidController* pid, FanController* fan,
                    ServoController* servo, ConfigManager* config, CookSession* session,
                    AlarmManager* alarm, ErrorManager* error);

    // Set callbacks for incoming WebSocket commands
    void onSetpoint(SetpointCallback cb)  { _onSetpoint = cb; }
    void onAlarm(AlarmCallback cb)        { _onAlarm = cb; }
    void onSession(SessionCallback cb)    { _onSession = cb; }
    void onFanMode(FanModeCallback cb)    { _onFanMode = cb; }

    // Send history replay to a specific client on connect
    void sendHistory(uint8_t clientId);

    // Force-send data to all clients immediately (bypasses interval)
    void broadcastNow();

    // Get number of connected WebSocket clients
    uint8_t getClientCount() const;

    // Set the current setpoint (for inclusion in data messages)
    void setSetpoint(float sp) { _setpoint = sp; }

    // Set estimated completion time (epoch, or 0 for null)
    void setEstimatedTime(uint32_t est) { _estimatedTime = est; }

private:
    // Build the data payload from current sensor/PID state
    bbq_protocol::DataPayload buildDataPayload();

    // Handle incoming WebSocket messages
    void handleWebSocketMessage(uint8_t clientId, const char* data, size_t len);

    // WebSocket event handler
    void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                   AwsEventType type, void* arg, uint8_t* data, size_t len);

#ifndef NATIVE_BUILD
    AsyncWebServer* _server;
    AsyncWebSocket* _ws;
#endif

    // Module references
    TempManager*    _temp;
    PidController*  _pid;
    FanController*  _fan;
    ServoController* _servo;
    ConfigManager*  _config;
    CookSession*    _session;
    AlarmManager*   _alarm;
    ErrorManager*   _error;

    // State
    float    _setpoint;
    uint32_t _estimatedTime;

    // Timing
    unsigned long _lastBroadcastMs;

    // Callbacks
    SetpointCallback _onSetpoint;
    AlarmCallback    _onAlarm;
    SessionCallback  _onSession;
    FanModeCallback  _onFanMode;
};
