#pragma once

#ifdef SIMULATOR_BUILD

#include "../web_protocol.h"
#include <vector>
#include <cstdint>

// Forward declare mongoose struct
struct mg_mgr;

class SimWebServer {
public:
    SimWebServer();
    ~SimWebServer();

    // Initialize HTTP server + WebSocket on given port.
    // staticDir: path to firmware/data/ for serving web UI files.
    void begin(int port, const char* staticDir);

    // Non-blocking tick — call from main loop
    void tick();

    // Broadcast a data message to all connected WebSocket clients
    void broadcastData(const bbq_protocol::DataPayload& data);

    // Accumulate a history point (called each sim update)
    void addHistoryPoint(const bbq_protocol::HistoryPoint& point);

    // Clear history (new session)
    void clearHistory();

    // Perform a new-session reset: clear history + broadcast to all WS clients
    void resetSession();

    // Current state for history envelope
    void setState(float setpoint, float meat1Target, float meat2Target);

    // Callbacks for incoming commands
    void onSetpoint(void (*cb)(float));
    void onAlarm(void (*cb)(const char*, float));
    void onNewSession(void (*cb)());
    void onSessionDownload(void (*cb)());
    void onFanMode(void (*cb)(const char*));

    // Number of connected WebSocket clients
    int getClientCount() const;

    // Static event handler (mongoose callback)
    static void eventHandler(struct mg_connection* c, int ev, void* ev_data);

private:
    struct mg_mgr* _mgr;
    char _staticDir[256];
    int _port;

    // Session history for replay
    std::vector<bbq_protocol::HistoryPoint> _history;
    float _setpoint;
    float _meat1Target;
    float _meat2Target;

    // Callbacks
    void (*_onSetpoint)(float);
    void (*_onAlarm)(const char*, float);
    void (*_onNewSession)();
    void (*_onSessionDownload)();
    void (*_onFanMode)(const char*);

    // Handle incoming WS message
    void handleMessage(struct mg_connection* c, const char* data, size_t len);

    // Send history to a newly connected client
    void sendHistory(struct mg_connection* c);

    // Build and send CSV download to a client
    void sendCSVDownload(struct mg_connection* c);
};

// Global pointer for mongoose static callback to access instance
extern SimWebServer* g_simWebServer;

#endif // SIMULATOR_BUILD
